/***************************************************************************
                          packet_out.h  -  description
                             -------------------
    begin                : Mon Aug 4 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef PACKET_OUT_H
#define PACKET_OUT_H

#include "aodv.h"
#include <linux/netdevice.h>



unsigned int output_handler(unsigned int hooknum, struct sk_buff *skb,
                            const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *));
unsigned int prerouting_handler(unsigned int hooknum, struct sk_buff *skb,
                            const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *));
#endif
