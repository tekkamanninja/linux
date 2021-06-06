// SPDX-License-Identifier: GPL-2.0-only
/*
 * Allwinner sun20i (D1) wakeup irqchip driver.
 */

#include <linux/bitmap.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/syscore_ops.h>

#define SUN20I_HWIRQ_OFFSET		16
#define SUN20I_NR_HWIRQS		160

#define SUN20I_HWIRQ_BIT(hwirq)		((hwirq) % 32)
#define SUN20I_HWIRQ_REG(hwirq)		((hwirq) / 32)

#define SUN20I_WAKEUP_EN_REG		0x0020
#define SUN20I_WAKEUP_MASK_REG(n)	(0x0024 + 4 * (n))
#define SUN20I_IRQ_MODE_REG(n)		(0x0060 + 4 * (n))

static void __iomem *base;

static int sun20i_intc_irq_set_type(struct irq_data *data, unsigned int type)
{
	u32 reg = SUN20I_IRQ_MODE_REG(SUN20I_HWIRQ_REG(data->hwirq));
	u32 bit = BIT(SUN20I_HWIRQ_BIT(data->hwirq));
	u32 val;

	val = readl(base + reg);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		val |= bit;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		val &= ~bit;
		break;
	default:
		return -EINVAL;
	}

	writel_relaxed(val, base + reg);

	return 0;
}

static int sun20i_intc_irq_set_wake(struct irq_data *data, unsigned int on)
{
	u32 reg = SUN20I_WAKEUP_MASK_REG(SUN20I_HWIRQ_REG(data->hwirq));
	u32 bit = BIT(SUN20I_HWIRQ_BIT(data->hwirq));
	u32 val;

	val = readl(base + reg);

	if (on)
		val |= bit;
	else
		val &= ~bit;

	writel_relaxed(val, base + reg);

	return 0;
}

static struct irq_chip sun20i_intc_wakeup_chip = {
	.name			= "sun20i-intc",
	.irq_mask		= irq_chip_mask_parent,
	.irq_unmask		= irq_chip_unmask_parent,
	.irq_eoi		= irq_chip_eoi_parent,
	.irq_set_affinity	= irq_chip_set_affinity_parent,
	.irq_set_type		= sun20i_intc_irq_set_type,
	.irq_set_wake		= sun20i_intc_irq_set_wake,
};

static int sun20i_intc_domain_translate(struct irq_domain *domain,
					struct irq_fwspec *fwspec,
					unsigned long *hwirq,
					unsigned int *type)
{
	if (fwspec->param_count < 2)
		return -EINVAL;
	if (fwspec->param[0] < SUN20I_HWIRQ_OFFSET)
		return -EINVAL;

	*hwirq = fwspec->param[0] - SUN20I_HWIRQ_OFFSET;
	*type  = fwspec->param[1] & IRQ_TYPE_SENSE_MASK;

	return 0;
}

static int sun20i_intc_domain_alloc(struct irq_domain *domain,
				    unsigned int virq,
				    unsigned int nr_irqs, void *arg)
{
	struct irq_fwspec *fwspec = arg;
	unsigned long hwirq;
	unsigned int type;
	int i, ret;

	ret = sun20i_intc_domain_translate(domain, fwspec, &hwirq, &type);
	if (ret)
		return ret;
	if (hwirq + nr_irqs > SUN20I_NR_HWIRQS)
		return -EINVAL;

	ret = irq_domain_alloc_irqs_parent(domain, virq, nr_irqs, fwspec);
	if (ret)
		return ret;

	for (i = 0; i < nr_irqs; ++i, ++hwirq, ++virq)
		irq_domain_set_hwirq_and_chip(domain, virq, hwirq,
					      &sun20i_intc_wakeup_chip, 0);

	return 0;
}

static const struct irq_domain_ops sun20i_intc_domain_ops = {
	.translate	= sun20i_intc_domain_translate,
	.alloc		= sun20i_intc_domain_alloc,
	.free		= irq_domain_free_irqs_common,
};

static int __init sun20i_intc_init(struct device_node *node,
				   struct device_node *parent)
{
	struct irq_domain *domain, *parent_domain;

	parent_domain = irq_find_host(parent);
	if (!parent_domain) {
		pr_err("%pOF: Failed to obtain parent domain\n", node);
		return -ENXIO;
	}

	base = of_io_request_and_map(node, 0, NULL);
	if (IS_ERR(base)) {
		pr_err("%pOF: Failed to map MMIO region\n", node);
		return PTR_ERR(base);
	}

	domain = irq_domain_add_hierarchy(parent_domain, 0, 0, node,
					  &sun20i_intc_domain_ops, NULL);
	if (!domain) {
		pr_err("%pOF: Failed to allocate domain\n", node);
		iounmap(base);
		return -ENOMEM;
	}

	return 0;
}
IRQCHIP_DECLARE(sun20i_intc, "allwinner,sun20i-d1-intc", sun20i_intc_init);
