// SPDX-License-Identifier: GPL-2.0-only

#include <linux/clk-provider.h>
#include <linux/of_address.h>

#include "../clk.h"

#include "ccu_common.h"

#include "ccu_div.h"
#include "ccu_gate.h"
#include "ccu_mux.h"

#include "ccu-sun50i-rtc.h"

#define LOSC_FREQ			32768
#define LOSC_FREQ_SHIFT			15

#define LOSC_CTRL_KEY			0x16aa0000

#define IOSC_32K_CLK_DIV_REG		0x8
#define IOSC_32K_CLK_DIV		GENMASK(4, 0)
#define IOSC_32K_PRE_DIV		32

#define IOSC_CLK_CALI_REG		0xc
#define IOSC_CLK_CALI_DIV_ONES		22
#define IOSC_CLK_CALI_EN		BIT(1)
#define IOSC_CLK_CALI_SRC_SEL		BIT(0)

#define DCXO_CTRL_REG			0x160
#define DCXO_CTRL_CLK16M_RC_EN		BIT(0)

bool have_ext_osc32k;
bool have_iosc_calib;

static int ccu_iosc_enable(struct clk_hw *hw)
{
	struct ccu_common *cm = hw_to_ccu_common(hw);

	return ccu_gate_helper_enable(cm, DCXO_CTRL_CLK16M_RC_EN);
}

static void ccu_iosc_disable(struct clk_hw *hw)
{
	struct ccu_common *cm = hw_to_ccu_common(hw);

	return ccu_gate_helper_disable(cm, DCXO_CTRL_CLK16M_RC_EN);
}

static int ccu_iosc_is_enabled(struct clk_hw *hw)
{
	struct ccu_common *cm = hw_to_ccu_common(hw);

	return ccu_gate_helper_is_enabled(cm, DCXO_CTRL_CLK16M_RC_EN);
}

static unsigned long ccu_iosc_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct ccu_common *cm = hw_to_ccu_common(hw);

	if (have_iosc_calib) {
		u32 reg = readl(cm->base + IOSC_CLK_CALI_REG);

		/*
		 * Recover the IOSC frequency by shifting the ones place of
		 * (fixed-point divider * 32768) into bit zero.
		 */
		if (reg & IOSC_CLK_CALI_EN)
			return reg >> (IOSC_CLK_CALI_DIV_ONES - LOSC_FREQ_SHIFT);
	}

	return 16000000;
}

static unsigned long ccu_iosc_recalc_accuracy(struct clk_hw *hw,
					      unsigned long parent_accuracy)
{
	return 300000000;
}

static const struct clk_ops ccu_iosc_ops = {
	.enable			= ccu_iosc_enable,
	.disable		= ccu_iosc_disable,
	.is_enabled		= ccu_iosc_is_enabled,
	.recalc_rate		= ccu_iosc_recalc_rate,
	.recalc_accuracy	= ccu_iosc_recalc_accuracy,
};

static struct ccu_common iosc_clk = {
	.reg		= DCXO_CTRL_REG,
	.hw.init	= CLK_HW_INIT_NO_PARENT("iosc", &ccu_iosc_ops,
						CLK_GET_RATE_NOCACHE),
};

static int ccu_iosc_32k_enable(struct clk_hw *hw)
{
	struct ccu_common *cm = hw_to_ccu_common(hw);
	unsigned long flags;
	u32 reg;

	if (!have_iosc_calib)
		return 0;

	spin_lock_irqsave(cm->lock, flags);

	reg = readl(cm->base + IOSC_CLK_CALI_REG);
	writel(reg | IOSC_CLK_CALI_EN | IOSC_CLK_CALI_SRC_SEL,
	       cm->base + IOSC_CLK_CALI_REG);

	spin_unlock_irqrestore(cm->lock, flags);

	return 0;
}

static void ccu_iosc_32k_disable(struct clk_hw *hw)
{
	struct ccu_common *cm = hw_to_ccu_common(hw);
	unsigned long flags;
	u32 reg;

	if (!have_iosc_calib)
		return;

	spin_lock_irqsave(cm->lock, flags);

	reg = readl(cm->base + IOSC_CLK_CALI_REG);
	writel(reg & ~(IOSC_CLK_CALI_EN | IOSC_CLK_CALI_SRC_SEL),
	       cm->base + IOSC_CLK_CALI_REG);

	spin_unlock_irqrestore(cm->lock, flags);
}

static unsigned long ccu_iosc_32k_recalc_rate(struct clk_hw *hw,
					      unsigned long parent_rate)
{
	struct ccu_common *cm = hw_to_ccu_common(hw);
	u32 reg;

	if (have_iosc_calib) {
		reg = readl(cm->base + IOSC_CLK_CALI_REG);

		if (reg & IOSC_CLK_CALI_SRC_SEL)
			return LOSC_FREQ;
	}

	reg = readl(cm->base + IOSC_32K_CLK_DIV_REG) & IOSC_32K_CLK_DIV;

	return parent_rate / IOSC_32K_PRE_DIV / (reg + 1);
}

static unsigned long ccu_iosc_32k_recalc_accuracy(struct clk_hw *hw,
						  unsigned long parent_accuracy)
{
	struct ccu_common *cm = hw_to_ccu_common(hw);

	if (have_iosc_calib) {
		u32 reg = readl(cm->base + IOSC_CLK_CALI_REG);

		if (reg & IOSC_CLK_CALI_SRC_SEL)
			return 0;
	}

	return parent_accuracy;
}

static const struct clk_ops ccu_iosc_32k_ops = {
	.enable			= ccu_iosc_32k_enable,
	.disable		= ccu_iosc_32k_disable,
	.recalc_rate		= ccu_iosc_32k_recalc_rate,
	.recalc_accuracy	= ccu_iosc_32k_recalc_accuracy,
};

static struct ccu_common iosc_32k_clk = {
	.hw.init	= CLK_HW_INIT_HW("iosc-32k", &iosc_clk.hw,
					 &ccu_iosc_32k_ops, 0),
};

static const struct clk_hw *ext_osc32k[] = { NULL };
static SUNXI_CCU_GATE_HWS(ext_osc32k_gate_clk, "ext-osc32k-gate",
			  ext_osc32k, 0x0, BIT(4), 0);

static const struct clk_hw *osc32k_parents[] = { &iosc_32k_clk.hw,
						 &ext_osc32k_gate_clk.common.hw };
static SUNXI_CCU_MUX_HW_WITH_KEY(osc32k_clk, "osc32k", osc32k_parents,
				 0x0, 0, 1, LOSC_CTRL_KEY, 0);

static struct clk_fixed_rate dcxo24M_clk = {
	.fixed_rate	= 24000000,
	.hw.init	= CLK_HW_INIT_NO_PARENT("dcxo24M", &clk_fixed_rate_ops, 0),
};

static struct ccu_gate dcxo24M_32k_clk = {
	.enable	= BIT(16),
	.common	= {
		.reg		= 0x60,
		.prediv		= 750,
		.features	= CCU_FEATURE_ALL_PREDIV,
		.hw.init	= CLK_HW_INIT_HW("dcxo24M-32k",
						 &dcxo24M_clk.hw,
						 &ccu_gate_ops,
						 0),
	},
};

static CLK_FIXED_FACTOR_HW(rtc_32k_clk, "rtc-32k",
			   &osc32k_clk.common.hw, 1, 1, 0);

static const struct clk_hw *rtc_32k_parents[] = { &osc32k_clk.common.hw,
						  &dcxo24M_32k_clk.common.hw };
static SUNXI_CCU_MUX_HW_WITH_KEY(rtc_32k_mux_clk, "rtc-32k", rtc_32k_parents,
				 0x0, 1, 1, LOSC_CTRL_KEY, 0);

static CLK_FIXED_FACTOR(rtc_1k_clk, "rtc-1k",
			"rtc-32k", 32, 1, 0);

static const char *rtc_32k_fanout_parents[] = { "osc32k",
						"pll-periph0-32k",
						"dcxo24M-32k" };
static SUNXI_CCU_MUX_WITH_GATE(rtc_32k_fanout_clk, "rtc-32k-fanout",
			       rtc_32k_fanout_parents, 0x60, 1, 2, BIT(0), 0);

static SUNXI_CCU_M_WITH_GATE(rtc_spi_clk, "rtc-spi", "r-ahb",
			     0x310, 0, 5, BIT(31), 0);

static struct ccu_common *sun50i_h6_rtc_ccu_clks[] = {
	&iosc_clk,
	&iosc_32k_clk,
	&ext_osc32k_gate_clk.common,
	&osc32k_clk.common,
	&dcxo24M_32k_clk.common,
	&rtc_32k_mux_clk.common,
};

static struct ccu_common *sun50i_h616_rtc_ccu_clks[] = {
	&iosc_clk,
	&iosc_32k_clk,
	&osc32k_clk.common,
	&dcxo24M_32k_clk.common,
	&rtc_32k_fanout_clk.common,
};

static struct ccu_common *sun50i_r329_rtc_ccu_clks[] = {
	&iosc_clk,
	&iosc_32k_clk,
	&ext_osc32k_gate_clk.common,
	&osc32k_clk.common,
	&dcxo24M_32k_clk.common,
	&rtc_32k_mux_clk.common,
	&rtc_32k_fanout_clk.common,
	&rtc_spi_clk.common,
};

static struct clk_hw_onecell_data sun50i_h6_rtc_ccu_hw_clks = {
	.num = CLK_NUMBER,
	.hws = {
		[CLK_OSC32K]		= &osc32k_clk.common.hw,
		[CLK_RTC_32K_FANOUT]	= NULL,
		[CLK_IOSC]		= &iosc_clk.hw,
		[CLK_DCXO24M]		= &dcxo24M_clk.hw,

		[CLK_IOSC_32K]		= &iosc_32k_clk.hw,
		[CLK_EXT_OSC32K_GATE]	= &ext_osc32k_gate_clk.common.hw,
		[CLK_DCXO24M_32K]	= &dcxo24M_32k_clk.common.hw,
		[CLK_RTC_32K]		= &rtc_32k_mux_clk.common.hw,
		[CLK_RTC_1K]		= &rtc_1k_clk.hw,
		[CLK_RTC_SPI]		= NULL,
	},
};

static struct clk_hw_onecell_data sun50i_h616_rtc_ccu_hw_clks = {
	.num = CLK_NUMBER,
	.hws = {
		[CLK_OSC32K]		= &osc32k_clk.common.hw,
		[CLK_RTC_32K_FANOUT]	= &rtc_32k_fanout_clk.common.hw,
		[CLK_IOSC]		= &iosc_clk.hw,
		[CLK_DCXO24M]		= &dcxo24M_clk.hw,

		[CLK_IOSC_32K]		= &iosc_32k_clk.hw,
		[CLK_EXT_OSC32K_GATE]	= NULL,
		[CLK_DCXO24M_32K]	= &dcxo24M_32k_clk.common.hw,
		[CLK_RTC_32K]		= &rtc_32k_clk.hw,
		[CLK_RTC_1K]		= &rtc_1k_clk.hw,
		[CLK_RTC_SPI]		= NULL,
	},
};

static struct clk_hw_onecell_data sun50i_r329_rtc_ccu_hw_clks = {
	.num = CLK_NUMBER,
	.hws = {
		[CLK_OSC32K]		= &osc32k_clk.common.hw,
		[CLK_RTC_32K_FANOUT]	= &rtc_32k_fanout_clk.common.hw,
		[CLK_IOSC]		= &iosc_clk.hw,
		[CLK_DCXO24M]		= &dcxo24M_clk.hw,

		[CLK_IOSC_32K]		= &iosc_32k_clk.hw,
		[CLK_EXT_OSC32K_GATE]	= &ext_osc32k_gate_clk.common.hw,
		[CLK_DCXO24M_32K]	= &dcxo24M_32k_clk.common.hw,
		[CLK_RTC_32K]		= &rtc_32k_mux_clk.common.hw,
		[CLK_RTC_1K]		= &rtc_1k_clk.hw,
		[CLK_RTC_SPI]		= &rtc_spi_clk.common.hw,
	},
};

static const struct sunxi_ccu_desc sun50i_h6_rtc_ccu_desc = {
	.ccu_clks	= sun50i_h6_rtc_ccu_clks,
	.num_ccu_clks	= ARRAY_SIZE(sun50i_h6_rtc_ccu_clks),

	.hw_clks	= &sun50i_h6_rtc_ccu_hw_clks,
};

static const struct sunxi_ccu_desc sun50i_h616_rtc_ccu_desc = {
	.ccu_clks	= sun50i_h616_rtc_ccu_clks,
	.num_ccu_clks	= ARRAY_SIZE(sun50i_h616_rtc_ccu_clks),

	.hw_clks	= &sun50i_h616_rtc_ccu_hw_clks,
};

static const struct sunxi_ccu_desc sun50i_r329_rtc_ccu_desc = {
	.ccu_clks	= sun50i_r329_rtc_ccu_clks,
	.num_ccu_clks	= ARRAY_SIZE(sun50i_r329_rtc_ccu_clks),

	.hw_clks	= &sun50i_r329_rtc_ccu_hw_clks,
};

static void __init sunxi_rtc_ccu_init(struct device_node *node,
				      const struct sunxi_ccu_desc *desc)
{
	void __iomem *reg;

	reg = of_iomap(node, 0);
	if (IS_ERR(reg)) {
		pr_err("%pOF: Could not map the clock registers\n", node);
		return;
	}

	if (have_ext_osc32k) {
		rtc_32k_fanout_parents[1] = "ext-osc32k-gate";
		*ext_osc32k = of_clk_get_hw(node, 0, "ext-osc");
		if (IS_ERR(*ext_osc32k)) {
			struct clk_init_data *init = (struct clk_init_data *)
				ext_osc32k_gate_clk.common.hw.init;

			init->num_parents = 0;
		}
	} else {
		struct clk_init_data *init = (struct clk_init_data *)
			osc32k_clk.common.hw.init;

		init->num_parents = 1;
	}

	sunxi_ccu_probe(node, reg, desc);
}

static void __init sun50i_h6_rtc_ccu_setup(struct device_node *node)
{
	have_ext_osc32k = 1;
	have_iosc_calib = 1;

	sunxi_rtc_ccu_init(node, &sun50i_h6_rtc_ccu_desc);
}
CLK_OF_DECLARE_DRIVER(sun50i_h6_rtc_ccu, "allwinner,sun50i-h6-rtc",
		      sun50i_h6_rtc_ccu_setup);

static void __init sun50i_h616_rtc_ccu_setup(struct device_node *node)
{
	have_ext_osc32k = 0;
	have_iosc_calib = 1;

	sunxi_rtc_ccu_init(node, &sun50i_h616_rtc_ccu_desc);
}
CLK_OF_DECLARE_DRIVER(sun50i_h616_rtc_ccu, "allwinner,sun50i-h616-rtc",
		      sun50i_h616_rtc_ccu_setup);

static void __init sun50i_r329_rtc_ccu_setup(struct device_node *node)
{
	have_ext_osc32k = 1;
	have_iosc_calib = 0;

	sunxi_rtc_ccu_init(node, &sun50i_r329_rtc_ccu_desc);
}
CLK_OF_DECLARE_DRIVER(sun50i_r329_rtc_ccu, "allwinner,sun50i-r329-rtc",
		      sun50i_r329_rtc_ccu_setup);
