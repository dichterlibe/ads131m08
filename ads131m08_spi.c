/*********************************************************************
  Filename:       ads131m08_spi.c
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
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/mutex.h>

#include "ads131m08.h"
///////////////////////////////////////////////////////////////////////

#define	MAX_BUF_SZ	(2048)
#define	BYTE_PER_WORD	ADS131M08_BYTE_PER_WORD
#define	XFER_LEN	((2+ADC_CH_NUM) * BYTE_PER_WORD)
//////////////////////////////////////////////////////////////////////
static list_t	*xfer_list;
static struct spi_device *spi_hdlr;
static DEFINE_MUTEX(spi_io_lock);

typedef struct
{
   list_elm_t head;
   uint8_t rx_buf[MAX_BUF_SZ];
   uint8_t tx_buf[MAX_BUF_SZ];
   struct spi_message spi_msg;
   struct spi_transfer spi_xfer;
} xfer_t;

#define	XFER_BUF_NUM	8
static xfer_t *xfer_new(void)
{
   xfer_t *xfer = (xfer_t *)kzalloc(sizeof(xfer_t), GFP_KERNEL);
   if (!xfer)
      return NULL;

   spin_lock_init(&xfer->head.lock);
   return xfer;
}

static void free_xfer(const void *args)
{
   xfer_t *xfer = (xfer_t *)args;
   kfree(xfer);
}

static void xfer_buf_free(void)
{
   if (xfer_list)
      list_cleanup(xfer_list, free_xfer);
}

static int xfer_buf_init(void)
{
   int i;
   xfer_t *xfer;

   xfer_list = list_new(ads131m08_dev);
   if (!xfer_list)
      return -ENOMEM;

   for (i = 0; i < XFER_BUF_NUM; i++)
   {
      xfer = xfer_new();
      if (!xfer)
         return -ENOMEM;
      list_append(xfer_list, xfer);
   }

   return 0;
}

static xfer_t *get_xfer_buf(void)
{
   return list_fetch(xfer_list);
}

static void return_xfer_buf(xfer_t *xfer)
{
   if (xfer)
      list_append(xfer_list, xfer);
}

/////////////////////////////////////////////////////////////////////////////
#define CMD_NULL                0x0000
#define CMD_RESET               0x0011
#define CMD_STANDBY             0x0022
#define CMD_WAKEUP              0x0033
#define CMD_LOCK                0x0555
#define CMD_UNLOCK              0x0655
#define CMD_RREG                0xA000
#define CMD_WREG                0x6000

/////////////////////////////////////////////////////////////////////////////

static int word2frame(uint16_t *data,uint8_t *ret_buf,int cnt)
{
   int i,idx=0;

   for(i=0;i<cnt;i++)
   {
      ret_buf[idx++] = UPPER(data[i]);
      ret_buf[idx++] = LOWER(data[i]);
      ret_buf[idx++] = 0;
   }
   return idx;
}

static int frame2word(uint8_t *frame,void *buf,int cnt)
{
   int idx=0,i;
   uint16_t *ret = (uint16_t *)buf;

   for(i=0;i<cnt;i++)
   {
      ret[i]  = frame[idx++]<<8;
      ret[i] |= frame[idx++];
      idx++;
   }
   return idx;
}

static int frame2long(uint8_t *frame,void *buf,int cnt)
{
   int idx=0,i;
   int32_t *ret,val;

   ret = (int32_t *)buf;

   for(i=0;i<cnt;i++)
   {
      val  = frame[idx++] << 16;
      val |= frame[idx++] << 8;
      val |= frame[idx++];
      if(val & 0x800000)
         val |= 0xFF000000;
      ret[i] = val;
   }
   return idx;
}

static int adc_trx(uint16_t cmd, uint16_t *tx,int tx_cnt, void *rx,int rx_cnt,
		int (*frame_conv)(uint8_t *frame,void *buf,int cnt),uint16_t *status)
{
   int xfer_cnt = 0, xfer_len, idx, ret;
   uint8_t *rx_buf, *tx_buf;
   uint16_t tmp16 = 0;
   xfer_t *xfer;
   struct spi_transfer spi_xfer = { 0 };

   if (!spi_hdlr)
      return -ENODEV;

   xfer_cnt = tx_cnt > rx_cnt ? tx_cnt : rx_cnt;
   if (xfer_cnt == 0)
      xfer_cnt = 1;
   else
      xfer_cnt += 2;
   xfer_len = xfer_cnt * BYTE_PER_WORD;

   if (xfer_len > MAX_BUF_SZ)
      return -EINVAL;

   xfer = get_xfer_buf();
   if (xfer == NULL)
   {
      pr_err("%s: %s,no available xfer buf\n", ads131m08_dev_name, __func__);
      return -ENOMEM;
   }

   tx_buf = xfer->tx_buf;
   rx_buf = xfer->rx_buf;
   memset(tx_buf, 0, xfer_len);
   memset(rx_buf, 0, xfer_len);

   idx = word2frame(&cmd, tx_buf, 1);
   if (tx_cnt > 0)
      idx += word2frame(tx, &tx_buf[idx], tx_cnt);

   spi_xfer.len = xfer_len;
   spi_xfer.tx_buf = tx_buf;
   spi_xfer.rx_buf = rx_buf;

   mutex_lock(&spi_io_lock);
   ret = spi_sync_transfer(spi_hdlr, &spi_xfer, 1);
   mutex_unlock(&spi_io_lock);
   if (ret < 0)
   {
      return_xfer_buf(xfer);
      return ret;
   }

   idx = frame2word(rx_buf, &tmp16, 1);
   if (status)
      *status = tmp16;
   if (rx && frame_conv)
      frame_conv(&rx_buf[idx], rx, rx_cnt);

   return_xfer_buf(xfer);
   return ret;
}

static int read_stat(uint16_t *status)
{
   return adc_trx(CMD_NULL, NULL, 0, NULL, 0, NULL, status);
}

int ads131m08_read_stat(uint16_t *status)
{
   int ret;

   ret = read_stat(status);
   return ret;
}

int ads131m08_reg_read(uint8_t start_addr,int cnt,uint16_t *ret_reg)
{
   int ret;
   uint16_t status,cmd;

   cmd = (CMD_RREG | start_addr<<7 | (cnt-1));
   ret = adc_trx(cmd, NULL, 0, NULL, ADC_CH_NUM, NULL, NULL);
   if (ret < 0)
      return ret;

   ret = adc_trx(CMD_NULL, NULL, 0, ret_reg, cnt, frame2word, &status);
   if (ret < 0)
      return ret;
   if (cnt == 1)
      *ret_reg = status;
   return ret;
}

int ads131m08_reg_write(uint8_t start_addr,int cnt,uint16_t *reg_val)
{
   int ret;
   uint16_t cmd,status;

   cmd = (CMD_WREG | start_addr<<7 | (cnt-1));
   ret = adc_trx(cmd, reg_val, cnt, NULL, 0, NULL, NULL);
   if (ret < 0)
      return ret;

   ret = read_stat(&status);
   return ret;
}
////////////////////////////////////////////////////////////////
#define	DATA_CNT		(ADC_CH_NUM + 2)
#define	DATA_LEN		(DATA_CNT * BYTE_PER_WORD)

static void xfer_complete(void *arg)
{
   int idx = 0;
   bool ready = false;
   chunk_buf_t *active_buf = (chunk_buf_t *)arg;
   xfer_t *xfer;
   uint8_t *rx_buf;
   smpl_data_t *new_data;
   uint16_t tmp16;

   xfer = active_buf->xfer;
   if (!xfer)
      return;

   rx_buf = xfer->rx_buf;
   new_data = ads131m08_smpl_data_get(active_buf);
   if (!new_data)
   {
      return_xfer_buf(xfer);
      active_buf->xfer = NULL;
      ads131m08_chunk_buf_active(active_buf);
      return;
   }

   idx = frame2word(rx_buf, &tmp16, 1);
   frame2long(&rx_buf[idx], new_data->data, ADC_CH_NUM);
   return_xfer_buf(xfer);
   active_buf->xfer = NULL;

   BUF_LOCK(active_buf);
   active_buf->idx++;
   if(active_buf->idx >= active_buf->chunk_sz)
   {
      active_buf->stat = BUF_STAT_READY;
      ready = true;
   }
   BUF_UNLOCK(active_buf);

   if (ready)
      ads131m08_chunk_buf_ready(active_buf);
   else
      ads131m08_chunk_buf_active(active_buf);
}

int ads131m08_read_data(chunk_buf_t *active_buf)
{
   struct spi_message  *spi_msg;
   struct spi_transfer *spi_xfer;
   xfer_t *xfer;
   int ret;

   if (!active_buf)
      return -EINVAL;
   if (!spi_hdlr)
      return -ENODEV;

   xfer = get_xfer_buf();
   if (xfer == NULL)
   {
      pr_err("%s:%s,no available xfer buffer\n", ads131m08_dev_name, __func__);
      return -ENOMEM;
   }

   spi_xfer = &xfer->spi_xfer;
   spi_msg  = &xfer->spi_msg;
   memset(spi_xfer, 0, sizeof(*spi_xfer));
   memset(xfer->tx_buf, 0, DATA_LEN);
   memset(xfer->rx_buf, 0, DATA_LEN);

   spi_xfer->len = DATA_LEN;
   spi_xfer->tx_buf = xfer->tx_buf;
   spi_xfer->rx_buf = xfer->rx_buf;

   active_buf->xfer = xfer;
   spi_message_init(spi_msg);
   spi_message_add_tail(spi_xfer, spi_msg);

   mutex_lock(&spi_io_lock);
   ret = spi_sync_transfer(spi_hdlr, spi_xfer, 1);
   mutex_unlock(&spi_io_lock);
   if (ret < 0)
   {
      active_buf->xfer = NULL;
      return_xfer_buf(xfer);
      return ret;
   }

   xfer_complete(active_buf);
   return SUCCESS;
}

int ads131m08_read_data_discard(void)
{
   uint8_t tx_buf[DATA_LEN] = { 0 };
   uint8_t rx_buf[DATA_LEN] = { 0 };
   struct spi_transfer spi_xfer = { 0 };
   int ret;

   if (!spi_hdlr)
      return -ENODEV;

   spi_xfer.len = DATA_LEN;
   spi_xfer.tx_buf = tx_buf;
   spi_xfer.rx_buf = rx_buf;

   mutex_lock(&spi_io_lock);
   ret = spi_sync_transfer(spi_hdlr, &spi_xfer, 1);
   mutex_unlock(&spi_io_lock);
   return ret;
}

/////////////////////////////////////////////////////////////////////
static int cmd_strobe(uint16_t cmd,uint16_t *data,int cnt,int wait_usec)
{
   int ret;
   uint16_t stat;

   ret = adc_trx(cmd, data, cnt, NULL, 0, NULL, &stat);
   if (ret < 0)
      return ret;
   if (wait_usec > 0)
      udelay(wait_usec);
   return ret;
}

int ads131m08_cmd_standby(void)
{
   return cmd_strobe(CMD_STANDBY, NULL, 0, 0);
}

int ads131m08_cmd_wakeup(void)
{
   return cmd_strobe(CMD_WAKEUP, NULL, 0, 0);
}

int ads131m08_cmd_reset(void)
{
   int ret;
   uint16_t stat, data[8];

   memset(data, 0, sizeof(data));
   ret = cmd_strobe(CMD_RESET, data, 8, 10);
   if (ret < 0)
      return ret;

   ret = read_stat(&stat);
   return ret;
}

int ads131m08_cmd_lock(void)
{
   return cmd_strobe(CMD_LOCK, NULL, 0, 0);
}

int ads131m08_cmd_unlock(void)
{
   return cmd_strobe(CMD_UNLOCK, NULL, 0, 0);
}
/////////////////////////////////////////////////////////////////////

void ads131m08_spi_exit(void)
{
   spi_hdlr = NULL;
   xfer_buf_free();
   xfer_list = NULL;
}

int ads131m08_spi_init(struct spi_device *spi)
{
   int ret;

   ret = xfer_buf_init();
   if (ret < 0)
      return ret;

   spi_hdlr = spi;
   return 0;
}
