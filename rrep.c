/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "rrep.h"

extern u_int32_t g_mesh_ip;
extern u_int32_t g_null_ip;
extern aodv_route * g_local_route;

void convert_rrep_to_host(rrep * tmp_rrep) {
	tmp_rrep->dst_id = ntohl(tmp_rrep->dst_id);
	tmp_rrep->src_id = ntohl(tmp_rrep->src_id);
	tmp_rrep->path_metric = ntohl(tmp_rrep->path_metric);
}

void convert_rrep_to_network(rrep * tmp_rrep) {
	tmp_rrep->dst_id = htonl(tmp_rrep->dst_id);
	tmp_rrep->src_id = htonl(tmp_rrep->src_id);
	tmp_rrep->path_metric = htonl(tmp_rrep->path_metric);

}

int recv_rrep(task * tmp_packet) {
	aodv_route *send_route;
	aodv_route *recv_route;
	aodv_neigh *tmp_neigh = NULL;
	rrep *tmp_rrep;
	u_int32_t path_metric;
	int iam_destination = 0;
#ifdef DEBUG
	char dst_ip[16];
	char src_ip[16];
#endif
	tmp_rrep = tmp_packet->data;

	convert_rrep_to_host(tmp_rrep);

	tmp_neigh = find_aodv_neigh(tmp_packet->src_ip);
#ifdef DEBUG
	printk("Recive RREP from %s\n", inet_ntoa(tmp_packet->src_ip));
#endif
	if (tmp_neigh == NULL) {
#ifdef DEBUG
		printk("Ignoring RREP received from unknown neighbor\n");
#endif
		return 1;
	}

	//Update neighbor timelife
		delete_timer(tmp_neigh->ip, tmp_neigh->ip, NO_TOS, TASK_NEIGHBOR);
		insert_timer_simple(TASK_NEIGHBOR, HELLO_INTERVAL
				* (1 + ALLOWED_HELLO_LOSS) + 100, tmp_neigh->ip);
		update_timer_queue();
		tmp_neigh->lifetime = HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 20
				+ getcurrtime();
	tmp_rrep->num_hops++;

#ifdef DEBUG

	strcpy(src_ip, inet_ntoa(tmp_rrep->src_ip));
	strcpy(dst_ip, inet_ntoa(tmp_rrep->dst_ip));

	printk("received new rrep from %s to %s with ToS: %u - last hop: %s\n", dst_ip, src_ip, tmp_rrep->tos, inet_ntoa(tmp_packet->src_ip));
	printk("g_mesh_ip is %s\n", inet_ntoa(g_mesh_ip));
#endif

	if (tmp_rrep->src_ip == g_mesh_ip || (tmp_rrep->src_ip == g_null_ip && tmp_rrep->gateway == g_mesh_ip))
		iam_destination = 1;
//printk("--------------rrep:%s--------------\n",inet_ntoa(tmp_rrep->src_ip));
	if (iam_destination) { //I'm the source of the flow (the destination of the RREP)

#ifdef DEBUG
		printk("I'm the source of the flow (the destination of the RREP)\n");
#endif
		

		update_timer_queue();
		path_metric = tmp_rrep->path_metric;
		//Create (or update) the first hop of the route

		rreq_aodv_route(tmp_rrep->src_ip, tmp_rrep->dst_ip, tmp_rrep->tos, tmp_neigh, tmp_rrep->num_hops,
				tmp_rrep->dst_id, tmp_packet->dev, path_metric);

		
		send_route = find_aodv_route_by_id(tmp_rrep->dst_ip, tmp_rrep->dst_id);
		delete_timer(tmp_rrep->src_ip, tmp_rrep->dst_ip, tmp_rrep->tos,
					TASK_RESEND_RREQ);

		if (!send_route) {
#ifdef DEBUG
			printk("No reverse-route for RREP from: %s to: %s with TOS %u\n", dst_ip, src_ip, tmp_rrep->tos);
#endif
			return 0;
		}

		rrep_aodv_route(send_route);
		send_route->last_hop = g_mesh_ip;

#ifdef DTN_HELLO
		if(tmp_rrep->dttl!=NULL && tmp_rrep->dttl>0){//I'm the src of dtn hello,& notice DTN the neighbor node
			u_int32_t para[4];
			
			//leave a interface to query DTN about its geographical info
			
			para[0] = tmp_rrep->dst_ip;
			para[1] = tmp_rrep->x;
			para[2] = tmp_rrep->y;
			
			//类型处理
			para[3] = NULL;
			//
			send2dtn((void*)para,RE_DTN_HELLO);
#ifdef DEBUG0
			printk("=========Tell the DTN aboout the neighbor info:%s,%d========\n",
				inet_ntoa(tmp_rrep->dst_ip),tmp_rrep->x);
#endif
		}

		
#endif



#ifdef RECOVERYPATH

		/*****************************************************
            添加了通路处理部分，若有到该目的的断路条目，
            则产生相应的通路包，给dst的上一跳发送通路包，
            通知DTN层，以及删除相应断路条目均在gen_rcvp
            中实现
		******************************************************/

#ifdef CaiDebug
char s[20];
char d[20];
strcpy(s,inet_ntoa(tmp_rrep->src_ip));
strcpy(d,inet_ntoa(tmp_rrep->dst_ip));
        printk("The tmp_rrep's src is %s,dst is %s\n",s,d);
#endif
		brk_link *tmp_link;
		tmp_link = find_first_brk_link_with_dst(tmp_rrep->dst_ip);//rreq的源则为路由发现的目的地，也为断路表的目的地址
		if(tmp_link==NULL){   
#ifdef CaiDebug
printk("no brk links to this dst\n");
#endif
		}
		else{

		  	gen_rcvp(tmp_rrep->dst_ip);
		}
		/*****************************************************/
#endif

	}//end iam destination

	else {


		recv_route = find_aodv_route_by_id(tmp_rrep->src_ip, tmp_rrep->src_id); //the route created by the RREQ

#ifdef DTN
#ifdef DTN_HELLO
		extern u_int32_t dtn_hello_ip;
		extern dtn_register;
		aodv_route *new_recv;
		aodv_route *new_send;

		if( (tmp_rrep->dttl!=NULL) && (tmp_rrep->dttl>0) ){
			//it's a DTN hello rrep
			recv_route = find_aodv_route(dtn_hello_ip,tmp_rrep->src_ip,tmp_rrep->tos);
			if(recv_route){
				new_recv = create_aodv_route(tmp_rrep->dst_ip,tmp_rrep->src_ip,tmp_rrep->tos,tmp_rrep->dst_id);
				new_recv->last_hop = recv_route->last_hop;
				new_recv->next_hop = recv_route->next_hop;

				recv_route = new_recv;
			}
			
			
			if(dtn_register){			
				tmp_rrep->dttl = tmp_rrep->dttl - 1;
			}
			
#ifdef CaiDebug
	printk("rrep:dttl:%d\n",tmp_rrep->dttl);
	aodv_route *tmp_route;
	tmp_route = first_aodv_route();
	char src[20];
	char dst[20];
	char nex[20];
	strcpy(src,inet_ntoa(recv_route->src_ip));
	strcpy(dst,inet_ntoa(recv_route->dst_ip));
	strcpy(nex,inet_ntoa(recv_route->next_hop));
	printk("recv:%s---%s---%s\n",src,dst,nex);
	while(tmp_route){
		strcpy(src,inet_ntoa(tmp_route->src_ip));
		strcpy(dst,inet_ntoa(tmp_route->dst_ip));
		printk("tmp_route src:%s   dst:%s\n",src,dst);
		tmp_route = tmp_route->next;
	}
#endif
			
		}//dttl
	
#endif
#endif

		if (!recv_route) {
#ifdef DEBUG
			printk("Reverse Route has timed out! - RREP is not forwarded!\n");
#endif
			return 1;
		}

		path_metric = tmp_rrep->path_metric - recv_route->path_metric;
		//Create (or update) the route from source to destination
		rreq_aodv_route(tmp_rrep->src_ip, tmp_rrep->dst_ip, tmp_rrep->tos,
				tmp_neigh, tmp_rrep->num_hops,
				tmp_rrep->dst_id, tmp_packet->dev, path_metric);

		send_route = find_aodv_route_by_id(tmp_rrep->dst_ip, tmp_rrep->dst_id);


#ifdef DEBUG
		printk("recv_rrep: send_route %s to: %s with NEXT %s\n", inet_ntoa(send_route->src_ip), inet_ntoa(send_route->dst_ip), inet_ntoa(send_route->next_hop));
#endif
		if (!send_route) {
#ifdef DEBUG

			printk("No reverse-route for RREP from: %s to: %s with TOS %u\n", dst_ip, src_ip, tmp_rrep->tos);
			printk("Not Forwarding RREP!\n");
#endif
			return 0;
		}

#ifdef CaiDebug
	aodv_route *tmp_route;
	tmp_route = first_aodv_route();
	char src[20];
	char dst[20];
	char nex[20];
	strcpy(src,inet_ntoa(send_route->src_ip));
	strcpy(dst,inet_ntoa(send_route->dst_ip));
	strcpy(nex,inet_ntoa(send_route->next_hop));
	printk("send:%s---%s---%s\n",src,dst,nex);
	while(tmp_route){
		strcpy(src,inet_ntoa(tmp_route->src_ip));
		strcpy(dst,inet_ntoa(tmp_route->dst_ip));
		printk("tmp_route src:%s   dst:%s\n",src,dst);
		tmp_route = tmp_route->next;

	}
#endif
		rrep_aodv_route(recv_route);
		rrep_aodv_route(send_route);
		send_route->last_hop = recv_route->next_hop;
		recv_route->last_hop = send_route->next_hop;

		convert_rrep_to_network(tmp_rrep);
		send_message(recv_route->next_hop, NET_DIAMETER, tmp_rrep, sizeof(rrep));

	}

	return 0;
}

int gen_rrep(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos) {
	aodv_route *src_route;
	rrep *tmp_rrep;

#ifdef DEBUG0
	char src[16];
	char dst[16];
	strcpy (src, inet_ntoa(src_ip));
	strcpy (dst, inet_ntoa(dst_ip));
	printk("I'm generating a new rrep, from %s to %s\n",src, dst);
#endif
	if ((tmp_rrep = (rrep *) kmalloc(sizeof(rrep), GFP_ATOMIC)) == NULL) {
		printk("Can't allocate new rrep\n");
		return 0;
	}

	src_route = find_aodv_route(dst_ip, src_ip, tos);
	/* Get the source and destination IP address from the RREQ */
	if (!src_route) { //symmetric
#ifdef DEBUG0
			printk("RREP: No route to Source! src: %s\n", inet_ntoa(src_ip));
#endif

			return 1;
	}


	if ((tmp_rrep = (rrep *) kmalloc(sizeof(rrep), GFP_ATOMIC)) == NULL) {
		printk("Can't allocate new rrep\n");
		return 0;
	}

//it's a dtn neighbor,gen "special rrep" for telling src
#ifdef DTN_HELLO
	extern int dtn_register;
	extern u_int32_t dtn_hello_ip;
	extern u_int32_t longitude;
	extern u_int32_t latitude;
	//u_int32_t x;
	//u_int32_t y;
	u_int32_t new_dst_ip;
	u_int8_t dttl=0;
	aodv_route *new_route;
	if(dtn_register){
#ifdef CaiDebug
		printk("The src ip is %s\n",inet_ntoa(src_ip));
#endif
		if(dst_ip==dtn_hello_ip){//it's a dtn neighbor,gen "special rrep" for telling src
			//change the dst_ip to local ip
			new_dst_ip = g_mesh_ip;
			dttl = DTTL;
			//x = longitude;
			//y = latitue;
		
			//add a new one from src to local ip
			//do not remove the old one ,there may be some other neighbor to use it
			g_local_route->dst_id = g_local_route->dst_id + 1;
			new_route = create_aodv_route(g_mesh_ip, src_ip,tos,g_local_route->dst_id);

			if(new_route==NULL) return 0;
			new_route->next_hop = src_route->next_hop;
			new_route->last_hop = NULL;
			//update the src_route's lifetime
			//src_route->lifetime = getcurrtime() + DELETE_PERIOD;
			//let the new_route be the src_route,&create rrep route
			src_route = new_route;

#ifdef CaiDebug
			char src[20];
			char dst[20];
			char nex[20];
			strcpy(src,inet_ntoa(new_route->src_ip));
			strcpy(dst,inet_ntoa(new_route->dst_ip));
			strcpy(nex,inet_ntoa(new_route->next_hop));
			printk("---------src:%s  dst:%s  nex:%s----------\n",src,dst,nex);
#endif

		}
	}

#endif
	

	tmp_rrep->type = RREP_MESSAGE;
	tmp_rrep->a = 0;
	tmp_rrep->reserved1 = 0;
	tmp_rrep->src_ip = src_route->dst_ip;
	tmp_rrep->dst_ip = src_route->src_ip;//dst_ip;
	g_local_route->dst_id = g_local_route->dst_id + 1;
	tmp_rrep->dst_id = g_local_route->dst_id;
	tmp_rrep->num_hops = 0;
	tmp_rrep->src_id = src_route->dst_id;
	tmp_rrep->tos = tos;
	tmp_rrep->q=0;
	tmp_rrep->path_metric = src_route->path_metric;



	//Update the route to REPLIED state
	rrep_aodv_route(src_route);
	src_route->last_hop = src_route->src_ip;//dst_ip;
#ifdef DTN_HELLO
	tmp_rrep->dttl = dttl;
	tmp_rrep->x = longitude;
	tmp_rrep->y = latitude;
#endif


	convert_rrep_to_network(tmp_rrep);
	send_message(src_route->next_hop, NET_DIAMETER, tmp_rrep, sizeof(rrep));
	kfree(tmp_rrep);
	return 1;

}

