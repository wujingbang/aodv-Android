/***************************************************************************
                          rcvp.h  -  description
                             -------------------
    begin                : Thu May 15 2014
    copyright            : (C) 2014 by Cai Bingying
    email                :
 ***************************************************************************/
//#ifdef RECOVERYPATH
#include "rcvp.h"

//manage the recovery path packet
//处理通路包，产生通路包，接收到通路包，转发，通知DTN层

extern u_int32_t g_mesh_ip;
extern u_int32_t g_null_ip;
extern u_int8_t g_aodv_gateway;
extern u_int8_t g_routing_metric;


//genarate recovery path packe
int gen_rcvp(u_int32_t dst_ip){

#ifdef CaiDebug
    char dst[20];
    strcpy(dst,inet_ntoa(dst_ip));
    printk("----------------in gen rcvp(%s)----------------\n",dst);
#endif


    brk_link *tmp_link;
    rcvp *tmp_rcvp;
    brk_link *dead_link;
    //find the first brk link with dst_ip and then gen rcvp
    tmp_link = find_first_brk_link_with_dst(dst_ip);

    if(tmp_link == NULL){//not found, do not gen rcvp;
#ifdef CaiDebug 
       printk("No this dst,do not gen rcvp!\n");
#endif
        return 0;
    }
    //对所有经过本节点，目的为dst的断路条目进行处理：产生相应的通路包并删除该通路
    while(tmp_link!=NULL && tmp_link->dst_ip==dst_ip){

        if(tmp_link->src_ip!=g_mesh_ip){//不为源节点，不再转发通路包--I'm not the source,do not transmit rcvp
            if ((tmp_rcvp = (rcvp *) kmalloc(sizeof(rcvp), GFP_ATOMIC))
							== NULL) {
#ifdef CaiDebug
						printk("Can't allocate new rcvp\n");
#endif
						return 0;
					}

                tmp_rcvp->dst_ip = dst_ip;
                tmp_rcvp->dst_id = htonl(dst_ip);
                tmp_rcvp->src_ip = tmp_link->src_ip;
                tmp_rcvp->last_avail_ip = tmp_link->last_avail_ip;
                tmp_rcvp->type = RCVP_MESSAGE;
                tmp_rcvp->num_hops = 0;

#ifdef DTN
                extern int dtn_register;
               // printk("dtn_register=%d;last_hop:%s\n",dtn_register,inet_ntoa(tmp_link->last_hop));
                if(dtn_register==1)
                {
                    //notice DTN layer未写接口
#include <linux/sched.h>
//JL: Added kernel threads interface:
#include <linux/kthread.h>
                    u_int32_t para[4];
                    para[0] = tmp_rcvp->src_ip;
                    para[1] = tmp_rcvp->dst_ip;
                    para[2] = tmp_rcvp->last_avail_ip;
                    //类型处理
                    para[3] = (u_int32_t)tmp_rcvp->type;
                    //
                    send2dtn((void*)para,DTNPORT);

                }
#endif

                //将通路包沿断路表里的上一跳发回给源节点
                send_message(tmp_link->last_hop, NET_DIAMETER, tmp_rcvp,sizeof(rcvp));
                kfree(tmp_rcvp);
            }
        else{//若为源节点

            //通知DTN层
#ifdef DTN
                extern int dtn_register;
               // printk("dtn_register=%d;last_hop:%s\n",dtn_register,inet_ntoa(tmp_link->last_hop));
                if(dtn_register==1)
                {

#include <linux/sched.h>
//JL: Added kernel threads interface:
#include <linux/kthread.h>
                    u_int32_t para[4];
                    para[0] = tmp_link->src_ip;
                    para[1] = dst_ip;
                    para[2] = tmp_link->last_avail_ip;

                    //类型处理
                    para[3] = (u_int32_t)RCVP_MESSAGE;
                    //
                    send2dtn((void*)para,DTNPORT);
                }
#endif
        }

        //删除相应条目
        dead_link = tmp_link;
        tmp_link = tmp_link->next;
        remove_brk_link(dead_link);

    }//while

    return 1;


}


int recv_rcvp(task *tmp_packet){

    brk_link *tmp_link;
    rcvp *tmp_rcvp;
    rcvp *new_rcvp;
    int num_hops;

    tmp_rcvp = (rcvp *)tmp_packet->data;
    num_hops = tmp_rcvp->num_hops+1;

#ifdef CaiDebug
	printk("Recieved a recovery path packet from %s\n", inet_ntoa(tmp_packet->src_ip));
#endif

#ifdef DTN

    //添加与DTN交互部分
                extern int dtn_register;
                //printk("dtn_register=%d;last_avail:%s\n",dtn_register,inet_ntoa(tmp_rcvp->last_avail_ip));
                if(dtn_register==1)
                {
                    //notice DTN layer未写接口
#include <linux/sched.h>
//JL: Added kernel threads interface:
#include <linux/kthread.h>
                    u_int32_t para[4];
                    para[0] = tmp_rcvp->src_ip;
                    para[1] = tmp_rcvp->dst_ip;
                    para[2] = tmp_rcvp->last_avail_ip;
                    //类型处理
                    para[3] = (u_int32_t)tmp_rcvp->type;
                    //
                    send2dtn((void*)para,DTNPORT);

                }

#endif

    //找到指定的断路条目
    tmp_link = find_brk_link(tmp_rcvp->src_ip,tmp_rcvp->dst_ip);
    if(tmp_link==NULL){//not found
#ifdef CaiDebug
        printk("no brk link %s to %s\n",inet_ntoa( tmp_rcvp->src_ip), inet_ntoa(tmp_rcvp->dst_ip) );
#endif
        return 0;
    }

    if(tmp_rcvp->src_ip==g_mesh_ip){//已经到达源节点
        remove_brk_link(tmp_link);
        //kfree(tmp_rcvp);
#ifdef CaiDebug
        printk("rcvp has got the src\n");
#endif
        return 1;
    }
    else{

        //产生新的rcvp数据包以转发
        if ((new_rcvp = (rcvp *) kmalloc(sizeof(rcvp), GFP_ATOMIC)) == NULL) {
#ifdef CaiDebug
						printk("Can't allocate new rcvp\n");
#endif
						return 0;
            }

            new_rcvp->dst_ip = tmp_rcvp->dst_ip;
            new_rcvp->dst_id = htonl(tmp_rcvp->dst_ip);
            new_rcvp->num_hops = num_hops;
            new_rcvp->src_ip = tmp_rcvp->src_ip;
            new_rcvp->last_avail_ip = tmp_link->last_avail_ip;
            new_rcvp->type = RCVP_MESSAGE;


        send_message(tmp_link->last_hop, NET_DIAMETER, new_rcvp,sizeof(rcvp));
	kfree(new_rcvp);
	
        remove_brk_link(tmp_link);
#ifdef CaiDebug
        printk("rcvp has not got the src, sent to %s\n",inet_ntoa(tmp_link->last_hop));
#endif
        return 1;

    }
}


//gen route redirection packet
int gen_rrdp(u_int32_t src_ip,u_int32_t dst_ip,u_int32_t last_hop,unsigned char tos){
	
#ifdef CaiDebug
    	char src[20];
    	char dst[20];
    	strcpy(src,inet_ntoa(src_ip));
	strcpy(dst,inet_ntoa(dst_ip));
    printk("----------------in gen rrdp(%s   %s)----------------\n",src,dst);
#endif
	rrdp *tmp_rrdp;
	
	if ((tmp_rrdp = (rrdp *) kmalloc(sizeof(rrdp), GFP_ATOMIC)) == NULL) {
#ifdef DEBUG
		printk("Can't allocate new rrep\n");
#endif
		return 0;
	}

	tmp_rrdp->type = RRDP_MESSAGE;
	tmp_rrdp->dst_count = 0;
	tmp_rrdp->reserved = 0;
	tmp_rrdp->n = 0;
	tmp_rrdp->num_hops = 0;
	tmp_rrdp->dst_ip = dst_ip;
	tmp_rrdp->src_ip = src_ip;
	tmp_rrdp->tos = tos;

	send_message(last_hop, NET_DIAMETER, tmp_rrdp,sizeof(rrdp));
	kfree(tmp_rrdp);
					
	return 1;

}

int recv_rrdp(task *tmp_packet){

	aodv_route *tmp_route;
	rrdp *tmp_rrdp;
	int num_hops;

	tmp_rrdp = (rrdp *) tmp_packet->data;
	num_hops = tmp_rrdp->num_hops+1;

#ifdef CaiDebug
	printk("Recieved a route redirection packet from %s\n", inet_ntoa(tmp_packet->src_ip));
#endif

    	tmp_route = find_aodv_route(tmp_rrdp->src_ip,tmp_rrdp->dst_ip,tmp_rrdp->tos);

	if (tmp_route && tmp_route->next_hop == tmp_packet->src_ip) {
		//if there is any hop before me
		if(!(tmp_route->src_ip==g_mesh_ip || (tmp_route->src_ip == g_null_ip && g_aodv_gateway) )){
		//if (tmp_route->src_ip != g_mesh_ip || !(tmp_route->src_ip == g_null_ip && g_aodv_gateway) ) {

			if (tmp_route->state != INVALID) { //route with active traffic

				//tmp_rrdp->type = RERR_MESSAGE;
				//tmp_rrdp->dst_count = 0; //unused
				//tmp_rrdp->reserved = 0;
				//tmp_rrdp->n = 0;
				tmp_rrdp->num_hops = num_hops;
				//tmp_rerr->dst_ip = tmp_route->dst_ip;
				//tmp_rerr->dst_id = htonl(tmp_route->dst_id);

				send_message(tmp_route->last_hop, NET_DIAMETER, tmp_rrdp,
						sizeof(rrdp));

#ifdef CaiDebug
	char last[20];
	strcpy(last,inet_ntoa(tmp_route->last_hop));
	printk("--------------------------------------------\n");
	printk("the last hop is %s\n",last);
	printk("--------------------------------------------\n");
#endif

			}//Valid

		}//not the source
		else{

#ifdef CaiDebug
			printk("------------I am the rrdp source-----------");
#endif

#ifdef DTN

			extern int dtn_register;

#include <linux/sched.h>
//JL: Added kernel threads interface:
#include <linux/kthread.h>
			if( dtn_register==1 ){
				u_int32_t para[4];
				para[0] = tmp_rrdp->src_ip;
				para[1] = tmp_rrdp->dst_ip;
				para[2] = NULL;
				para[3] = (u_int32_t)tmp_rrdp->type;
				send2dtn((void*)para,DTNPORT);
#ifdef CaiDebug
				printk("------------send the rrdp to DTN-----------\n");
#endif	
			}
		
#endif
		    	kfree(tmp_rrdp);
		}//the source

		

	}
	
	return 1;

}



//#endif
