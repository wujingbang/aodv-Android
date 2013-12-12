/***************************************************************************
 rerr.c  -  description
 -------------------
 begin                : Mon Aug 11 2003
 copyright            : (C) 2003 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#include "rerr.h"

extern u_int32_t g_mesh_ip;
extern u_int32_t g_null_ip;
extern u_int8_t g_aodv_gateway;
extern u_int8_t g_routing_metric;

int reply_to_rrer(u_int32_t source, u_int32_t destination) {

	
	if (source == g_mesh_ip || (source == g_null_ip && g_aodv_gateway))
		return 1;

	return 0;
}


int gen_rerr(u_int32_t brk_dst_ip) {
	aodv_route *tmp_route;
	rerr *tmp_rerr;
	int expired_routes = 0;
	
	tmp_route = first_aodv_route();

	//go through list
	while (tmp_route != NULL) {

		if ((tmp_route->next_hop == brk_dst_ip) && !tmp_route->self_route
				&& !tmp_route->neigh_route) {

			if (!reply_to_rrer(tmp_route->src_ip, tmp_route->dst_ip)) { //i'm source of the route, don't send the rerr
				if (tmp_route->state != INVALID) { 

					if ((tmp_rerr = (rerr *) kmalloc(sizeof(rerr), GFP_ATOMIC))
							== NULL) {
#ifdef DEBUG
						printk("Can't allocate new rrep\n");
#endif
						return 0;
					}

					tmp_rerr->type = RERR_MESSAGE;
					tmp_rerr->dst_count = 0; //unused
					tmp_rerr->reserved = 0;
					tmp_rerr->n = 0;
					tmp_rerr->num_hops = 0;
					tmp_rerr->dst_ip = tmp_route->dst_ip;
					tmp_rerr->dst_id = htonl(tmp_route->dst_id);
					send_message(tmp_route->last_hop, NET_DIAMETER, tmp_rerr,
							sizeof(rerr));
					kfree(tmp_rerr);

				}

			}
			if (tmp_route->state != INVALID){
				expire_aodv_route(tmp_route);
			expired_routes++;
			}
		}
		//move on to the next entry
		tmp_route = tmp_route->next;

	}
	if (g_routing_metric == WCIM && expired_routes != 0)
	update_my_load();
	
	return 0;
}

int recv_rerr(task * tmp_packet) {
	aodv_route *tmp_route;
	rerr *tmp_rerr;
	int num_hops;
	int expired_routes = 0;
	
	tmp_rerr = (rerr *) tmp_packet->data;
	num_hops = tmp_rerr->num_hops+1;
#ifdef DEBUG
	printk("Recieved a route error from %s", inet_ntoa(tmp_packet->src_ip));
#endif

	tmp_route
			= find_aodv_route_by_id(tmp_rerr->dst_ip, ntohl(tmp_rerr->dst_id));

	if (tmp_route && tmp_route->next_hop == tmp_packet->src_ip) {

		if (!reply_to_rrer(tmp_route->src_ip, tmp_route->dst_ip)) {
			if (tmp_route->state != INVALID) { //route with active traffic

				tmp_rerr->type = RERR_MESSAGE;
				tmp_rerr->dst_count = 0; //unused
				tmp_rerr->reserved = 0;
				tmp_rerr->n = 0;
				tmp_rerr->num_hops = num_hops;
				tmp_rerr->dst_ip = tmp_route->dst_ip;
				tmp_rerr->dst_id = htonl(tmp_route->dst_id);
				send_message(tmp_route->last_hop, NET_DIAMETER, tmp_rerr,
						sizeof(rerr));

			}

		}
		if (tmp_route->state != INVALID) {
			expire_aodv_route(tmp_route);
			expired_routes++;
		}

	}
	if (g_routing_metric == WCIM && expired_routes != 0)
	update_my_load();
		

	return 0;

}
