/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * modifications copyright (C) 2015 NXP B.V.
 * stub creation for libnfc-nci testing 2016 Contrib
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include "pn5xx_i2c_stub.h"

#define PN54x_STUB_VERSION "v0.1"

/* Only pn548, pn547 and pn544 are supported */
#define CHIP "pn544"
#define DRIVER_CARD "PN54x NFC"
#define DRIVER_DESC "NFC driver stub " PN54x_STUB_VERSION " for PN54x Family"

struct pn54x_stub_dev	{
	struct miscdevice pn54x_device;
};

/**********************************************************
 * driver functions
 **********************************************************/
static ssize_t pn54x_stub_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	return -EIO; // always force error
}

static ssize_t pn54x_stub_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	return -EIO; // always force error
}

static int pn54x_stub_dev_open(struct inode *inode, struct file *filp)
{
//	struct pn54x_stub_dev *pn54x_stub_dev = container_of(filp->private_data,
//											   struct pn54x_stub_dev,
//											   pn54x_device);

//	filp->private_data = pn54x_stub_dev;

	pr_info("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	// pn544_enable(pn54x_dev, MODE_RUN);

	return 0;
}

static int pn54x_stub_dev_release(struct inode *inode, struct file *filp)
{
	// struct pn54x_dev *pn54x_dev = container_of(filp->private_data,
	//										   struct pn54x_dev,
	//										   pn54x_device);

	pr_info("%s : closing %d,%d\n", __func__, imajor(inode), iminor(inode));

	// pn544_disable(pn54x_dev);

	return 0;
}

static long  pn54x_stub_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	return 0;
}

static const struct file_operations pn54x_stub_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn54x_stub_dev_read,
	.write	= pn54x_stub_dev_write,
	.open	= pn54x_stub_dev_open,
	.release  = pn54x_stub_dev_release,
	.unlocked_ioctl  = pn54x_stub_dev_ioctl,
};

static struct pn54x_stub_dev pn54x_stub_device = {
	.pn54x_device = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = CHIP,
		.fops = &pn54x_stub_dev_fops,
	},
};

/*
 * module load/unload record keeping
 */

static int __init pn54x_stub_dev_init(void)
{
	int ret;
	pr_info("%s\n", __func__);
	ret = misc_register(&pn54x_stub_device.pn54x_device);
	if (ret != 0) {
		pr_err("%s: misc_register() failure: %d\n", __func__, ret);
	}
	return ret;
}

static void __exit pn54x_stub_dev_exit(void)
{
	pr_info("%s\n", __func__);
	misc_deregister(&pn54x_stub_device.pn54x_device);
}

module_init(pn54x_stub_dev_init);
module_exit(pn54x_stub_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau, Contrib");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
