#ifndef KEY_H
#define KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  KEY_ID_1 = 0,
  KEY_ID_2,
  KEY_ID_3,
  KEY_ID_4,
  KEY_ID_COUNT
} key_id_t;

typedef enum
{
  KEY_EVENT_NONE = 0,
  KEY_EVENT_1_PRESSED,
  KEY_EVENT_2_PRESSED,
  KEY_EVENT_3_PRESSED,
  KEY_EVENT_4_PRESSED
} key_event_t;

void key_init(void);
void key_scan(void);
uint8_t key_is_pressed(key_id_t key);
key_event_t key_get_event(void);
void key_stage2_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */
