#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "type.h"
#include "dbg.h"
#include "linux_spi.h"
#include "util.h"

///////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "linux_spi.h"

typedef struct _spi_tool_t
{
   int fd;
   uint8_t mode;
   uint8_t bits;
   uint32_t hz;
   struct spi_ioc_transfer xfer[2];
} spi_tool_t;
#define	SPI_TOOL(ptr)	((spi_tool_t *)ptr)

void spi_deinit(void *hdlr)
{
   spi_tool_t *spi_tool = SPI_TOOL(hdlr);
   if (spi_tool)
   {
      if (spi_tool->fd > 0) close(spi_tool->fd);
         free(spi_tool);
   }
}

void *spi_init(char *dev, uint32_t hz,int spi_mode)
{
   spi_tool_t *spi_tool = NULL;

   //// SPI structure를 zeroing을 안하면 invalid argument 뜸 주의 할것 !!!!
   spi_tool = (spi_tool_t *)buf_alloc(sizeof(spi_tool_t));


   if (!spi_tool) return NULL;

   spi_tool->hz = hz;

   spi_tool->xfer[0].tx_buf = 0;
   spi_tool->xfer[0].rx_buf = 0;
   spi_tool->xfer[0].len = 0; // Length of  command to write
   spi_tool->xfer[0].cs_change = 0;
   spi_tool->xfer[0].delay_usecs = 0,
   spi_tool->xfer[0].speed_hz = hz,
   spi_tool->xfer[0].bits_per_word = 8,

   spi_tool->xfer[1].rx_buf = 0;
   spi_tool->xfer[1].tx_buf = 0;
   spi_tool->xfer[1].len = 0; // Length of Data to read
   spi_tool->xfer[1].cs_change = 0;
   spi_tool->xfer[1].delay_usecs = 0;
   spi_tool->xfer[1].speed_hz = hz;
   spi_tool->xfer[1].bits_per_word = 8;


   if ((spi_tool->fd = open(dev, O_RDWR)) < 0)
   {
      spi_deinit(spi_tool);
      return NULL;
   }

   if (ioctl(spi_tool->fd, SPI_IOC_RD_MODE, &spi_tool->mode) < 0)
   {
      spi_deinit(spi_tool);
      return NULL;
   }

   /*
   가능한 mode. spidev.h에 선언되어 있음.

   #define SPI_CPHA        0x01
   #define SPI_CPOL        0x02

   CPOL : Clock Polarity  // 0: high active 1: low active
   CPHA : Clock Phase   // 0: non active -> active로 갈 때 샘플링
                        // 1: active -> non active로 갈 때 샘플링

   #define SPI_MODE_0      (0|0) 
   #define SPI_MODE_1      (0|SPI_CPHA)
   #define SPI_MODE_2      (SPI_CPOL|0)
   #define SPI_MODE_3      (SPI_CPOL|SPI_CPHA)

   #define SPI_CS_HIGH     0x04
   #define SPI_LSB_FIRST   0x08
   #define SPI_3WIRE       0x10
   #define SPI_LOOP        0x20
   #define SPI_NO_CS       0x40
   #define SPI_READY       0x80
   */
   spi_tool->mode = spi_mode==-1?SPI_MODE_0:spi_mode;
   if (ioctl(spi_tool->fd, SPI_IOC_WR_MODE, &spi_tool->mode) < 0)
   {
      spi_deinit(spi_tool);
      return NULL;
   }

   if (ioctl(spi_tool->fd, SPI_IOC_RD_BITS_PER_WORD, &spi_tool->bits) < 0)
   {
      spi_deinit(spi_tool);
      return NULL;
   }

   if (spi_tool->bits != 8)
   {
      spi_tool->bits = 8;
      if (ioctl(spi_tool->fd, SPI_IOC_WR_BITS_PER_WORD, &spi_tool->bits) < 0)
      {
         spi_deinit(spi_tool);
         return NULL;
      }
   }

   if (ioctl(spi_tool->fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_tool->hz)<0)
   {
      spi_deinit(spi_tool);
      return NULL;
   }

   return spi_tool;
}

int spi_read(void *hdlr, uint8_t *rx, int rx_len)
{
   spi_tool_t *spi_tool = SPI_TOOL(hdlr);
   return read(spi_tool->fd,rx,rx_len);
}
int spi_write(void *hdlr, uint8_t *tx, int tx_len)
{
   spi_tool_t *spi_tool = SPI_TOOL(hdlr);
   return write(spi_tool->fd,tx,tx_len);
}

///// Multi-byte multi-xfer communication
int spi_xfer(void *hdlr,uint8_t *tx1,uint8_t *rx1,int xfer1_cnt,
			uint8_t *tx2,uint8_t *rx2,int xfer2_cnt)
{
   int ret,xfer_cnt=0;
   spi_tool_t *spi_tool = SPI_TOOL(hdlr);

   spi_tool->xfer[0].tx_buf = (unsigned long)tx1;
   spi_tool->xfer[0].rx_buf = (unsigned long)rx1;
   spi_tool->xfer[0].len    = xfer1_cnt;
   spi_tool->xfer[1].tx_buf = (unsigned long)tx2;
   spi_tool->xfer[1].rx_buf = (unsigned long)rx2;
   spi_tool->xfer[1].len    = xfer2_cnt;

   if(xfer1_cnt>0) xfer_cnt++;
   if(xfer2_cnt>0) xfer_cnt++;

   if((ret=ioctl(spi_tool->fd,SPI_IOC_MESSAGE(xfer_cnt),spi_tool->xfer))<0)
   {
      DBG_LOG(LOG_DEBUG,"SPI_XFER: Failed :%s\n",strerror(errno));
      return FAIL;
   }
   return SUCCESS;
}

///// single-byte xfer
uint8_t spi_trx(void *hdlr,uint8_t cmd)
{
   uint8_t ret;
   if(spi_xfer(hdlr,&cmd,&ret,1,NULL,NULL,0)==FAIL)
      ret = 0;
   return ret;
}
