/*********************************************************************
  Filename:       ads131m08_main.c
  Revised:        Date: 2024/5/14
  Revision:       Revision: 0.1

  Description:
          - TI ADS131M08 Differential ADC kernel level driver
          - u-Infraway,Inc.
  Notes:
        Copyright (c) 2024 by u-Infraway, Inc., All Rights Reserved.

*********************************************************************/

/*******************************************************************
 * INCLUDES
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/time.h>
#include <linux/ktime.h>

#include "ads131m08.h"
///////////////////////////////////////////////////////////////////////
#define	DRIVER_NAME	"spi-ads131m08"
#define DEFAULT_SPI_MAX_SPEED_HZ 15000000U
//////////////////////////////////////////////////////////////////////
const char *ads131m08_dev_name = "ads131m08";
const char *ads131m08_class_name = "ads131m08_class";
const char *ads131m08_driver_name = DRIVER_NAME;
const char *ads131m08_driver_version = ADS131M08_DRIVER_VERSION;

static int ads131m08_probe(struct spi_device *spi)
{
   int ret;

   pr_info("ADS131M08 Probe (driver v%s)\n", ads131m08_driver_version);

   ret = ads131m08_data_init(spi);
   if (ret < 0)
      return ret;

   spi->mode = SPI_MODE_1;
   if (!spi->max_speed_hz)
      spi->max_speed_hz = DEFAULT_SPI_MAX_SPEED_HZ;

   ret = spi_setup(spi);
   if (ret)
   {
      pr_err("%s: spi_setup failed (%d)\n", ads131m08_dev_name, ret);
      return ret;
   }

   spi_set_drvdata(spi, ads131m08_data);
   ads131m08_data->spi = spi;

   ret = ads131m08_spi_init(spi);
   if (ret < 0)
      return ret;

   ret = ads131m08_gpio_init(ads131m08_data);
   if (ret < 0)
      goto err_spi;

   ret = ads131m08_iface_init(ads131m08_data);
   if (ret < 0)
      goto err_gpio;

   pr_info("%s: module loaded (driver v%s)\n",
           ads131m08_dev_name, ads131m08_driver_version);

   ads131m08_adc_rst();
   ret = ads131m08_cmd_standby();
   if (ret < 0)
   {
      pr_err("%s: failed to put ADC into standby (%d)\n", ads131m08_dev_name, ret);
      goto err_iface;
   }

   return SUCCESS;

err_iface:
   ads131m08_iface_exit(ads131m08_data);
err_gpio:
   ads131m08_gpio_exit(ads131m08_data);
err_spi:
   ads131m08_spi_exit();
   return ret;
}

static void ads131m08_remove(struct spi_device *spi)
{
   ads131m08_iface_exit(ads131m08_data);
   ads131m08_data_exit();
   ads131m08_gpio_exit(ads131m08_data);
   ads131m08_spi_exit();
   pr_info("%s: module removed\n", ads131m08_dev_name);
}
//////////////////////////////////////////////////////////////
static const struct of_device_id ads131m08_of_match[] = {
    { .compatible = "ti,ads131m08", },
    {},
};
MODULE_DEVICE_TABLE(of,ads131m08_of_match);

static const struct spi_device_id ads131m08_id[] = {
   {"ads131m08", 0},
   {}
};
MODULE_DEVICE_TABLE(spi, ads131m08_id);

static struct spi_driver ads131m08_driver = {
   .driver = {
      .name = DRIVER_NAME,
      .of_match_table = ads131m08_of_match,
   },
   .id_table = ads131m08_id,
   .probe = ads131m08_probe,
   .remove = ads131m08_remove,
};
module_spi_driver(ads131m08_driver);

MODULE_AUTHOR("u-Infraway.,Inc.");
MODULE_DESCRIPTION("ADS131M08 Driver for dlog");
MODULE_VERSION(ADS131M08_DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
