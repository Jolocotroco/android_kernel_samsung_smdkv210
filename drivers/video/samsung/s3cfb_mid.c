/* MID LCD Panel Driver for the SMDK
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-fb.h> 
#include <mach/regs-clock.h> 
#include <mach/regs-gpio.h>
#include <mach/gpio-mid.h> 

#include "s3cfb.h"

void s3cfb_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}

static void s3cfb_mid_lcd_onoff(unsigned int onoff)
{
    int err;

    unsigned int nGPIOs[] = {GPIO_LCD_BACKLIGHT_POWER, GPIO_LCD_BACKLIGHT_POWERB};

	if ((err = gpio_request(nGPIOs[0], "lcd-backlight-en"))) {
		printk(KERN_ERR "failed to request gpio for lcd control: %d\n", err);
		return;
	}

	if ((err = gpio_request(nGPIOs[1], "lcd-backlight-en"))) {
		printk(KERN_ERR "failed to request gpio for lcd control: %d\n", err);
        gpio_free(nGPIOs[0]);
		return;
	}

    printk("%s: %d\n", "mid_lcd_onoff", onoff);

    if (onoff) {
        gpio_direction_output(nGPIOs[0], 1);
        gpio_direction_output(nGPIOs[1], 0);
    } else {
        gpio_direction_output(nGPIOs[0], 0);
        gpio_direction_output(nGPIOs[1], 1);
    }

	mdelay(20);
    gpio_free(nGPIOs[1]);
    gpio_free(nGPIOs[0]);
}

static void s3cfb_mid_backlight_onoff(int onoff)
{
    int err;

    unsigned int nGPIO = GPIO_BACKLIGHT;
	err = gpio_request(nGPIO, "backlight-en");

	if (err) {
		printk(KERN_ERR "failed to request gpio for backlight control: %d\n", err);
		return;
	}

    printk("%s: %d\n", "mid_backlight_onoff", onoff);

    if (onoff)
        gpio_direction_output(nGPIO, 1);
    else
        gpio_direction_output(nGPIO, 0);

	mdelay(10);
    gpio_free(nGPIO);
}

int s3cfb_backlight_onoff(struct platform_device *pdev, int onoff)
{
    if (onoff) {
        s3cfb_mid_lcd_onoff(1);
        s3cfb_mid_backlight_onoff(1);
    } else {
        s3cfb_mid_backlight_onoff(0);
        s3cfb_mid_lcd_onoff(0);
    }

	return 0;
}

int s3cfb_backlight_on(struct platform_device *pdev)
{
    return s3cfb_backlight_onoff(pdev, 1);
}

int s3cfb_reset_lcd(struct platform_device *pdev)
{
    s3cfb_mid_backlight_onoff(0);
    s3cfb_mid_lcd_onoff(0);
	mdelay(180);
    s3cfb_mid_lcd_onoff(1);
    s3cfb_mid_backlight_onoff(1);

    return 0;
}

