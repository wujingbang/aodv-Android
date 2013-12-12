 /***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef TOS_H
#define TOS_H

#include "aodv.h"

void init_flow_type_table(void);
int read_flow_type_table_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data);
flow_type *find_flow_type(unsigned char tos);
void flush_flow_type_table(void);

#endif



