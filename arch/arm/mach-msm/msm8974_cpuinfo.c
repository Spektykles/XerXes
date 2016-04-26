/*
 * Copyright (c) 2016, Tom G., <roboter972@gmail.com>
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/fs.h>

#include "msm8974_cpuinfo.h"

struct dentry *cpuinfo_rootdir;

struct msm8974_cpuinfo {
	int sbin;
	int pvs;
	int pvs_ver;
} cpuinf;

static int debugfs_sbin_get(void *data, u64 *val)
{
	*val = cpuinf.sbin;

	return 0;
}

static int debugfs_pvs_get(void *data, u64 *val)
{
	*val = cpuinf.pvs;

	return 0;
}

static int debugfs_pvs_ver_get(void *data, u64 *val)
{
	*val = cpuinf.pvs_ver;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sbin_ops, debugfs_sbin_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(pvs_ops, debugfs_pvs_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(pvs_ver_ops, debugfs_pvs_ver_get, NULL, "%llu\n");

static int cpuinfo_debugfs_add(void)
{
	cpuinfo_rootdir = debugfs_create_dir("msm8974_cpuinfo", NULL);
	if (IS_ERR_OR_NULL(cpuinfo_rootdir)) {
		pr_err("Failed to create cpuinfo root dir. Error: %ld\n",
						PTR_ERR(cpuinfo_rootdir));
		return -ENODEV;
	}

	if (debugfs_create_file("Speedbin", S_IRUGO,
			cpuinfo_rootdir, NULL, &sbin_ops) == NULL)
		goto init_failed;
	if (debugfs_create_file("PVS", S_IRUGO,
			cpuinfo_rootdir, NULL, &pvs_ops) == NULL)
		goto init_failed;
	if (debugfs_create_file("PVS_Version", S_IRUGO,
			cpuinfo_rootdir, NULL, &pvs_ver_ops) == NULL)
		goto init_failed;

	return 0;

init_failed:
	debugfs_remove_recursive(cpuinfo_rootdir);
	return -ENOMEM;
}

int cpuinfo_debugfs_init(int speed, int pvs, int pvs_ver)
{
	int rc;

	cpuinf.sbin = speed;
	cpuinf.pvs = pvs;
	cpuinf.pvs_ver = pvs_ver;

	rc = cpuinfo_debugfs_add();
	if (rc < 0) {
		pr_err("Failed to initialize msm8974 cpuinfo debugfs\n");
		return rc;
	}

	return 0;
}

static void __exit cpuinfo_exit(void)
{
	debugfs_remove_recursive(cpuinfo_rootdir);
}

module_exit(cpuinfo_exit);
