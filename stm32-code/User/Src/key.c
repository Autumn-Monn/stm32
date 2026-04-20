#include "key.h"

#include "led.h"

#define KEY_SCAN_PERIOD_MS    10U
#define KEY_DEBOUNCE_TIME_MS  30U

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} key_hw_t;

typedef struct
{
  uint8_t last_raw_pressed;
  uint8_t stable_pressed;
  uint8_t pressed_event_pending;
  uint32_t last_change_tick;
} key_state_t;

static const key_hw_t g_key_map[KEY_ID_COUNT] = {
    [KEY_ID_1] = {KEY_1_GPIO_Port, KEY_1_Pin},
    [KEY_ID_2] = {KEY_2_GPIO_Port, KEY_2_Pin},
    [KEY_ID_3] = {KEY_3_GPIO_Port, KEY_3_Pin},
    [KEY_ID_4] = {KEY_4_GPIO_Port, KEY_4_Pin},
};

static key_state_t g_key_state[KEY_ID_COUNT];
static uint32_t g_last_scan_tick;

static uint8_t key_read_raw_pressed(key_id_t key)
{
  if (key >= KEY_ID_COUNT)
  {
    return 0U;
  }

  return (HAL_GPIO_ReadPin(g_key_map[key].port, g_key_map[key].pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

void key_init(void)
{
  key_id_t key;
  uint8_t raw_pressed;

  g_last_scan_tick = HAL_GetTick();

  for (key = KEY_ID_1; key < KEY_ID_COUNT; key++)
  {
    raw_pressed = key_read_raw_pressed(key);
    g_key_state[key].last_raw_pressed = raw_pressed;
    g_key_state[key].stable_pressed = raw_pressed;
    g_key_state[key].pressed_event_pending = 0U;
    g_key_state[key].last_change_tick = g_last_scan_tick;
  }
}

void key_scan(void)
{
  key_id_t key;
  uint32_t now;
  uint8_t raw_pressed;

  now = HAL_GetTick();
  if ((now - g_last_scan_tick) < KEY_SCAN_PERIOD_MS)
  {
    return;
  }

  g_last_scan_tick = now;

  for (key = KEY_ID_1; key < KEY_ID_COUNT; key++)
  {
    raw_pressed = key_read_raw_pressed(key);

    if (raw_pressed != g_key_state[key].last_raw_pressed)
    {
      g_key_state[key].last_raw_pressed = raw_pressed;
      g_key_state[key].last_change_tick = now;
      continue;
    }

    if ((g_key_state[key].stable_pressed != raw_pressed) &&
        ((now - g_key_state[key].last_change_tick) >= KEY_DEBOUNCE_TIME_MS))
    {
      g_key_state[key].stable_pressed = raw_pressed;
      if (raw_pressed != 0U)
      {
        g_key_state[key].pressed_event_pending = 1U;
      }
    }
  }
}

uint8_t key_is_pressed(key_id_t key)
{
  if (key >= KEY_ID_COUNT)
  {
    return 0U;
  }

  return g_key_state[key].stable_pressed;
}

key_event_t key_get_event(void)
{
  key_id_t key;

  for (key = KEY_ID_1; key < KEY_ID_COUNT; key++)
  {
    if (g_key_state[key].pressed_event_pending != 0U)
    {
      g_key_state[key].pressed_event_pending = 0U;
      return (key_event_t)(KEY_EVENT_1_PRESSED + key);
    }
  }

  return KEY_EVENT_NONE;
}

void key_stage2_demo(void)
{
  key_event_t event;

  do
  {
    event = key_get_event();

    switch (event)
    {
      case KEY_EVENT_1_PRESSED:
        led_toggle(LED_GREEN);
        break;

      case KEY_EVENT_2_PRESSED:
        led_toggle(LED_RED);
        break;

      case KEY_EVENT_3_PRESSED:
        led_toggle(LED_BLUE);
        break;

      case KEY_EVENT_4_PRESSED:
        led_off(LED_BOARD);
        led_off(LED_GREEN);
        led_off(LED_RED);
        led_off(LED_BLUE);
        break;

      case KEY_EVENT_NONE:
      default:
        break;
    }
  } while (event != KEY_EVENT_NONE);
}
