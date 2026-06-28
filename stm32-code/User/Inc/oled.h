#ifndef OLED_H
#define OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define OLED_WIDTH   128U
#define OLED_HEIGHT  64U
#define OLED_PAGES   8U
#define OLED_ADDR    0x78U

typedef enum
{
  HZ_WEN = 0,
  HZ_DU,
  HZ_SHI,
  HZ_ZI,
  HZ_DONG,
  HZ_SHOU,
  HZ_ZHUANG,
  HZ_TAI,
  HZ_ZHENG,
  HZ_CHANG,
  HZ_QUE,
  HZ_SHUI,
  HZ_GAO,
  HZ_SHE,
  HZ_ZHI_SET,
  HZ_ZHI_PLANT,
  HZ_WU,
  HZ_GUI,
  HZ_XI,
  HZ_NAI,
  HZ_HAN,
  HZ_YU,
  HZ_ZHI_VAL,
  HZ_XIA,
  HZ_XIANG,
  HZ_BAO,
  HZ_CUN,
  HZ_SHANG,
  HZ_DI,
  HZ_COUNT
} hz_index_t;

void oled_init(void);
void oled_clear(void);
void oled_refresh(void);

void oled_show_char(uint8_t row, uint8_t col, char ch);
void oled_show_string(uint8_t row, uint8_t col, const char *str);
void oled_show_hz(uint8_t row, uint8_t col, hz_index_t index);
void oled_show_number(uint8_t row, uint8_t col, int32_t num);

void oled_display_task(void);

#ifdef __cplusplus
}
#endif

#endif /* OLED_H */