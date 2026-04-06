/*********************************************************************
  Filename:       linux_spi.h
  Revised:        Date: 2021/10/8
  Revision:       Revision: 0.1

  Description:
          - ENUSTech Street Light Control System
  Notes:

  Copyright (c) 2021 by u-Infraway, Inc., All Rights Reserved.

*********************************************************************/
#ifndef __LINUX_SPI_H
#define __LINUX_SPI_H

/*****************************************
 * INCLUDES
 */
#include <linux/spi/spidev.h>
#include "type.h"

/*****************************************
 * CONSTANT
 */

/*****************************************
 * type defines
 */

/*****************************************
 * FUNCTIONS
 */
extern void *spi_init(char *dev, uint32_t hz,int spi_mode);
extern void spi_deinit(void *hdlr);
extern int spi_read(void *hdlr, uint8_t *rx, int rx_len);
extern int spi_write(void *hdlr, uint8_t *tx, int tx_len);
extern uint8_t spi_trx(void *hdlr,uint8_t cmd);
extern int spi_xfer(void *hdlr,uint8_t *tx1,uint8_t *rx1,int xfer1_cnt,
			       uint8_t *tx2,uint8_t *rx2,int xfer2_cnt);

#endif // __NLC_DEV_H

