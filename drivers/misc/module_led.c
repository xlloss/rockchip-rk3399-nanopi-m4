/* 
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <asm/io.h>

#define DRIVER_NAME "MODULE_LED"
#define RK3X_PMUGRF_BASE 0xFF320000
#define RK3X_PMUGRF_GPIO0A_IOMUX RK3X_PMUGRF_BASE + 0x0
#define RK3X_GPIO_1_BASE 0xFF730000
#define RK3X_GPIO_1_SWPORTA_DR RK3X_GPIO_1_BASE + 0x0
#define RK3X_GPIO_1_SWPORTA_DDR RK3X_GPIO_1_BASE + 0x04

void __iomem *gpioa_iomux, *gpioa_data, *gpioa_dir;

static int led_init(void)
{
    unsigned int gpioa_iomux_r, gpioa_data_r, gpioa_dir_r;

    printk("Led driver init\n");

    gpioa_iomux = ioremap(RK3X_PMUGRF_GPIO0A_IOMUX, 4);
    gpioa_dir = ioremap(RK3X_GPIO_1_SWPORTA_DDR, 4);
    gpioa_data = ioremap(RK3X_GPIO_1_SWPORTA_DR, 4);

    gpioa_iomux_r = readl(gpioa_iomux);
    gpioa_iomux_r = gpioa_iomux_r | (1 << 17 | 1 << 16);
    writel(gpioa_iomux_r, gpioa_iomux);

    gpioa_iomux_r = gpioa_iomux_r & ~(1 << 1 | 1 << 0);
    writel(gpioa_iomux_r, gpioa_iomux);

    gpioa_dir_r = readl(gpioa_dir);
    gpioa_dir_r = gpioa_dir_r | (1 << 0);
    writel(gpioa_dir_r, gpioa_dir);

    gpioa_data_r = readl(gpioa_data);
    gpioa_data_r = gpioa_data_r & ~(1 << 0);
    writel(gpioa_data_r, gpioa_data);

    return 0;
}

static void led_exit(void)
{
    unsigned int gpioa_data_r;

    printk("Led driver removed\n");

    gpioa_data_r = readl(gpioa_data);
    gpioa_data_r = gpioa_data_r | (1 << 0);
    writel(gpioa_data_r, gpioa_data);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("slash.linux.c@gmail.com");
