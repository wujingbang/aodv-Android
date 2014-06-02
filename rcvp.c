/***************************************************************************
                          rcvp.h  -  description
                             -------------------
    begin                : Thu May 15 2014
    copyright            : (C) 2014 by Cai Bingying
    email                :
 ***************************************************************************/

#include "aodv.h"
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
        printk("No this dst,do not gen rcvp!\n");
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
                printk("dtn_register=%d;last_hop:%s\n",dtn_register,inet_ntoa(tmp_link->last_hop));
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
                    send2dtn((void*)para);

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
                printk("dtn_register=%d;last_hop:%s\n",dtn_register,inet_ntoa(tmp_link->last_hop));
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
                    send2dtn((void*)para);
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


int recv_rcvp(task * tmp_packet){

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
                printk("dtn_register=%d;last_avail:%s\n",dtn_register,inet_ntoa(tmp_rcvp->last_avail_ip));
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
printk("debug1\n");
                    send2dtn((void*)para);
printk("debug2\n");

                }

#endif

    //找到指定的断路条目
    tmp_link = find_brk_link(tmp_rcvp->src_ip,tmp_rcvp->dst_ip);
    if(tmp_link==NULL){//not found
        printk("no brk link %s to %s\n",inet_ntoa( tmp_rcvp->src_ip), inet_ntoa(tmp_rcvp->dst_ip) );
        return 0;
    }

    if(tmp_rcvp->src_ip==g_mesh_ip){//已经到达源节点
        remove_brk_link(tmp_link);
        kfree(tmp_rcvp);
        printk("rcvp has got the src\n");
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

#ifdef CaiDebug
            char rcvp_dst[20];
            char rcvp_src[20];
            char rcvp_lasta[20];
            char brk_dst[20];
            char brk_src[20];
            char brk_lasta[20];
            strcpy(rcvp_dst,inet_ntoa(tmp_rcvp->dst_ip));
            strcpy(rcvp_src,inet_ntoa(tmp_rcvp->src_ip));
            strcpy(rcvp_lasta,inet_ntoa(tmp_rcvp->last_avail_ip));
            strcpy(brk_dst,inet_ntoa(tmp_link->dst_ip));
            strcpy(brk_src,inet_ntoa(tmp_link->src_ip));
            strcpy(brk_lasta,inet_ntoa(tmp_link->last_avail_ip));

            printk("tmp_rcvp VS tmp_link : %s-%s, %s-%s, %s-%s\n",rcvp_dst,brk_dst,rcvp_src,brk_src,rcvp_lasta,brk_lasta);
#endif



        send_message(tmp_link->last_hop, NET_DIAMETER, new_rcvp,sizeof(rcvp));
	kfree(new_rcvp);
	
        remove_brk_link(tmp_link);
        printk("rcvp has not got the src, sent to %s\n",inet_ntoa(tmp_link->last_hop));
        return 1;

    }
}
