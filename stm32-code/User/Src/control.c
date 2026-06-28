#include "control.h"
#include "key.h"
#include "led.h"
#include "relay.h"
#include "beep.h"
#include "soil.h"
#include "ds18b20.h"
#include "debug_uart.h"
#include "data_store.h"

#define CONTROL_PERIOD_MS       100U
#define ALARM_TOGGLE_MS         300U
#define TEMP_HYSTERESIS_RAW     32      /* 2C * 16 = 32 raw units */

static const threshold_config_t PRESETS[3] = {
  { 0, 20, 40, 10, 38 },
  { 1, 30, 60, 15, 35 },
  { 2, 50, 80, 18, 32 },
};

volatile control_state_t g_ctrl;

static uint32_t g_ctrl_tick = 0U;
static uint32_t g_alarm_tick = 0U;

static void control_auto_logic(void);
static void control_update_led(void);
static void control_update_alarm(void);
static void control_log_status_change(sys_status_t old_st);

void control_init(void)
{
  data_store_load_thresholds((threshold_config_t *)&g_ctrl.thresh);

  g_ctrl.mode         = SYS_MODE_AUTO;
  g_ctrl.status       = SYS_STATUS_NORMAL;
  g_ctrl.pump_on      = 0U;
  g_ctrl.fan_on       = 0U;
  g_ctrl.alarm_active = 0U;
  g_ctrl.alarm_muted  = 0U;
  g_ctrl.temp_raw     = 0;
  g_ctrl.soil_val     = 0U;
  g_ctrl.setting_index = 0U;

  relay_off(RELAY_PUMP);
  relay_off(RELAY_FAN);
  beep_off();
  led_off(LED_RED);
  led_off(LED_BLUE);
  led_on(LED_GREEN);

  g_ctrl_tick  = HAL_GetTick();
  g_alarm_tick = HAL_GetTick();

  debug_uart_send_line("[CTRL] Init: AUTO mode");
}

/* -------- main control task -------- */

void control_task(void)
{
  uint32_t now = HAL_GetTick();
  if ((now - g_ctrl_tick) < CONTROL_PERIOD_MS)
  {
    return;
  }
  g_ctrl_tick = now;

  g_ctrl.soil_val = soil_read_avg(16);
  g_ctrl.soil_pct = soil_adc_to_percent(g_ctrl.soil_val);
  if (ds18b20_is_valid())
  {
    g_ctrl.temp_raw = ds18b20_get_cached_raw();
  }

  if (g_ctrl.mode == SYS_MODE_AUTO)
  {
    control_auto_logic();
  }

  control_update_led();
  control_update_alarm();
}

/* -------- auto control with hysteresis -------- */

static void control_auto_logic(void)
{
  sys_status_t old_st = g_ctrl.status;
  uint8_t  soil_pct = g_ctrl.soil_pct;
  int16_t  temp = g_ctrl.temp_raw;
  int16_t  temp_low_raw  = (int16_t)g_ctrl.thresh.temp_low * 16;
  int16_t  temp_high_raw = (int16_t)g_ctrl.thresh.temp_high * 16;

  /* --- soil / pump: on below soil_low%, off at soil_high% --- */
  if (soil_pct < g_ctrl.thresh.soil_low)
  {
    if (!g_ctrl.pump_on)
    {
      relay_on(RELAY_PUMP);
      g_ctrl.pump_on = 1U;
      debug_uart_send_line("[CTRL] Pump ON (dry)");
    }
  }
  else if (soil_pct >= g_ctrl.thresh.soil_high)
  {
    if (g_ctrl.pump_on)
    {
      relay_off(RELAY_PUMP);
      g_ctrl.pump_on = 0U;
      debug_uart_send_line("[CTRL] Pump OFF (wet)");
    }
  }

  /* --- temp / fan: on above temp_high, off with hysteresis --- */
  if (ds18b20_is_valid())
  {
    if (temp > temp_high_raw)
    {
      if (!g_ctrl.fan_on)
      {
        relay_on(RELAY_FAN);
        g_ctrl.fan_on = 1U;
        debug_uart_send_line("[CTRL] Fan ON (hot)");
      }
    }
    else if (temp <= (temp_high_raw - TEMP_HYSTERESIS_RAW))
    {
      if (g_ctrl.fan_on)
      {
        relay_off(RELAY_FAN);
        g_ctrl.fan_on = 0U;
        debug_uart_send_line("[CTRL] Fan OFF (cool)");
      }
    }
  }

  /* --- status priority: extreme dry > over-temp > low-temp > dry warn > normal ---
     spec (3): 三色LED 红=缺水 蓝=低温 绿=正常; 报警=极端干燥 或 超温 */
  if (soil_pct < g_ctrl.thresh.soil_low)
  {
    g_ctrl.status = SYS_STATUS_DRY;
  }
  else if (ds18b20_is_valid() && temp > temp_high_raw)
  {
    g_ctrl.status = SYS_STATUS_HOT;
  }
  else if (ds18b20_is_valid() && temp < temp_low_raw)
  {
    g_ctrl.status = SYS_STATUS_COLD;
  }
  else if (soil_pct < g_ctrl.thresh.soil_high)
  {
    g_ctrl.status = SYS_STATUS_DRY;
  }
  else
  {
    g_ctrl.status = SYS_STATUS_NORMAL;
  }

  if (g_ctrl.status != old_st)
  {
    g_ctrl.alarm_muted = 0U;
    control_log_status_change(old_st);
  }

  /* --- alarm: critical (soil < soil_low% OR temp > temp_high) --- */
  g_ctrl.alarm_active = ((soil_pct < g_ctrl.thresh.soil_low) ||
                          (ds18b20_is_valid() && temp > temp_high_raw)) ? 1U : 0U;
}

/* -------- LED indicator -------- */

static void control_update_led(void)
{
  if (g_ctrl.mode == SYS_MODE_SETTINGS)
  {
    led_off(LED_GREEN);
    led_off(LED_RED);
    led_on(LED_BLUE);
    return;
  }

  switch (g_ctrl.status)
  {
    case SYS_STATUS_NORMAL:
      led_on(LED_GREEN);
      led_off(LED_RED);
      led_off(LED_BLUE);
      break;

    case SYS_STATUS_DRY:
      /* 红=缺水: 极端干燥(报警)时闪烁, 普通缺水常亮 */
      led_off(LED_GREEN);
      led_off(LED_BLUE);
      if (g_ctrl.alarm_active)
        led_toggle(LED_RED);
      else
        led_on(LED_RED);
      break;

    case SYS_STATUS_COLD:
      /* 蓝=低温: 常亮指示, 低温本身不触发蜂鸣报警 */
      led_off(LED_GREEN);
      led_off(LED_RED);
      led_on(LED_BLUE);
      break;

    case SYS_STATUS_HOT:
      /* 超温报警: 红灯闪烁 + 蜂鸣器(见 control_update_alarm), OLED 显示"高温" */
      led_off(LED_GREEN);
      led_off(LED_BLUE);
      led_toggle(LED_RED);
      break;
  }
}

/* -------- alarm buzzer -------- */

static void control_update_alarm(void)
{
  if (!g_ctrl.alarm_active || g_ctrl.alarm_muted)
  {
    beep_off();
    return;
  }

  uint32_t now = HAL_GetTick();
  if ((now - g_alarm_tick) >= ALARM_TOGGLE_MS)
  {
    g_alarm_tick = now;
    beep_toggle();
  }
}

/* -------- key handler -------- */

void control_key_handler(void)
{
  key_event_t evt;

  while ((evt = key_get_event()) != KEY_EVENT_NONE)
  {
    if (g_ctrl.mode == SYS_MODE_SETTINGS)
    {
      /* --- settings mode keys --- */
      switch (evt)
      {
        case KEY_EVENT_1_PRESSED:
          g_ctrl.setting_index++;
          if (g_ctrl.setting_index >= SETTING_COUNT)
          {
            g_ctrl.setting_index = 0U;
          }
          break;

        case KEY_EVENT_2_PRESSED:
        {
          threshold_config_t *s = (threshold_config_t *)&g_ctrl.setting_buf;
          switch (g_ctrl.setting_index)
          {
            case 0:
              if (s->plant_type > 0U)
              {
                s->plant_type--;
                *s = PRESETS[s->plant_type];
              }
              break;
            case 1: if (s->soil_low  >= 1U)  s->soil_low  -= 1U; break;
            case 2: if (s->soil_high >= 1U)  s->soil_high -= 1U; break;
            case 3: if (s->temp_low  > 0U)   s->temp_low--;      break;
            case 4: if (s->temp_high > 20U)  s->temp_high--;     break;
          }
          break;
        }

        case KEY_EVENT_3_PRESSED:
        {
          threshold_config_t *s = (threshold_config_t *)&g_ctrl.setting_buf;
          switch (g_ctrl.setting_index)
          {
            case 0:
              if (s->plant_type < 2U)
              {
                s->plant_type++;
                *s = PRESETS[s->plant_type];
              }
              break;
            case 1: if (s->soil_low  <= 99U) s->soil_low  += 1U; break;
            case 2: if (s->soil_high <= 99U) s->soil_high += 1U; break;
            case 3: if (s->temp_low  < 30U)  s->temp_low++;      break;
            case 4: if (s->temp_high < 50U)  s->temp_high++;     break;
          }
          break;
        }

        case KEY_EVENT_4_PRESSED:
        {
          threshold_config_t *s = (threshold_config_t *)&g_ctrl.setting_buf;
          *((threshold_config_t *)&g_ctrl.thresh) = *s;
          data_store_save_thresholds(s);
          g_ctrl.mode = SYS_MODE_AUTO;
          g_ctrl.alarm_muted = 0U;
          debug_uart_send_line("[CTRL] Settings saved, back to AUTO");
          break;
        }

        default:
          break;
      }
    }
    else
    {
      /* --- run mode keys --- */
      switch (evt)
      {
        case KEY_EVENT_1_PRESSED:
          if (g_ctrl.mode == SYS_MODE_AUTO)
          {
            g_ctrl.mode = SYS_MODE_MANUAL;
            debug_uart_send_line("[CTRL] Mode: MANUAL");
          }
          else
          {
            g_ctrl.mode = SYS_MODE_AUTO;
            debug_uart_send_line("[CTRL] Mode: AUTO");
          }
          break;

        case KEY_EVENT_1_LONG:
          g_ctrl.setting_buf = *((const threshold_config_t *)&g_ctrl.thresh);
          g_ctrl.setting_index = 0U;
          g_ctrl.mode = SYS_MODE_SETTINGS;
          debug_uart_send_line("[CTRL] Mode: SETTINGS");
          break;

        case KEY_EVENT_2_PRESSED:
          if (g_ctrl.mode == SYS_MODE_MANUAL)
          {
            if (g_ctrl.pump_on)
            {
              relay_off(RELAY_PUMP);
              g_ctrl.pump_on = 0U;
              debug_uart_send_line("[CTRL] Manual: Pump OFF");
            }
            else
            {
              relay_on(RELAY_PUMP);
              g_ctrl.pump_on = 1U;
              debug_uart_send_line("[CTRL] Manual: Pump ON");
            }
          }
          break;

        case KEY_EVENT_3_PRESSED:
          if (g_ctrl.mode == SYS_MODE_MANUAL)
          {
            if (g_ctrl.fan_on)
            {
              relay_off(RELAY_FAN);
              g_ctrl.fan_on = 0U;
              debug_uart_send_line("[CTRL] Manual: Fan OFF");
            }
            else
            {
              relay_on(RELAY_FAN);
              g_ctrl.fan_on = 1U;
              debug_uart_send_line("[CTRL] Manual: Fan ON");
            }
          }
          break;

        case KEY_EVENT_4_PRESSED:
          g_ctrl.alarm_muted = !g_ctrl.alarm_muted;
          if (g_ctrl.alarm_muted)
          {
            beep_off();
            debug_uart_send_line("[CTRL] Alarm muted");
          }
          else
          {
            debug_uart_send_line("[CTRL] Alarm unmuted");
          }
          break;

        default:
          break;
      }
    }
  }
}

/* -------- debug log -------- */

static void control_log_status_change(sys_status_t old_st)
{
  (void)old_st;
  debug_uart_send_string("[CTRL] Status: ");
  switch (g_ctrl.status)
  {
    case SYS_STATUS_NORMAL: debug_uart_send_line("NORMAL"); break;
    case SYS_STATUS_DRY:    debug_uart_send_line("DRY");    break;
    case SYS_STATUS_COLD:   debug_uart_send_line("COLD");   break;
    case SYS_STATUS_HOT:    debug_uart_send_line("HOT");    break;
  }
}