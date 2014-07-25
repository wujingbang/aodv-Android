/***************************************************************************
                          rcvp.h  -  description
                             -------------------
    begin                : Thu May 15 2014
    copyright            : (C) 2014 by Cai Bingying
    email                :
 ***************************************************************************/
//#ifdef RECOVERYPATH
#ifndef RCVP_H
#define RCVP_H

#include "aodv.h"

int gen_rcvp(u_int32_t des_ip);
int recv_rcvp(task *tmp_packet);

int gen_rrdp(u_int32_t src_ip,u_int32_t dst_ip,u_int32_t last_hop,unsigned char tos);
int recv_rrdp(task *tmp_packet);

#endif
//#endif
