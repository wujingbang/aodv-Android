/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef SRC_LIST_H
#define SRC_LIST_H

#include "aodv.h"

void init_src_list(void);
void flush_src_list(void);
void delete_src_list_entry(u_int32_t ip);
src_list_entry *  insert_src_list_entry(u_int32_t ip);
src_list_entry* find_src_list_entry(u_int32_t ip);
int find_first_rttable(void);

int read_src_list_proc(char *buffer, char **buffer_location, off_t offset,
		 int buffer_length, int *eof, void *data);

#endif

