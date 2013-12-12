/***************************************************************************
                          utils.h  -  description
                             -------------------
    begin                : Wed Jul 30 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef UTILS_H
#define UTILS_H

#include "aodv.h"
#include "aodv_dev.h"

u_int64_t getcurrtime(void);
char *inet_ntoa(u_int32_t ina);
int inet_aton(const char *cp, u_int32_t * addr);
u_int32_t calculate_netmask(int t);
int calculate_prefix(u_int32_t t);
int seq_greater(u_int32_t seq_one,u_int32_t seq_two);
int seq_less_or_equal(u_int32_t seq_one,u_int32_t seq_two);
int aodv_subnet_test(u_int32_t tmp_ip);

//MCC
int is_internal(u_int32_t dest_ip);
int char_compar(unsigned char tos1, unsigned char tos2);
int is_internet(u_int32_t dest_ip);
int get_nominalrate(int rate);

#endif
