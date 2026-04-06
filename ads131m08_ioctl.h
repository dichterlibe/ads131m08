/*********************************************************************
  Filename:       ads131m08_ioctl.h
  Revised:        Date: 2022/7/9
  Revision:       Revision: 0.1

  Description:
          - ADS1220 Delta Sigma 24-bit ADC driver
  Notes:

  Copyright (c) 2022 by u-Infraway, Inc., All Rights Reserved.

*********************************************************************/
#ifndef __ADS131M08_IOCTL_H
#define __ADS131M08_IOCTL_H

/*****************************************
 * INCLUDES
 */
#include <linux/ioctl.h>

/*****************************************
 * Typedefines
 */
struct ads_ioctl_reg
{
   uint16_t	start_addr;
   uint16_t	reg_count;
   uint16_t	data[0];
};

/*****************************************
 * commands
 */
#define	  ADC_MAGIC	'M'
#define   ADS_IOCTL_CMD_RESET     _IO(ADC_MAGIC,   1)
#define   ADS_IOCTL_CMD_LOCK      _IO(ADC_MAGIC,   2)
#define   ADS_IOCTL_CMD_UNLOCK    _IO(ADC_MAGIC,   3)
#define   ADS_IOCTL_CMD_WAKEUP    _IO(ADC_MAGIC,   4)
#define   ADS_IOCTL_CMD_STANDBY   _IO(ADC_MAGIC,   5)
#define   ADS_IOCTL_CMD_RREG      _IOWR(ADC_MAGIC, 6, struct ads_ioctl_reg)
#define   ADS_IOCTL_CMD_WREG      _IOWR(ADC_MAGIC, 7, struct ads_ioctl_reg)
#define   ADS_IOCTL_CMD_CHUNK_SZ  _IOWR(ADC_MAGIC, 8, unsigned long)
#define   ADS_IOCTL_CMD_STAT      _IOWR(ADC_MAGIC, 9, unsigned long)

/*****************************************
 * CONSTANT
 */

///////////////////////////////////////////////////////////

#endif
