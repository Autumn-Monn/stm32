#ifndef ESP8266_H
#define ESP8266_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ---- WiFi 配置 ---- */
#define ESP_WIFI_SSID      "HONORMagic8"
#define ESP_WIFI_PASS      "99999999"

/* ---- OneNET MQTT 配置 ---- */
#define MQTT_PRODUCT_ID    "lGAVyi7tu9"
#define MQTT_DEVICE_NAME   "temperature"
#define MQTT_TOKEN         "version=2018-10-31&res=products%2FlGAVyi7tu9%2Fdevices%2Ftemperature&et=1904803798&method=md5&sign=y0U3QpZP5%2BE7uD%2BwuS177w%3D%3D"
#define MQTT_BROKER        "mqtts.heclouds.com"
#define MQTT_PORT          1883

/* ---- 上报间隔 ---- */
#define MQTT_PUBLISH_INTERVAL_MS  30000U      // 30 秒上报一次

typedef enum
{
  ESP_CONN_IDLE = 0,
  ESP_CONN_HW_RESET,
  ESP_CONN_WAIT_READY,
  ESP_CONN_AT_TEST,
  ESP_CONN_ATE0,
  ESP_CONN_CWMODE,
  ESP_CONN_CWJAP,
  ESP_CONN_WIFI_OK,
  ESP_CONN_MQTT_USERCFG,
  ESP_CONN_MQTT_CONN,
  ESP_CONN_MQTT_WAIT,
  ESP_CONN_MQTT_SUB1,
  ESP_CONN_MQTT_SUB2,
  ESP_CONN_MQTT_OK,
  ESP_PUB_CMD,
  ESP_PUB_DATA,
  ESP_REPLY_CMD,
  ESP_REPLY_DATA,
  ESP_CONN_ERROR
} esp_conn_state_t;

void             esp8266_init(void);
void             esp8266_task(void);
uint8_t          esp8266_is_connected(void);
uint8_t          esp8266_mqtt_is_connected(void);
void             esp8266_rx_callback(void);
esp_conn_state_t esp8266_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP8266_H */
