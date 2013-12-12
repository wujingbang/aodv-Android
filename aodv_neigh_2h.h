/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef AODV_NEIGH_2H_H
#define AODV_NEIGH_2h_H


#include "aodv.h"


void init_aodv_neigh_list_2h(void);
void delete_aodv_neigh_list_2h(void);
int delete_aodv_neigh_2h(u_int32_t ip);
aodv_neigh_2h *find_aodv_neigh_2h(u_int32_t target_ip);
aodv_neigh_2h *create_aodv_neigh_2h(u_int32_t ip);

#endif