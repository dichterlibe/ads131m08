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
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/delay.h>

#include "ads131m08.h"
///////////////////////////////////////////////////////////////////////
static const char *pin_name = "ads131m08_drdy";
static bool irq_registered;

void ads131m08_adc_rst(void)
{
   if (!ads131m08_data || !ads131m08_data->reset_gpiod)
      return;

   gpiod_set_value_cansleep(ads131m08_data->reset_gpiod, 1);
   msleep(1);
   gpiod_set_value_cansleep(ads131m08_data->reset_gpiod, 0);
}

void ads131m08_adc_sync(void)
{
   if (!ads131m08_data || !ads131m08_data->reset_gpiod)
      return;

   gpiod_set_value_cansleep(ads131m08_data->reset_gpiod, 1);
   udelay(1);
   gpiod_set_value_cansleep(ads131m08_data->reset_gpiod, 0);
}

int ads131m08_gpio_set_irq(ads131m08_data_t *data,
			irqreturn_t (*isr)(int irq, void *data),
			irqreturn_t (*isr_thread)(int irq, void *data))
{
    int ret;

    if (!data)
        return -EINVAL;
    if (irq_registered)
        return 0;
    if (data->gpio_irq_no <= 0)
        return -EINVAL;

    ret = request_threaded_irq(data->gpio_irq_no, isr, isr_thread,
                               IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                               pin_name, data);
    if (ret) {
        pr_err("%s:request_threaded_irq failed %d\n", ads131m08_dev_name, ret);
        return ret;
    }

    irq_registered = true;
    return 0;
}

void ads131m08_gpio_clr_irq(ads131m08_data_t *data)
{
    if (irq_registered && data) {
        free_irq(data->gpio_irq_no, data);
        irq_registered = false;
    }
}

int ads131m08_gpio_init(ads131m08_data_t *data)
{
   int ret;
   struct device *dev;

   if (!data || !data->spi)
      return -EINVAL;

   dev = &data->spi->dev;
   irq_registered = false;

   data->drdy_gpiod = devm_gpiod_get(dev, "drdy", GPIOD_IN);
   if (IS_ERR(data->drdy_gpiod))
   {
      ret = PTR_ERR(data->drdy_gpiod);
      pr_err("%s: failed to get drdy-gpios (%d)\n", ads131m08_dev_name, ret);
      return ret;
   }

   data->reset_gpiod = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
   if (IS_ERR(data->reset_gpiod))
   {
      ret = PTR_ERR(data->reset_gpiod);
      pr_err("%s: failed to get reset-gpios (%d)\n", ads131m08_dev_name, ret);
      return ret;
   }

   ret = gpiod_to_irq(data->drdy_gpiod);
   if (ret < 0)
   {
      pr_err("%s: gpiod_to_irq failed (%d)\n", ads131m08_dev_name, ret);
      return ret;
   }

   data->gpio_irq_no = ret;
   pr_info("ads131m08:IRQ:%d\n", data->gpio_irq_no);
   return SUCCESS;
}

void ads131m08_gpio_exit(ads131m08_data_t *data)
{
   ads131m08_gpio_clr_irq(data);
}

//////////////////////////////////////////////////////////////////////
