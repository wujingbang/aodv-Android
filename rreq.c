/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "rreq.h"

extern u_int8_t g_aodv_gateway;
extern u_int8_t g_routing_metric;
extern aodv_route * g_local_route;
extern u_int32_t g_mesh_ip;
extern u_int32_t g_null_ip;
extern u_int32_t g_default_gw;

void convert_rreq_to_host(rreq * tmp_rreq) {
	tmp_rreq->dst_id = ntohl(tmp_rreq->dst_id);
	tmp_rreq->path_metric = ntohl(tmp_rreq->path_metric);
}

void convert_rreq_to_network(rreq * tmp_rreq) {
	tmp_rreq->dst_id = htonl(tmp_rreq->dst_id);
	tmp_rreq->path_metric = htonl(tmp_rreq->path_metric);
}

int recv_rreq(task * tmp_packet) {
	int error;
	rreq *tmp_rreq = NULL;
	aodv_neigh *tmp_neigh;
	int iam_destination = 0;

	tmp_rreq = tmp_packet->data;
	convert_rreq_to_host(tmp_rreq);

	if (tmp_rreq->dst_ip == g_mesh_ip || tmp_rreq->gateway == g_mesh_ip)
		iam_destination = 1;

	if (tmp_packet->ttl <= 1 && !iam_destination) //If i'm destination, i can receive it
	{
#ifdef 	CaiDebug
		printk("AODV TTL for RREQ from: %s expired\n", inet_ntoa(tmp_rreq->src_ip));
#endif
		return -ETIMEDOUT;
	}

	tmp_neigh = find_aodv_neigh(tmp_packet->src_ip);

	if (tmp_neigh == NULL) {
		printk("Ignoring RREQ received from unknown neighbor\n");
		return 1;
	}

	//Update neighbor timelife
	delete_timer(tmp_neigh->ip, tmp_neigh->ip, NO_TOS, TASK_NEIGHBOR);
	insert_timer_simple(TASK_NEIGHBOR, HELLO_INTERVAL
			* (1 + ALLOWED_HELLO_LOSS) + 100, tmp_neigh->ip);
	update_timer_queue();
	tmp_neigh->lifetime = HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 20
			+ getcurrtime();

	if (g_mesh_ip == tmp_rreq->src_ip) {
#ifdef DEBUG
		printk("I'm the generator of the incoming RREQ - Discarted\n");
#endif
		return 1;
	}

	if (g_aodv_gateway && tmp_rreq->dst_ip == g_null_ip && tmp_rreq->gateway != g_mesh_ip) {
		printk("I'm not the gateway of this source - I cannot apply as an intermediate node to its default route\n");
		return 1;
	}

	tmp_rreq->num_hops++;

	if (g_routing_metric == ETT) {
		error = ett_metric(tmp_neigh, tmp_rreq);
		if (error) {
#ifdef DEBUG
			printk("Metric too high - RREQ Discarted\n");
#endif
			return 1;
		}
	}

	else if (g_routing_metric == WCIM) {
		error = wcim_metric(tmp_neigh, tmp_rreq);
		if (error) {
#ifdef DEBUG
			printk("Metric too high - RREQ Discarted\n");
#endif
			return 1;
		}
	} else

		//number of hops
		tmp_rreq->path_metric =tmp_rreq->num_hops;

	
	if (rreq_aodv_route(tmp_rreq->dst_ip, tmp_rreq->src_ip, tmp_rreq->tos,
			tmp_neigh, tmp_rreq->num_hops, tmp_rreq->dst_id, tmp_packet->dev,
			tmp_rreq->path_metric)) {

		if (iam_destination) {

			//If there is a rrep to be sent, it would use the newest route
			//If not, wait a little (RREP_TIMER) for other possible incoming RREQ's (better path?) before sending the RREP
			if (!find_timer(tmp_rreq->src_ip, tmp_rreq->dst_ip, tmp_rreq->tos,
					TASK_SEND_RREP)) {
				insert_timer_directional(TASK_SEND_RREP, RREP_TIMER, 0,
						tmp_rreq->src_ip, tmp_rreq->dst_ip, tmp_rreq->tos);
				update_timer_queue();
			}
			return 0;
		}
		
#ifdef DTN_HELLO
		//manage dttl
		//if it's a rreq for looking for DTN neighbors
		//the dst would not be any ip of local net
		extern dtn_register;
		if(dtn_register){
			if(tmp_rreq->dttl>=1){
				u_int32_t para[4];
				para[0] = tmp_rreq->src_ip;
				para[1] = (u_int32_t)tmp_rreq->tos;
				para[2] = NULL;
				para[3] = NULL;
				send2dtn((void*)para,DTN_LOCATION_PORT);			
				//gen_rrep(tmp_rreq->src_ip,tmp_rreq->dst_ip,tmp_rreq->tos);
				tmp_rreq->dttl = tmp_rreq->dttl - 1;
			}	
		}
#endif



		//else forwarding RREQ
		convert_rreq_to_network(tmp_rreq);

		local_broadcast(tmp_packet->ttl - 1, tmp_rreq, sizeof(rreq));

		return 0;
	}

	return 1;

}

int resend_rreq(task * tmp_packet) {
	rreq *out_rreq;
	u_int8_t out_ttl;

	if (tmp_packet->retries <= 0) {
		ipq_drop_ip(tmp_packet->src_ip, tmp_packet->dst_ip, tmp_packet->tos);
#ifdef DEBUG
	char src[16];
	char dst[16];
	strcpy(src, inet_ntoa(tmp_packet->src_ip));
	strcpy(dst, inet_ntoa(tmp_packet->dst_ip));
	printk("RREQ has achieved its retry limit - The route from %s to %s is not possible\n", src, dst);
#endif
		return 0;
	}

	if ((out_rreq = (rreq *) kmalloc(sizeof(rreq), GFP_ATOMIC)) == NULL) {
#ifdef DEBUG
		printk( "Can't allocate new rreq\n");
#endif
		return 0;
	}
	out_ttl = NET_DIAMETER;

	/* Get our own sequence number */
	g_local_route->dst_id = g_local_route->dst_id + 1;
	out_rreq->dst_id = g_local_route->dst_id;
	out_rreq->dst_ip = tmp_packet->dst_ip;
	if (out_rreq->dst_ip == g_null_ip)
			out_rreq->gateway = g_default_gw;

		else
			out_rreq->gateway = g_null_ip;

	out_rreq->src_ip = tmp_packet->src_ip;
	out_rreq->type = RREQ_MESSAGE;
	out_rreq->num_hops = 0;
	out_rreq->j = 0;
	out_rreq->r = 0;
	out_rreq->d = 1;
	out_rreq->u = 0;
	out_rreq->reserved = 0;
	out_rreq->g = 0;

	out_rreq->path_metric = 0;
	out_rreq->tos = tmp_packet->tos;

#ifdef DTN_HELLO
		out_rreq->dttl = 0;
#endif 

/*
#ifdef DTN_HELLO
	extern u_int32_t dtn_hello_ip;
	if(tmp_packet->dst_ip == dtn_hello_ip)
		out_rreq->dttl = DTTL;
#endif 
*/

	convert_rreq_to_network(out_rreq);
	local_broadcast(out_ttl, out_rreq, sizeof(rreq));

		insert_timer_directional(TASK_RESEND_RREQ, 0, tmp_packet->retries - 1,
				out_rreq->src_ip, out_rreq->dst_ip, out_rreq->tos);

	update_timer_queue();

	kfree(out_rreq);

	return 0;
}

/****************************************************

 gen_rreq
 ----------------------------------------------------
 Generates a RREQ! wahhooo!
 ****************************************************/
int gen_rreq(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos) {
	rreq *out_rreq;
	u_int8_t out_ttl;
	flow_type *new_flow;

//printk("-----------------gen rreq--------------\n");
//	new_flow = find_flow_type(tos);
//
//	if (new_flow == NULL) {
//		printk("new_flow is NULL\n");
//		return 0;
//	}dst_ip

//////
//printk("--------gen_rreq %s-------\n",inet_ntoa(dst_ip));	
/////
#ifdef DTN_HELLO
	extern u_int32_t dtn_hello_ip;
	u_int8_t dttl=0;
	if(dst_ip == dtn_hello_ip)
	{	
		dttl = DTTL;
		//printk("It's a DTN hello!\n");
	}
#endif

//	if (find_timer(src_ip, dst_ip, tos, TASK_RESEND_RREQ)) {
//			printk("don't flood the net with new rreq, wait...\n");
//			return 1;
//	}
#ifdef DEBUG
	else {
		char src[16];
		char dst[16];
		strcpy(src, inet_ntoa(src_ip));
		strcpy(dst, inet_ntoa(dst_ip));
		printk("gen_rreq: Generates a RREQ from %s to %s!\n", src, dst);
	}
#endif

	/* Allocate memory for the rreq message */
	if ((out_rreq = (rreq *) kmalloc(sizeof(rreq), GFP_ATOMIC)) == NULL) {
		printk("Can't allocate new rreq\n");
		return 1;
	}

	out_ttl = NET_DIAMETER;

	/* Get our own sequence number */
	g_local_route->dst_id = g_local_route->dst_id + 1;
	out_rreq->dst_id = g_local_route->dst_id;
	out_rreq->dst_ip = dst_ip;
	if (out_rreq->dst_ip == g_null_ip)
		out_rreq->gateway = g_default_gw;
	else
		out_rreq->gateway = g_null_ip;

//wujingbang
/*
	if (g_default_gw == g_null_ip){
#ifdef DEBUG
		printk("There isn't a default Gateway! - External traffic will be dropped until an active gateway is discovered\n");
#endif
		kfree(out_rreq);
		return 0;
	}
*/
	out_rreq->src_ip = src_ip;
	out_rreq->type = RREQ_MESSAGE;
	out_rreq->num_hops = 0;
	out_rreq->j = 0;
	out_rreq->r = 0;
	out_rreq->d = 1;
	out_rreq->u = 0;
	out_rreq->reserved = 0;
	out_rreq->g = 0;
	out_rreq->path_metric = 0;
	out_rreq->tos = tos;
#ifdef DTN_HELLO
	out_rreq->dttl = dttl;
#endif

	convert_rreq_to_network(out_rreq);
	local_broadcast(out_ttl, out_rreq, sizeof(rreq));
	insert_timer_directional(TASK_RESEND_RREQ, 0, RREQ_RETRIES, src_ip,
			dst_ip, tos);
	update_timer_queue();
	kfree(out_rreq);

	return 1;

}
