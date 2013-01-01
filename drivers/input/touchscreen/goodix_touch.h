/* 
* include/linux/goodix_touch.h 
* 
* Copyright (C) 2008 Goodix, Inc. 
* 
* This software is licensed under the terms of the GNU General Public 
* License version 2, as published by the Free Software Foundation, and 
* may be copied, distributed, and modified under those terms. 
* 
* This program is distributed in the hope that it will be useful, 
* but WITHOUT ANY WARRANTY; without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
* GNU General Public License for more details. 
* 
* Change Date: 
* 2010.11.11, add point_queue's definiens. 
* 2011.03.09, rewrite point_queue's definiens. 
* 2011.05.12, delete point_queue for Android 2.2/Android 2.3 and so on. 
* 2012.11.13, zoulz modified for tiny210v2 GT801 touchscreen 
*/ 

#ifndef __LINUX_GOODIX_TOUCH_H__ 
#define __LINUX_GOODIX_TOUCH_H__ 

#include <linux/earlysuspend.h> 
#include <linux/hrtimer.h> 
#include <linux/i2c.h> 
#include <linux/input.h> 
#include <plat/ts.h> /*If defined and used platform data */ 


#define GOODIX_I2C_NAME "Goodix-TS" 
//#define GOODIX_I2C_NAME "gt80x-ts" 
#define GUITAR_GT80X 


//Touch screen resolution 
#define TOUCH_MAX_HEIGHT 800 
#define TOUCH_MAX_WIDTH 480 
//The resolution of the display changes according to the specific platform and associated with the map coordinates of the touch screen
#define SCREEN_MAX_HEIGHT 800 
#define SCREEN_MAX_WIDTH 480 

//this is for S3C6400, not suitable for tiny210 
//#define SHUTDOWN_PORT S3C64XX_GPF(3) // SHUTDOWN pin numbers 
//#define INT_PORT S3C64XX_GPL(10) // Int IO port 
//#define INT_CFG S3C_GPIO_SFN(3) // IO configer,EINT type 

#define GOODIX_MULTI_TOUCH 
#ifndef GOODIX_MULTI_TOUCH 
#define MAX_FINGER_NUM 1 
#else 
#define MAX_FINGER_NUM 5 // maximum support hand index(<=5) 
#endif 

#if defined(INT_PORT) 
#if MAX_FINGER_NUM <= 3 
#define READ_BYTES_NUM 1+2+MAX_FINGER_NUM*5 
#elif MAX_FINGER_NUM == 4 
#define READ_BYTES_NUM 1+28 
#elif MAX_FINGER_NUM == 5 
#define READ_BYTES_NUM 1+34 
#endif 
#else 
#define READ_BYTES_NUM 1+34 
#endif 

//#define swap(x, y) do { typeof(x) z = x; x = y; y = z; } while (0) 


#define FLAG_MASK 0x01 
enum finger_state { 
FLAG_UP = 0, 
FLAG_DOWN = 1, 
FLAG_INVALID = 2, 
}; 

struct point_node { 
uint8_t id; 
//uint8_t retry; 
enum finger_state state; 
uint8_t pressure; 
unsigned int x; 
unsigned int y; 
}; 

struct goodix_ts_data { 
int retry; 
int panel_type; 
char phys[32]; 
struct i2c_client *client; 
struct input_dev *input_dev; 
uint8_t use_irq; 
uint8_t use_shutdown; 
uint32_t gpio_shutdown; 
uint32_t gpio_irq; 
uint32_t screen_width; 
uint32_t screen_height; 
struct hrtimer timer; 
struct work_struct work; 
struct early_suspend early_suspend; 
int (*power)(struct goodix_ts_data * ts, int on); 
}; 

/* Notice: This definition used by platform_data. 
* It should be move this struct info to platform head file such as plat/ts.h. 
* If not used in client, it will be NULL in function of goodix_ts_probe. 
*/ 
struct goodix_i2c_platform_data { 
uint32_t gpio_irq; //IRQ port, use macro such as "gpio_to_irq" to get Interrupt Number. 
uint32_t irq_cfg; //IRQ port config, must refer to master's Datasheet. 
uint32_t gpio_shutdown; //Shutdown port number 
uint32_t shutdown_cfg; //Shutdown port config 
uint32_t screen_width; //screen width 
uint32_t screen_height; //screen height 
}; 

#endif /* __LINUX_GOODIX_TOUCH_H__ */ 
