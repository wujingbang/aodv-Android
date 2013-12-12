/***************************************************************************
                          socket.h  -  description
                             -------------------
    begin                : Wed Aug 13 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef SOCKET_H
#define SOCKET_H


#include "aodv.h"

int init_sock(struct socket *sock, u_int32_t ip, char *dev_name);
void close_sock(void);
int local_broadcast(u_int8_t ttl, void *data, const size_t datalen);
int send_message(u_int32_t dst_ip, u_int8_t ttl, void *data, const size_t datalen);
int send_ett_probe(u_int32_t dst_ip, void *data1, const size_t datalen1, void *data2, const size_t datalen2);


#endif
