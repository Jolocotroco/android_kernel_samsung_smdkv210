/* linux/arch/arm/mach-s3c64xx/dev-ts.c
 *
 * Copyright (c) 2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>, <ben-linux@fluff.org>
 *
 * Adapted by Maurus Cuelenaere for s3c64xx
 *
 * S3C64XX series device definition for touchscreen device
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>

#include <mach/irqs.h>
#include <mach/map.h>

#include <plat/ts.h>
#include <plat/devs.h>
#include <plat/cpu.h>

static struct resource s3c_ts_resource[] = {
	[0] = {
		.start = SAMSUNG_PA_ADC1,
		.end   = SAMSUNG_PA_ADC1 + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_PENDN1,
		.end   = IRQ_PENDN1,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_ADC1,
		.end   = IRQ_ADC1,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_ts = {
	.name		    = "s3c-ts",
	.id		        = -1,
	.num_resources  = ARRAY_SIZE(s3c_ts_resource),
	.resource	    = s3c_ts_resource,
};

void __init s3c_ts_set_platdata(struct s3c_ts_mach_info *pd)
{
	struct s3c_ts_mach_info *npd;

	if (!pd) {
		printk(KERN_ERR "%s: no platform data\n", __func__);
		return;
	}

	npd = kmemdup(pd, sizeof(struct s3c_ts_mach_info), GFP_KERNEL);
	if (npd) {
		memcpy(npd, pd, sizeof(*npd));
		s3c_device_ts.dev.platform_data = npd;
	} else {
		pr_err("no memory for Touchscreen platform data\n");
	}
}

