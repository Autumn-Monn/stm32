#include "esp8266.h"
#include "usart.h"
#include "control.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart1;

/* ---- 时间常数 ---- */
#define ESP_RX_BUF_SIZE    512U       // 接收缓冲区大小
#define HW_RESET_LOW_MS    100U       // 硬件复位拉低时间
#define HW_RESET_BOOT_MS   2000U      // 硬件复位后等待 ESP8266 启动时间
#define RECONNECT_DELAY_MS 5000U      // 重连延迟时间
#define AT_RETRY_MAX       3U         // AT 命令重试次数
#define CWJAP_RETRY_MAX    2U         // AT+CWJAP 重试次数
#define MQTT_CONN_RETRY_MAX  2U       // AT+MQTTCONN 重试次数
#define MQTT_SETTLE_MS       500U     // MQTT 连接稳定等待时间
#define MQTT_PUB_TIMEOUT_MS 6000U     // MQTTPUBRAW 命令超时等待时间

/* ---- RX 线性缓冲区 ---- */
static uint8_t           g_rx_byte;
static char              g_rx_buf[ESP_RX_BUF_SIZE];
static volatile uint16_t g_rx_len = 0U;

/* ---- AT 引擎状态 ---- */
typedef enum
{
  AT_IDLE = 0,
  AT_WAIT_RESP
} at_state_t;

static at_state_t  g_at_state   = AT_IDLE;
static const char *g_at_expect  = NULL;
static uint32_t    g_at_timeout = 0U;
static uint32_t    g_at_start   = 0U;

/* ---- 连接序列状态机 ---- */
static esp_conn_state_t g_conn_state = ESP_CONN_IDLE;
static uint32_t         g_conn_tick  = 0U;
static uint8_t          g_retry_cnt  = 0U;

/* ---- MQTT 发布状态 ---- */
static uint32_t g_pub_tick = 0U;        /* 上次发布时间戳 */
static char     g_pub_payload[256];     /* 上报数据载荷缓冲区 */

/* ---- MQTT 回复状态 ---- */
static char     g_reply_payload[64];    /* 下行回复载荷缓存 */
static uint8_t  g_reply_pending = 0U;   /* 待发送回复标志 */

/* ================================================================
 *  内部函数 - AT 引擎
 * ================================================================ */

/* 清空接收缓冲区（关中断保护） */
static void rx_buf_clear(void)
{
  __disable_irq();
  g_rx_len    = 0U;
  g_rx_buf[0] = '\0';
  __enable_irq();
}

#define UART_TX_TIMEOUT_MS  50U

/* 发送 AT 命令并等待期望响应 */
static void at_send(const char *cmd, const char *expect, uint32_t timeout_ms)
{
  rx_buf_clear();
  HAL_UART_Transmit(&huart1, (uint8_t *)cmd,
                     (uint16_t)strlen(cmd), UART_TX_TIMEOUT_MS);
  g_at_expect  = expect;
  g_at_timeout = timeout_ms;
  g_at_start   = HAL_GetTick();
  g_at_state   = AT_WAIT_RESP;
}

/* 检查 AT 响应：返回 1=匹配期望，2=超时，0=等待中 */
static uint8_t at_check(void)
{
  if (g_at_state != AT_WAIT_RESP)
  {
    return 0U;
  }

  if (g_at_expect != NULL && strstr(g_rx_buf, g_at_expect) != NULL)
  {
    g_at_state = AT_IDLE;
    return 1U;
  }

  if ((HAL_GetTick() - g_at_start) >= g_at_timeout)
  {
    g_at_state = AT_IDLE;
    return 2U;
  }

  return 0U;
}

/* ================================================================
 *  内部函数 - WiFi 连接
 * ================================================================ */

/* 硬件复位 ESP8266（拉低 RST 引脚） */
static void hw_reset_start(void)
{
  HAL_GPIO_WritePin(ESP8266_RST_GPIO_Port, ESP8266_RST_Pin, GPIO_PIN_RESET);
  g_conn_tick  = HAL_GetTick();
  g_conn_state = ESP_CONN_HW_RESET;
  g_retry_cnt  = 0U;
}

/* 发送 AT+CWJAP 连接 WiFi */
static void start_cwjap(void)
{
  static char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n",
           ESP_WIFI_SSID, ESP_WIFI_PASS);
  at_send(cmd, "WIFI GOT IP", 15000);
  g_conn_state = ESP_CONN_CWJAP;
}

/* ================================================================
 *  内部函数 - MQTT 连接
 * ================================================================ */

/* 配置 MQTT 用户信息（产品ID、设备名、密钥） */
static void start_mqtt_usercfg(void)
{
  static char cmd[320];
  snprintf(cmd, sizeof(cmd),
    "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
    MQTT_DEVICE_NAME, MQTT_PRODUCT_ID, MQTT_TOKEN);
  at_send(cmd, "OK", 6000);
  g_conn_state = ESP_CONN_MQTT_USERCFG;
}

/* 连接 MQTT 服务器 */
static void start_mqtt_conn(void)
{
  static char cmd[80];
  snprintf(cmd, sizeof(cmd),
    "AT+MQTTCONN=0,\"%s\",%d,1\r\n", MQTT_BROKER, MQTT_PORT);
  at_send(cmd, "+MQTTCONNECTED", 15000);
  g_conn_state = ESP_CONN_MQTT_CONN;
}

/* 订阅属性设置回复主题 */
static void start_mqtt_sub1(void)
{
  static char cmd[96];
  snprintf(cmd, sizeof(cmd),
    "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/post/reply\",0\r\n",
    MQTT_PRODUCT_ID, MQTT_DEVICE_NAME);
  at_send(cmd, "OK", 6000);
  g_conn_state = ESP_CONN_MQTT_SUB1;
}

/* 订阅属性设置下行命令主题 */
static void start_mqtt_sub2(void)
{
  static char cmd[96];
  snprintf(cmd, sizeof(cmd),
    "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",0\r\n",
    MQTT_PRODUCT_ID, MQTT_DEVICE_NAME);
  at_send(cmd, "OK", 6000);
  g_conn_state = ESP_CONN_MQTT_SUB2;
}

/* ================================================================
 *  内部函数 - MQTT 数据上报
 * ================================================================ */

/* 构造 MQTT 数据上报 JSON 载荷 */
static void mqtt_build_payload(void)
{
  int16_t temp_int  = g_ctrl.temp_raw / 16;
  int16_t temp_frac = (g_ctrl.temp_raw % 16) * 10 / 16;
  if (temp_frac < 0) temp_frac = -temp_frac;

  unsigned soil_pct = (unsigned)g_ctrl.soil_pct;

  snprintf(g_pub_payload, sizeof(g_pub_payload),
    "{\"id\":\"123\",\"params\":{"
    "\"temperature\":{\"value\":%d.%d},"
    "\"soil_moisture\":{\"value\":%u},"
    "\"pump_state\":{\"value\":%s},"
    "\"fan_state\":{\"value\":%s},"
    "\"mode\":{\"value\":%u},"
    "\"status\":{\"value\":%u}}}",
    (int)temp_int, (int)temp_frac,
    soil_pct,
    g_ctrl.pump_on ? "true" : "false",
    g_ctrl.fan_on  ? "true" : "false",
    (unsigned)g_ctrl.mode,
    (unsigned)g_ctrl.status);
}

/* 发送 MQTTPUBRAW 命令，启动数据上报 */
static void mqtt_start_pub_cmd(void)
{
  static char cmd[128];
  uint16_t payload_len = (uint16_t)strlen(g_pub_payload);
  snprintf(cmd, sizeof(cmd),
    "AT+MQTTPUBRAW=0,\"$sys/%s/%s/thing/property/post\",%u,0,0\r\n",
    MQTT_PRODUCT_ID, MQTT_DEVICE_NAME, (unsigned)payload_len);
  at_send(cmd, ">", MQTT_PUB_TIMEOUT_MS);
  g_conn_state = ESP_PUB_CMD;
}

/* 发送 MQTTPUBRAW 命令，返回下行指令回复 */
static void mqtt_start_reply_cmd(void)
{
  static char cmd[128];
  uint16_t payload_len = (uint16_t)strlen(g_reply_payload);
  snprintf(cmd, sizeof(cmd),
    "AT+MQTTPUBRAW=0,\"$sys/%s/%s/thing/property/set_reply\",%u,0,0\r\n",
    MQTT_PRODUCT_ID, MQTT_DEVICE_NAME, (unsigned)payload_len);
  at_send(cmd, ">", MQTT_PUB_TIMEOUT_MS);
  g_conn_state = ESP_REPLY_CMD;
}

/* ================================================================
 *  内部函数 - MQTT 下行命令解析
 * ================================================================ */

/* 在 json 中查找 "key": 后面的值，返回: 1=true, 0=false/0, >0=整数, -1=未找到 */
static int mqtt_parse_value(const char *json, const char *key_colon)
{
  char *p = strstr(json, key_colon);
  if (p == NULL) return -1;
  p += strlen(key_colon);
  while (*p == ' ') p++;
  if (strncmp(p, "true", 4) == 0) return 1;
  if (strncmp(p, "false", 5) == 0) return 0;
  return atoi(p);
}

/* 检查并解析收到的下行属性设置命令 */
static void mqtt_check_downlink(void)
{
  char *p = strstr(g_rx_buf, "+MQTTSUBRECV:");
  if (p == NULL) return;

  if (strstr(p, "property/set") == NULL)
  {
    rx_buf_clear();
    return;
  }

  char *json = strchr(p, '{');
  if (json == NULL)
  {
    rx_buf_clear();
    return;
  }

  int val;

  val = mqtt_parse_value(json, "\"mode\":");
  if (val >= 0)
  {
    if (val == 0) g_ctrl.mode = SYS_MODE_AUTO;
    else if (val == 1) g_ctrl.mode = SYS_MODE_MANUAL;
  }

  val = mqtt_parse_value(json, "\"pump_state\":");
  if (val >= 0)
  {
    g_ctrl.pump_on = (uint8_t)val;
    if (g_ctrl.mode == SYS_MODE_AUTO) g_ctrl.mode = SYS_MODE_MANUAL;
  }

  val = mqtt_parse_value(json, "\"fan_state\":");
  if (val >= 0)
  {
    g_ctrl.fan_on = (uint8_t)val;
    if (g_ctrl.mode == SYS_MODE_AUTO) g_ctrl.mode = SYS_MODE_MANUAL;
  }

  /* 提取请求 id，构建回复 */
  char *id_start = strstr(json, "\"id\":\"");
  if (id_start != NULL)
  {
    id_start += 6;
    char *id_end = strchr(id_start, '"');
    if (id_end != NULL)
    {
      int id_len = (int)(id_end - id_start);
      if (id_len > 10) id_len = 10;
      char id_buf[12];
      memcpy(id_buf, id_start, (size_t)id_len);
      id_buf[id_len] = '\0';
      snprintf(g_reply_payload, sizeof(g_reply_payload),
        "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id_buf);
      g_reply_pending = 1U;
    }
  }

  rx_buf_clear();
}

/* ================================================================
 *  内部函数 - 断线检测
 * ================================================================ */

/* 检测 MQTT 断线或 WiFi 断连，发现后进入错误恢复状态 */
static uint8_t mqtt_check_disconnect(void)
{
  if (strstr(g_rx_buf, "+MQTTDISCONNECTED:") != NULL ||
      strstr(g_rx_buf, "WIFI DISCONNECT") != NULL)
  {
    rx_buf_clear();
    g_conn_tick  = HAL_GetTick();
    g_conn_state = ESP_CONN_ERROR;
    return 1U;
  }
  return 0U;
}

/* ================================================================
 *  公共 API 接口
 * ================================================================ */

/* 初始化 ESP8266：使能 RST、清缓冲区、启动UART接收中断、硬件复位 */
void esp8266_init(void)
{
  HAL_GPIO_WritePin(ESP8266_RST_GPIO_Port, ESP8266_RST_Pin, GPIO_PIN_SET);
  rx_buf_clear();
  g_at_state = AT_IDLE;

  HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1);

  hw_reset_start();
}

/* UART 接收中断回调：逐字节存入环形缓冲区 */
void esp8266_rx_callback(void)
{
  if (g_rx_len < (ESP_RX_BUF_SIZE - 1U))
  {
    g_rx_buf[g_rx_len++] = (char)g_rx_byte;
    g_rx_buf[g_rx_len]   = '\0';
  }
  HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1);
}

/*===============================================================*
 *  主循环任务：驱动 WiFi 连接、MQTT 通信与数据上报
 *===============================================================*/

void esp8266_task(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t  resp;

  switch (g_conn_state)
  {
  case ESP_CONN_IDLE:
    break;

  /* ---- WiFi 连接阶段 ---- */

  case ESP_CONN_HW_RESET:
    if ((now - g_conn_tick) >= HW_RESET_LOW_MS)
    {
      HAL_GPIO_WritePin(ESP8266_RST_GPIO_Port, ESP8266_RST_Pin,
                        GPIO_PIN_SET);
      g_conn_tick  = now;
      g_conn_state = ESP_CONN_WAIT_READY;
    }
    break;

  case ESP_CONN_WAIT_READY:
    if ((now - g_conn_tick) >= HW_RESET_BOOT_MS)
    {
      g_retry_cnt = 0U;
      at_send("AT\r\n", "OK", 3000);
      g_conn_state = ESP_CONN_AT_TEST;
    }
    break;

  case ESP_CONN_AT_TEST:
    resp = at_check();
    if (resp == 1U)
    {
      at_send("ATE0\r\n", "OK", 2000);
      g_conn_state = ESP_CONN_ATE0;
    }
    else if (resp == 2U)
    {
      if (++g_retry_cnt < AT_RETRY_MAX)
      {
        at_send("AT\r\n", "OK", 3000);
      }
      else
      {
        hw_reset_start();
      }
    }
    break;

  case ESP_CONN_ATE0:
    resp = at_check();
    if (resp == 1U)
    {
      at_send("AT+CWMODE=1\r\n", "OK", 2000);
      g_conn_state = ESP_CONN_CWMODE;
    }
    else if (resp == 2U)
    {
      hw_reset_start();
    }
    break;

  case ESP_CONN_CWMODE:
    resp = at_check();
    if (resp == 1U)
    {
      g_retry_cnt = 0U;
      start_cwjap();
    }
    else if (resp == 2U)
    {
      hw_reset_start();
    }
    break;

  case ESP_CONN_CWJAP:
    resp = at_check();
    if (resp == 1U)
    {
      g_conn_state = ESP_CONN_WIFI_OK;
    }
    else if (resp == 2U)
    {
      if (++g_retry_cnt < CWJAP_RETRY_MAX)
      {
        start_cwjap();
      }
      else
      {
        g_conn_tick  = now;
        g_conn_state = ESP_CONN_ERROR;
      }
    }
    break;

  /* ---- MQTT 连接阶段 ---- */

  case ESP_CONN_WIFI_OK:
    g_retry_cnt = 0U;
    start_mqtt_usercfg();
    break;

  case ESP_CONN_MQTT_USERCFG:
    resp = at_check();
    if (resp == 1U)
    {
      g_retry_cnt = 0U;
      start_mqtt_conn();
    }
    else if (resp == 2U)
    {
      g_conn_tick  = now;
      g_conn_state = ESP_CONN_ERROR;
    }
    break;

  case ESP_CONN_MQTT_CONN:
    resp = at_check();
    if (resp == 1U)
    {
      g_conn_tick  = now;
      g_conn_state = ESP_CONN_MQTT_WAIT;
    }
    else if (resp == 2U)
    {
      if (++g_retry_cnt < MQTT_CONN_RETRY_MAX)
      {
        start_mqtt_conn();
      }
      else
      {
        g_conn_tick  = now;
        g_conn_state = ESP_CONN_ERROR;
      }
    }
    break;

  case ESP_CONN_MQTT_WAIT:
    if ((now - g_conn_tick) >= MQTT_SETTLE_MS)
    {
      g_retry_cnt = 0U;
      start_mqtt_sub1();
    }
    break;

  case ESP_CONN_MQTT_SUB1:
    resp = at_check();
    if (resp == 1U)
    {
      g_retry_cnt = 0U;
      start_mqtt_sub2();
    }
    else if (resp == 2U)
    {
      if (++g_retry_cnt < MQTT_CONN_RETRY_MAX)
      {
        start_mqtt_sub1();
      }
      else
      {
        g_retry_cnt = 0U;
        start_mqtt_sub2();
      }
    }
    break;

  case ESP_CONN_MQTT_SUB2:
    resp = at_check();
    if (resp == 1U)
    {
      g_pub_tick   = 0U;
      g_conn_state = ESP_CONN_MQTT_OK;
    }
    else if (resp == 2U)
    {
      if (++g_retry_cnt < MQTT_CONN_RETRY_MAX)
      {
        start_mqtt_sub2();
      }
      else
      {
        g_pub_tick   = 0U;
        g_conn_state = ESP_CONN_MQTT_OK;
      }
    }
    break;

  /* ---- MQTT 运行阶段 ---- */

  case ESP_CONN_MQTT_OK:
    if (mqtt_check_disconnect()) break;
    mqtt_check_downlink();

    if (g_reply_pending)
    {
      g_reply_pending = 0U;
      mqtt_start_reply_cmd();
    }
    else if ((now - g_pub_tick) >= MQTT_PUBLISH_INTERVAL_MS)
    {
      mqtt_build_payload();
      mqtt_start_pub_cmd();
    }
    break;

  case ESP_PUB_CMD:
    if (mqtt_check_disconnect()) break;
    resp = at_check();
    if (resp == 1U)
    {
      rx_buf_clear();
      HAL_UART_Transmit(&huart1, (uint8_t *)g_pub_payload,
                        (uint16_t)strlen(g_pub_payload), UART_TX_TIMEOUT_MS);
      g_at_expect  = "OK";
      g_at_timeout = MQTT_PUB_TIMEOUT_MS;
      g_at_start   = HAL_GetTick();
      g_at_state   = AT_WAIT_RESP;
      g_conn_state = ESP_PUB_DATA;
    }
    else if (resp == 2U)
    {
      g_pub_tick   = now;
      g_conn_state = ESP_CONN_MQTT_OK;
    }
    break;

  case ESP_PUB_DATA:
    if (mqtt_check_disconnect()) break;
    resp = at_check();
    if (resp == 1U)
    {
      g_pub_tick   = now;
      g_conn_state = ESP_CONN_MQTT_OK;
    }
    else if (resp == 2U)
    {
      g_pub_tick   = now;
      g_conn_state = ESP_CONN_MQTT_OK;
    }
    break;

  case ESP_REPLY_CMD:
    if (mqtt_check_disconnect()) break;
    resp = at_check();
    if (resp == 1U)
    {
      rx_buf_clear();
      HAL_UART_Transmit(&huart1, (uint8_t *)g_reply_payload,
                        (uint16_t)strlen(g_reply_payload), UART_TX_TIMEOUT_MS);
      g_at_expect  = "OK";
      g_at_timeout = MQTT_PUB_TIMEOUT_MS;
      g_at_start   = HAL_GetTick();
      g_at_state   = AT_WAIT_RESP;
      g_conn_state = ESP_REPLY_DATA;
    }
    else if (resp == 2U)
    {
      g_conn_state = ESP_CONN_MQTT_OK;
    }
    break;

  case ESP_REPLY_DATA:
    if (mqtt_check_disconnect()) break;
    resp = at_check();
    if (resp == 1U || resp == 2U)
    {
      g_conn_state = ESP_CONN_MQTT_OK;
    }
    break;

  /* ---- 错误恢复 ---- */

  case ESP_CONN_ERROR:
    if ((now - g_conn_tick) >= RECONNECT_DELAY_MS)
    {
      hw_reset_start();
    }
    break;
  }
}

/* 查询 ESP8266 是否处于通信正常状态 */
uint8_t esp8266_is_connected(void)
{
  return (g_conn_state == ESP_CONN_MQTT_OK ||
          g_conn_state == ESP_PUB_CMD ||
          g_conn_state == ESP_PUB_DATA ||
          g_conn_state == ESP_REPLY_CMD ||
          g_conn_state == ESP_REPLY_DATA) ? 1U : 0U;
}

/* MQTT 连接状态查询（别名） */
uint8_t esp8266_mqtt_is_connected(void)
{
  return esp8266_is_connected();
}

/* 获取当前连接状态 */
esp_conn_state_t esp8266_get_state(void)
{
  return g_conn_state;
}
