/***************************************************************************
 aodv_route.h  -  description
 -------------------
 begin                : Mon Jul 29 2002
 copyright            : (C) 2002 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef AODV_ROUTE_H
#define AODV_ROUTE_H

#include "aodv.h"

int read_route_table_proc(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data);

u_int8_t compute_load(u_int16_t rate_link, unsigned char tos);
void update_my_load(void);

void init_aodv_route_table(void);
int cleanup_aodv_route_table(void);
void expire_aodv_route(aodv_route * tmp_route);

int flush_aodv_route_table(void);

aodv_route *find_aodv_route(u_int32_t src_ip, u_int32_t dst_ip,
		unsigned char tos);
aodv_route *find_aodv_route_by_id(u_int32_t dst_ip, u_int32_t dst_id);

aodv_route *create_aodv_route(u_int32_t src_ip, u_int32_t dst_ip,
		unsigned char tos, u_int32_t dst_id);
aodv_route *first_aodv_route(void);

void remove_aodv_route(aodv_route * dead_route);
int rreq_aodv_route(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos,
		aodv_neigh *next_hop, u_int8_t metric, u_int32_t dst_id,
		struct net_device *dev, u_int32_t path_metric);
int rrep_aodv_route(aodv_route *tmp_route);

extern int sysctl(int *__name, int __nlen, void *__oldval, size_t *__oldlenp,
		void *__newval, size_t __newlen);

#endif

