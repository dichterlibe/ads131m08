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
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/errno.h>

#include "ads131m08.h"
#include "ads131m08_ioctl.h"

///////////////////////////////////////////////////////////////////////////
static struct proc_dir_entry *proc_file;

static int proc_show(struct seq_file *m, void *v)
{
   ads131m08_data_buf_seq_show(m);
   return 0;
}

static int proc_open_fn(struct inode *inode, struct file *file)
{
   return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_file_ops =
{
    .proc_open = proc_open_fn,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int proc_file_init(void)
{
   proc_file = proc_create(ads131m08_dev_name, 0444, NULL, &proc_file_ops);
   return proc_file ? SUCCESS : FAIL;
}

static void proc_file_exit(void)
{
   if (proc_file)
      proc_remove(proc_file);
}
/////////////////////////////////////////////////////////////////
static atomic_t	open_cnt;

int ads131m08_open_stat(void)
{
   return atomic_read(&open_cnt);
}

static int ads131m08_open(struct inode *inode, struct file *file)
{
   if (atomic_cmpxchg(&open_cnt, 0, 1) != 0)
      return -EBUSY;

   file->private_data = ads131m08_data;
   return 0;
}

static int ads131m08_release(struct inode *inode, struct file *file)
{
   ads131m08_run_cmd(ADC_STOP);
   atomic_set(&open_cnt, 0);
   return 0;
}
////////////////////////////////////////////////////////////////////////////////
void ads131m08_iface_wakeup(void)
{
   wake_up_interruptible(&ads131m08_data->wq);
}

static ssize_t ads131m08_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
   ssize_t idx = 0, ret;
   size_t max_smpl_cnt;
   chunk_buf_t *ready_buf;

   if (count < sizeof(smpl_data_t))
      return -EINVAL;

   for (;;) {
      ret = wait_event_interruptible(ads131m08_data->wq,
                                     ads131m08_data_smpl_cnt(NULL, NULL) > 0 ||
                                     ads131m08_run_stat() == ADC_STOP);
      if (ret == -ERESTARTSYS)
      {
         pr_info("wait_event_interruptible error:%ld\n", ret);
         return -EINTR;
      }

      ready_buf = ads131m08_data_get();
      if (ready_buf)
         break;

      if (ads131m08_run_stat() == ADC_STOP)
         return 0;
   }

   max_smpl_cnt = count / sizeof(smpl_data_t);
   BUF_LOCK(ready_buf);
   idx = ready_buf->idx;
   BUF_UNLOCK(ready_buf);
   ret = min_t(size_t, max_smpl_cnt, idx) * sizeof(smpl_data_t);

   if (copy_to_user(buf, ready_buf->data, ret))
      ret = -EFAULT;
   ads131m08_data_return(ready_buf);
   return ret;
}

static ssize_t ads131m08_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
   return -EINVAL;
}

static unsigned int ads131m08_poll(struct file *file, poll_table *wait)
{
   unsigned int mask = 0;

   poll_wait(file, &ads131m08_data->wq, wait);
   if (ads131m08_data_smpl_cnt(NULL, NULL) > 0)
      mask |= POLLIN | POLLRDNORM;
   return mask;
}

static int ioctl_reg(unsigned long arg, int read_reg,
	int (*reg_func)(uint8_t start_addr, int cnt, uint16_t *reg_data))
{
    int ret;
    struct ads_ioctl_reg user_hdr;
    struct ads_ioctl_reg *kernel_data;
    size_t size, copy_sz;

    if (copy_from_user(&user_hdr, (void __user *)arg, sizeof(user_hdr))) {
        pr_err("ioctl_reg: failed to copy header from user\n");
        return -EFAULT;
    }

    if (user_hdr.reg_count == 0 || user_hdr.reg_count > 1024) {
        pr_err("ioctl_reg: invalid reg_count = %u\n", user_hdr.reg_count);
        return -EINVAL;
    }

    size = sizeof(user_hdr) + user_hdr.reg_count * sizeof(uint16_t);

    if (!access_ok((void __user *)arg, size)) {
        pr_err("ioctl_reg: access_ok failed\n");
        return -EFAULT;
    }

    kernel_data = kzalloc(size, GFP_KERNEL);
    if (!kernel_data) {
        pr_err("ioctl_reg: failed to allocate kernel_data\n");
        return -ENOMEM;
    }

    copy_sz = read_reg ? sizeof(user_hdr) : size;
    if (copy_from_user(kernel_data, (void __user *)arg, copy_sz)) {
        pr_err("ioctl_reg: failed to copy full data from user\n");
        kfree(kernel_data);
        return -EFAULT;
    }

    ret = reg_func(kernel_data->start_addr, kernel_data->reg_count, kernel_data->data);
    if (ret < 0) {
        kfree(kernel_data);
        return ret;
    }

    if (read_reg) {
        if (copy_to_user(((struct ads_ioctl_reg __user *)arg)->data,
                         kernel_data->data,
                         kernel_data->reg_count * sizeof(uint16_t))) {
            pr_err("ioctl_reg: failed to copy data to user\n");
            kfree(kernel_data);
            return -EFAULT;
        }
    }

    kfree(kernel_data);
    return 0;
}

static int ioctl_rreg(unsigned long arg)
{
   return ioctl_reg(arg, 1, ads131m08_reg_read);
}

static int ioctl_wreg(unsigned long arg)
{
   return ioctl_reg(arg, 0, ads131m08_reg_write);
}

static int ioctl_chunk_sz(unsigned long chunk_sz)
{
   if (ads131m08_run_stat() == ADC_RUN)
      return -EBUSY;

   if (chunk_sz > 0)
   {
      pr_info("chunk_sz set:%lu\n", chunk_sz);
      ads131m08_data_resize_chunk_buf((int)chunk_sz);
   }
   return ads131m08_data_get_chunk_sz();
}

static int cmd_reset(void)
{
   int ret;

   ret = ads131m08_cmd_reset();
   if (ret < 0)
      return ret;

   return ads131m08_cmd_standby();
}

static int cmd_stat(unsigned long arg)
{
   int stat = (int)arg;

   switch (stat)
   {
      case 0:
      case 1:
         return ads131m08_run_cmd(stat);
      default:
         break;
   }
   return ads131m08_run_stat();
}

static bool ads131m08_ioctl_allowed_while_running(unsigned int cmd)
{
   switch (cmd)
   {
      case ADS_IOCTL_CMD_STANDBY:
      case ADS_IOCTL_CMD_STAT:
         return true;
      default:
         return false;
   }
}

static long ads131m08_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
   int ret = -ENOTTY;

   if (ads131m08_run_stat() == ADC_RUN && !ads131m08_ioctl_allowed_while_running(cmd))
      return -EBUSY;

   switch (cmd)
   {
      case ADS_IOCTL_CMD_RESET:
         ret = cmd_reset();
         break;
      case ADS_IOCTL_CMD_STANDBY:
         ret = ads131m08_run_cmd(ADC_STOP);
         break;
      case ADS_IOCTL_CMD_WAKEUP:
         ret = ads131m08_run_cmd(ADC_RUN);
         break;
      case ADS_IOCTL_CMD_LOCK:
         ret = ads131m08_cmd_lock();
         break;
      case ADS_IOCTL_CMD_UNLOCK:
         ret = ads131m08_cmd_unlock();
         break;
      case ADS_IOCTL_CMD_RREG:
         ret = ioctl_rreg(arg);
         break;
      case ADS_IOCTL_CMD_WREG:
         ret = ioctl_wreg(arg);
         break;
      case ADS_IOCTL_CMD_CHUNK_SZ:
         ret = ioctl_chunk_sz(arg);
         break;
      case ADS_IOCTL_CMD_STAT:
         ret = cmd_stat(arg);
         break;
      default:
         break;
   }

   return ret;
}

static struct file_operations ads131m08_fops =
{
   .owner = THIS_MODULE,
   .open = ads131m08_open,
   .release = ads131m08_release,
   .read = ads131m08_read,
   .write = ads131m08_write,
   .poll = ads131m08_poll,
   .unlocked_ioctl = ads131m08_ioctl,
};

static int fops_init(ads131m08_data_t *data)
{
   int ret = FAIL;
   struct cdev *cdev_ptr = &data->cdev;
   dev_t dev_num;
   struct device *dev_ret;

   ret = alloc_chrdev_region(&dev_num, 0, 1, ads131m08_driver_name);
   if (ret < 0)
   {
      pr_err("%s:Failed to allocate char device region\n", ads131m08_dev_name);
      return ret;
   }

   data->dev_num = dev_num;
   cdev_init(cdev_ptr, &ads131m08_fops);
   cdev_ptr->owner = THIS_MODULE;

   ret = cdev_add(cdev_ptr, dev_num, 1);
   if (ret < 0)
   {
      pr_err("%s:Failed to add cdev\n", ads131m08_dev_name);
      unregister_chrdev_region(dev_num, 1);
      return ret;
   }

   data->cl = class_create(ads131m08_class_name);
   if (IS_ERR(data->cl))
   {
      pr_err("%s:Failed to create class\n", __func__);
      ret = PTR_ERR(data->cl);
      cdev_del(cdev_ptr);
      unregister_chrdev_region(dev_num, 1);
      return ret;
   }

   dev_ret = device_create(data->cl, NULL, dev_num, NULL, ads131m08_dev_name);
   if (IS_ERR(dev_ret))
   {
      ret = PTR_ERR(dev_ret);
      class_destroy(data->cl);
      cdev_del(cdev_ptr);
      unregister_chrdev_region(dev_num, 1);
      return ret;
   }

   return SUCCESS;
}

static void fops_exit(ads131m08_data_t *data)
{
   if (!data || !data->cl)
      return;

   device_destroy(data->cl, data->dev_num);
   class_destroy(data->cl);
   cdev_del(&data->cdev);
   unregister_chrdev_region(data->dev_num, 1);
}

/////////////////////////////////////////////////////////////////
int ads131m08_iface_init(ads131m08_data_t *data)
{
   atomic_set(&open_cnt, 0);

   if (proc_file_init() < 0)
      return FAIL;

   if (fops_init(data) < 0)
   {
      proc_file_exit();
      return FAIL;
   }
   return SUCCESS;
}

void ads131m08_iface_exit(ads131m08_data_t *data)
{
   proc_file_exit();
   fops_exit(data);
}
