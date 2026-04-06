/*
 *      Intelligent Light Control System
 *      Copyright (c) 2002 NVERGENCE, Inc.
 *      All right reserved.
 *
 *      source come from uC-lib
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "list.h"

/***************************************************
 * MACRO
 */
//#define	LIST_LOCK(listp)	pthread_mutex_lock(&listp->lock)
//#define	LIST_UNLOCK(listp)	pthread_mutex_unlock(&listp->lock)
//#define	LIST_LOCK(listp)
//#define	LIST_UNLOCK(listp)

/***************************************************
 * FUNCTION DEFINITIONS
 */

/*********************************************************************
 * @fn      list_append
 *
 * @brief   Append list element to list
 *
 * @param   list_t *head
 * @param   list_elm_t *elm
 *
 * @return  None
 */
void list_append(list_t *list,void *elm)
{
   list_elm_t *elem;
   if(list==NULL || elm==NULL)
	return;
   elem = (list_elm_t *)elm;
   elem->parent = list;
  
   LIST_LOCK(list); 
   if(list->head==NULL)
   {
      elem->prev = elem->next = NULL;
      list->head = list->tail = elem;
   }
   else
   {
      elem->prev = list->tail;
      elem->prev->next = elem;
      elem->next = NULL;
      list->tail = elem;
   }
   list->count++;
   LIST_UNLOCK(list);
}

void list_insert(list_t *list,void *prev_elem,void *elem)
{
   list_elm_t *prev_elm,*elm;
   if(list==NULL || elem==NULL)
      return;
   prev_elm = (list_elm_t *)prev_elem;
   elm = (list_elm_t *)elem;
   elm->parent = list;
   LIST_LOCK(list);
   if(list->head==NULL)
   {
      elm->prev = elm->next = NULL;
      list->head = list->tail = elm;
   }
   else 
   {
      if(prev_elm==NULL)
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
	 if(elm->next==NULL)
         {
	    list->tail = elm;
         }
         else
         {
            elm->next->prev = elm;
         }
      }
   }
   list->count++;
   LIST_UNLOCK(list);
}

void list_insert_sort(list_t *list,void *new,int (*comp_func)(void *next,void *new))
{
   void *prev,*next,*last;

   prev=NULL;

   if((last=list_get_last_elm) && comp_func(last,new)==FALSE)
   {
      list_append(list,new);
      return;
   }

   while((next=list_iter(list,prev)))
   {
      if(comp_func(next,new))
      {
         list_insert(list,prev,new);
         return;
      }
      prev = next;
   }
   list_append(list,new);
}

void *list_fetch(list_t *list)
{
   list_elm_t *ret;
   if(list==NULL)
      return NULL;
   LIST_LOCK(list);
   ret = list->head;
   if(ret)
   {
      list->head = ret->next;
      if(list->head)
	  list->head->prev = NULL;
      else
	 list->tail = NULL;
      list->count--;
      ret->parent = NULL;
   }
   LIST_UNLOCK(list);
   return ret;
}



/*********************************************************************
 * @fn      search_list
 *
 * @brief   Search list element
 *
 * @param   list_t *head
 * @param   list_elm_t *elm
 *
 * @return  None
 */
void *list_search(list_t *list,void *arg,
			int comp_func(void *arg,void *elm))
{
   list_elm_t *next;

   LIST_LOCK(list);
   next = list->head;
   while(next)
   {
      if(comp_func(arg,next)==TRUE)
	  break;
      next = next->next;
   }
   LIST_UNLOCK(list);
   return next;
}

void *list_get_idx(list_t *list,int idx)
{
   int cnt;
   list_elm_t *next=NULL;
   LIST_LOCK(list);
   if(idx<list->count)
   {
      next = list->head;
      cnt = idx;
      while(cnt)
      {
	 next = next->next;
	 cnt--;
      }
   }
   LIST_UNLOCK(list);
   return next;
}

/*********************************************************************
 * @fn      list_iter
 *
 * @brief   List iteration
 *
 * @param   list_t *list
 * @param   list_elm_t *prev
 *
 * @return  list_elm_t *next
 */
void *list_iter(list_t *list,void *prev)
{
   if(prev==NULL)
	return list->head;
   return ((list_elm_t *)prev)->next;
}

void *list_get_last_elm(list_t *list)
{
   return list->tail;
}

void *list_get_first_elm(list_t *list)
{
   return list->head;
}

/*********************************************************************
 * @fn      delete_list
 *
 * @brief   Delete list element
 *
 * @param   list_elm_t *elm
 *
 * @return  None
 */
void list_del_elm(void *elem,int free_elm)
{
   list_t *list;
   list_elm_t *elm = (list_elm_t *)elem;
   if(elm==NULL)
      return;
   list = elm->parent;
   if(list==NULL)
   {
      free(elm);
      return;
   }

   LIST_LOCK(list);
   if(elm->prev)
   {
      elm->prev->next = elm->next;
   }
   else // it's head element
   {
      list->head = elm->next;
   }
   if(elm->next)
   {
      elm->next->prev = elm->prev;
   }
   else  // it's tail element
   {
      list->tail = elm->prev;
   }
   elm->prev = elm->next = NULL;
   list->count--;
   LIST_UNLOCK(list);

   if(free_elm)
     free(elm);
   return;
}

void list_iter_do(list_t *list,void (*action)(void *args))
{
   list_elm_t *elm,*next;

   LIST_LOCK(list);
   elm = list->head;
   while(elm)
   {
      next = elm->next;
      action(elm);
      elm = next;
   }
   list->head = list->tail = 0;
   LIST_UNLOCK(list);
   return;
}

/*********************************************************************
 * @fn      delete_all_list
 *
 * @brief   Delete all list element
 *
 * @param   list_t *list
 *
 * @return  None
 */
void list_cleanup(list_t *list,void (*free_elm)(void *args))
{
   void (*free_func)(void *args);
   free_func = (free_elm?free_elm:free);
   LIST_LOCK(list);
   list->count = 0;
   LIST_UNLOCK(list);

   list_iter_do(list,free_func);
   return;
}

void list_purge(list_t *list,void (*free_elm)(void *args))
{
   list_cleanup(list,free_elm);
   free(list);
}


/*********************************************************************
 * @fn      alloc_list_elm
 *
 * @brief   Allocation list element
 *
 * @param   int size
 *
 * @return  void (*arg)
 */
void *list_alloc_elm(int size)
{
   list_elm_t *elm;

   elm = (list_elm_t *)calloc(1,size);
   if(elm==NULL)
	return NULL;
   elm->prev = elm->next = NULL;
   elm->parent = NULL;
   pthread_mutex_init(&elm->lock,NULL);
   return elm;
}

void list_lock(list_t *list)
{
   if(list==NULL)
	return;
   pthread_mutex_lock(&list->lock);
}

void list_unlock(list_t *list)
{
   if(list==NULL)
	return;
   pthread_mutex_unlock(&list->lock);
}

int list_get_cnt(list_t *list)
{
   int ret;
   if(!list)
      return 0;
   LIST_LOCK(list);
   ret = list->count;
   LIST_UNLOCK(list);
   return ret;
}

list_t *list_new(void)
{
   list_t *new;
   new = (list_t *)calloc(1,sizeof(list_t));
   pthread_mutex_init(&new->lock,NULL);
   return new;
}

void list_print(list_t *list)
{
   void *node=NULL;
   printf("LIST<%X,%d>:",list,list_get_cnt(list));
   while((node=list_iter(list,node)))
   {
      printf("[%X]=>",node);
   }
   printf("\n");
}
