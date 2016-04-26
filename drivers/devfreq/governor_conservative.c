/*
 * Copyright (c) 2015, Tom G. <roboter972@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Brief description:
 * The conservative kgsl policy acts exactly like it's cpufreq scaling driver
 * counterpart. It attemps to scale frequency in small steps depending on the
 * current GPU load (calculated using total- and busytime-statistics) every
 * sample_ms.
 */

#include <linux/devfreq.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#include "governor.h"

static DEFINE_SPINLOCK(hp_lock);

/*
 * Sampling interval.
 */
static unsigned int sample_ms = 50;

/*
 * Total and busytime stats used to calculate the current GPU load.
 */
static unsigned long walltime_total, busytime_total;

/*
 * Load thresholds.
 */
struct load_thresholds {
	unsigned int up_threshold;
	unsigned int down_threshold;
};

static struct load_thresholds thresholds[] = {
	{120,	30},	/* 450 MHz @freqlevel 0 */
	{ 95,	25},	/* 320 MHz @freqlevel 1 */
	{ 85,	 5},	/* 200 MHz @freqlevel 2 */
	{  5,	 0}	/*  27 MHz @freqlevel 3 */
};

static int devfreq_conservative_func(struct devfreq *devfreq,
					unsigned long *freq, u32 *flag)
{
	struct devfreq_dev_status stats;
	unsigned long flags;
	unsigned int load_hist, level, curr_level;
	int result;

	level = 0;
	stats.private_data = NULL;

	result = devfreq->profile->get_dev_status(devfreq->dev.parent, &stats);
	if (result)
		return result;

	if (!stats.total_time)
		return 1;

	*freq = stats.current_frequency;

	walltime_total += stats.total_time;
	busytime_total += stats.busy_time;

	pr_debug("walltime before div: %lu\n", walltime_total);
	pr_debug("busytime before div: %lu\n", busytime_total);

	load_hist = (100 * busytime_total) / walltime_total;

	pr_debug("busytime after mult: %lu\n", busytime_total);
	pr_debug("load_hist: %u\n", load_hist);

	walltime_total = busytime_total = 0;

	spin_lock_irqsave(&hp_lock, flags);

	curr_level = devfreq_get_freq_level(devfreq, stats.current_frequency);

	if (load_hist < thresholds[curr_level].down_threshold)
		level = 1;
	else if (load_hist > thresholds[curr_level].up_threshold)
		level = -1;

	spin_unlock_irqrestore(&hp_lock, flags);

	pr_debug("level: %u\n", level);

	if (level != 0)
		*freq = devfreq->profile->freq_table[curr_level + level];

	return 0;
}

static int conservative_handler(struct devfreq *devfreq,
					unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		devfreq_monitor_start(devfreq);
		break;
	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(devfreq);
		break;
	case DEVFREQ_GOV_INTERVAL:
		sample_ms = *(unsigned int *)data;
		devfreq_interval_update(devfreq, &sample_ms);
		break;
	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(devfreq);
		break;
	case DEVFREQ_GOV_RESUME:
		devfreq_monitor_resume(devfreq);
		break;
	}

	return 0;
}

static struct devfreq_governor conservative = {
	.name = "conservative",
	.get_target_freq = devfreq_conservative_func,
	.event_handler = conservative_handler
};

static int __init devfreq_conservative_init(void)
{
	return devfreq_add_governor(&conservative);
}
subsys_initcall(devfreq_conservative_init);

static void __exit devfreq_conservative_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&conservative);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);
}
module_exit(devfreq_conservative_exit);
MODULE_LICENSE("GPL");
