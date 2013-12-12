/***************************************************************************
                          rrep.h  -  description
                             -------------------
    begin                : Wed Aug 6 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef RREP_H
#define RREP_H

#include "aodv.h"

int recv_rrep(task * tmp_packet);
int gen_rrep(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos);


#endif
