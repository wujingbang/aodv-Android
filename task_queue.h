/***************************************************************************
                          task_queue.h  -  description
                             -------------------
    begin                : Tue Jul 8 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/

/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H


#include "aodv.h"


task *get_task(void);
task *create_task(int type);
int insert_task(int type, struct sk_buff *packet);
int insert_task_from_timer(task * timer_task);
int insert_task_at_front(task * new_task);
void init_task_queue(void);
void cleanup_task_queue(void);

#endif
