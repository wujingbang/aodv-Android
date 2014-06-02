/***************************************************************************
                          rcvp.h  -  description
                             -------------------
    begin                : Thu May 15 2014
    copyright            : (C) 2014 by Cai Bingying
    email                :
 ***************************************************************************/

#ifndef RCVP_H
#define RCVP_H

#include "aodv.h"

int gen_rcvp(u_int32_t des_ip);
int recv_rcvp(task * tmp_packet);

#endif
