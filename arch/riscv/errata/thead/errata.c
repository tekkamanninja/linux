// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/alternative.h>
#include <asm/dma-noncoherent.h>
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

#ifdef CONFIG_RISCV_DMA_NONCOHERENT
/*
 * dcache.ipa rs1 (invalidate)
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000001    01010      rs1       000      00000  0001011
 *
 * dcache.cpa rs1 (clean)
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000001    01001      rs1       000      00000  0001011
 *
 * dcache.cipa rs1 (clean then invalidate)
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000001    01011      rs1       000      00000  0001011
 *
 * sync.s
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000000    11001     00000      000      00000  0001011
 */
#define DCACHE_IPA_A0	".long 0x02a5000b"
#define DCACHE_CPA_A0	".long 0x0295000b"
#define DCACHE_CIPA_A0	".long 0x02b5000b"

#define SYNC_S		".long 0x0190000b"

#define CACHE_OP_RANGE(OP, start, size) \
	register unsigned long i asm("a0") = start & ~(L1_CACHE_BYTES - 1); \
	for (; i < ALIGN(start + size, L1_CACHE_BYTES); i += L1_CACHE_BYTES) \
		__asm__ __volatile__(OP); \
	 __asm__ __volatile__(SYNC_S);

static void c900_cache_invalidate(phys_addr_t start, size_t size)
{
	CACHE_OP_RANGE(DCACHE_IPA_A0, start, size);
}

static void c900_cache_clean(phys_addr_t start, size_t size)
{
	CACHE_OP_RANGE(DCACHE_CPA_A0, start, size);
}

static void c900_cache_flush(phys_addr_t start, size_t size)
{
	CACHE_OP_RANGE(DCACHE_CIPA_A0, start, size);
}

static struct riscv_dma_cache_sync c900_dma_cache_sync = {
	.cache_invalidate = c900_cache_invalidate,
	.cache_clean = c900_cache_clean,
	.cache_flush = c900_cache_flush,
};
#endif

void __init thead_errata_patch_func(struct alt_entry *begin, struct alt_entry *end,
				     unsigned long archid, unsigned long impid)
{
#ifdef CONFIG_RISCV_DMA_NONCOHERENT
	riscv_dma_cache_sync_set(&c900_dma_cache_sync);
#endif
}
