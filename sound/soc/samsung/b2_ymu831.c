/*
 *  b2_ymu831.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/firmware.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/delay.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <plat/clock.h>

#include "i2s.h"
#include "i2s-regs.h"

#include "../codecs/ymu831/ymu831.h"

#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
static struct clk *xtal_24mhz_ap;
#else
static bool clkout_enabled;
static void b2_enable_mclk(bool on)
{
	pr_info("%s: %s\n", __func__, on ? "on" : "off");

	clkout_enabled = on;
	/* XUSBXTI 24MHz via XCLKOUT */
	writel(on ? 0x0900 : 0x0901, EXYNOS_PMU_DEBUG);
}
#endif

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		pr_err("%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
	pr_debug("%s: EPLL rate = %ld\n",
		__func__, clk_get_rate(fout_epll));
out:
	clk_put(fout_epll);

	return 0;
}
#endif

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
/*
 * ESPRESSO I2S DAI operations. (AP Master)
 */
static int b2_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, sclk, bfs, psr, rfs, ret;
	unsigned long rclk;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 48000:
	case 96000:
	case 192000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 12288000:
	case 18432000:
		psr = 4;
		break;
	case 24576000:
	case 36864000:
		psr = 2;
		break;
	case 49152000:
	case 73728000:
		psr = 1;
		break;
	default:
		pr_err("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;
	pll = sclk;
	set_aud_pll_rate(pll);

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}
#else
/*
 * B2 I2S DAI operations. (AP Slave)
 */
static int b2_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* Set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, MC_ASOC_BCLK_MULT,
					MC_ASOC_LRCK_X32);
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int dummy_delay_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}
static int dummy_delay_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int delay = ucontrol->value.integer.value[0];

	if (delay > 1000)
		delay = 1000;
	else if (delay < 0)
		delay = 0;

	pr_info("Delay for %d msec\n", delay);

	msleep_interruptible(delay);
	return 1;
}
static const struct snd_kcontrol_new b2_snd_controls[] = {
	SOC_SINGLE_EXT("Dummy Delay", SND_SOC_NOPM, 0, 1000, 0,
				dummy_delay_get, dummy_delay_put),
};

static int i2s_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;

	err = snd_soc_add_codec_controls(codec, b2_snd_controls,
					ARRAY_SIZE(b2_snd_controls));
	if (err < 0)
		return err;
	return 0;
}

static struct snd_soc_ops b2_ops = {
	.hw_params = b2_hw_params,
};

static struct snd_soc_dai_link b2_dai[] = {
	{
		.name = "B2 PRI",
		.stream_name = "i2s",
		.cpu_dai_name = "samsung-i2s.2",
		.codec_dai_name = "ymu831-da0",
		.platform_name = "samsung-audio",
		.codec_name = "spi1.0", /* BUS 1, CH-SEL 0 */
		.ops = &b2_ops,
		.init = i2s_dai_init,
	}
};

static int b2_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	int ymu831_suspend_status = 0;

	ymu831_suspend_status = ymu831_get_codec_suspend_status(codec);

	if (ymu831_suspend_status == 0) {
		/* codec is not using. disable mclk */
		dev_info(codec->dev, "turn off XTAL_24MHZ_AP");
#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
		clk_disable(xtal_24mhz_ap);
#else
		b2_enable_mclk(false);
#endif
	} else if (ymu831_suspend_status > 0) {
		/* codec is using */
		dev_info(codec->dev, "codec bias level is not off");
		dev_info(codec->dev, "keep alive XTAL_24MHZ_AP");
	} else {
		/* error */
		dev_err(codec->dev, "can not get ymu831 suspend status, %d",
							ymu831_suspend_status);
		dev_err(codec->dev, "assume ymu831 active");
	}

#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
	exynos_sys_powerdown_xusbxti_control(xtal_24mhz_ap->usage ?
						true : false);
#else
	exynos_sys_powerdown_xusbxti_control(clkout_enabled ?
						true : false);
#endif
	dev_info(codec->dev, "Current PMU_DEBUG reg. [0x%X]\n",
					readl(EXYNOS_PMU_DEBUG));
	return 0;
}

static int b2_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	int ymu831_suspend_status = 0;

	ymu831_suspend_status = ymu831_get_codec_suspend_status(codec);

	if (ymu831_suspend_status == 0) {
		/* enable mclk */
		dev_info(codec->dev, "trun on XTAL_24MHZ_AP");
#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
		clk_enable(xtal_24mhz_ap);
#else
		b2_enable_mclk(true);
#endif
	} else if (ymu831_suspend_status > 0) {
		dev_info(codec->dev, "XTAL_24MHZ_AP has already alived");
		if (0x0901 == (0xFFFF & readl(EXYNOS_PMU_DEBUG))) {
			dev_info(codec->dev, "But PMU_DEBUG clkout disabled");
			dev_info(codec->dev, "So forced enable XTAL_24MHZ_AP");
#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
			clk_enable(xtal_24mhz_ap);
#else
			b2_enable_mclk(true);
#endif
		}
	} else {
		dev_err(codec->dev, "can not get ymu831 suspend status %d",
							ymu831_suspend_status);
		dev_err(codec->dev, "forced enable XTAL_24MHZ_AP");
#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
		clk_enable(xtal_24mhz_ap);
#else
		b2_enable_mclk(true);
#endif
	}

	return 0;
}

static struct snd_soc_card b2_snd_card = {
	.name = "ymu831",
	.suspend_post = b2_suspend_post,
	.resume_pre = b2_resume_pre,
	.dai_link = b2_dai,
	.num_links = ARRAY_SIZE(b2_dai),
};

static int __devinit b2_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &b2_snd_card;
	int ret;

#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
	xtal_24mhz_ap = clk_get(NULL, "clk_out");
	if (IS_ERR(xtal_24mhz_ap)) {
		pr_err("%s: Faild to get clk_out %ld\n",
				__func__, PTR_ERR(xtal_24mhz_ap));
		return PTR_ERR(xtal_24mhz_ap);
	}

	ret = clk_enable(xtal_24mhz_ap);
	if (ret > 0)
		pr_err("%s: XTAL_24MHZ_AP enable failed\n", __func__);
	else
		pr_info("%s: XTAL_24MHZ_AP enabled\n", __func__);
#else
	b2_enable_mclk(true);
#endif
	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "%s: snd_soc_register_card() failed %d",
					__func__, ret);
#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
		clk_disable(xtal_24mhz_ap);
#endif
	}

	return ret;
}

static int __devexit b2_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
	clk_put(xtal_24mhz_ap);
#endif

	return 0;
}
static struct platform_driver b2_snd_machine_driver = {
	.driver = {
		.name = "b2-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = b2_audio_probe,
	.remove = __devexit_p(b2_audio_remove),
};

module_platform_driver(b2_snd_machine_driver);

MODULE_DESCRIPTION("ALSA SoC B2 YMU831");
MODULE_LICENSE("GPL");
