/*
 *      Intelligent Light Control System
 *      Copyright (c) 2002 NVERGENCE, Inc.
 *      All right reserved.
 *
 *      source come from uC-lib
 *
 */
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "list.h"
#include "ads131m08.h"

void list_append(list_t *list,void *elm)
{
   list_elm_t *elem;

   if (!list || !elm)
      return;

   elem = (list_elm_t *)elm;
   elem->parent = list;
   elem->next = NULL;

   LIST_LOCK(list);
   if (list->head == NULL)
   {
      elem->prev = NULL;
      list->head = list->tail = elem;
   }
   else
   {
      elem->prev = list->tail;
      elem->prev->next = elem;
      list->tail = elem;
   }
   list->count++;
   LIST_UNLOCK(list);
}

void list_insert(list_t *list,void *prev_elem,void *elem)
{
   list_elm_t *prev_elm, *elm;

   if (!list || !elem)
      return;

   prev_elm = (list_elm_t *)prev_elem;
   elm = (list_elm_t *)elem;
   elm->parent = list;

   LIST_LOCK(list);
   if (list->head == NULL)
   {
      elm->prev = elm->next = NULL;
      list->head = list->tail = elm;
   }
   else if (prev_elm == NULL)
   {
      elm->prev = NULL;
      elm->next = list->head;
      list->head->prev = elm;
      list->head = elm;
   }
   else
   {
      elm->prev = prev_elm;
      elm->next = prev_elm->next;
      prev_elm->next = elm;
      if (elm->next == NULL)
         list->tail = elm;
      else
         elm->next->prev = elm;
   }
   list->count++;
   LIST_UNLOCK(list);
}

void list_insert_sort(list_t *list,void *new,int (*comp_func)(void *next,void *new))
{
   void *prev, *next, *last;

   if (!list || !new || !comp_func)
      return;

   prev = NULL;
   last = list_get_last_elm(list);
   if (last && !comp_func(last, new))
   {
      list_append(list, new);
      return;
   }

   while ((next = list_iter(list, prev)))
   {
      if (comp_func(next, new))
      {
         list_insert(list, prev, new);
         return;
      }
      prev = next;
   }

   list_append(list, new);
}

void *list_fetch(list_t *list)
{
   list_elm_t *ret;

   if (!list)
      return NULL;

   LIST_LOCK(list);
   ret = list->head;
   if (ret)
   {
      list->head = ret->next;
      if (list->head)
         list->head->prev = NULL;
      else
         list->tail = NULL;

      list->count--;
      ret->parent = NULL;
      ret->prev = NULL;
      ret->next = NULL;
   }
   LIST_UNLOCK(list);
   return ret;
}

void *list_search(list_t *list,void *arg,
				int comp_func(void *arg,void *elm))
{
   list_elm_t *next;

   if (!list || !comp_func)
      return NULL;

   LIST_LOCK(list);
   next = list->head;
   while (next)
   {
      if (comp_func(arg, next))
         break;
      next = next->next;
   }
   LIST_UNLOCK(list);
   return next;
}

void *list_get_idx(list_t *list,int idx)
{
   int cnt;
   list_elm_t *next = NULL;

   if (!list || idx < 0)
      return NULL;

   LIST_LOCK(list);
   if (idx < list->count)
   {
      next = list->head;
      cnt = idx;
      while (cnt-- && next)
         next = next->next;
   }
   LIST_UNLOCK(list);
   return next;
}

void *list_iter(list_t *list,void *prev)
{
   if (!list)
      return NULL;
   if (prev == NULL)
      return list->head;
   return ((list_elm_t *)prev)->next;
}

void *list_get_last_elm(list_t *list)
{
   if (!list)
      return NULL;
   return list->tail;
}

void *list_get_first_elm(list_t *list)
{
   if (!list)
      return NULL;
   return list->head;
}

void list_del_elm(void *elem,int free_elm)
{
   list_t *list;
   list_elm_t *elm = (list_elm_t *)elem;

   if (!elm)
      return;

   list = elm->parent;
   if (!list)
   {
      if (free_elm)
         kfree(elm);
      return;
   }

   LIST_LOCK(list);
   if (elm->prev)
      elm->prev->next = elm->next;
   else
      list->head = elm->next;

   if (elm->next)
      elm->next->prev = elm->prev;
   else
      list->tail = elm->prev;

   elm->prev = NULL;
   elm->next = NULL;
   elm->parent = NULL;
   list->count--;
   LIST_UNLOCK(list);

   if (free_elm)
      kfree(elm);
}

void list_iter_do(list_t *list,void (*action)(const void *args))
{
   list_elm_t *elm, *next;

   if (!list || !action)
      return;

   LIST_LOCK(list);
   elm = list->head;
   while (elm)
   {
      next = elm->next;
      action(elm);
      elm = next;
   }
   list->head = NULL;
   list->tail = NULL;
   list->count = 0;
   LIST_UNLOCK(list);
}

void list_cleanup(list_t *list,void (*free_elm)(const void *args))
{
   list_elm_t *elm;
   void (*free_func)(const void *args);

   if (!list)
      return;

   free_func = free_elm ? free_elm : kfree;
   while ((elm = list_fetch(list)))
      free_func(elm);
}

void *list_alloc_elm(int size)
{
   list_elm_t *elm;

   elm = (list_elm_t *)kzalloc(size, GFP_KERNEL);
   if (!elm)
      return NULL;

   elm->prev = NULL;
   elm->next = NULL;
   elm->parent = NULL;
   spin_lock_init(&elm->lock);
   return elm;
}

void list_lock(list_t *list)
{
   if (!list)
      return;
   spin_lock(&list->lock);
}

void list_unlock(list_t *list)
{
   if (!list)
      return;
   spin_unlock(&list->lock);
}

int list_get_cnt(list_t *list)
{
   int ret;

   if (!list)
      return 0;

   LIST_LOCK(list);
   ret = list->count;
   LIST_UNLOCK(list);
   return ret;
}

list_t *list_new(struct device *dev)
{
   list_t *new;

   new = (list_t *)devm_kzalloc(dev, sizeof(list_t), GFP_KERNEL);
   if (!new)
      return NULL;

   spin_lock_init(&new->lock);
   new->count = 0;
   new->head = NULL;
   new->tail = NULL;
   return new;
}

void list_print(list_t *list)
{
   void *node = NULL;
   char print_buf[512];
   int idx = 0;

   idx = snprintf(print_buf, sizeof(print_buf), "LIST<%p,%d>:", list, list_get_cnt(list));
   while ((node = list_iter(list, node)) && idx < sizeof(print_buf))
      idx += scnprintf(&print_buf[idx], sizeof(print_buf) - idx, "[%p]=>", node);

   pr_info("%s\n", print_buf);
}
