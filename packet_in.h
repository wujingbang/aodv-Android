/***************************************************************************
                          packet_in.h  -  description
                             -------------------
    begin                : Mon Jul 29 2002
    copyright            : (C) 2002 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef PACKET_IN_H
#define PACKET_IN_H

#include "aodv.h"

unsigned int input_handler(unsigned int hooknum, struct sk_buff *skb,
                           const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *));


#endif
