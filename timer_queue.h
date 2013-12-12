/***************************************************************************
                          timer_queue.h  -  description
                             -------------------
    begin                : Mon Jul 14 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef TIMER_QUEUE_H
#define TIMER_QUEUE_H


#include "aodv.h"


void timer_queue_signal(void);
int init_timer_queue(void);
void update_timer_queue(void);
int read_timer_queue_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data);

int insert_timer_simple(u_int8_t task_type, u_int64_t delay, u_int32_t ip);
int insert_timer_directional(u_int8_t task_type, u_int64_t delay, u_int8_t retries, u_int32_t src_ip, u_int32_t dst_ip,
		unsigned char tos);
int find_timer(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos, u_int8_t type);
void delete_timer(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos, u_int8_t type);


#endif
