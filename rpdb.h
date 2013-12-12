/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef RPDB_H
#define RPDB_H

#include "aodv.h"


int rpdb_rule (unsigned char type, int rt_table, u_int32_t ip_src, u_int32_t ip_dst);
int rpdb_route (unsigned char type, int rt_table, unsigned char tos, u_int32_t ip_src, u_int32_t ip_dst, u_int32_t ip_gw, int dev_index, int metric);

#endif
