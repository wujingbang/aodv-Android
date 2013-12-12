/***************************************************************************
                          hello.h  -  description
                             -------------------
    begin                : Wed Aug 13 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef HELLO_H
#define HELLO_H


#include "aodv.h"

int gen_hello(void);
int recv_hello(task * tmp_packet);
void compute_etx(void);
#endif
