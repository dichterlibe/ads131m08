/*********************************************************************
  Filename:       queue.c
  Revised:        Date: 2021/1/28
  Revision:       Revision: 1.0

  Description:
          - u-Infraway.,Inc 
          - FIFO queue implementation
  Notes:
        Copyright (c) 2022 by u-Infraway.,Inc.

*********************************************************************/

/***************************************************
 * INCLUDES
 */
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "ads131m08.h"
#include "queue.h"

/***************************************************
 * Struct defines
 */
//#define	Q_LOCK(q)	mutex_lock(&q->lock)
//#define	Q_UNLOCK(q)	mutex_unlock(&q->lock)
#define	Q_LOCK(q,flags)		spin_lock_irqsave(&q->lock,flags)
#define	Q_UNLOCK(q,flags)	spin_unlock_irqrestore(&q->lock,flags)
#define	LOCK_FLAGS		unsigned long flags;

/***************************************************
 * Functions defines
 */
#define	CALC_Q_SZ(head,tail,q_sz) ((head==tail?0:(tail>head?(tail-head):(q_sz-head+tail))))
static inline int calc_que_sz(que_t *que)
{
   int ret;
   if(que->head==que->tail) 
      ret=0;
   else if(que->tail>que->head)
      ret=(que->tail-que->head);
   else
      ret=(que->q_sz-que->head+que->tail);
   return ret;
}
static inline int is_full(que_t *que)
{
   if(calc_que_sz(que)==que->q_sz)
      return true;
   return false;
}
static inline int is_empty(que_t *que)
{
   return calc_que_sz(que)==0?true:false;
}

void *que_new(int q_sz,struct device *dev)
{
   que_t *new;
   new = (que_t *)devm_kzalloc(dev,sizeof(que_t),GFP_KERNEL);
   new->q_sz = q_sz;
   new->que = (void **)devm_kzalloc(dev,sizeof(void *) * q_sz,GFP_KERNEL);
   //mutex_init(&new->lock);
   spin_lock_init(&new->lock);
   return new;
}

void que_del(void *que)
{
   que_t *q = (que_t *)que;
   kfree(q->que);
   kfree(q);
}

int que_enq(void *que,void *item)
{
   int ret = FAIL;
   LOCK_FLAGS
   que_t *q = (que_t *)que;

   Q_LOCK(q,flags);
   if(!is_full(q))
   {
      q->que[q->tail++] = item;
      ret = SUCCESS;
   }
   //pr_info("ENQ:queue head:%d,tail:%d,sz=%d\n",q->head,q->tail,calc_que_sz(q));
   Q_UNLOCK(q,flags);
   return ret;
}

void *que_deq(void *que)
{
   que_t *q = (que_t *)que;
   void *ret=NULL;
   LOCK_FLAGS

   Q_LOCK(q,flags);
   if(!is_empty(q))
   {
      ret=q->que[q->head++];
      if(q->tail==q->q_sz)
	 q->tail=0;
      if(q->head==q->q_sz)
	q->head = 0;
      //pr_info("DEQ:queue head:%d,tail:%d,sz=%d\n",q->head,q->tail,calc_que_sz(q));
   }
   Q_UNLOCK(q,flags);
   return ret;
}

void *que_peek(void *que)
{
   que_t *q = (que_t *)que;
   void *ret=NULL;
   LOCK_FLAGS
   Q_LOCK(q,flags);
   if(!is_empty(q))
   {
      ret = q->que[q->head];
   }
   Q_UNLOCK(q,flags);
   return ret;
}

int que_isfull(void *que)
{
   int ret;
   LOCK_FLAGS
   que_t *q = (que_t *)que;
   Q_LOCK(q,flags);
   ret = is_full(q);
   Q_UNLOCK(q,flags);
   return ret;
}

int que_isempty(void *que)
{
   int ret;
   LOCK_FLAGS
   que_t *q = (que_t *)que;
   Q_LOCK(q,flags);
   ret = is_empty(q);
   Q_UNLOCK(q,flags);
   return ret;
}

int que_chksz(void *que)
{
   int ret;
   LOCK_FLAGS
   que_t *q = (que_t *)que;
   Q_LOCK(q,flags);
   ret = calc_que_sz(q);
   Q_UNLOCK(q,flags);
   return ret;
}

void que_cleanup(void *que)
{
   void *ret;
   while((ret=que_deq(que)))
   {
      kfree(ret);
   }
}

//////////////////////////////////////////////////////////////////////////
