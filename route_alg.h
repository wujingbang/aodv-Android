/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef ROUTE_ALG_H
#define ROUTE_ALG_H

#include "aodv.h"

u_int8_t compute_coef(u_int16_t size, u_int16_t rate);
int wcim_metric(aodv_neigh *tmp_neigh, rreq *tmp_rreq);
int ett_metric(aodv_neigh *tmp_neigh, rreq *tmp_rreq);
#endif
