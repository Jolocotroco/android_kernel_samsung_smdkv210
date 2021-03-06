/*
 * linux/arch/arm/plat-samsung/dev-keypad.c
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/platform_device.h>
#include <mach/irqs.h>
#include <mach/map.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/keypad.h>

static struct resource s3c_keypad_resource[] = {
	[0] = {
		.start = SAMSUNG_PA_KEYPAD,
		.end   = SAMSUNG_PA_KEYPAD + SAMSUNG_SZ_KEYPAD - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_KEYPAD,
		.end   = IRQ_KEYPAD,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device deviceKeypad = {
        .name             = "keypad",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_keypad_resource),
        .resource         = s3c_keypad_resource,
};
EXPORT_SYMBOL(deviceKeypad);

void __init samsung_keypad_set_platdata(struct samsung_keypad_platdata *pd)
{
	struct samsung_keypad_platdata *npd;

	npd = s3c_set_platdata(pd, sizeof(struct samsung_keypad_platdata),
			&deviceKeypad);

	if (!npd->cfg_gpio)
		npd->cfg_gpio = samsung_keypad_cfg_gpio;
}
