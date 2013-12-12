/*
 * This is a module which is used for queueing IPv4 packets and
 * communicating with userspace via netlink.
 *

 * (C) 2000 James Morris, this code is GPL.
 *
 * 2002-01-10: I modified this code slight to offer more comparisions... - Luke Klein-Berndt
 * 2000-03-27: Simplified code (thanks to Andi Kleen for clues).
 * 2000-05-20: Fixed notifier problems (following Miguel Freitas' report).
 * 2000-06-19: Fixed so nfmark is copied to metadata (reported by Sebastian
 *             Zander).
 * 2000-08-01: Added Nick Williams' MAC support.
 *
 */
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#include "packet_queue.h"

#define IPQ_QMAX_DEFAULT 1024
#define IPQ_PROC_FS_NAME "ip_queue"
#define NET_IPQ_QMAX 2088
#define NET_IPQ_QMAX_NAME "ip_queue_maxlen"

typedef struct ipq_rt_info {
    __u8 tos;
    __u32 daddr;
    __u32 saddr;
} ipq_rt_info_t;

typedef struct ipq_queue_element {
    struct list_head list;		/* Links element into queue */
    int verdict;			/* Current verdict */
    struct nf_queue_entry *entry;		/* Extra info from netfilter */
    struct sk_buff *skb;		/* Packet inside */
    ipq_rt_info_t rt_info;		/* May need post-mangle routing */
} ipq_queue_element_t;

typedef int (*ipq_send_cb_t)(ipq_queue_element_t *e);

typedef struct ipq_queue {
    int len;			/* Current queue len */
    int *maxlen;			/* Maximum queue len, via sysctl */
    unsigned char flushing;		/* If queue is being flushed */
    unsigned char terminate;	/* If the queue is being terminated */
    struct list_head list;		/* Head of packet queue */
  //  int queuenum;
    spinlock_t lock;		/* Queue spinlock */
} ipq_queue_t;

static ipq_queue_t *q;


//static int netfilter_receive(struct sk_buff *skb, struct nf_info *info, void *data);
static int netfilter_receive(struct nf_queue_entry *entry, unsigned int queuenum);

static const struct nf_queue_handler nfqh = {
	.name = "nf_receive",
	.outfn = &netfilter_receive,
};

/****************************************************************************
 *
 * Packet queue
 *
 ****************************************************************************/
/* Dequeue a packet if matched by cmp, or the next available if cmp is NULL */
static ipq_queue_element_t * ipq_dequeue( int (*cmp)(ipq_queue_element_t *, u_int32_t, u_int32_t, unsigned char), u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos)
{
    struct list_head *i;
    
    spin_lock_bh(&q->lock);
    for (i = q->list.prev; i != &q->list; i = i->prev) {
        ipq_queue_element_t *e = (ipq_queue_element_t *)i;

        if (!cmp || cmp(e, src_ip, dst_ip, tos)) {
            list_del(&e->list);
            q->len--;
            spin_unlock_bh(&q->lock);
            return e;
        }
    }
    spin_unlock_bh(&q->lock);
    return NULL;
}


/* Flush all packets */
static void ipq_flush(void)
{
    ipq_queue_element_t *e;
   
    spin_lock_bh(&q->lock);
    q->flushing = 1;
    spin_unlock_bh(&q->lock);
    while ((e = ipq_dequeue( NULL, 0,0,0))) {
        e->verdict = NF_DROP;
        nf_reinject(e->entry, e->verdict);
        kfree(e);
    }
    spin_lock_bh(&q->lock);
    q->flushing = 0;
    spin_unlock_bh(&q->lock);
}


static int ipq_create_queue(int *sysctl_qmax)
{
    int status;
    
    q = kmalloc(sizeof(ipq_queue_t), GFP_KERNEL);
    if (q == NULL) {
        return 0;
    }
    q->len = 0;
    q->maxlen = sysctl_qmax;
    q->flushing = 0;
    q->terminate = 0;
    INIT_LIST_HEAD(&q->list);
    spin_lock_init(&q->lock);

    status = nf_register_queue_handler(PF_INET, &nfqh);
    if (status < 0) {
      printk("Could not create Packet Queue! %d\n",status);
        kfree(q);
        return 0;
    }
    return 1;
}

static int ipq_enqueue(ipq_queue_t *g, struct nf_queue_entry *entry)
{
    ipq_queue_element_t *e;

    if (q!=g)
    {
        printk("trying to enqueue but do not have right queue!!!\n");
        return 0;
    }

    e = kmalloc(sizeof(*e), GFP_ATOMIC);
    if (e == NULL) {
        printk(KERN_ERR "ip_queue: OOM in enqueue\n");
        return -ENOMEM;
    }

    e->verdict = NF_DROP;
    e->entry = entry;
    e->skb = entry->skb;

    if (e->entry->hook == NF_INET_POST_ROUTING ) {
        struct iphdr *iph = ip_hdr(e->skb);

        e->rt_info.tos = iph->tos;
        e->rt_info.daddr = iph->daddr;
        e->rt_info.saddr = iph->saddr;
    }

    spin_lock_bh(&q->lock);
    if (q->len >= *q->maxlen) {
        spin_unlock_bh(&q->lock);
        if (net_ratelimit())
            printk(KERN_WARNING "ip_queue: full at %d entries, "
                   "dropping packet(s).\n", q->len);
        goto free_drop;
    }
    if (q->flushing || q->terminate) {
        spin_unlock_bh(&q->lock);
        goto free_drop;
    }
    list_add(&e->list, &q->list);
    q->len++;
    spin_unlock_bh(&q->lock);
#ifdef DEBUG
	printk ( "ipq_enqueue: Enqueue done, len: %d\n", q->len );
#endif	
    return q->len;
free_drop:
    kfree(e);
    return -EBUSY;
}

static void ipq_destroy_queue(void)
{
	
    nf_unregister_queue_handler(PF_INET,&nfqh);

    spin_lock_bh(&q->lock);
    q->terminate = 1;
    spin_unlock_bh(&q->lock);
    ipq_flush();
    kfree(q);
}

/* With a chainsaw... */
static int route_me_harder(struct sk_buff *skb)
{	
    struct iphdr *iph = ip_hdr(skb);
    struct rtable *rt;
#ifdef DEBUG
	char src[20];
	char dst[20];
	strcpy(src, inet_ntoa(iph->saddr));
	strcpy(dst, inet_ntoa(iph->daddr));

	printk ("route_me_harder: prepare for send, from %s to %s\n", src, dst);
#endif
    //wujingbang
	struct flowi4 fl4 = {
			.daddr = iph->daddr,
			.saddr = iph->saddr,
			.flowi4_tos = RT_TOS(iph->tos)|0/*RTO_CONN*/
//			.flowi4_oif = tb[RTA_OIF] ? nla_get_u32(tb[RTA_OIF]) : 0,
//			.flowi4_mark = mark,
	};


//wujingbang
//    if (ip_route_output_key(&init_net, &rt, &key) != 0) {
//    if (ip_route_output_key(&init_net, &(key.u.ip4)) != 0) {
//   	#ifdef DEBUG
//        printk("route_me_harder: No more route.\n");
//	      #endif 
//	       return -EINVAL;
//   }
	
	struct net *net = sock_net(skb->sk);
	rt = ip_route_output_flow(net, &fl4, NULL);
	if ( rt == NULL ) {
#ifdef DEBUG
		printk("route_me_harder: No more route.\n");
#endif 
	       return -EINVAL;
	}
#ifdef DEBUG
	else {
		strcpy(src, inet_ntoa(rt->rt_src));
		strcpy(dst, inet_ntoa(rt->rt_dst));
		printk("route_me_harder: Routing table %s to %s.\n", src, dst);
		printk("Packet protocol: %d\n", iph->protocol);
	}
#endif 
    
    /* Drop old route. */
#ifdef KERNEL2_6_26
    dst_release(skb->dst);
//    skb->dst = &rt->u.dst;
#else
    skb_dst_drop(skb);
//   skb_dst_set(skb, &rt->u.dst);
    skb_dst_set(skb, &rt->dst);
#endif
    return 0;
}

static inline int id_cmp(ipq_queue_element_t *e,u_int32_t id)
{
	
    return (id == (unsigned long )e);
}


static inline int dev_cmp(ipq_queue_element_t *e, u_int32_t ifindex, u_int32_t fill1, unsigned char fill2)
{
	
    if (e->entry->indev)
        if (e->entry->indev->ifindex == ifindex)
            return 1;
    if (e->entry->outdev)
        if (e->entry->outdev->ifindex == ifindex)
            return 1;
    return 0;
}

static inline int ip_cmp(ipq_queue_element_t *e, u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos)
{
	
	
    if ((e->rt_info.saddr == src_ip) && (e->rt_info.daddr == dst_ip) && (e->rt_info.tos == tos))
        return 1;
    
    return 0;
}


/* Drop any queued packets associated with device ifindex */
static void ipq_dev_drop(int ifindex)
{
    ipq_queue_element_t *e;
   
    while ((e = ipq_dequeue(dev_cmp,  ifindex, 0, 0))) {
        e->verdict = NF_DROP;
        nf_reinject(e->entry, e->verdict);
        kfree(e);
    }
}

//MCC - source and ToS
void ipq_send_ip(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos)
{
	ipq_queue_element_t *e;
	e = ipq_dequeue( ip_cmp, src_ip, dst_ip, tos);
	if ( e== NULL ) {
#ifdef DEBUG
		printk ("ipq_send_ip: ipq_dequeue return NULL!!\n");		
#endif
		return;
	}

	do {
		
#ifdef DEBUG
		if ( e ) 
			printk ("ipq_send_ip: begin to send!\n");
#endif

		e->verdict = NF_ACCEPT;
#ifdef DEBUG
		struct iphdr *iph = ip_hdr(e->skb);
		printk ("ipq_send_ip: e->skb->protocol: %d \n", e->skb->protocol);
		printk ("ipq_send_ip: iph->protocol: %d \n", iph->protocol);
#endif 
	   
        	route_me_harder(e->skb);
        	nf_reinject(e->entry, e->verdict);

        	kfree(e);
	} while((e = ipq_dequeue( ip_cmp, src_ip, dst_ip, tos))) ;
}
//MCC - source and ToS


void ipq_drop_ip(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos)
{
    ipq_queue_element_t *e;
    
    while ((e = ipq_dequeue( ip_cmp,  src_ip, dst_ip, tos))) {
    e->verdict = NF_DROP;
	icmp_send(e->skb,ICMP_DEST_UNREACH,ICMP_HOST_UNREACH,0);
	nf_reinject(e->entry, e->verdict);
        kfree(e);
    }
}


/****************************************************************************
 *
 * Netfilter interface
 *
 ****************************************************************************/

/*
 * Packets arrive here from netfilter for queuing to userspace.
 * All of them must be fed back via nf_reinject() or Alexey will kill Rusty.
 */
/*int ipq_insert_packet(struct sk_buff *skb,
                      struct nf_info *info)
{
	printk("aqui13\n");
    return ipq_enqueue(q, skb, info);
}
*/

static int netfilter_receive(struct nf_queue_entry *entry,unsigned int queuenum)
{
#ifdef DEBUG
	printk ( "netfilter_receive: Now enqueue the packet!\n" );
#endif	
    	return ipq_enqueue(q, entry);
}

/****************************************************************************
 *
 * System events
 *
 ****************************************************************************/

static int receive_event(struct notifier_block *this,
                         unsigned long event, void *ptr)
{
    struct net_device *dev = ptr;
    
    /* Drop any packets associated with the downed device */
    if (event == NETDEV_DOWN)
        ipq_dev_drop( dev->ifindex);
    return NOTIFY_DONE;
}

struct notifier_block ipq_dev_notifier = {
            receive_event,
            NULL,
            0
        };

/****************************************************************************
 *
 * Sysctl - queue tuning.
 *
 ****************************************************************************/

static int sysctl_maxlen = IPQ_QMAX_DEFAULT;

static struct ctl_table_header *ipq_sysctl_header;

//wujingbang
static ctl_table ipq_table[] = {
	{
//		.ctl_name	= NET_IPQ_QMAX,
		.procname	= NET_IPQ_QMAX_NAME,
		.data		= &sysctl_maxlen,
		.maxlen		= sizeof(sysctl_maxlen),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{  }
};

static ctl_table ipq_dir_table[] = {
                                       {NET_IPV4, "ipv4", NULL, 4, 0555, ipq_table, 0, 0, 0, 0, 0},
                                       { 0 }
                                   };

static ctl_table ipq_root_table[] = {
                                        {CTL_NET, "net", NULL, 4, 0555, ipq_dir_table, 0, 0, 0, 0, 0},
                                        { 0 }
                                    };

/****************************************************************************
 *
 * Module stuff.
 *
 ****************************************************************************/

int init_packet_queue(void)
{
    int status = 0;
    //struct proc_dir_entry *proc;

    status= ipq_create_queue(&sysctl_maxlen);
    if (status == 0) {
        printk(KERN_ERR "ip_queue: initialisation failed: unable to "
               "create queue\n");

        return status;
    }
    /*	proc = proc_net_create(IPQ_PROC_FS_NAME, 0, ipq_get_info);
    	if (proc) proc->owner = THIS_MODULE;
    	else {
    		ipq_destroy_queue(nlq);
    		sock_release(nfnl->socket);
    		return -ENOMEM;
    	}*/
//#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
//     ipq_sysctl_header = register_sysctl_table(ipq_root_table);
//#else
//   ipq_sysctl_header = register_sysctl_table(ipq_root_table, 0);
//#endif
    return status;
}

void cleanup_packet_queue(void)
{
    unregister_sysctl_table(ipq_sysctl_header);
    //proc_net_remove(IPQ_PROC_FS_NAME);

    ipq_destroy_queue();

}



