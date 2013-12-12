 /***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef PROBE_ETT_H
#define PROBE_ETT_H

#include "aodv.h"


void send_probe(u_int32_t ip);
void recv_sprobe(task * tmp_packet);
void recv_lprobe(task * tmp_packet);
void send_ett_info (aodv_neigh *tmp_neigh, u_int16_t my_rate, int fixed);
void recv_ett_info (task * tmp_packet);
void delay_vector_init(u_int32_t *r_delays);
void reset_ett(aodv_neigh *reset_neigh);
#endif
