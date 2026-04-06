#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <sys/poll.h>
#include <pthread.h>
#include "ads131m08_ioctl.h"
#include "list.h"

static const char *dev_name = "/dev/ads131m08";
#define	ADC_CH_NUM		8
#define	SUCCESS	0
#define	FAIL	-1
typedef struct
{
   int64_t ts;
   int32_t data[ADC_CH_NUM];
} smpl_data_t;
#define		DEFAULT_CHUNK_SZ	320
#define	PKT_SZ	sizeof(smpl_data_t)

static void dump_buffer_fd(FILE *fd,const void *buffer,int len)
{
   int i,tmp_len,j,start=0;
   const uint8_t *buf = (const uint8_t *)buffer;

   fprintf(fd,"   +------------------------------------------------\n");
   fprintf(fd,"   | 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F \n");
   fprintf(fd,"---+------------------------------------------------\n");
   tmp_len = ((len%0x10)==0?len:((len/0x10)+1)*0x10);
   for(i=0;i<tmp_len;i++)
   {
      if((i%0x10)==0)
      {
         fprintf(fd,"%3X|",i);
         start = i;
      }
      if(i<len)
         fprintf(fd,"%02X ",(uint8_t)buf[i]);
      else
         fprintf(fd,"xx ");

      if(((i+1)%0x10)==0) // if EOL
      {
         int idx=start;
         fprintf(fd,"  ");
         for(j=0;j<0x10;j++)
         {
            uint8_t tmp8 = buf[idx];
            if(idx<len)
               fprintf(fd,"%c ",isgraph(tmp8)?tmp8:'.');
            idx++;
         }
         fprintf(fd,"\n");
      }
   }
   fprintf(fd,"Total %d byte(s)\n",len);
}

static void dump_buffer(const void *buffer, int len)
{
   dump_buffer_fd(stdout,buffer,len);
}

static int make_token(char *buf,char *token_ptr[],int max_cnt,const char *delim)
{
   int token_cnt=0;

   if(buf==NULL)
        return 0;
   if((token_ptr[token_cnt++]=strtok((char*)buf,delim))==NULL)
        return 0;
   while((token_ptr[token_cnt]=strtok(NULL,delim))!=NULL)
   {
      token_cnt++;
      if(token_cnt == max_cnt)
        break;
   }
   return token_cnt;
}
static void *buf_alloc(int size)
{
   void *ret = calloc(1,size);
   if(!ret)
   {
      fprintf(stderr,"Can't alloc memory.. I'm dying!!\n");
      exit(-1);
   }
   return ret;
}
//////////////////////////////////////////////////////////////////////////////////

static int print_pkt(smpl_data_t *pkt,FILE *outfd)
{
   static int64_t prev_ts;
   int64_t ts;
   int i,idx=0,print_idx=0;
   char print_buf[1024];
   ts = pkt->ts;
#if 0
   print_idx=sprintf(print_buf,"%lld,%d,",ts,ts-prev_ts);
   prev_ts = ts;
   for(i=0;i<ADC_CH_NUM;i++)
   {
      print_idx+=sprintf(&print_buf[print_idx],"%d,",pkt->data[i]);
   }
   print_idx--;
   print_buf[print_idx]='\0';
   fprintf(outfd,"%s\n",print_buf);
#else
   fprintf(outfd,"%lld,%d\n",ts,ts-prev_ts);
   prev_ts = ts;
   
   //fflush(stdout);
#endif
   return 0;
}
/////////////////////////////////////////////////////////////////////
static int test_loop(int fd,FILE *outfd)
{
   int ret,idx,pkt_cnt,i,buf_sz;
   smpl_data_t *buf;
   struct pollfd fds;

   buf_sz = PKT_SZ * DEFAULT_CHUNK_SZ;
   buf = buf_alloc(buf_sz);
   memset(&fds,0,sizeof(struct pollfd));
   fds.fd = fd;
   fds.events = POLLIN;
   while(1)
   {
      poll(&fds,1,-1);
      ret=read(fd,buf,buf_sz);
      if(ret<0)
      {
         printf("Read ERROR:%s\n",strerror(errno));
         continue;
      }
      pkt_cnt = ret/PKT_SZ;
      idx = 0;
      for(i=0;i<pkt_cnt;i++)
      {
         print_pkt(&buf[i],outfd);
      }
   }
}

static int launch_thread(void *(*task)(void *),void *args)
{
   int ret;
   pthread_t thr_id;
   if((ret=pthread_create(&thr_id,NULL,task,args))!=SUCCESS)
      return ret;
   pthread_detach(thr_id);
   return SUCCESS;
}


/////////////////////////////////////////////////////////////
typedef struct
{
   const char *cmd;
   void (*hdlr)(int fd,char **args,int cnt);
} cmd_t;

static void wakeup_hdlr(int fd,char **args,int cnt);
static void standby_hdlr(int fd,char **args,int cnt);
static void stat_hdlr(int fd,char **args,int cnt);
static void rreg_hdlr(int fd,char **args,int cnt);
static void wreg_hdlr(int fd,char **args,int cnt);
static void reset_hdlr(int fd,char **args,int cnt);
static void chunk_sz_hdlr(int fd,char **args,int cnt);
static void stat_hdlr(int fd,char **args,int cnt);
static cmd_t	cmd_list[] = 
{
   {"start",   wakeup_hdlr},
   {"stop",    standby_hdlr},
   {"stat",    stat_hdlr},
   {"rreg",    rreg_hdlr},
   {"wreg",    wreg_hdlr},
   {"reset",   reset_hdlr},
   {"chunk_sz",chunk_sz_hdlr},
};
#define	CMD_NUM		(sizeof(cmd_list)/sizeof(cmd_list[0]))
static void parse_cmd(char *cmd_buf,int fd)
{
   int token_cnt,i;
   char *token[10];
   cmd_t *cmd;
   token_cnt = make_token(cmd_buf,token,10," \n\r");
   if(token_cnt==0)
      return;
   for(i=0;i<CMD_NUM;i++)
   {
      cmd = &cmd_list[i];
      if(strcmp(cmd->cmd,token[0])==0)
      {
         cmd->hdlr(fd,&token[1],token_cnt-1);
	 break;
      }
   }
   return;
}
static void wakeup_hdlr(int fd,char **args,int cnt)
{
   if(ioctl(fd,ADS_IOCTL_CMD_WAKEUP,0)<0)
   {
      printf("ioctl failed:%s\n",strerror(errno));
   }
}
static void standby_hdlr(int fd,char **args,int cnt)
{
   if(ioctl(fd,ADS_IOCTL_CMD_STANDBY,0)<0)
   {
      printf("ioctl failed:%s\n",strerror(errno));
   }
}
static void rreg_hdlr(int fd,char **args,int token_cnt)
{
   struct ads_ioctl_reg *reg_data;
   int addr,cnt,i;
   if(token_cnt!=2)
   {
      printf("Usage:rreg <addr> <count>\n");
      return;
   }
   addr = atoi(args[0]);
   cnt  = atoi(args[1]);
   //printf("RREG ADDR:<%d> CNT:<%d>\n",addr,cnt);
   reg_data = buf_alloc(sizeof(struct ads_ioctl_reg) + sizeof(uint16_t) * cnt);
   reg_data->start_addr = addr;
   reg_data->reg_count  = cnt;
   if(ioctl(fd,ADS_IOCTL_CMD_RREG,reg_data)<0)
   {
      printf("ioctl failed:%s\n",strerror(errno));
   }
   printf("----+-----\n");
   printf("ADDR|VALUE\n");
   printf("----+-----\n");
   for(i=0;i<cnt;i++)
   {
      printf("%04X|%04X\n",addr+i,reg_data->data[i]);
   }
   free(reg_data);
}
static void wreg_hdlr(int fd,char **args,int token_cnt)
{
   struct ads_ioctl_reg *reg_data;
   int addr,cnt,i;
   if(token_cnt < 2)
   {
      printf("Usage:wreg <start_addr> <val1> <val2> ...\n");
      return;
   }
   addr = atoi(args[0]);
   cnt  = token_cnt-1;
   reg_data = buf_alloc(sizeof(struct ads_ioctl_reg) + sizeof(uint16_t) * cnt);
   reg_data->start_addr = addr;
   reg_data->reg_count  = cnt;
   for(i=0;i<(token_cnt-1);i++)
   {
      reg_data->data[i] = strtoul(args[i+1],NULL,16);
   }
   if(ioctl(fd,ADS_IOCTL_CMD_WREG,reg_data)<0)
   {
      printf("ioctl failed:%s\n",strerror(errno));
      goto out;
   }
   printf("----+-----\n");
   printf("ADDR|VALUE\n");
   printf("----+-----\n");
   for(i=0;i<cnt;i++)
   {
      printf("%04X|%04X\n",addr+i,reg_data->data[i]);
   }
out:
   free(reg_data);
}

static void reset_hdlr(int fd,char **args,int cnt)
{
   if(ioctl(fd,ADS_IOCTL_CMD_RESET,0)<0)
   {
      printf("ioctl failed:%s\n",strerror(errno));
   }
}
static void chunk_sz_hdlr(int fd,char **args,int token_cnt)
{
   int ret;
   unsigned long chunk_sz;
   if(token_cnt<1)
   {
      printf("Usage:chunk_sz <chunk size>\n");
      return;
   }
   chunk_sz = strtoul(args[0],NULL,10);
   if((ret=ioctl(fd,ADS_IOCTL_CMD_CHUNK_SZ,chunk_sz))<0)
   {
      printf("ioctl failed:%s\n",strerror(errno));
   }
   else
   {
      printf("chunk_sz:%d\n",ret);
   }
}

static void stat_hdlr(int fd,char **args,int token_cnt)
{
   int ret;
   int stat;
   if(token_cnt!=1)
   {
      printf("Usage:stat <-1|0|1>\n");
   }
   stat = atoi(args[0]);
   if((ret=ioctl(fd,ADS_IOCTL_CMD_STAT,stat))<0)
   {
      printf("ioctl failed:%s\n",strerror(errno));
   }
   else
   {
      printf("stat:%d\n",ret);
   }
}

////////////////////////////////////////////////////////////////
static void *cmd_loop(void *args)
{
   int dev_fd = *(int *)args;
   char cmd_buf[512];
   do
   {
      printf(">> ");
      fgets(cmd_buf,512,stdin);
      parse_cmd(cmd_buf,dev_fd);
   } while(1);
   pthread_exit(NULL);
}
////////////////////////////////////////////

////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
   FILE *outfd;
   int fd,ret,idx=0,i;
   int64_t ts;
   int32_t data[ADC_CH_NUM];
   uint8_t buf[PKT_SZ];

   if(argc==1)
      outfd = stdout;
   else if(argc==2)
      outfd = fopen(argv[1],"a+");

   if((fd = open(dev_name,O_RDWR))<0)
   {
      perror("Can't open device file\n");
      return errno;
   }
   launch_thread(cmd_loop,&fd);
   test_loop(fd,outfd);
   close(fd);
   return 0;
}
