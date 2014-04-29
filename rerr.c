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
	
//cai
static struct proc_dir_entry *test,*aodv_dir;

int reply_to_rrer(u_int32_t source, u_int32_t destination) {

	
	if (source == g_mesh_ip || (source == g_null_ip && g_aodv_gateway))
		return 1;

	return 0;
}

//brk_dst_ip为已断开链路的目的ip
int gen_rerr(u_int32_t brk_dst_ip) {
	aodv_route *tmp_route;
	rerr *tmp_rerr;
	int expired_routes = 0;

	printk("----------------in gen rerr(%s)------------------\n",inet_ntoa(brk_dst_ip));

#ifdef DEBUG
	//when delete anything,read route table
	//static struct proc_dir_entry *test;
	test=create_proc_read_entry("routes", 0, aodv_dir, read_route_table_proc, NULL);
#endif
	
	//找到第一条aodv路由
	tmp_route = first_aodv_route();

#ifdef CaiDebug
//test
	if(tmp_route==NULL)
		printk("tmp_route is null\n");
	else
		printk("tmp_route->state is %s\n",inet_ntoa(tmp_route->state));
#endif

	//go through list
	//遍历所有下一跳为brk_dst_ip的路由条目，并给该节点发送rerr或无效化该路由
	while (tmp_route != NULL) {

		printk("tmp_route->next_hop is %s\n",inet_ntoa(tmp_route->next_hop));
		if ((tmp_route->next_hop == brk_dst_ip) && (!tmp_route->self_route)&& (!tmp_route->neigh_route)){

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

#ifdef DTN
					//if there is DTN register,current node will be the
					//last DTN hop near the brk node
					extern int dtn_register;
					printk("dtn_register=%d;last_avail:%s\n",dtn_register,inet_ntoa(tmp_rerr->last_avail_ip));
					if(dtn_register==1)
					{
						tmp_rerr->last_avail_ip = g_mesh_ip;
						printk("g_mesh_ip:%s\n",inet_ntoa(g_mesh_ip));
					}
#endif

#ifdef CaiDebug
					//cai:test gen rerr
	char local[20];
 	strcpy(local,inet_ntoa(g_mesh_ip));
	char brk[20];
strcpy(brk,inet_ntoa(brk_dst_ip));
	char last[20];
strcpy(last,inet_ntoa(tmp_route->last_hop));
	
	printk("-------------------------------------------\n");
									
	printk("rerr src is %s,brk_dst_ip is %s,the last hop is %s\n",local,brk,last);
	printk("--------------------------------------------\n");
#endif
			send_message(tmp_route->last_hop, NET_DIAMETER, tmp_rerr,sizeof(rerr));
					kfree(tmp_rerr);

				}

			}
			if (tmp_route->state != INVALID){
				expire_aodv_route(tmp_route);
			expired_routes++;
#ifdef CaiDebug
	//char local[20] = inet_ntoa(g_mesh_ip);
	//char brk[20] = inet_ntoa(brk_dst_ip);
	//char last[20] = inet_ntoa(tmp_route->last_hop);	
	char local[20];
 	strcpy(local,inet_ntoa(g_mesh_ip));
	char last[20];
	strcpy(last,inet_ntoa(tmp_route->last_hop));	
	printk("---------------------------------------------\n");			
	printk("invalidate tmp_route %s to %s,expored_route is %d\n",local,last,expired_routes);			
	printk("---------------------------------------------\n");
#endif
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
	printk("Recieved a route error from %s\n", inet_ntoa(tmp_packet->src_ip));
#endif

	tmp_route
			= find_aodv_route_by_id(tmp_rerr->dst_ip, ntohl(tmp_rerr->dst_id));

#ifdef DTN

	extern int dtn_register;

#include <linux/sched.h>
//JL: Added kernel threads interface:
#include <linux/kthread.h>
	u_int32_t para[2];
	para[0] = tmp_rerr->dst_ip;
	para[1] = tmp_rerr->last_avail_ip;
#ifdef CaiDebug
	printk("dtn_register: %d;last_avail:%s\n", dtn_register,inet_ntoa(tmp_rerr->last_avail_ip));
#endif
	if(dtn_register==1){
		if(para[1]==NULL)//current node is the nearest DTN node to brk link
			para[1] = g_mesh_ip;
//		kthread_run(&send2dtn, (void*)para, "send2dtn");
		send2dtn((void*)para);//tmp_rerr->dst_ip, tmp_rerr->last_avail_ip);
	}
#endif
	if (tmp_route && tmp_route->next_hop == tmp_packet->src_ip) {
		//if there is any hop before me
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
#ifdef CaiDebug
	char src[20];
	strcpy(src,inet_ntoa(tmp_packet->src_ip));
	char nex[20];
	strcpy(nex,inet_ntoa(tmp_route->next_hop));				
	printk("--------------------------------------------\n");				
	printk("get rerr from %s,the last hop is %s\n",src,nex);				

	printk("--------------------------------------------\n");
#endif


			}

		}

		if (tmp_route->state != INVALID) {
			expire_aodv_route(tmp_route);
			expired_routes++;
#ifdef CaiDebug
	char local[20];
	strcpy(local,inet_ntoa(tmp_packet->src_ip));
	char nex[20];
	strcpy(nex,inet_ntoa(tmp_route->next_hop));
	//char nex[20] = inet_ntoa(tmp_route->next_hop);	

	printk("------------------------------------------\n");			
	printk("invalidate tmp_route %s to %s,expored_route is %d\n",local,nex,expired_routes);			
	printk("---------------------------------------------\n");
#endif
		}

	}
	if (g_routing_metric == WCIM && expired_routes != 0)
	update_my_load();
		

	return 0;

}

