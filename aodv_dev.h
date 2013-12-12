/***************************************************************************
                          aodv_dev.h  -  description
                             -------------------
    begin                : Thu Aug 7 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef AODV_DEV_H
#define AODV_DEV_H

#include <linux/netdevice.h> 
/* struct net_device */
#include <asm/uaccess.h>
#include <linux/if.h>
#include <linux/rtnetlink.h>

/*#include <linux/types.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>*/

#include "aodv.h"

int init_aodv_dev(char *name);
aodv_dev *find_aodv_dev_by_dev(struct net_device *dev);
void free_dev(void);


#endif
