/***************************************************************************
                          rerr.h  -  description
                             -------------------
    begin                : Mon Aug 11 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef RERR_H
#define RERR_H

#include "aodv.h"

int gen_rerr(u_int32_t brk_dst_ip);
int recv_rerr(task * tmp_packet);

#endif
