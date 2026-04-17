#ifndef __GPIO_H
#define __GPIO_H


/**********************************
包含头文件
**********************************/
#include "sys.h"


/**********************************
重定义关键词
**********************************/
#define LEDS_GPIO_PORT                GPIOC        	//最小系统LED
#define LEDS_GPIO_PIN                 GPIO_Pin_13
#define LED_SYS                       PCout(13)

#define BEEP_PORT                     GPIOB					//蜂鸣器引脚
#define BEEP_PIN                      GPIO_Pin_1
#define BEEP 									    		PBout(1)

#define LED_PORT                      GPIOB					//LED灯引脚
#define LED_PIN                       GPIO_Pin_0
#define LED 									    		PBout(0)

#define FS_PORT                       GPIOA					//风扇引脚
#define FS_PIN                        GPIO_Pin_4
#define FS									    		  PAout(4)

#define JR_PORT                       GPIOA					//加热引脚
#define JR_PIN                        GPIO_Pin_5
#define JR 									    		  PAout(5)

#define JS_PORT                       GPIOA					//水泵引脚
#define JS_PIN                        GPIO_Pin_6
#define JS 									          PAout(6)

#define BG_PORT                       GPIOA					//补光引脚
#define BG_PIN                        GPIO_Pin_7
#define BG 									    		  PAout(7)


/**********************************
函数声明
**********************************/
void Gpio_Init(void);																//GPIO初始化


#endif

