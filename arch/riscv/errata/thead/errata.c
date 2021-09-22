// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/alternative.h>
#include <asm/vendorid_list.h>
#include <asm/errata_list.h>

void __init thead_errata_setup_vm(unsigned long archid, unsigned long impid)
{
#ifdef CONFIG_64BIT

#undef _SVPBMT_MASK
#undef _SVPBMT_PMA
#undef _SVPBMT_NC
#undef _SVPBMT_IO

/*
 * T-HEAD C9xx PTE format:
 * | 63 | 62 | 61 | 60 | 59-8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *   SO   C    B    SH   RSW    D   A   G   U   X   W   R   V
 *   ^    ^    ^    ^    ^
 * BIT(63): SO - Strong Order
 * BIT(62): C  - Cacheable
 * BIT(61): B  - Bufferable
 * BIT(60): SH - Shareable
 *
 * MT_MASK : [63 - 59]
 * MT_PMA  : C + SH
 * MT_NC   : (none)
 * MT_IO   : SO
 */
#define _SVPBMT_MASK	0xf800000000000000
#define _SVPBMT_PMA	0x5000000000000000
#define _SVPBMT_NC	0x0
#define _SVPBMT_IO	0x8000000000000000

	int i;

	__riscv_svpbmt.mask	= _SVPBMT_MASK;
	__riscv_svpbmt.mt_pma	= _SVPBMT_PMA;
	__riscv_svpbmt.mt_nc	= _SVPBMT_NC;
	__riscv_svpbmt.mt_io	= _SVPBMT_IO;

	for ( i = 0; i < 16; i++) {
		protection_map[i].pgprot |= _SVPBMT_PMA;
	}
#endif
}

void __init thead_errata_patch_func(struct alt_entry *begin, struct alt_entry *end,
				     unsigned long archid, unsigned long impid)
{
}
