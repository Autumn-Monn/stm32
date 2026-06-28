#ifndef CONTROL_H
#define CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "data_store.h"

#define SETTING_COUNT  5

typedef enum
{
  SYS_MODE_AUTO = 0,
  SYS_MODE_MANUAL,
  SYS_MODE_SETTINGS
} sys_mode_t;

typedef enum
{
  SYS_STATUS_NORMAL = 0,
  SYS_STATUS_DRY,
  SYS_STATUS_COLD,
  SYS_STATUS_HOT
} sys_status_t;

typedef struct
{
  sys_mode_t   mode;
  sys_status_t status;
  uint8_t      pump_on;
  uint8_t      fan_on;
  uint8_t      alarm_active;
  uint8_t      alarm_muted;
  int16_t      temp_raw;
  uint16_t     soil_val;
  uint8_t      soil_pct;
  threshold_config_t thresh;
  uint8_t      setting_index;
  threshold_config_t setting_buf;
} control_state_t;

extern volatile control_state_t g_ctrl;

void control_init(void);
void control_task(void);
void control_key_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_H */