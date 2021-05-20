/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _CCU_SUN50I_RTC_H
#define _CCU_SUN50I_RTC_H

#include <dt-bindings/clock/sun50i-rtc.h>

#define CLK_IOSC_32K		4
#define CLK_EXT_OSC32K_GATE	5
#define CLK_DCXO24M_32K		6
#define CLK_RTC_32K		7

#define CLK_NUMBER		(CLK_RTC_SPI + 1)

#endif /* _CCU_SUN50I_RTC_H */
