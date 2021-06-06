/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017 SiFive
 */

#include <linux/linkage.h>
#include <asm/unistd.h>
#include <asm/cache.h>

/*
 * icache.ipa rs1
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000001    11000      rs1       000      00000  0001011
 *
 * icache.iva rs1
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000001    10000      rs1       000      00000  0001011
 *
 * sync.is
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000000    11011     00000      000      00000  0001011
 */
#define ICACHE_IPA_X5	.long 0x0382800b
#define ICACHE_IVA_X5	.long 0x0302800b
#define SYNC_IS		.long 0x01b0000b

	.text
/* int __vdso_flush_icache(void *start, void *end, unsigned long flags); */
ENTRY(__vdso_flush_icache)
	.cfi_startproc
	srli	t0, a0, L1_CACHE_SHIFT
	slli	t0, t0, L1_CACHE_SHIFT
	addi	a1, a1, (L1_CACHE_BYTES - 1)
	srli	a1, a1, L1_CACHE_SHIFT
	slli	a1, a1, L1_CACHE_SHIFT
1:	ICACHE_IVA_X5
	addi	t0, t0, L1_CACHE_BYTES
	bne	t0, a1, 1b
	SYNC_IS
	li a0, 0
	ret
	.cfi_endproc
ENDPROC(__vdso_flush_icache)
