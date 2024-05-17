/* SPDX-License-Identifier: GPL-2.0 */
/*
 * XuanTie TH1520 ALSA Soc Audio Layer
 *
 * Copyright (C) 2024 Alibaba Group Holding Limited.
 *
 * Author: Shuofeng Ren <shuofeng.rsf@linux.alibaba.com>
 *
 */
 
#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <sound/dmaengine_pcm.h>

#include <dt-bindings/pinctrl/th1520-fm-aon-pinctrl.h>

#include "th1520-i2s.h"
#include "th1520-pcm.h"

int th1520_audio_cpr_set(struct th1520_i2s_priv *chip, unsigned int cpr_off,
			 unsigned int mask, unsigned int val)
{
       if(!chip->audio_cpr_regmap)
               return 0;

       return regmap_update_bits(chip->audio_cpr_regmap, cpr_off, mask, val);
}

bool th1520_i2s_wr_reg(struct device *dev, unsigned int reg)
{
	return true;
}

bool th1520_i2s_rd_reg(struct device *dev, unsigned int reg)
{
	return true;
}

int th1520_pcm_probe(struct platform_device *pdev, struct th1520_i2s_priv *i2s,
		     size_t size)
{
	int ret = th1520_pcm_dma_init(pdev, size);

	if (ret)
		pr_err("th1520_pcm_dma_init error\n");

	return 0;
}

static int th1520_audio_pinconf_set(struct device *dev, unsigned int pin_id,
				    unsigned int val)
{
	unsigned int shift;
	unsigned int mask = 0;
	struct th1520_i2s_priv *priv = dev_get_drvdata(dev);

	if(!priv->audio_pin_regmap)
		return 0;

	priv->cfg_off = 0xC;

	shift = (((pin_id-25) % 2) << 4);
	mask |= (0xFFFF << shift);
	val = (val << shift);

	return regmap_update_bits(priv->audio_pin_regmap,
				  TH1520_AUDIO_PAD_CONFIG(pin_id),mask, val);
}

int th1520_audio_pinctrl(struct device *dev)
{
	struct th1520_i2s_priv *i2s_priv = dev_get_drvdata(dev);

	if (!strcmp(i2s_priv->name, AUDIO_I2S0)) {
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA6, 0x4);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA7, 0x4);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA9, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA10, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA11, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA12, 0x8);
	} else if (!strcmp(i2s_priv->name, AUDIO_I2S1)) {
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA6, 0x4);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA7, 0x4);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA13, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA14, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA15, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA17, 0x8);
	} else if (!strcmp(i2s_priv->name, AUDIO_I2S2)) {
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA6, 0x5);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA7, 0x5);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA18, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA19, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA21, 0x8);
		th1520_audio_pinconf_set(i2s_priv->dev, FM_AUDIO_CFG_PA22, 0x8);
	}

	return 0;
}

MODULE_AUTHOR("shuofeng.ren <shuofeng.rsf@linux.alibaba.com>");
MODULE_DESCRIPTION("Xuantie TH1520 audio driver");
MODULE_LICENSE("GPL v2");
