/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "route_alg.h"

extern aodv_dev* g_mesh_dev;
extern aodv_neigh *aodv_neigh_list;
extern aodv_neigh_2h *aodv_neigh_list_2h;

int ett_metric(aodv_neigh *tmp_neigh, rreq *tmp_rreq) {
	
	if (tmp_neigh->recv_rate == 0 || tmp_neigh->send_rate == 0  || tmp_neigh->etx_metric == 0)
			return 1;
	
	tmp_rreq->path_metric +=(PROBE_PACKET*1000)/(tmp_neigh->recv_rate*tmp_neigh->etx_metric); //ETT in microseconds
	return 0;
}

u_int8_t compute_coef(u_int16_t size, u_int16_t rate) {

	u_int32_t other_delays = 122*100; //122 usecs (DIFS, SIFS, ACK)
	u_int32_t headers = 470*1000; //470 bits
	u_int32_t nominal_delay = size*8000;
	u_int32_t coef = nominal_delay + headers;
	u_int32_t u_rate = rate;

	if (size == 0 || rate == 0)
		return 0;

	nominal_delay = nominal_delay/u_rate;
	coef= coef/u_rate;
	coef += other_delays;
	coef = (nominal_delay*100)/coef;
	return (u_int8_t)coef;
}

int wcim_metric(aodv_neigh *tmp_neigh, rreq *tmp_rreq) {

	int i, alpha;
	u_int8_t coef_recv;
	u_int8_t load;
	u_int16_t recv_bw;
	flow_type *flow;
	aodv_neigh *load_neigh;
	aodv_neigh_2h *load_neigh_2h;
	
	if (tmp_neigh->recv_rate == 0 || tmp_neigh->send_rate == 0  || tmp_neigh->etx_metric == 0)
			return 1;
	
	flow = find_flow_type(tmp_rreq->tos);
	coef_recv = compute_coef(flow->avg_size, tmp_neigh->recv_rate);
	recv_bw = (tmp_neigh->recv_rate*tmp_neigh->etx_metric*coef_recv)/100; //in Kbps
	
	load = g_mesh_dev->load; //my own load
	
	load_neigh = aodv_neigh_list;
	while (load_neigh != NULL){  //my one-hop neighbors
		
		if (load_neigh->ip == tmp_neigh->ip) 
			load += tmp_neigh->load_metric.load;//load of my neighbor
		else{	
			alpha = 2; //hidden node?
			for (i=0; i< NEIGH_TABLESIZE; i++) {
				if (tmp_neigh->load_metric.neigh_tx[i] != 0) {
					if (tmp_neigh->load_metric.neigh_tx[i] == load_neigh->ip) { //it's also a neighbor of the transmitter
						alpha = 1;
						break;
					}
				}
			}
			//alpha = 1 if neigbor of the transmitter (break)
			//alpha = 2 if hidden node (not found in load_metric.neigh_tx[i])
			load+=load_neigh->load_metric.load*alpha; 
		}
		
		load_neigh = load_neigh->next;	
	}
	
	load_neigh_2h = aodv_neigh_list_2h;
	while (load_neigh_2h != NULL){  //my two-hop neighbors
			alpha = 1; //two-hops neigbhor?
			for (i=0; i< NEIGH_TABLESIZE; i++) {
				if (tmp_neigh->load_metric.neigh_tx[i] != 0) {
						if (tmp_neigh->load_metric.neigh_tx[i] == load_neigh_2h->ip) { //it's also a neighbor of the transmitter
							alpha = 2;
							break;
						}
					}
			}
				
			//alpha = 1/2 if a two-hops neighbor of the receiver
			//alpha = 1 if a one-hop neighbor of the receiver
			load+=(load_neigh_2h->load*alpha)/2; 
			load_neigh_2h = load_neigh_2h->next;	
	}
	
	recv_bw = (recv_bw*(100-load))/100;
	if (recv_bw <= flow->avg_rate) //not enough bw for the new flow
		return 1;
		
	tmp_rreq->path_metric +=(flow->avg_size*8000)/recv_bw; //in microseconds
	return 0;

}

