/*********************************************************************
  Filename:       queue.h
  Revised:        Date: 2022/4/14
  Revision:       Revision: 1.0

  Description:
          - define queue functions

          u-Infraway,Inc.
  Notes:

  Copyright (c) 2022 by u-Infraway, Inc., All Rights Reserved.

*********************************************************************/

#ifndef QUEUE_H
#define QUEUE_H

/*****************************************
 * INCLUDES
 */
#include <linux/spinlock.h>

/*****************************************
 * TYPE DEFINES
 */
typedef struct
{
   int q_sz;
   int head;
   int tail;
   void **que;
   //struct mutex lock;
   spinlock_t lock;
} que_t;

/*****************************************
 * CONSTANT DEFINE
 */

/*****************************************
 * MACRO DEFINE
 */

/*****************************************
 * EXTERNAL FUNCTIONS
 */
extern void *que_new(int q_sz,struct device *dev);
extern void que_del(void *que);
extern int que_enq(void *que,void *item);
extern void *que_deq(void *que);
extern void *que_peek(void *que);
extern int que_isfull(void *que);
extern int que_isempty(void *que);
extern int que_chksz(void *que);
extern void que_cleanup(void *que);

#endif
