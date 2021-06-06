/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2015 Regents of the University of California
 */

#ifndef _ASM_RISCV_CACHEFLUSH_H
#define _ASM_RISCV_CACHEFLUSH_H

#include <linux/mm.h>

static inline void local_flush_icache_all(void)
{
	asm volatile ("fence.i" ::: "memory");
}

#define PG_dcache_clean PG_arch_1

static inline void flush_dcache_page(struct page *page)
{
	if (test_bit(PG_dcache_clean, &page->flags))
		clear_bit(PG_dcache_clean, &page->flags);
}
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1

#define ICACHE_IPA_X5   ".long 0x0382800b"
#define ICACHE_IVA_X5   ".long 0x0302800b"
#define SYNC_IS         ".long 0x01b0000b"

static inline void flush_icache_range(unsigned long start, unsigned long end)
{
	register unsigned long tmp asm("x5") = start & (~(L1_CACHE_BYTES-1));

	for (; tmp < ALIGN(end, L1_CACHE_BYTES); tmp += L1_CACHE_BYTES) {
		__asm__ __volatile__ (
				ICACHE_IVA_X5
				:
				: "r" (tmp)
				: "memory");
	}

	__asm__ __volatile__(SYNC_IS);

	return;
}

static inline void flush_icache_range_phy(unsigned long start, unsigned long end)
{
	register unsigned long tmp asm("x5") = start & (~(L1_CACHE_BYTES-1));

	for (; tmp < ALIGN(end, L1_CACHE_BYTES); tmp += L1_CACHE_BYTES) {
		__asm__ __volatile__ (
				ICACHE_IPA_X5
				:
				: "r" (tmp)
				: "memory");
	}

	__asm__ __volatile__(SYNC_IS);

	return;
}

static inline void __flush_icache_page(struct page *page) {
	unsigned long start = PFN_PHYS(page_to_pfn(page));

	flush_icache_range_phy(start, start + PAGE_SIZE);

	return;
}

/*
 * RISC-V doesn't have an instruction to flush parts of the instruction cache,
 * so instead we just flush the whole thing.
 */
#define flush_icache_range(start, end) flush_icache_range(start, end)
#define flush_icache_user_page(vma, pg, addr, len) \
	flush_icache_mm(vma->vm_mm, 0)

#ifndef CONFIG_SMP

#define flush_icache_all() local_flush_icache_all()
#define flush_icache_mm(mm, local) flush_icache_all()

#else /* CONFIG_SMP */

void flush_icache_all(void);
void flush_icache_mm(struct mm_struct *mm, bool local);

#endif /* CONFIG_SMP */

/*
 * Bits in sys_riscv_flush_icache()'s flags argument.
 */
#define SYS_RISCV_FLUSH_ICACHE_LOCAL 1UL
#define SYS_RISCV_FLUSH_ICACHE_ALL   (SYS_RISCV_FLUSH_ICACHE_LOCAL)

#include <asm-generic/cacheflush.h>

#endif /* _ASM_RISCV_CACHEFLUSH_H */
