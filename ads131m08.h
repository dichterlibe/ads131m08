/*********************************************************************
  Filename:       ads131m08.h
  Revised:        Date: 2022/4/7
  Revision:       Revision: 0.1

  Description:
          - ADS1220 Delta Sigma 24-bit ADC driver
  Notes:

  Copyright (c) 2022 by u-Infraway, Inc., All Rights Reserved.

*********************************************************************/
#ifndef __ADS131M08_H
#define __ADS131M08_H

/*****************************************
 * INCLUDES
 */
#include <linux/spi/spi.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio/consumer.h>
#include "list.h"

struct seq_file;

/*****************************************
 * CONSTANT
 */
#define	FAIL		-1
#define	SUCCESS		0

#define	ADC_RUN		1
#define	ADC_STOP	0

///////////////////////////////////////////////////////////
enum
{
   ADS131M08_CH0,
   ADS131M08_CH1,
   ADS131M08_CH2,
   ADS131M08_CH3,
   ADS131M08_CH4,
   ADS131M08_CH5,
   ADS131M08_CH6,
   ADS131M08_CH7,
   ADS131M08_CH_NUM
};
#define	ADC_CH_NUM	ADS131M08_CH_NUM
#define ADS131M08_BYTE_PER_WORD        3
#define ADS131M08_DRIVER_VERSION      "2.0"
///////////////////////////////////////////////////////////

/// Macro
#define	ASSIGN_PTR_VAL(ptr,val)	do {if(ptr) {*ptr = val;}}while(0)
#define	GET_BIT(var,pos)	((var>>pos)&0x1)
#define	UPPER(var)		((var>>8)&0xFF)
#define	LOWER(var)		(var&0xFF)

///////////////////////////////////////////////////////////
// type defines
typedef struct
{
   ktime_t ts;
   int32_t data[ADC_CH_NUM];
} smpl_data_t;

#define	MAX_CHUNK_SZ		320
#define	DEFAULT_CHUNK_SZ	40
enum
{
   BUF_STAT_FREE,
   BUF_STAT_READY,
   BUF_STAT_ACTIVE,
   BUF_STAT_NUM
};
typedef struct
{
   list_elm_t head;
   int stat;
   smpl_data_t data[MAX_CHUNK_SZ];
   int chunk_sz;
   int idx;	// Data index
   spinlock_t lock;
   void		*xfer;
} chunk_buf_t;

#define BUF_LOCK(buf)   spin_lock(&(buf)->lock)
#define BUF_UNLOCK(buf) spin_unlock(&(buf)->lock)

typedef struct
{
   struct spi_device *spi;
   wait_queue_head_t wq;
   struct cdev cdev;
   dev_t dev_num;
   struct class *cl;
   int gpio_irq_no;
   struct gpio_desc *drdy_gpiod;
   struct gpio_desc *reset_gpiod;
} ads131m08_data_t;

/*****************************************
 * variables
 */
extern ads131m08_data_t *ads131m08_data;
extern const char *ads131m08_dev_name;
extern const char *ads131m08_class_name;
extern const char *ads131m08_driver_name;
extern const char *ads131m08_driver_version;
extern struct device    *ads131m08_dev;

/*****************************************
 * FUNCTIONS
 */
/// ads131m08_main.c

/// ads131m08_gpio.c, GPIO interrupt routine
extern int ads131m08_gpio_init(ads131m08_data_t *data);
extern void ads131m08_gpio_exit(ads131m08_data_t *data);
extern void ads131m08_adc_rst(void);
extern void ads131m08_adc_sync(void);
extern int ads131m08_gpio_set_irq(ads131m08_data_t *data,
				irqreturn_t (*isr)(int irq,void *data),
				irqreturn_t (*isr_thread)(int irq,void *data));
extern void ads131m08_gpio_clr_irq(ads131m08_data_t *data);

/// ads131m08_iface.c
extern int ads131m08_open_stat(void);
extern void ads131m08_iface_exit(ads131m08_data_t *data);
extern int ads131m08_iface_init(ads131m08_data_t *data);
extern void ads131m08_iface_wakeup(void);

/// ads131m08_spi.c
extern int ads131m08_spi_init(struct spi_device *spi);
extern void ads131m08_spi_exit(void);
extern int ads131m08_cmd_unlock(void);
extern int ads131m08_cmd_lock(void);
extern int ads131m08_cmd_reset(void);
extern int ads131m08_cmd_wakeup(void);
extern int ads131m08_cmd_standby(void);
extern int ads131m08_read_data(chunk_buf_t *active_buf);
extern int ads131m08_read_data_discard(void);

extern int ads131m08_reg_write(uint8_t start_addr,int cnt,uint16_t *reg_val);
extern int ads131m08_reg_read(uint8_t start_addr,int cnt,uint16_t *ret_reg);
extern int ads131m08_read_stat(uint16_t *status);
extern int ads131m08_read_data_async(void);

/// ads131m08_data.c
extern int ads131m08_data_init(struct spi_device *spi);
extern void ads131m08_data_exit(void);
extern irqreturn_t ads131m08_data_isr(int irq,void *data);
extern void ads131m08_data_resize_chunk_buf(int chunk_sz);
extern chunk_buf_t *ads131m08_data_get(void);
extern void ads131m08_data_return(const void *data);
extern int ads131m08_data_get_chunk_sz(void);

extern int ads131m08_data_smpl_cnt(int *free_cnt,int *active_cnt);
extern void ads131m08_data_buf_seq_show(struct seq_file *m);
extern void ads131m08_data_cleanup(void);
extern int ads131m08_run_cmd(int run);
extern int ads131m08_run_stat(void);

extern void ads131m08_adc_unlock(void);
extern int ads131m08_adc_lock_cnt(int cnt);
extern void ads131m08_adc_lock_release(void);

extern smpl_data_t *ads131m08_smpl_data_get(chunk_buf_t *chunk_buf);
extern void ads131m08_chunk_buf_ready(chunk_buf_t *chunk_buf);
extern void ads131m08_chunk_buf_active(chunk_buf_t *chunk_buf);

#endif // __ADS131M08_H
