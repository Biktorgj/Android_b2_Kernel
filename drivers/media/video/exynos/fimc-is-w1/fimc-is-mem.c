/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/of.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
#include <linux/exynos_iovmm.h>
#else
#include <plat/iovmm.h>
#endif

#if defined(CONFIG_VIDEOBUF2_ION)
static void *fimc_is_ion_init(struct platform_device *pdev)
{
	return vb2_ion_create_context(&pdev->dev, SZ_4K,
					VB2ION_CTX_IOMMU |
					VB2ION_CTX_VMCONTIG |
					VB2ION_CTX_KVA_ONDEMAND);
}

static unsigned long plane_addr(struct vb2_buffer *vb, u32 plane_no)
{
	void *cookie = vb2_plane_cookie(vb, plane_no);
	dma_addr_t dva = 0;

	WARN_ON(vb2_ion_dma_address(cookie, &dva) != 0);

	return dva;
}

static unsigned long plane_kvaddr(struct vb2_buffer *vb, u32 plane_no)
{
	void *kvaddr = vb2_plane_vaddr(vb, plane_no);

	return (unsigned long)kvaddr;
}

const struct fimc_is_vb2 fimc_is_vb2_ion = {
	.ops		= &vb2_ion_memops,
	.init		= fimc_is_ion_init,
	.cleanup	= vb2_ion_destroy_context,
	.plane_addr	= plane_addr,
	.plane_kvaddr	= plane_kvaddr,
	.resume		= vb2_ion_attach_iommu,
	.suspend	= vb2_ion_detach_iommu,
	.set_cacheable	= vb2_ion_set_cached,
};
#endif

#if defined(CONFIG_VIDEOBUF2_DMA_CMA)
static unsigned long plane_kvaddr(struct vb2_buffer *vb, u32 plane_no)
{
	void *kvaddr = vb2_plane_vaddr(vb, plane_no);

	return (unsigned long)kvaddr;
}

void *fimc_dma_contig_init(struct platform_device *pdev)
{
	return vb2_dma_contig_init_ctx(&pdev->dev);
}


const struct fimc_is_vb2 fimc_is_vb2_dma_contig = {
	.ops		= &vb2_dma_contig_memops,
	.init		= fimc_dma_contig_init,
	.cleanup	= vb2_dma_contig_cleanup_ctx,
	.plane_addr	= vb2_dma_contig_plane_dma_addr,
	.plane_kvaddr	= plane_kvaddr,
/*	.resume		= vb2_ion_attach_iommu,*/
/*	.suspend	= vb2_ion_detach_iommu,*/
	.set_cacheable	= vb2_dma_contig_set_cacheable,
};

#endif

int fimc_is_mem_probe(struct fimc_is_mem *this,
	struct platform_device *pdev)
{
	u32 ret = 0;

#if defined(CONFIG_VIDEOBUF2_DMA_CMA)
	this->vb2 = &fimc_is_vb2_dma_contig;
#else
	this->vb2 = &fimc_is_vb2_ion;
#endif
	this->alloc_ctx = this->vb2->init(pdev);
	if (IS_ERR(this->alloc_ctx)) {
		ret = PTR_ERR(this->alloc_ctx);
		goto p_err;
	}

	/* FIXME: should be different by device type */
	exynos_create_iovmm(&pdev->dev, 1, 4);

p_err:
	return ret;
}
