#include <stdio.h>
#include <stdint.h>
#include "ads131m08_ioctl.h"

typedef struct
{
   const char *str;
   unsigned long cmd;
} cmd_t;

static cmd_t	cmd_list[] = 
{
   {"CMD_RESET",   	ADS_IOCTL_CMD_RESET},
   {"CMD_LOCK",  	ADS_IOCTL_CMD_LOCK},
   {"CMD_UNLOCK",  	ADS_IOCTL_CMD_UNLOCK},
   {"CMD_WAKEUP",  	ADS_IOCTL_CMD_WAKEUP},
   {"CMD_STANDBY",  	ADS_IOCTL_CMD_STANDBY},
   {"CMD_RREG",  	ADS_IOCTL_CMD_RREG},
   {"CMD_WREG",  	ADS_IOCTL_CMD_WREG},
   {"CMD_CHUNK_SZ",  	ADS_IOCTL_CMD_CHUNK_SZ},
   {"CMD_STAT",  	ADS_IOCTL_CMD_STAT},
};
#define	CMD_NUM	(sizeof(cmd_list)/sizeof(cmd_list[0]))

void main(void)
{
   cmd_t *cmd;
   int i;
   for(i=0;i<CMD_NUM;i++)
   {
      cmd = &cmd_list[i];
      printf("CMD<%s>:%X\n",cmd->str,cmd->cmd);
   }
}
