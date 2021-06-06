// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/libfdt.h>
#include <linux/pgtable.h>
#include <asm/image.h>
#include <asm/soc.h>

/*
 * This is called extremly early, before parse_dtb(), to allow initializing
 * SoC hardware before memory or any device driver initialization.
 */
void __init soc_early_init(void)
{
	void (*early_fn)(const void *fdt);
	const struct of_device_id *s;
	const void *fdt = dtb_early_va;

	for (s = (void *)&__soc_early_init_table_start;
	     (void *)s < (void *)&__soc_early_init_table_end; s++) {
		if (!fdt_node_check_compatible(fdt, 0, s->compatible)) {
			early_fn = s->data;
			early_fn(fdt);
			return;
		}
	}
}

static void __init thead_init(void)
{
	__riscv_custom_pte.cache = 0x7000000000000000;
	__riscv_custom_pte.mask  = 0xf800000000000000;
	__riscv_custom_pte.io    = BIT(63);
	__riscv_custom_pte.wc    = 0;
}

void __init soc_setup_vm(void)
{
	unsigned long vendor_id =
		((struct riscv_image_header *)(&_start))->res1;

	switch (vendor_id) {
	case THEAD_VENDOR_ID:
	// Do not rely on the bootloader...
	default:
		thead_init();
		break;
	}
};
