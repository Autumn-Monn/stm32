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
  sys_mode_t   mode;  // 当前系统模式
  sys_status_t status;  // 当前系统状态
  uint8_t      pump_on; // 水泵状态
  uint8_t      fan_on;  // 风扇状态
  uint8_t      alarm_active; // 报警标志
  uint8_t      alarm_muted; // 静音标志
  int16_t      temp_raw; // 温度原始值
  uint16_t     soil_val; // 土壤湿度值
  uint8_t      soil_pct; // 土壤湿度百分比    
  threshold_config_t thresh; // 阈值配置
  uint8_t      setting_index; // 当前设置索引
  threshold_config_t setting_buf; // 设置模式下的临时阈值缓冲区
} control_state_t;

extern volatile control_state_t g_ctrl;

void control_init(void);
void control_task(void);
void control_key_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_H */