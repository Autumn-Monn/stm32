#include "oled.h"
#include "oled_font.h"
#include "control.h"
#include "esp8266.h"
#include "ds18b20.h"
#include "soil.h"
#include "i2c.h"

extern I2C_HandleTypeDef hi2c1;

#define OLED_DISPLAY_PERIOD_MS  500U

static uint8_t  g_oled_buf[OLED_PAGES][OLED_WIDTH];
static uint32_t g_oled_display_tick = 0U;

/* ---------- low-level I2C communication ---------- */

static void oled_write_cmd(uint8_t cmd)
{
  HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100);
}

static void oled_write_data_buf(uint8_t *data, uint16_t len)
{
  HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, data, len, 100);
}

/* ---------- init / clear / refresh ---------- */

void oled_init(void)
{
  HAL_Delay(100);

  oled_write_cmd(0xAE);
  oled_write_cmd(0x20);
  oled_write_cmd(0x10);
  oled_write_cmd(0xB0);
  oled_write_cmd(0xC8);
  oled_write_cmd(0x00);
  oled_write_cmd(0x10);
  oled_write_cmd(0x40);
  oled_write_cmd(0x81);
  oled_write_cmd(0x7F);
  oled_write_cmd(0xA1);
  oled_write_cmd(0xA6);
  oled_write_cmd(0xA8);
  oled_write_cmd(0x3F);
  oled_write_cmd(0xA4);
  oled_write_cmd(0xD3);
  oled_write_cmd(0x00);
  oled_write_cmd(0xD5);
  oled_write_cmd(0xF0);
  oled_write_cmd(0xD9);
  oled_write_cmd(0x22);
  oled_write_cmd(0xDA);
  oled_write_cmd(0x12);
  oled_write_cmd(0xDB);
  oled_write_cmd(0x20);
  oled_write_cmd(0x8D);
  oled_write_cmd(0x14);
  oled_write_cmd(0xAF);

  oled_clear();
  oled_refresh();

  g_oled_display_tick = HAL_GetTick();
}

void oled_clear(void)
{
  for (uint8_t p = 0U; p < OLED_PAGES; p++)
  {
    for (uint8_t c = 0U; c < OLED_WIDTH; c++)
    {
      g_oled_buf[p][c] = 0x00U;
    }
  }
}

void oled_refresh(void)
{
  for (uint8_t p = 0U; p < OLED_PAGES; p++)
  {
    oled_write_cmd(0xB0U + p);
    oled_write_cmd(0x00U);
    oled_write_cmd(0x10U);
    oled_write_data_buf(g_oled_buf[p], OLED_WIDTH);
  }
}

/* ---------- drawing primitives ---------- */

void oled_show_char(uint8_t row, uint8_t col, char ch)
{
  if (ch < ' ' || ch > '~')
  {
    return;
  }

  uint8_t page_top = row * 2U;
  uint16_t offset = (uint16_t)(ch - ' ') * 16U;

  if (page_top + 1U >= OLED_PAGES || col + 8U > OLED_WIDTH)
  {
    return;
  }

  for (uint8_t i = 0U; i < 8U; i++)
  {
    g_oled_buf[page_top][col + i]     = OLED_F8X16[offset + i];
    g_oled_buf[page_top + 1][col + i] = OLED_F8X16[offset + i + 8];
  }
}

void oled_show_string(uint8_t row, uint8_t col, const char *str)
{
  while (*str != '\0')
  {
    if (col + 8U > OLED_WIDTH)
    {
      row++;
      col = 0U;
    }
    if (row >= 4U)
    {
      break;
    }
    oled_show_char(row, col, *str);
    col += 8U;
    str++;
  }
}

void oled_show_hz(uint8_t row, uint8_t col, hz_index_t index)
{
  if (index >= HZ_COUNT)
  {
    return;
  }

  uint8_t page_top = row * 2U;

  if (page_top + 1U >= OLED_PAGES || col + 16U > OLED_WIDTH)
  {
    return;
  }

  const uint8_t *data = OLED_HZ16X16[index];

  for (uint8_t i = 0U; i < 16U; i++)
  {
    g_oled_buf[page_top][col + i]     = data[i];
    g_oled_buf[page_top + 1][col + i] = data[i + 16];
  }
}

void oled_show_number(uint8_t row, uint8_t col, int32_t num)
{
  char buf[12];
  uint8_t idx = 0U;
  uint32_t abs_val;

  if (num < 0)
  {
    buf[idx++] = '-';
    abs_val = (uint32_t)(-(num + 1)) + 1U;
  }
  else
  {
    abs_val = (uint32_t)num;
  }

  char tmp[10];
  uint8_t len = 0U;

  if (abs_val == 0U)
  {
    tmp[len++] = '0';
  }
  else
  {
    while (abs_val > 0U)
    {
      tmp[len++] = '0' + (char)(abs_val % 10U);
      abs_val /= 10U;
    }
  }

  for (uint8_t i = len; i > 0U; i--)
  {
    buf[idx++] = tmp[i - 1U];
  }
  buf[idx] = '\0';

  oled_show_string(row, col, buf);
}

/* ---------- temperature formatting helper ---------- */

static void show_temp_value(uint8_t row, uint8_t col, int16_t raw)
{
  int16_t integer = raw / 16;
  uint16_t frac = ((uint16_t)(raw > 0 ? raw : -raw) & 0x0FU) * 625U;
  uint8_t frac_1 = (uint8_t)(frac / 1000U);

  char temp_str[10];
  uint8_t pos = 0U;

  if (raw < 0 && integer == 0)
  {
    temp_str[pos++] = '-';
  }
  if (integer < 0)
  {
    temp_str[pos++] = '-';
    integer = -integer;
  }
  if (integer >= 100)
  {
    temp_str[pos++] = '0' + (char)(integer / 100);
  }
  if (integer >= 10)
  {
    temp_str[pos++] = '0' + (char)((integer / 10) % 10);
  }
  temp_str[pos++] = '0' + (char)(integer % 10);
  temp_str[pos++] = '.';
  temp_str[pos++] = '0' + (char)frac_1;
  temp_str[pos++] = 'C';
  temp_str[pos] = '\0';

  oled_show_string(row, col, temp_str);
}

/* ---------- run mode display ---------- */

static void oled_show_run_screen(void)
{
  /* Row 0: 温度: XX.XC */
  oled_show_hz(0, 0, HZ_WEN);
  oled_show_hz(0, 16, HZ_DU);
  oled_show_string(0, 32, ":");

  if (ds18b20_is_valid())
  {
    show_temp_value(0, 40, g_ctrl.temp_raw);
  }
  else
  {
    oled_show_string(0, 40, "N/A");
  }

  /* Row 1: 湿度: XX% */
  oled_show_hz(1, 0, HZ_SHI);
  oled_show_hz(1, 16, HZ_DU);
  oled_show_string(1, 32, ":");
  oled_show_number(1, 40, (int32_t)g_ctrl.soil_pct);
  oled_show_string(1, 64, "%");

  /* Row 2: 自动/手动:P:OFF F:OFF */
  if (g_ctrl.mode == SYS_MODE_AUTO)
  {
    oled_show_hz(2, 0, HZ_ZI);
    oled_show_hz(2, 16, HZ_DONG);
  }
  else
  {
    oled_show_hz(2, 0, HZ_SHOU);
    oled_show_hz(2, 16, HZ_DONG);
  }
  oled_show_string(2, 32, ":P:");
  oled_show_string(2, 56, g_ctrl.pump_on ? "ON " : "OFF");
  oled_show_string(2, 80, " F:");
  oled_show_string(2, 104, g_ctrl.fan_on ? "ON" : "OF");

  /* Row 3: 状态:正常/缺水/高温 */
  oled_show_hz(3, 0, HZ_ZHUANG);
  oled_show_hz(3, 16, HZ_TAI);
  oled_show_string(3, 32, ":");

  switch (g_ctrl.status)
  {
    case SYS_STATUS_NORMAL:
      oled_show_hz(3, 40, HZ_ZHENG);
      oled_show_hz(3, 56, HZ_CHANG);
      break;
    case SYS_STATUS_DRY:
      oled_show_hz(3, 40, HZ_QUE);
      oled_show_hz(3, 56, HZ_SHUI);
      if (g_ctrl.alarm_active)
        oled_show_string(3, 72, "!");
      break;
    case SYS_STATUS_COLD:
      oled_show_hz(3, 40, HZ_DI);
      oled_show_hz(3, 56, HZ_WEN);
      break;
    case SYS_STATUS_HOT:
      oled_show_hz(3, 40, HZ_GAO);
      oled_show_hz(3, 56, HZ_WEN);
      if (g_ctrl.alarm_active)
        oled_show_string(3, 72, "!");
      break;
  }

  /* WiFi/MQTT 状态指示（右上角） */
  esp_conn_state_t esp_st = esp8266_get_state();
  switch (esp_st)
  {
    case ESP_CONN_IDLE:           oled_show_string(0, 96, "W:--"); break;
    case ESP_CONN_HW_RESET:
    case ESP_CONN_WAIT_READY:     oled_show_string(0, 96, "W:Rs"); break;
    case ESP_CONN_AT_TEST:
    case ESP_CONN_ATE0:
    case ESP_CONN_CWMODE:         oled_show_string(0, 96, "W:AT"); break;
    case ESP_CONN_CWJAP:          oled_show_string(0, 96, "W:Jn"); break;
    case ESP_CONN_WIFI_OK:
    case ESP_CONN_MQTT_USERCFG:   oled_show_string(0, 96, "M:Cf"); break;
    case ESP_CONN_MQTT_CONN:
    case ESP_CONN_MQTT_WAIT:      oled_show_string(0, 96, "M:Cn"); break;
    case ESP_CONN_MQTT_SUB1:      oled_show_string(0, 96, "M:S1"); break;
    case ESP_CONN_MQTT_SUB2:      oled_show_string(0, 96, "M:S2"); break;
    case ESP_CONN_MQTT_OK:
    case ESP_PUB_CMD:
    case ESP_PUB_DATA:
    case ESP_REPLY_CMD:
    case ESP_REPLY_DATA:          oled_show_string(0, 96, "M:OK"); break;
    case ESP_CONN_ERROR:          oled_show_string(0, 96, " Err"); break;
  }
}

/* ---------- settings mode display ---------- */

static void oled_show_settings_screen(void)
{
  const threshold_config_t *s = (const threshold_config_t *)&g_ctrl.setting_buf;
  uint8_t idx = g_ctrl.setting_index;

  /* Row 0: == 设置 == */
  oled_show_string(0, 0, "==");
  oled_show_hz(0, 24, HZ_SHE);
  oled_show_hz(0, 40, HZ_ZHI_SET);
  oled_show_string(0, 56, "==");

  /* Row 1: cursor + current setting name + value */
  oled_show_string(1, 0, ">");

  switch (idx)
  {
    case 0:
      oled_show_hz(1, 8, HZ_ZHI_PLANT);
      oled_show_hz(1, 24, HZ_WU);
      oled_show_string(1, 40, ":");
      switch (s->plant_type)
      {
        case 0:
          oled_show_hz(1, 48, HZ_NAI);
          oled_show_hz(1, 64, HZ_HAN);
          break;
        case 1:
          oled_show_hz(1, 48, HZ_CHANG);
          oled_show_hz(1, 64, HZ_GUI);
          break;
        case 2:
          oled_show_hz(1, 48, HZ_XI);
          oled_show_hz(1, 64, HZ_SHI);
          break;
      }
      break;

    case 1:
      oled_show_hz(1, 8, HZ_SHI);
      oled_show_hz(1, 24, HZ_DU);
      oled_show_hz(1, 40, HZ_XIA);
      oled_show_string(1, 56, ":");
      oled_show_number(1, 64, (int32_t)s->soil_low);
      oled_show_string(1, 88, "%");
      break;

    case 2:
      oled_show_hz(1, 8, HZ_SHI);
      oled_show_hz(1, 24, HZ_DU);
      oled_show_hz(1, 40, HZ_SHANG);
      oled_show_string(1, 56, ":");
      oled_show_number(1, 64, (int32_t)s->soil_high);
      oled_show_string(1, 88, "%");
      break;

    case 3:
      oled_show_hz(1, 8, HZ_WEN);
      oled_show_hz(1, 24, HZ_DU);
      oled_show_hz(1, 40, HZ_XIA);
      oled_show_string(1, 56, ":");
      oled_show_number(1, 64, (int32_t)s->temp_low);
      oled_show_string(1, 88, "C");
      break;

    case 4:
      oled_show_hz(1, 8, HZ_WEN);
      oled_show_hz(1, 24, HZ_DU);
      oled_show_hz(1, 40, HZ_SHANG);
      oled_show_string(1, 56, ":");
      oled_show_number(1, 64, (int32_t)s->temp_high);
      oled_show_string(1, 88, "C");
      break;
  }

  /* Row 2: 阈值: X/5 */
  oled_show_hz(2, 0, HZ_YU);
  oled_show_hz(2, 16, HZ_ZHI_VAL);
  oled_show_string(2, 32, ":");
  oled_show_number(2, 40, (int32_t)(idx + 1));
  oled_show_string(2, 48, "/");
  oled_show_number(2, 56, SETTING_COUNT);

  /* Row 3: K1:下项 K4:保存 */
  oled_show_string(3, 0, "K1:");
  oled_show_hz(3, 24, HZ_XIA);
  oled_show_hz(3, 40, HZ_XIANG);
  oled_show_string(3, 64, "K4:");
  oled_show_hz(3, 88, HZ_BAO);
  oled_show_hz(3, 104, HZ_CUN);
}

/* ---------- display task ---------- */

void oled_display_task(void)
{
  uint32_t now = HAL_GetTick();
  if ((now - g_oled_display_tick) < OLED_DISPLAY_PERIOD_MS)
  {
    return;
  }
  g_oled_display_tick = now;

  oled_clear();

  if (g_ctrl.mode == SYS_MODE_SETTINGS)
  {
    oled_show_settings_screen();
  }
  else
  {
    oled_show_run_screen();
  }

  oled_refresh();
}