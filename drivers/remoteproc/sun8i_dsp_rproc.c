// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 Samuel Holland <samuel@sholland.org>

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/reset.h>
#include <linux/soc/sunxi/sunxi_sram.h>

#include "remoteproc_internal.h"

#define SUN8I_DSP_RESET_VEC_REG		0x0000
#define SUN8I_DSP_CTRL_REG0		0x0004
#define SUN8I_DSP_CTRL_REG0_RUN_STALL		BIT(0)
#define SUN8I_DSP_CTRL_REG0_RESET_VEC_SEL	BIT(1)
#define SUN8I_DSP_CTRL_REG0_DSP_CLKEN		BIT(2)
#define SUN8I_DSP_CTRL_REG1		0x0008
#define SUN8I_DSP_PRID_REG		0x000c
#define SUN8I_DSP_PRID_REG_PRID_MASK		(0xff << 0)
#define SUN8I_DSP_STAT_REG		0x0010
#define SUN8I_DSP_STAT_REG_PFAULT_INFO_VALID	BIT(0)
#define SUN8I_DSP_STAT_REG_PFAULT_ERROR		BIT(1)
#define SUN8I_DSP_STAT_REG_DOUBLE_EXCE_ERROR	BIT(2)
#define SUN8I_DSP_STAT_REG_XOCD_MODE		BIT(3)
#define SUN8I_DSP_STAT_REG_DEBUG_MODE		BIT(4)
#define SUN8I_DSP_STAT_REG_PWAIT_MODE		BIT(5)
#define SUN8I_DSP_STAT_REG_IRAM0_LOAD_STORE	BIT(6)
#define SUN8I_DSP_BIST_CTRL_REG		0x0014
#define SUN8I_DSP_BIST_CTRL_REG_EN		BIT(0)
#define SUN8I_DSP_BIST_CTRL_REG_WDATA_PAT_MASK	(0x7 << 1)
#define SUN8I_DSP_BIST_CTRL_REG_ADDR_MODE_SEL	BIT(4)
#define SUN8I_DSP_BIST_CTRL_REG_REG_SEL_MASK	(0x7 << 5)
#define SUN8I_DSP_BIST_CTRL_REG_BUSY		BIT(8)
#define SUN8I_DSP_BIST_CTRL_REG_STOP		BIT(9)
#define SUN8I_DSP_BIST_CTRL_REG_ERR_CYC_MASK	(0x3 << 10)
#define SUN8I_DSP_BIST_CTRL_REG_ERR_PAT_MASK	(0x7 << 12)
#define SUN8I_DSP_BIST_CTRL_REG_ERR_STA		BIT(15)
#define SUN8I_DSP_BIST_CTRL_REG_SELECT_MASK	(0xf << 16)
#define SUN8I_DSP_JTRST_REG		0x001c
#define SUN8I_DSP_VER_REG		0x0020
#define SUN8I_DSP_VER_REG_MINOR_VER_MASK	(0x1f << 0)
#define SUN8I_DSP_VER_REG_MAJOR_VER_MASK	(0x1f << 16)

#define SUN8I_DSP_CLK_FREQ		400000000

struct sun8i_dsp_rproc {
	void __iomem		*cfg_base;
	struct clk		*cfg_clk;
	struct reset_control	*cfg_reset;
	struct reset_control	*dbg_reset;
	struct clk		*dsp_clk;
	struct reset_control	*dsp_reset;
	struct mbox_client	client;
	struct mbox_chan	*rx_chan;
	struct mbox_chan	*tx_chan;
};

static int sun8i_dsp_rproc_start(struct rproc *rproc)
{
	struct sun8i_dsp_rproc *dsp = rproc->priv;
	int ret;
	u32 val;

	ret = sunxi_sram_claim(rproc->dev.parent);
	if (ret)
		return ret;

	ret = clk_prepare_enable(dsp->cfg_clk);
	if (ret)
		goto err_release_sram;

	ret = reset_control_deassert(dsp->cfg_reset);
	if (ret)
		goto err_gate_cfg;

	ret = reset_control_deassert(dsp->dbg_reset);
	if (ret)
		goto err_reset_cfg;

	writel(rproc->bootaddr, dsp->cfg_base + SUN8I_DSP_RESET_VEC_REG);

	val = readl(dsp->cfg_base + SUN8I_DSP_CTRL_REG0);
	val |= SUN8I_DSP_CTRL_REG0_RESET_VEC_SEL |
	       SUN8I_DSP_CTRL_REG0_RUN_STALL;
	writel(val, dsp->cfg_base + SUN8I_DSP_CTRL_REG0);

	ret = clk_prepare_enable(dsp->dsp_clk);
	if (ret)
		goto err_reset_dbg;

	ret = reset_control_deassert(dsp->dsp_reset);
	if (ret)
		goto err_gate_dsp;

	val &= ~SUN8I_DSP_CTRL_REG0_RUN_STALL;
	writel(val, dsp->cfg_base + SUN8I_DSP_CTRL_REG0);

	return 0;

err_gate_dsp:
	clk_disable_unprepare(dsp->dsp_clk);
err_reset_dbg:
	reset_control_assert(dsp->dbg_reset);
err_reset_cfg:
	reset_control_assert(dsp->cfg_reset);
err_gate_cfg:
	clk_disable_unprepare(dsp->cfg_clk);
err_release_sram:
	sunxi_sram_release(rproc->dev.parent);

	return ret;
}

static int sun8i_dsp_rproc_stop(struct rproc *rproc)
{
	struct sun8i_dsp_rproc *dsp = rproc->priv;

	reset_control_assert(dsp->dsp_reset);
	clk_disable_unprepare(dsp->dsp_clk);
	reset_control_assert(dsp->dbg_reset);
	reset_control_assert(dsp->cfg_reset);
	clk_disable_unprepare(dsp->cfg_clk);
	sunxi_sram_release(rproc->dev.parent);

	return 0;
}

static void sun8i_dsp_rproc_kick(struct rproc *rproc, int vqid)
{
	struct sun8i_dsp_rproc *dsp = rproc->priv;
	long msg = vqid;
	int ret;

	ret = mbox_send_message(dsp->tx_chan, (void *)msg);
	if (ret)
		dev_warn(&rproc->dev, "Failed to kick: %d\n", ret);
}

static const struct rproc_ops sun8i_dsp_rproc_ops = {
	.start		= sun8i_dsp_rproc_start,
	.stop		= sun8i_dsp_rproc_stop,
	.kick		= sun8i_dsp_rproc_kick,
};

static void sun8i_dsp_rproc_mbox_rx_callback(struct mbox_client *client, void *msg)
{
	struct rproc *rproc = dev_get_drvdata(client->dev);

	rproc_vq_interrupt(rproc, (long)msg);
}

static void sun8i_dsp_rproc_mbox_free(void *data)
{
	struct sun8i_dsp_rproc *dsp = data;

	if (!IS_ERR_OR_NULL(dsp->tx_chan))
		mbox_free_channel(dsp->tx_chan);
	if (!IS_ERR_OR_NULL(dsp->rx_chan))
		mbox_free_channel(dsp->rx_chan);
}

static int sun8i_dsp_rproc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct sun8i_dsp_rproc *dsp;
	const char *firmware_name;
	struct rproc *rproc;
	int i, ret;
	u32 freq;

	firmware_name = NULL;
	of_property_read_string(np, "firmware-name", &firmware_name);
	rproc = devm_rproc_alloc(dev, dev_name(dev), &sun8i_dsp_rproc_ops,
				 firmware_name, sizeof(struct sun8i_dsp_rproc));
	if (!rproc)
		return -ENOMEM;

	dev_set_drvdata(dev, rproc);
	dsp = rproc->priv;

	i = of_property_match_string(np, "reg-names", "cfg");
	if (i < 0)
		return -EINVAL;

	dsp->cfg_base = devm_platform_ioremap_resource(pdev, i);
	if (IS_ERR(dsp->cfg_base))
		return dev_err_probe(dev, PTR_ERR(dsp->cfg_base),
				     "Failed to map cfg\n");

	dsp->cfg_clk = devm_clk_get(dev, "cfg");
	if (IS_ERR(dsp->cfg_clk))
		return dev_err_probe(dev, PTR_ERR(dsp->cfg_clk),
				     "Failed to get %s clock\n", "cfg");

	dsp->cfg_reset = devm_reset_control_get_exclusive(dev, "cfg");
	if (IS_ERR(dsp->cfg_reset))
		return dev_err_probe(dev, PTR_ERR(dsp->cfg_reset),
				     "Failed to get %s reset\n", "cfg");

	dsp->cfg_reset = devm_reset_control_get_exclusive(dev, "dbg");
	if (IS_ERR(dsp->cfg_reset))
		return dev_err_probe(dev, PTR_ERR(dsp->cfg_reset),
				     "Failed to get %s reset\n", "dbg");

	dsp->dsp_clk = devm_clk_get(dev, "dsp");
	if (IS_ERR(dsp->dsp_clk))
		return dev_err_probe(dev, PTR_ERR(dsp->dsp_clk),
				     "Failed to get %s clock\n", "dsp");

	freq = SUN8I_DSP_CLK_FREQ;
	of_property_read_u32(np, "clock-frequency", &freq);
	ret = clk_set_rate(dsp->dsp_clk, freq);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to set clock frequency\n");

	dsp->dsp_reset = devm_reset_control_get_exclusive(dev, "dsp");
	if (IS_ERR(dsp->dsp_reset))
		return dev_err_probe(dev, PTR_ERR(dsp->dsp_reset),
				     "Failed to get %s reset\n", "dsp");

	dsp->client.dev = dev;
	dsp->client.rx_callback = sun8i_dsp_rproc_mbox_rx_callback;

	ret = devm_add_action(dev, sun8i_dsp_rproc_mbox_free, dsp);
	if (ret)
		return ret;

	dsp->rx_chan = mbox_request_channel_byname(&dsp->client, "rx");
	if (IS_ERR(dsp->rx_chan))
		return dev_err_probe(dev, PTR_ERR(dsp->rx_chan),
				     "Failed to request RX channel\n");

	dsp->tx_chan = mbox_request_channel_byname(&dsp->client, "tx");
	if (IS_ERR(dsp->tx_chan))
		return dev_err_probe(dev, PTR_ERR(dsp->tx_chan),
				     "Failed to request TX channel\n");

	ret = devm_rproc_add(dev, rproc);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to register rproc\n");

	return 0;
}

static const struct of_device_id sun8i_dsp_rproc_of_match[] = {
	{ .compatible = "allwinner,sun20i-d1-dsp" },
	{}
};
MODULE_DEVICE_TABLE(of, sun8i_dsp_rproc_of_match);

static struct platform_driver sun8i_dsp_rproc_driver = {
	.probe		= sun8i_dsp_rproc_probe,
	.driver		= {
		.name		= "sun8i-dsp-rproc",
		.of_match_table	= sun8i_dsp_rproc_of_match,
	},
};
module_platform_driver(sun8i_dsp_rproc_driver);

MODULE_AUTHOR("Samuel Holland <samuel@sholland.org>");
MODULE_DESCRIPTION("Allwinner sun8i DSP remoteproc driver");
MODULE_LICENSE("GPL");
