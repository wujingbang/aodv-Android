/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
//MCC - ST_RREQ

#include "st_rreq.h"

extern u_int8_t g_aodv_gateway;
extern u_int32_t g_broadcast_ip;
extern u_int8_t g_hopcnt_routing;
extern u_int32_t g_null_ip;
extern u_int8_t g_routing_metric;
extern u_int32_t g_mesh_ip;

void convert_rreq_to_host_st(rreq_st * tmp_rreq) {
	tmp_rreq->dst_id = ntohl(tmp_rreq->dst_id);
	tmp_rreq->path_metric = ntohl(tmp_rreq->path_metric);
}

void convert_rreq_to_network_st(rreq_st * tmp_rreq) {
	tmp_rreq->dst_id = htonl(tmp_rreq->dst_id);
	tmp_rreq->path_metric = htonl(tmp_rreq->path_metric);
}

/*
 * Generates a ST-RREQ
 */

int gen_st_rreq(void) {
	aodv_route *tmp_route;
	rreq_st *out_rreq;
	u_int8_t out_ttl;

	if (!g_aodv_gateway) {
#ifdef DEBUG
		printk( "kernel_aodv: Shouldn't be sending st_rreq when not gateway\n");
#endif
		return 0;
	}

	// Allocate memory for the rreq message

	if ((out_rreq = (rreq_st *) kmalloc(sizeof(rreq_st), GFP_ATOMIC)) == NULL) {
#ifdef DEBUG
		printk( "AODV: Can't allocate new rreq\n");
#endif
		return 0;
	}

	tmp_route = find_aodv_route(g_mesh_ip, g_mesh_ip, 0);
	if (tmp_route == NULL) {
#ifdef DEBUG
		printk( "AODV: Can't get route to myself!!\n");
#endif
		kfree(out_rreq);
		return -EHOSTUNREACH;
	}

	tmp_route->dst_id = tmp_route->dst_id +1;
	out_rreq->dst_id = tmp_route->dst_id;
	out_rreq->u = TRUE;
	out_ttl = NET_DIAMETER;

	// Fill in the package
	out_rreq->gw_ip = g_mesh_ip;
	out_rreq->type = ST_RREQ_MESSAGE;
	out_rreq->num_hops = 0;
	out_rreq->path_metric = 0;
	out_rreq->j = 0;
	out_rreq->r = 0;
	out_rreq->d = 1;
	out_rreq->reserved = 1;
	out_rreq->second_reserved = 0; // set to 1 for hopcnt routing
	out_rreq->g = 0;

	convert_rreq_to_network_st(out_rreq);

	local_broadcast(out_ttl, out_rreq, sizeof(rreq_st));

	kfree(out_rreq);
	insert_timer_simple(TASK_ST, ST_INTERVAL, g_mesh_ip);
update_timer_queue();
	return 0;
}

int recv_rreq_st(task * tmp_packet) {
	rreq_st *tmp_rreq;
	aodv_neigh *tmp_neigh;

	tmp_rreq = tmp_packet->data;
	convert_rreq_to_host_st(tmp_rreq);

	tmp_neigh = find_aodv_neigh(tmp_packet->src_ip);

	if (tmp_neigh == NULL) {
#ifdef DEBUG
		printk( "Ignoring RREQ_BG received from unknown neighbor\n");
#endif
		return 1;
	}
	
	//Update neighbor timelife
	delete_timer(tmp_neigh->ip, tmp_neigh->ip, NO_TOS, TASK_NEIGHBOR);
		insert_timer_simple(TASK_NEIGHBOR, HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 100, tmp_neigh->ip);
		update_timer_queue();
		tmp_neigh->lifetime = HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 20
				+ getcurrtime();

	if ((g_mesh_ip == tmp_rreq->gw_ip)) {
#ifdef DEBUG
		printk("I'm the generator of the incoming ST-RREQ - Discarted\n");
#endif
		return 1;
	}

	tmp_rreq->num_hops++;

	if (g_routing_metric == HOPS)
		tmp_rreq->path_metric = tmp_rreq->num_hops;
	else {
		if (tmp_neigh->recv_rate == 0 || tmp_neigh->send_rate == 0 || tmp_neigh->etx_metric == 0){
#ifdef DEBUG
			printk("Metric too high - ST-RREQ Discarted\n");
#endif
			return 1;
		}		
			tmp_rreq->path_metric +=(PROBE_PACKET*1000)/(tmp_neigh->recv_rate*tmp_neigh->etx_metric); //ETT in microseconds	
	}

	if (update_gw(tmp_rreq->gw_ip, tmp_rreq->dst_id, tmp_rreq->num_hops,
			tmp_rreq->path_metric)) {

		convert_rreq_to_network_st(tmp_rreq);
		local_broadcast(tmp_packet->ttl - 1, tmp_rreq, sizeof(rreq_st));
		return 0;
	}

	return 1;
}
