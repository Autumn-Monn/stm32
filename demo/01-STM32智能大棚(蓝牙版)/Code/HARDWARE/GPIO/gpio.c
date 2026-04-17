#include "gpio.h"


/****
*******	LED IO初始化
*****/    
void Gpio_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC,ENABLE);			    //使能端口时钟

	GPIO_InitStructure.GPIO_Pin  = LEDS_GPIO_PIN;  						//引脚配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 					//设置成推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 			//IO口速度为50MHz
 	GPIO_Init(LEDS_GPIO_PORT, &GPIO_InitStructure);						//根据设定参数初始化
	GPIO_SetBits(LEDS_GPIO_PORT,LEDS_GPIO_PIN);  							//IO口输出高

	GPIO_InitStructure.GPIO_Pin  = LED_PIN; 									//引脚配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 					//设置成推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 			//IO口速度为50MHz
 	GPIO_Init(LED_PORT, &GPIO_InitStructure);									//根据设定参数初始化
	GPIO_SetBits(LED_PORT,LED_PIN); 				 									//IO口输出高

	GPIO_InitStructure.GPIO_Pin  = BEEP_PIN; 									//引脚配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 					//设置成推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 			//IO口速度为50MHz
	GPIO_Init(BEEP_PORT, &GPIO_InitStructure);								//根据设定参数初始化
	GPIO_ResetBits(BEEP_PORT,BEEP_PIN); 				 							//IO口输出低
		
	GPIO_InitStructure.GPIO_Pin  = FS_PIN; 									//引脚配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 					//设置成推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 			//IO口速度为50MHz
	GPIO_Init(FS_PORT, &GPIO_InitStructure);								//根据设定参数初始化
	GPIO_ResetBits(FS_PORT,FS_PIN); 				 							//IO口输出低
	
	GPIO_InitStructure.GPIO_Pin  = JR_PIN; 									//引脚配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 					//设置成推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 			//IO口速度为50MHz
	GPIO_Init(JR_PORT, &GPIO_InitStructure);								//根据设定参数初始化
	GPIO_ResetBits(JR_PORT,JR_PIN); 				 							//IO口输出低
	
	GPIO_InitStructure.GPIO_Pin  = JS_PIN; 									//引脚配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 					//设置成推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 			//IO口速度为50MHz
	GPIO_Init(JS_PORT, &GPIO_InitStructure);								//根据设定参数初始化
	GPIO_ResetBits(JS_PORT,JS_PIN); 				 							//IO口输出低
	
	GPIO_InitStructure.GPIO_Pin  = BG_PIN; 									//引脚配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 					//设置成推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 			//IO口速度为50MHz
	GPIO_Init(BG_PORT, &GPIO_InitStructure);								//根据设定参数初始化
	GPIO_ResetBits(BG_PORT,BG_PIN); 				 							//IO口输出低
	
}

