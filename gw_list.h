/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef GW_LIST_H
#define GW_LIST_H

#include "aodv.h"


int update_gw(u_int32_t gw_ip, u_int32_t dst_id, u_int8_t num_hops, u_int32_t path_metric);
void init_gw_list(void);
void delete_gw_list(void);
int is_gw(u_int32_t ip);
int read_gw_list(char *buffer, char **buffer_location, off_t offset,
		 int buffer_length, int *eof, void *data);

void update_gw_lifetimes(void);

#endif