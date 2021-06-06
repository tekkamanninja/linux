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

static struct irq_chip sun20i_intc_chip = {
	.name			= "sun20i-intc",
	.irq_mask		= irq_chip_mask_parent,
	.irq_unmask		= irq_chip_unmask_parent,
	.irq_eoi		= irq_chip_eoi_parent,
	.irq_set_affinity	= irq_chip_set_affinity_parent,
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

	*hwirq = fwspec->param[0];
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
	if (hwirq + nr_irqs > SUN20I_HWIRQ_OFFSET + SUN20I_NR_HWIRQS)
		return -EINVAL;

	ret = irq_domain_alloc_irqs_parent(domain, virq, nr_irqs, fwspec);
	if (ret)
		return ret;

	for (i = 0; i < nr_irqs; ++i, ++hwirq, ++virq)
		irq_domain_set_hwirq_and_chip(domain, virq, hwirq,
					      &sun20i_intc_chip, 0);

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

	domain = irq_domain_add_hierarchy(parent_domain, 0, 0, node,
					  &sun20i_intc_domain_ops, NULL);
	if (!domain) {
		pr_err("%pOF: Failed to allocate domain\n", node);
		return -ENOMEM;
	}

	return 0;
}
IRQCHIP_DECLARE(sun20i_intc, "allwinner,sun20i-d1-intc", sun20i_intc_init);
