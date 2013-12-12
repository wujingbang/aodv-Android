/***************************************************************************
                          aodv_neigh.h  -  description
                             -------------------
    begin                : Thu Jul 31 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#ifndef AODV_NEIGH_H
#define AODV_NEIGH_H


#include "aodv.h"

void init_aodv_neigh_list(void);
void delete_aodv_neigh_list(void);
aodv_neigh *find_aodv_neigh(u_int32_t target_ip);
int read_neigh_proc(char *buffer, char **buffer_location, off_t offset, int buffer_length,int *eof,void *data);
int read_rel_list_proc(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data);
int read_ett_list_proc(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data);
int read_node_load_proc(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data);
aodv_neigh *first_aodv_neigh(void);
aodv_neigh *create_aodv_neigh(u_int32_t ip);
int delete_aodv_neigh(u_int32_t ip);
int route_aodv_neigh( u_int32_t ip);
void cleanup_neigh_routes(void);
void neigh_read_lock(void);
void neigh_read_unlock(void);
void neigh_write_lock(void);
void neigh_write_unlock(void);
#endif
