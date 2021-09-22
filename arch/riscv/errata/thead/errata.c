// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <asm/alternative.h>
#include <asm/vendorid_list.h>
#include <asm/errata_list.h>

void __init thead_errata_setup_vm(unsigned long archid, unsigned long impid)
{
}

void __init thead_errata_patch_func(struct alt_entry *begin, struct alt_entry *end,
				     unsigned long archid, unsigned long impid)
{
}
