/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef RREQ_H
#define RREQ_H


#include "aodv.h"

int recv_rreq(task * tmp_packet);
int gen_rreq(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos);
int resend_rreq(task * tmp_packet);

#endif
