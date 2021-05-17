/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header provides constants for binding nvidia,tegra186-hsp.
 */

#ifndef _DT_BINDINGS_MAILBOX_SUN20I_D1_MSGBOX_H_
#define _DT_BINDINGS_MAILBOX_SUN20I_D1_MSGBOX_H_

/* First cell: channel (transmitting user) */
#define MBOX_USER_CPUX			0
#define MBOX_USER_DSP			1
#define MBOX_USER_RISCV			2

/* Second cell: direction (RX if phandle references local mailbox, else TX) */
#define MBOX_RX				0
#define MBOX_TX				1

#endif /* _DT_BINDINGS_MAILBOX_SUN20I_D1_MSGBOX_H_ */
