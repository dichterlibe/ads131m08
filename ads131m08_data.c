/*********************************************************************
  Filename:       ads131m08_data.c
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
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/irq_work.h>
#include <linux/ratelimit.h>
#include <linux/seq_file.h>

#include "ads131m08.h"
#include "list.h"
///////////////////////////////////////////////////////////////////////
#define	CHUNK_BUF_CNT		8
ads131m08_data_t        *ads131m08_data;
struct device    *ads131m08_dev;
static list_t	*ready_list = NULL;
static list_t	*active_list = NULL;
static list_t	*free_list = NULL;
static atomic_t  	chunk_sz;
static chunk_buf_t	*chunk_buf_list;

static irqreturn_t ads131m08_isr(int irq,void *data);
static irqreturn_t ads131m08_irq_thread(int irq,void *data);
//////////////////////////////////////////////////////////
// run_stat
static atomic_t run_stat;
static atomic64_t drop_cnt;

int ads131m08_run_stat(void)
{
   return atomic_read(&run_stat);
}

int ads131m08_run_cmd(int run)
{
   int ret;

   if (ads131m08_run_stat() == run)
      return SUCCESS;

   if (run)
   {
      ads131m08_data_cleanup();
      atomic_set(&run_stat, ADC_RUN);

      ret = ads131m08_gpio_set_irq(ads131m08_data, ads131m08_isr, ads131m08_irq_thread);
      if (ret < 0)
      {
         atomic_set(&run_stat, ADC_STOP);
         return ret;
      }

      ret = ads131m08_cmd_wakeup();
      if (ret < 0)
      {
         ads131m08_gpio_clr_irq(ads131m08_data);
         atomic_set(&run_stat, ADC_STOP);
         ads131m08_data_cleanup();
         return ret;
      }
   }
   else
   {
      atomic_set(&run_stat, ADC_STOP);
      ads131m08_gpio_clr_irq(ads131m08_data);
      ret = ads131m08_cmd_standby();
      ads131m08_data_cleanup();
      if (ret < 0)
         return ret;
   }

   return SUCCESS;
}
///////////////////////////////////////////////////////////////////////////
int ads131m08_data_get_chunk_sz(void)
{
   return atomic_read(&chunk_sz);
}

static void init_chunk_buf(chunk_buf_t *buf,int buf_sz)
{
   buf->chunk_sz = buf_sz;
   buf->idx = 0;
   buf->stat = BUF_STAT_FREE;
   buf->xfer = NULL;
   spin_lock_init(&buf->lock);
   spin_lock_init(&buf->head.lock);
}

void ads131m08_data_resize_chunk_buf(int new_sz)
{
   int i;
   chunk_buf_t *buf;

   if (new_sz <= 0)
      return;
   if (new_sz > MAX_CHUNK_SZ)
      new_sz = MAX_CHUNK_SZ;

   for (i = 0; i < CHUNK_BUF_CNT; i++)
   {
      buf = &chunk_buf_list[i];
      BUF_LOCK(buf);
      buf->chunk_sz = new_sz;
      buf->idx = 0;
      BUF_UNLOCK(buf);
   }
   atomic_set(&chunk_sz, new_sz);
}

/////////////////////////////////////////////////////////////////////////////////////////
static atomic_t	adc_lock;

static int ads131m08_adc_try_lock(void)
{
   return atomic_cmpxchg(&adc_lock, 0, 1) == 0;
}

int ads131m08_adc_lock_cnt(int cnt)
{
   int i;

   for (i = 0; i < cnt; i++)
   {
      if (ads131m08_adc_try_lock())
         return SUCCESS;
      udelay(1);
   }
   return FAIL;
}

void ads131m08_adc_unlock(void)
{
   atomic_set(&adc_lock, 0);
}

void ads131m08_adc_lock_release(void)
{
   atomic_set(&adc_lock, 0);
}
///////////////////////////////////////////////////////
smpl_data_t *ads131m08_smpl_data_get(chunk_buf_t *chunk_buf)
{
   smpl_data_t *ret;
   int idx;

   if (!chunk_buf)
      return NULL;

   BUF_LOCK(chunk_buf);
   idx = chunk_buf->idx;
   ret = &chunk_buf->data[idx];
   BUF_UNLOCK(chunk_buf);
   return ret;
}

void ads131m08_chunk_buf_ready(chunk_buf_t *chunk_buf)
{
   list_append(ready_list, chunk_buf);
   ads131m08_iface_wakeup();
}

void ads131m08_chunk_buf_active(chunk_buf_t *chunk_buf)
{
   list_append(active_list, chunk_buf);
}

static atomic64_t smpl_ts;
static irqreturn_t ads131m08_irq_thread(int irq,void *data)
{
   chunk_buf_t *active_buf;
   int ret;
   smpl_data_t *new_data;

   active_buf = list_fetch(active_list);
   if (active_buf == NULL)
      active_buf = list_fetch(free_list);

   if (active_buf == NULL)
   {
      atomic64_inc(&drop_cnt);
      ret = ads131m08_read_data_discard();
      if (ret < 0)
         pr_warn_ratelimited("%s:%s discard read failed, ret=%d\n",
                             ads131m08_dev_name, __func__, ret);
      return IRQ_HANDLED;
   }

   BUF_LOCK(active_buf);
   active_buf->stat = BUF_STAT_ACTIVE;
   BUF_UNLOCK(active_buf);

   new_data = ads131m08_smpl_data_get(active_buf);
   if (new_data)
      new_data->ts = atomic64_read(&smpl_ts);

   ret = ads131m08_read_data(active_buf);
   if (ret < 0)
   {
      pr_err("%s:%s failed, ret=%d\n", ads131m08_dev_name, __func__, ret);
      ads131m08_chunk_buf_active(active_buf);
   }
   return IRQ_HANDLED;
}

static irqreturn_t ads131m08_isr(int irq,void *data)
{
   atomic64_set(&smpl_ts, ktime_get_real());
   return IRQ_WAKE_THREAD;
}

chunk_buf_t *ads131m08_data_get(void)
{
   return list_fetch(ready_list);
}

void ads131m08_data_return(const void *data)
{
   chunk_buf_t *buf = (chunk_buf_t *)data;

   if (!buf)
      return;

   BUF_LOCK(buf);
   buf->idx = 0;
   buf->stat = BUF_STAT_FREE;
   buf->xfer = NULL;
   BUF_UNLOCK(buf);
   list_append(free_list, buf);
}

int ads131m08_data_smpl_cnt(int *free_cnt,int *active_cnt)
{
   if (free_cnt)
      *free_cnt = list_get_cnt(free_list);
   if (active_cnt)
      *active_cnt = list_get_cnt(active_list);
   return list_get_cnt(ready_list);
}

void ads131m08_data_buf_seq_show(struct seq_file *m)
{
   int i, stat, b_idx, chunk_sz_local;
   chunk_buf_t *buf;

   seq_printf(m, "DRIVER_VERSION:%s\n", ads131m08_driver_version);

   seq_printf(m,
              "RUN<%d>,OPEN<%d>,LIST_CNT:%d,%d,%d,DROP:%lld\n",
              ads131m08_run_stat(),
              ads131m08_open_stat(),
              list_get_cnt(free_list),
              list_get_cnt(ready_list),
              list_get_cnt(active_list),
              atomic64_read(&drop_cnt));

   for (i = 0; i < CHUNK_BUF_CNT; i++)
   {
      buf = &chunk_buf_list[i];
      BUF_LOCK(buf);
      stat = buf->stat;
      b_idx = buf->idx;
      chunk_sz_local = buf->chunk_sz;
      BUF_UNLOCK(buf);

      seq_printf(m, "<%d> stat:%d,idx:%d,chunk_sz:%d\n",
                 i + 1, stat, b_idx, chunk_sz_local);
   }
}

void ads131m08_data_cleanup(void)
{
   if (!ads131m08_data)
      return;

   list_cleanup(ready_list, ads131m08_data_return);
   list_cleanup(active_list, ads131m08_data_return);
   wake_up_interruptible(&ads131m08_data->wq);
}

int ads131m08_data_init(struct spi_device *spi)
{
   int i;
   chunk_buf_t *buf;

   ads131m08_data = devm_kzalloc(&spi->dev, sizeof(ads131m08_data_t), GFP_KERNEL);
   if (!ads131m08_data)
      return -ENOMEM;

   atomic_set(&chunk_sz, DEFAULT_CHUNK_SZ);
   atomic_set(&run_stat, 0);
   atomic_set(&adc_lock, 0);
   atomic64_set(&smpl_ts, 0);
   atomic64_set(&drop_cnt, 0);
   ads131m08_dev = &spi->dev;
   ads131m08_data->spi = spi;
   init_waitqueue_head(&ads131m08_data->wq);

   chunk_buf_list = devm_kzalloc(ads131m08_dev,
                                 sizeof(chunk_buf_t) * CHUNK_BUF_CNT,
                                 GFP_KERNEL);
   if (!chunk_buf_list)
      return -ENOMEM;

   ready_list = list_new(ads131m08_dev);
   free_list = list_new(ads131m08_dev);
   active_list = list_new(ads131m08_dev);
   if (!ready_list || !free_list || !active_list)
      return -ENOMEM;

   for (i = 0; i < CHUNK_BUF_CNT; i++)
   {
      buf = &chunk_buf_list[i];
      init_chunk_buf(buf, DEFAULT_CHUNK_SZ);
      ads131m08_data_return(buf);
   }

   return SUCCESS;
}

void ads131m08_data_exit(void)
{
   ads131m08_run_cmd(ADC_STOP);
}
