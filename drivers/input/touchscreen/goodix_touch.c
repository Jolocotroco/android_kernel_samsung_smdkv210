/*--------------------------------------------------------------------------------------------------------- 
* driver/input/touchscreen/goodix_touch.c 
* 
* Copyright(c) 2010 Goodix Technology Corp. 
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
*---------------------------------------------------------------------------------------------------------*/ 
#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/slab.h> 
#include <linux/time.h> 
#include <linux/delay.h> 
#include <linux/device.h> 
#include <linux/earlysuspend.h> 
#include <linux/platform_device.h> 
#include <linux/hrtimer.h> 
#include <linux/i2c.h> 
#include <linux/input.h> 
#include <linux/interrupt.h> 
#include <linux/io.h> 
#include <linux/irq.h> 
#include <mach/gpio.h> 
#include <plat/gpio-cfg.h> 
#include "goodix_touch.h" 
/* #include <linux/goodix_queue.h> //used before android 2.2 */ 

#ifndef GUITAR_GT80X 
#error The code does not match the hardware version. 
#endif 

const char *s3c_ts_name = "Goodix TouchScreen"; 
static struct workqueue_struct *goodix_wq; 

/*used by GT80X-IAP module */ 
struct i2c_client * i2c_connect_client = NULL; 
EXPORT_SYMBOL(i2c_connect_client); 

#ifdef CONFIG_HAS_EARLYSUSPEND 
static void goodix_ts_early_suspend(struct early_suspend *h); 
static void goodix_ts_late_resume(struct early_suspend *h); 
#endif 

/* 0----1----2----3----4----5----6----7----8----9----A----B----C----D----E----F */ 
#if 0 
static uint8_t config_info_GUITAR_CONFIG_43[ 54 ] = { 
0x30,0x19,0x05,0x06,0x28,0x02,0x14,0x14,0x10,0x50,0xB8,TOUCH_MAX_WIDTH>>8,TOUCH_MAX_WIDTH&0xFF,TOUCH_MAX_HEIGHT>>8,TOUCH_MAX_HEIGHT&0xFF,0x01, 
0x23,0x45,0x67,0x89,0xAB,0xCD,0xE1,0x00,0x00,0x32,0x32,0x05,0xCF,0x20,0x07,0x0B, 
0x8B,0x50,0x3C,0x1E,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
0x00,0x00,0x00,0x00,0x00,0x01 
}; 
static uint8_t config_info_TCL_5_INCH[ 54 ] = { 
0x30,0x19,0x05,0x06,0x28,0x02,0x14,0x14,0x10,0x40,0xB8,TOUCH_MAX_WIDTH>>8,TOUCH_MAX_WIDTH&0xFF,TOUCH_MAX_HEIGHT>>8,TOUCH_MAX_HEIGHT&0xFF,0x01, 
0x23,0x45,0x67,0x89,0xAB,0xCD,0xE1,0x00,0x00,0x00,0x00,0x0D,0xCF,0x20,0x03,0x05, 
0x83,0x50,0x3C,0x1E,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
0x00,0x00,0x00,0x00,0x00,0x01 
}; 
#endif 
static uint8_t config_info_TINY210_7_INCH_800_480[ 54 ] = { 
// 0x30,0x0f,0x05,0x06,0x28,0x02,0x14,0x14,0x10,0x28,0xb2,0x01,0xe0,0x03,0x20,0xed, // 480->0x01e0 800->0x0320 
0x30,0x0f,0x05,0x06,0x28,0x02,0x14,0x14,0x10,0x28,0xb2,TOUCH_MAX_WIDTH>>8,TOUCH_MAX_WIDTH&0xFF,TOUCH_MAX_HEIGHT>>8,TOUCH_MAX_HEIGHT&0xFF,0xed, 
0xcb,0xa9,0x87,0x65,0x43,0x21,0x00,0x00,0x00,0x00,0x00,0x4d,0xc1,0x20,0x01,0x01, 
0x41,0x64,0x3c,0x1e,0x28,0x0e,0x00,0x00,0x00,0x00,0x50,0x3c,0x32,0x71,0x00,0x00, 
0x00,0x00,0x00,0x00,0x00,0x01 
}; 

/********************************************************************** 
本程序中I2C通信方式为： 
7bit从机地址｜读写位+ buf（数据地址+读写数据） 
-------------------------------------------------------------------- 
｜ 从机地址 ｜ buf[0](数据地址) | buf[1]~buf[MAX-1](写入或读取到的数据) | 
-------------------------------------------------------------------- 
移植前请根据自身主控格式修改！ ！ 
***********************************************************************/ 

//Function as i2c_master_receive, and return 2 if operation is successful. 
static int i2c_read_bytes( struct i2c_client *client, uint8_t *buf, uint16_t len ) 
{ 
struct i2c_msg msgs[2]; 

//发送写地址 
//Write : which register address to read 
msgs[0].flags = !I2C_M_RD;//写消息 
msgs[0].addr = client->addr; 
msgs[0].len = 1; 
msgs[0].buf = buf; 
//接收数据 
//Read : data from that register address 
msgs[1].flags = I2C_M_RD; 
msgs[1].addr = client->addr; 
msgs[1].len = len-1; 
msgs[1].buf = buf+1; 

return i2c_transfer( client->adapter, msgs, 2 ); 
} 

//Function as i2c_master_send, and return 1 if operation is successful. 
static int i2c_write_bytes( struct i2c_client *client, uint8_t *data, uint16_t len ) 
{ 
struct i2c_msg msg; 

msg.flags = !I2C_M_RD;//写消息 
msg.addr = client->addr; 
msg.len = len; 
msg.buf = data; 

return i2c_transfer( client->adapter, &msg, 1 ); 
} 

/******************************************************* 
功能： 
GT80X初始化函数，用于发送配置信息参数： 
ts: struct goodix_ts_data 
return： 
执行结果码，0表示正常执行 
*******************************************************/ 
static bool goodix_init_panel( struct goodix_ts_data *ts ) 
{ 
int ret; 
int count; 

//There are some demo configs. MUST be changed as some different panels. 
for( count = 5; count > 0; count-- ){ 

ret = i2c_write_bytes( ts->client, config_info_TINY210_7_INCH_800_480, sizeof( config_info_TINY210_7_INCH_800_480 ) ); 
if( ret == 1 ){ //Initiall success 
break; 
} 
else{ 
msleep( 10 ); 
} 
} 

return ( ( ret == 1 ) ? true : false ); 
} 

/*读取GT80X的版本号并打印*/ 
static int goodix_read_version( struct goodix_ts_data *ts ) 
{ 
#define GT80X_VERSION_LENGTH 40 
int ret; 
uint8_t version[ 2 ] = { 0x69,0xff }; //command of reading Guitar's version 
uint8_t version_data[ GT80X_VERSION_LENGTH + 1 ] = { 0x6a };//store touchscreen version infomation 

memset( version_data+1, 0, GT80X_VERSION_LENGTH ); 

ret = i2c_write_bytes( ts->client, version, 2 ); 
if( ret != 1 ){ 
return ret; 
} 

msleep( 50 ); 

// version_data -> [0]:write:register [1-39]:read:version [40]:\0:version-string-NULL 
ret = i2c_read_bytes( ts->client, version_data, GT80X_VERSION_LENGTH ); 
if( ret != 2 ){ 
strncpy( version_data+1, "NULL", 4 ); 
} 
dev_info( &ts->client->dev, "GT80X Version: %s\n", version_data+1 ); 

version[1] = 0x00; //cancel the command 
i2c_write_bytes( ts->client, version, 2 ); 

return 0; 
} 

/******************************************************* 
功能： 
触摸屏工作函数 
由中断触发，接受1组坐标数据，校验后再分析输出参数： 
ts: client私有数据结构体 
return： 
执行结果码，0表示正常执行 
********************************************************/ 
static void goodix_ts_work_func( struct work_struct *work ) 
{ 
static struct point_node pointer[ MAX_FINGER_NUM ]; 
static uint8_t finger_last = 0; //last time fingers' state 

struct point_node * p = NULL; 
uint8_t read_position = 0; 
uint8_t point_data[ READ_BYTES_NUM ]={ 0 }; 
uint8_t finger, finger_current; //record which finger is changed 
uint8_t check_sum = 0; 
unsigned int x, y; 
int count = 0; 
int ret = -1; 

struct goodix_ts_data *ts = container_of(work, struct goodix_ts_data, work); 

if( (ts->use_shutdown) && (gpio_get_value( ts->gpio_shutdown )) ){ 
goto NO_ACTION; //The data is invalid. 
} 

//if i2c-transfer is failed, let it restart less than 10 times. FOR DEBUG. 
/* 
if( ts->retry > 9) { 
if(!ts->use_irq && (ts->timer.state != HRTIMER_STATE_INACTIVE)) 
hrtimer_cancel(&ts->timer); 
dev_info(&(ts->client->dev), "Because of transfer error, %s stop working.\n",s3c_ts_name); 
return ; 
} 
*/ 
ret = i2c_read_bytes( ts->client, point_data, sizeof( point_data ) ); 
if( ret <= 0 ){ 

dev_dbg( &(ts->client->dev), "I2C transfer error. ERROR Number:%d\n ", ret ); 

ts->retry++; 
if( ts->retry >= 100 ){ //It's not normal for too much i2c-error. 

dev_err( &(ts->client->dev ), "Reset the chip for i2c error.\n " ); 
ts->retry = 0; 

if( ts->power ){ 
ts->power( ts, 0 ); 
ts->power( ts, 1 ); 
} 
else{ 
goodix_init_panel( ts ); 
msleep( 200 ); 
} 
} 
goto XFER_ERROR; 
} 

//如果能够保证在INT中断后及时的读取坐标数据，可以不进行校验 
if( !ts->use_irq ){ 

switch( point_data[1] & 0x1f ){ 
case 0: 
break; 

case 1: 
for( count=1; count<8; count++ ){ 
check_sum += (int)point_data[ count ]; 
} 
read_position = 8; 
break; 

case 2: 
case 3: 
for( count=1; count<13; count++ ){ 
check_sum += (int)point_data[ count ]; 
} 
read_position = 13; 
break; 

default: //(point_data[1]& 0x1f) > 3 
for( count=1; count<34;count++ ){ 
check_sum += (int)point_data[ count ]; 
} 
read_position = 34; 
break; 
} 

if( check_sum != point_data[ read_position ] ){ 
goto XFER_ERROR; 
} 
} 

//The bits indicate which fingers pressed down 
finger_current = point_data[1] & 0x1f; 
finger = finger_current^finger_last; 

if( ( finger == 0 ) && ( finger_current == 0 ) ){ 
goto NO_ACTION; //no action 
} 
else if( finger == 0 ){ 
goto BIT_NO_CHANGE; //the same as last time 
} 

//check which point(s) DOWN or UP 
for( count = 0; count < MAX_FINGER_NUM; count++ ){ 

p = &pointer[count]; 
p->id = count; 
if( ( finger_current & FLAG_MASK ) != 0 ){ 
p->state = FLAG_DOWN; 
} 
else{ 
if( ( finger & FLAG_MASK ) != 0 ){ //send press release. 
p->state = FLAG_UP; 
} 
else{ 
p->state = FLAG_INVALID; 
} 
} 

finger >>= 1; 
finger_current >>= 1; 
} 
finger_last = point_data[1] & 0x1f; //restore last presse state. 

BIT_NO_CHANGE: 
for( count = 0; count < MAX_FINGER_NUM; count++ ){ 

p = &pointer[count]; 

if( p->state == FLAG_INVALID ){ 
continue; 
} 
if( p->state == FLAG_UP ){ 
x = y = 0; 
p->pressure = 0; 
continue; 
} 

if( p->id < 3 ){ 
read_position = p->id *5 + 3; 
} 
else{ 
read_position = 29; 
} 

if( p->id != 3 ){ 
x = (unsigned int)( point_data[ read_position ]<<8) + (unsigned int)( point_data[ read_position+1 ] ); 
y = (unsigned int)( point_data[ read_position+2 ]<<8) + (unsigned int)( point_data[ read_position+3 ] ); 
p->pressure = point_data[ read_position+4 ]; 
} 
#if MAX_FINGER_NUM > 3 
else { 
x = (unsigned int)( point_data[18]<<8 ) + (unsigned int)( point_data[25] ); 
y = (unsigned int)( point_data[26]<<8 ) + (unsigned int)( point_data[27] ); 
p->pressure = point_data[28]; 
} 
#endif 

// 将触摸屏的坐标映射到LCD坐标上. 触摸屏短边为X轴，LCD坐标一般长边为X轴，可能需要调整原点位置 
x = x * SCREEN_MAX_WIDTH / TOUCH_MAX_WIDTH; 
y = y * SCREEN_MAX_HEIGHT / TOUCH_MAX_HEIGHT ; 

p->x = SCREEN_MAX_HEIGHT - y; 
p->y = x; 
} 

/*
if( pointer[0].state == FLAG_DOWN ){ 
input_report_abs( ts->input_dev, ABS_X, pointer[0].x ); 
input_report_abs( ts->input_dev, ABS_Y, pointer[0].y ); 
} 
input_report_abs( ts->input_dev, ABS_PRESSURE, pointer[0].pressure); 
input_report_key( ts->input_dev, BTN_TOUCH, ( ( pointer[0].state == FLAG_INVALID) ? FLAG_UP : pointer[0].state ) ); 
*/

/* ABS_MT_TOUCH_MAJOR is used as ABS_MT_PRESSURE in android. */ 
// for android
for( count = 0; count < MAX_FINGER_NUM; count++ ){ 

p = &pointer[count]; 
if( p->state == FLAG_INVALID ){ 
continue; 
} 

if( p->state == FLAG_DOWN ){ 

input_report_abs( ts->input_dev, ABS_MT_POSITION_X, p->x ); 
input_report_abs( ts->input_dev, ABS_MT_POSITION_Y, p->y ); 
dev_dbg( &(ts->client->dev), "Id:%d, x:%d, y:%d\n", p->id, p->x, p->y ); 
} 

input_report_abs( ts->input_dev, ABS_MT_TRACKING_ID, p->id ); 
input_report_abs( ts->input_dev, ABS_MT_TOUCH_MAJOR, p->pressure ); 
input_report_abs( ts->input_dev, ABS_MT_WIDTH_MAJOR, p->pressure ); 

input_mt_sync( ts->input_dev ); 
} 
// for android

input_sync( ts->input_dev ); 

XFER_ERROR: 
NO_ACTION: 
if( ts->use_irq ){ 
enable_irq( ts->client->irq ); 
} 
} 

/******************************************************* 
功能： 
计时器响应函数 
由计时器触发，调度触摸屏工作函数运行；之后重新计时参数： 
timer：函数关联的计时器 
return： 
计时器工作模式，HRTIMER_NORESTART表示不需要自动重启 
********************************************************/ 
static enum hrtimer_restart goodix_ts_timer_func( struct hrtimer *timer ) 
{ 
struct goodix_ts_data *ts; 

ts = container_of( timer, struct goodix_ts_data, timer ); 

queue_work( goodix_wq, &ts->work ); 
if( ts->timer.state != HRTIMER_STATE_INACTIVE ){ 
hrtimer_start( &ts->timer, ktime_set( 0, 16000000 ), HRTIMER_MODE_REL ); 
} 

return HRTIMER_NORESTART; 
} 

/******************************************************* 
功能： 
中断响应函数 
由中断触发，调度触摸屏处理函数运行 
********************************************************/ 
static irqreturn_t goodix_ts_irq_handler( int irq, void *dev_id ) 
{ 
struct goodix_ts_data *ts; 

ts = dev_id; 

disable_irq_nosync( ts->client->irq ); 
queue_work( goodix_wq, &ts->work ); 

return IRQ_HANDLED; 
} 

/******************************************************* 
功能： 
GT80X的电源管理参数： 
on:设置GT80X运行模式，0为进入Sleep模式 
return： 
是否设置成功，小于0表示设置失败 
********************************************************/ 
static int goodix_ts_power( struct goodix_ts_data * ts, int on ) 
{ 
int ret = 0; 


if( !ts->use_shutdown ){ 
return -1; 
} 

if( on ){ 
gpio_set_value( ts->gpio_shutdown, 0 ); 
msleep( 5 ); 

if( gpio_get_value( ts->gpio_shutdown ) ){ //has been waked up 
ret = -1; 
} 
else { 
msleep( 200 ); 
} 
} 
else{ 
gpio_set_value( ts->gpio_shutdown, 1 ); 
msleep( 5 ); 

if( gpio_get_value( ts->gpio_shutdown ) ){ //has been suspended 
ret = 0; 
} 
} 

dev_dbg( &ts->client->dev, "Set Guitar's Shutdown %s. Ret:%d.\n", on?"HIGH":"LOW", ret ); 
return ret; 
} 

//Test i2c to check device. Before it SHUTDOWN port Must be low state 30ms or more. 
static bool goodix_i2c_test(struct i2c_client * client) 
{ 
int ret, retry; 
uint8_t test_data[1] = { 0 }; //only write a data address. 

for(retry=0; retry < 5; retry++) 
{ 
ret =i2c_write_bytes(client, test_data, 1); //Test i2c. 
if (ret == 1) 
break; 
msleep(5); 
} 

return ret==1 ? true : false; 
} 

/******************************************************* 
功能： 
触摸屏探测函数 
在注册驱动时调用（要求存在对应的client）； 
用于IO,中断等资源申请；设备注册；触摸屏初始化等工作参数： 
client：待驱动的设备结构体 
id：设备ID 
return： 
执行结果码，0表示正常执行 
********************************************************/ 
static int goodix_ts_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{ 
struct goodix_i2c_platform_data *pdata; 
struct goodix_ts_data *ts; 
int ret = 0; 

dev_dbg( &client->dev, "Install touchscreen driver for guitar.\n" ); 

if ( !i2c_check_functionality( client->adapter, I2C_FUNC_I2C ) ){ 
dev_err(&client->dev, "System need I2C function.\n"); 
ret = -ENODEV; 
goto err_check_functionality_failed; 
} 

ts = kzalloc( sizeof( *ts ), GFP_KERNEL ); 
if( ts == NULL ){ 
ret = -ENOMEM; 
goto err_alloc_data_failed; 
} 

/* 获取预定义资源*/ 
pdata = client->dev.platform_data; 
if( pdata ){ 
/* use this pdata such as that: */ 
ts->gpio_shutdown = pdata->gpio_shutdown; 
ts->gpio_irq = pdata->gpio_irq; 

s3c_gpio_cfgpin(ts->gpio_irq, pdata->irq_cfg); //INT 
//s3c_gpio_cfgpin(ts->gpio_shutdown, pdata->shutdown_cfg); //Output 
} 
else{ 
/*If pdata is not used, get value from head files. */ 
#ifdef SHUTDOWN_PORT 
ts->gpio_shutdown = SHUTDOWN_PORT; 
#endif 
#ifdef INT_PORT 
ts->gpio_irq = INT_PORT; 
s3c_gpio_cfgpin(ts->gpio_irq, INT_CFG); //Set INT function 
client->irq = gpio_to_irq(ts->gpio_irq); 
#endif 
} 

if( ts->gpio_shutdown ){ 

ret = gpio_request(ts->gpio_shutdown, "TS_SHUTDOWN"); //Request IO 
if( ret < 0 ){ 
printk( KERN_ALERT "Failed to request GPIO:%d, ERRNO:%d\n", ts->gpio_shutdown, ret ); 
goto err_gpio_request; 
} 

gpio_direction_output(ts->gpio_shutdown, 0); //Touchscreen is waiting to wake up 
msleep( 2 ); 
ret = gpio_get_value( ts->gpio_shutdown ); 
if( ret ){ 
printk( KERN_ALERT "Can't set touchscreen to work.\n" ); 
goto err_i2c_failed; 
} 
ts->use_shutdown = true; 
msleep(25); //waiting for initialization of Guitar. 
} 
else{ 
ts->use_shutdown = false; 
} 

//this is for s3c2400, not suitable for tiny210 
#if 0 
int count=0; 
i2c_connect_client = client; //used by Guitar Updating. 
//TODO: used to set speed of i2c transfer. Should be change as your paltform. 
s3c24xx_set_i2c_clockrate( client->adapter, 250, &count ); //set i2c <=250kHz 
dev_dbg( &client->dev, "i2c set frequency:%dkHz.\n", count ); 
#endif 

ret = goodix_i2c_test( client ); 
if( !ret ){ 
dev_err( &client->dev, "Warnning: I2C connection might be something wrong!\n" ); 
goto err_i2c_failed; 
} 

if( ts->use_shutdown ){ 
gpio_set_value( ts->gpio_shutdown, 1 ); //suspend 
} 

INIT_WORK( &ts->work, goodix_ts_work_func ); 
ts->client = client; 
i2c_set_clientdata( client, ts ); 

ts->input_dev = input_allocate_device(); 
if( !ts->input_dev ){ 
ret = -ENOMEM; 
dev_dbg( &client->dev, "Failed to allocate input device\n" ); 
goto err_input_dev_alloc_failed; 
} 

ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ; 

/*
ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH); 
ts->input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE); 
input_set_abs_params(ts->input_dev, ABS_X, 0, SCREEN_MAX_HEIGHT, 0, 0); 
input_set_abs_params(ts->input_dev, ABS_Y, 0, SCREEN_MAX_WIDTH, 0, 0); 
input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0); 
*/

// for android 
ts->input_dev->absbit[0] = BIT_MASK(ABS_MT_TRACKING_ID) | 
BIT_MASK(ABS_MT_TOUCH_MAJOR) | 
BIT_MASK(ABS_MT_WIDTH_MAJOR) | 
BIT_MASK(ABS_MT_POSITION_X) | 
BIT_MASK(ABS_MT_POSITION_Y); 
input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0); 
input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0); 
input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_HEIGHT, 0, 0); 
input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_WIDTH, 0, 0); 
input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MAX_FINGER_NUM, 0, 0); 
// for android

sprintf( ts->phys, "input/goodix-ts" ); 
ts->input_dev->name = s3c_ts_name; 
ts->input_dev->phys = ts->phys; 
ts->input_dev->id.bustype = BUS_I2C; 
ts->input_dev->id.vendor = 0xDEAD; 
ts->input_dev->id.product = 0xBEEF; 
ts->input_dev->id.version = 0x1105; 

ret = input_register_device( ts->input_dev ); 
if( ret ){ 
dev_err( &client->dev,"Unable to register %s input device\n", ts->input_dev->name ); 
goto err_input_register_device_failed; 
} 

if( client->irq ){ 

ret = gpio_request( ts->gpio_irq, "TS_INT" ); //Request IO 
if( ret < 0 ){ 
dev_err( &client->dev, "Failed to request GPIO:%d, ERRNO:%d\n", ts->gpio_irq, ret ); 
goto err_int_request_failed; 
} 

ret = request_irq( client->irq, goodix_ts_irq_handler, IRQ_TYPE_EDGE_RISING, client->name, ts ); 
if( ret != 0 ){ 
dev_err( &client->dev,"Can't allocate touchscreen's interrupt! ERRNO:%d\n", ret ); 
gpio_direction_input( ts->gpio_irq ); 
gpio_free( ts->gpio_irq ); 
ts->use_irq = false; 
goto err_int_request_failed; 
} 
else{ 
disable_irq( client->irq ); 
ts->use_irq = true; 
dev_dbg( &client->dev,"Request EIRQ %d succesd on GPIO:%d\n",client->irq, ts->gpio_irq ); 
} 
} 
else{ 
ts->use_irq = false; 
} 

err_int_request_failed: 
if( !ts->use_irq ){ 
hrtimer_init( &ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL ); 
ts->timer.function = goodix_ts_timer_func; 
hrtimer_start( &ts->timer, ktime_set( 1, 0 ), HRTIMER_MODE_REL ); 
} 

flush_workqueue( goodix_wq ); 
if( ts->use_shutdown ){ 
gpio_set_value( ts->gpio_shutdown, 0 ); 
ts->power = goodix_ts_power; 
msleep( 30 ); 
} 

ret = goodix_init_panel( ts ); 
if( !ret ){ 
goto err_init_godix_ts; 
} 

if( ts->use_irq ){ 
enable_irq( client->irq ); 
} 

goodix_read_version( ts ); 
//msleep(260); 

#ifdef CONFIG_HAS_EARLYSUSPEND 
ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1; 
ts->early_suspend.suspend = goodix_ts_early_suspend; 
ts->early_suspend.resume = goodix_ts_late_resume; 
register_early_suspend( &ts->early_suspend ); 
#endif 
dev_dbg( &client->dev, "Start %s in %s mode\n", ts->input_dev->name, ( ts->use_irq ? "Interrupt" : "Polling" ) ); 
return 0; 

err_init_godix_ts: 
if( ts->use_irq ){ 
free_irq( client->irq, ts ); 
gpio_free( ts->gpio_irq ); 
} 

err_input_register_device_failed: 
input_free_device( ts->input_dev ); 

err_input_dev_alloc_failed: 
i2c_set_clientdata( client, NULL ); 

err_i2c_failed: 
if( ts->use_shutdown ){ 
gpio_direction_input( ts->gpio_shutdown ); 
gpio_free( ts->gpio_shutdown ); 
} 

err_gpio_request: 
kfree( ts ); 

err_alloc_data_failed: 
err_check_functionality_failed: 
return ret; 
} 


/******************************************************* 
功能： 
驱动资源释放参数： 
client：设备结构体 
return： 
执行结果码，0表示正常执行 
********************************************************/ 
static int goodix_ts_remove( struct i2c_client *client ) 
{ 
struct goodix_ts_data *ts; 


ts = i2c_get_clientdata( client ); 
#ifdef CONFIG_HAS_EARLYSUSPEND 
unregister_early_suspend( &ts->early_suspend ); 
#endif 
if( ts->use_irq ){ 
free_irq( client->irq, ts ); 
gpio_free( ts->gpio_irq ); 
} 
else{ 
hrtimer_cancel( &ts->timer ); 
} 

if( ts->use_shutdown ){ 
gpio_direction_input( ts->gpio_shutdown ); 
gpio_free( ts->gpio_shutdown ); 
} 

dev_notice( &client->dev,"The driver is removing...\n" ); 
i2c_set_clientdata( client, NULL ); 
input_unregister_device( ts->input_dev ); 

if( ts->input_dev ){ 
input_free_device( ts->input_dev ); 
//kfree( ts->input_dev ); 
} 
kfree( ts ); 

return 0; 
} 

//停用设备 
static int goodix_ts_suspend( struct i2c_client *client, pm_message_t mesg ) 
{ 
int ret; 
struct goodix_ts_data *ts; 

/* prevent the interrupt from occuring 
* or stop the high revolution timer 
*/ 
ts = i2c_get_clientdata( client ); 
if( ts->use_irq ){ 
disable_irq( client->irq ); 
} 
else if( ts->timer.state ){ 
hrtimer_cancel( &ts->timer ); 
} 

ret = cancel_work_sync( &ts->work ); 
if( ret && ts->use_irq ){ //irq was disabled twice. 
enable_irq( client->irq ); 
} 

if( ts->power ){ 

ret = ts->power( ts,0 ); 
if( ret < 0 ){ 
dev_warn( &client->dev, "%s power off failed\n", s3c_ts_name ); 
} 
} 
return 0; 
} 

//重新唤醒 
static int goodix_ts_resume( struct i2c_client *client ) 
{ 
int ret; 
struct goodix_ts_data *ts; 

ts = i2c_get_clientdata( client ); 
if( ts->power ){ 

ret = ts->power( ts, 1 ); 
if( ret < 0 ){ 
dev_warn( &client->dev, "%s power on failed\n", s3c_ts_name ); 
} 
} 

if( ts->use_irq ){ 
enable_irq( client->irq ); 
} 
else{ 
hrtimer_start( &ts->timer, ktime_set( 1, 0 ), HRTIMER_MODE_REL ); 
} 

return 0; 
} 

#ifdef CONFIG_HAS_EARLYSUSPEND 
static void goodix_ts_early_suspend( struct early_suspend *h ) 
{ 
struct goodix_ts_data *ts; 

ts = container_of( h, struct goodix_ts_data, early_suspend ); 
goodix_ts_suspend( ts->client, PMSG_SUSPEND ); 
} 

static void goodix_ts_late_resume( struct early_suspend *h ) 
{ 
struct goodix_ts_data *ts; 

ts = container_of( h, struct goodix_ts_data, early_suspend ); 
goodix_ts_resume( ts->client ); 
} 
#endif 

//可用于该驱动的设备名—设备ID 列表 
//only one client 
static const struct i2c_device_id goodix_ts_id[] = { 
{ GOODIX_I2C_NAME, 0 }, 
{ } 
}; 

//设备驱动结构体 
static struct i2c_driver goodix_ts_driver = { 
.probe = goodix_ts_probe, 
.remove = goodix_ts_remove, 
#ifndef CONFIG_HAS_EARLYSUSPEND 
.suspend = goodix_ts_suspend, 
.resume = goodix_ts_resume, 
#endif 
.id_table = goodix_ts_id, 
.driver = { 
.name = GOODIX_I2C_NAME, 
.owner = THIS_MODULE, 
}, 
}; 

//驱动加载函数 
//called when insert this module into kernel 
static int __devinit goodix_ts_init( void ) 
{ 
int ret; 


goodix_wq = create_singlethread_workqueue( "goodix_wq" ); 
if( !goodix_wq ){ 

printk( KERN_ALERT "creat %s workqueue failed.\n", s3c_ts_name ); 
ret = -ENOMEM; 
} 
else{ 
ret = i2c_add_driver( &goodix_ts_driver ); 
} 

return ret; 
} 

//驱动卸载函数 
//called when remove this module from kernel 
static void __exit goodix_ts_exit( void ) 
{ 
if( goodix_wq ){ 
destroy_workqueue( goodix_wq ); 
} 

i2c_del_driver( &goodix_ts_driver ); 
} 

late_initcall( goodix_ts_init ); 
module_exit( goodix_ts_exit ); 

MODULE_DESCRIPTION("Goodix Touchscreen Driver"); 
MODULE_LICENSE("GPL v2");
