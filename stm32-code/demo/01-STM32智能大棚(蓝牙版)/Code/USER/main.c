#include "sys.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "math.h"
#include "delay.h"
#include "gpio.h"
#include "key.h"
#include "oled.h"
#include "usart.h"
#include "ds18b20.h"
#include "adc.h"


/**********************************
变量定义
**********************************/
uint8_t key_num = 0;									//按键扫描标志位	
uint8_t flag_display = 0;							//显示界面标志位

uint16_t time_num = 0;								//10ms计时
uint16_t light_value = 0;							//光照数值
uint16_t light_min = 50;							//光照最小值
uint16_t soil_value = 0;							//土壤湿度数值
uint16_t soil_max = 50;								//土壤湿度最大值
uint16_t soil_min = 20;								//土壤湿度最小值
uint16_t temp_value = 0;							//温度数值
uint16_t temp_max = 40;								//温度最大值
uint16_t temp_min = 10;								//温度最小值

_Bool flag_temp_min = 0;							//温度最小值声光报警标志位
_Bool flag_temp_max = 0;							//温度最大值声光报警标志位
_Bool flag_soil_min = 0;							//湿度最小值声光报警标志位

extern uint8_t usart1_buf[256];				//串口1接收数组
char display_buf[16];									//显示数组


/**********************************
函数声明
**********************************/
void Key_function(void);							//按键函数
void Monitor_function(void);					//监测函数
void Display_function(void);					//显示函数
void Manage_function(void);						//处理函数


/****
*******	主函数 
*****/
int main(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //配置中断优先分组
	Delay_Init();	    	 								//延时初始化	  
	Gpio_Init();		  									//IO初始化
	Key_Init();		  										//按键初始化
	Oled_Init();		  									//OLED初始化
	Oled_Clear_All();										//清屏
	Usart1_Init(9600);    							//串口1初始化
	DS18B20_Init();											//DS18B20初始化
	Adc_Init();													//ADC初始化
	Delay_ms(1000);
	Delay_ms(1000);
	
	while(1)
	{
		Key_function();										//按键函数
		Monitor_function();								//监测函数
		Display_function();								//显示函数
		Manage_function();								//处理函数

		time_num++;												//计时变量+1
		Delay_ms(10);
		if(time_num %10 == 0)
			LED_SYS = ~LED_SYS;
		if(time_num >= 5000)
		{
			time_num = 0;
		}
	}
}

/****
*******按键函数
*****/
void Key_function(void)
{
	key_num = Chiclet_Keyboard_Scan(0);		//按键扫描
	if(key_num != 0)											//有按键按下
	{
		switch(key_num)
		{
			case 1:								            //按键1，切换设置界面
				flag_display++;
				if(flag_display >= 6)
					flag_display = 0;
				
				Oled_Clear_All();					      //清屏
			break;

			case 2:											      //按键2
				switch(flag_display)
				{					
					case 1:												//界面1：温度最大值+1
						if(temp_max < 99)
							temp_max++;
					break;
					
					case 2:												//界面2：温度最小值+1
						if(temp_min < temp_max-1)
							temp_min++;
					break;
					
					case 3:												//界面3：湿度最大值+1
						if(soil_max < 99)
							soil_max++;
					break;
					
					case 4:												//界面4：湿度最小值+1
						if(soil_min < soil_max-1)
							soil_min++;
					break;
					
					case 5:												//界面5：光照最小值
						if(light_min < 99)
							light_min++;
					break;
					
					default:
					break;
				}
			break;

			case 3:														//按键3
				switch(flag_display)
				{					
					case 1:												//界面1：温度最大值-1
						if(temp_max > temp_min+1)
							temp_max--;
					break;
					
					case 2:												//界面2：温度最小值-1
						if(temp_min > 0)
							temp_min--;
					break;
					
					case 3:												//界面3：湿度最大值-1
						if(soil_max > soil_min+1)
							soil_max--;
					break;
					
					case 4:												//界面4：湿度最小值-1
						if(soil_min > 0)
							soil_min--;
					break;
					
					case 5:												//界面5：光照最小值-1
						if(light_min > 0)
							light_min--;
					break;
					
					default:
					break;
				}
			break;

			default:
				
			break;
		}
	}
}

/****
*******监测函数
*****/
void Monitor_function(void)
{
	if(flag_display == 0)									//测量界面
	{
		if(time_num % 5 == 0)								//获取数据
		{
			temp_value = DS18B20_Get_Temp();
			light_value = 99-30*(Get_Adc_Average(0,3)*3.3/4096.0);	
			soil_value = 99-30*(Get_Adc_Average(1,3)*3.3/4096.0);	
		}
		
		if(time_num % 30 == 0)							//发送数据
		{
			UsartPrintf(USART1,"温度：%d.%dC\r\n",temp_value/10,temp_value%10);
			UsartPrintf(USART1,"湿度：%d%%\r\n",soil_value);
			UsartPrintf(USART1,"光照：%dLux\r\n",light_value);
		}
	}
}

/****
*******显示函数
*****/
void Display_function(void)
{
	switch(flag_display)									//根据不同的显示模式标志位，显示不同的界面
	{
		case 0:									      			//界面0：显示温度，湿度，光照的数值
			Oled_ShowCHinese(1,1,"土壤环境监测");
		
			Oled_ShowCHinese(2,0,"温度：");
			sprintf(display_buf,"%d.%dC  ",temp_value/10,temp_value%10);
			Oled_ShowString(2,6,display_buf);
		
			Oled_ShowCHinese(3,0,"湿度：");
			sprintf(display_buf,"%d%%  ",soil_value);
			Oled_ShowString(3,6,display_buf);

			Oled_ShowCHinese(4,0,"光照：");
			sprintf(display_buf,"%dLux  ",light_value);
			Oled_ShowString(4,6,display_buf);
		break;
		
		case 1:															//界面1：设置温度最大值
			Oled_ShowCHinese(1,0,"设置温度最大值");
			if(time_num % 5 == 0)
			{
				sprintf(display_buf,"%d  ",temp_max);
				Oled_ShowString(2, 7, display_buf);
			}
			if(time_num % 10 == 0)
			{
				Oled_ShowString(2, 7, "    ");
			}
		break;
		
		case 2:															//界面2：设置温度最小值
			Oled_ShowCHinese(1,0,"设置温度最小值");
			if(time_num % 5 == 0)
			{
				sprintf(display_buf,"%d  ",temp_min);
				Oled_ShowString(2, 7, display_buf);
			}
			if(time_num % 10 == 0)
			{
				Oled_ShowString(2, 7, "    ");
			}
		break;
		
		case 3:															//界面3：设置湿度最大值
			Oled_ShowCHinese(1,0,"设置湿度最大值");
			if(time_num % 5 == 0)
			{
				sprintf(display_buf,"%d  ",soil_max);
				Oled_ShowString(2, 7, display_buf);
			}
			if(time_num % 10 == 0)
			{
				Oled_ShowString(2, 7, "    ");
			}
		break;
		
		case 4:														//界面4：设置湿度最小值
			Oled_ShowCHinese(1,0,"设置湿度最小值");
			if(time_num % 5 == 0)
			{
				sprintf(display_buf,"%d  ",soil_min);
				Oled_ShowString(2, 7, display_buf);
			}
			if(time_num % 10 == 0)
			{
				Oled_ShowString(2, 7, "    ");
			}
		break;

		case 5:														//界面5：设置光照最小值
			Oled_ShowCHinese(1,0,"设置光照最小值");
			if(time_num % 5 == 0)
			{
				sprintf(display_buf,"%d  ",light_min);
				Oled_ShowString(2, 7, display_buf);
			}
			if(time_num % 10 == 0)
			{
				Oled_ShowString(2, 7, "    ");
			}
		break;
		
		default:
			
		break;
	}
}

/****
*******处理函数
*****/
void Manage_function(void)
{
	if(flag_display == 0)                  //测量界面
	{
		if(temp_value < temp_min*10)				//温度小于最小值，开启加热，和声光报警
		{
			flag_temp_min = 1;
			flag_temp_max = 0;
			FS = 0;
			JR = 1;		
		}
		else if(temp_value > temp_max*10) 	//温度大于最大值，开启风扇，和声光报警
		{
			flag_temp_min = 0;
			flag_temp_max = 1;
			FS = 1;
			JR = 0;			
		}
		else																//温度在两者之间关闭风扇和加热和声光报警
		{
			flag_temp_min = 0;
			flag_temp_max = 0;
			FS = 0;
			JR = 0;			
		}
		if(soil_value < soil_min)						//湿度小于最小值开启水泵和声光报警
		{
			flag_soil_min = 1;
			JS = 1;
		}
		else if(soil_value > soil_max)			//湿度大于最大值关闭水泵和声光报警
		{
			flag_soil_min = 0;
			JS = 0;		
		}
		else																//湿度在两者之间关闭声光报警
		{
			flag_soil_min = 0;	
		}
		if(light_value < light_min)					//光照小于最小值开启补光
			BG = 1;
		else																//光照大于最小值关闭补光
			BG = 0;
		
		if(flag_temp_min == 1 || flag_temp_max == 1 || flag_soil_min == 1) 
		{
			if(time_num % 3 == 0)							//开启声光报警											
			{
				LED = ~LED;
				BEEP = ~BEEP;
			}
		}
		else																//关闭声光报警	
		{
			LED = 1;
			BEEP = 0;
		}		
	}
	else													         //设置界面
	{
		LED = 1;
		BEEP = 0;
		FS = 0;
		JS = 0;
		JR = 0;
		BG = 0;		
	}
}

