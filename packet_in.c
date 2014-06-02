/***************************************************************************
 packet_in.c  -  description
 -------------------
 begin                : Mon Jul 29 2002
 copyright            : (C) 2002 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "packet_in.h"

#ifdef DTN
	extern int dtn_register;
#endif
#ifdef BLACKLIST
	extern u_int32_t aodv_blacklist_ip[10];
	extern u_int32_t dtn_blacklist_ip[10];
	extern int aodv_blacksize;
	extern int dtn_blacksize;
#endif

int valid_aodv_packet(int numbytes, int type, char *data, struct timeval tv) {

	rerr *tmp_rerr;

	rrep *tmp_rrep;
	hello *tmp_hello;
	rreq *tmp_rreq;
	rreq_st *tmp_strreq;
	rcvp *tmp_rcvp;

	s_probe *tmp_sprobe;
	l_probe *tmp_lprobe;
	ett_info *tmp_ett_info;

	switch (type) {

	case ST_RREQ_MESSAGE:
		tmp_strreq = (rreq_st *) data;
		if (numbytes == sizeof(rreq_st)) {
			return 1;
		}
		break;

	case RREP_MESSAGE:

		tmp_rrep = (rrep *) data;
		if (numbytes == sizeof(rrep)) {
			return 1;
		}
		break;

	case RERR_MESSAGE:
		tmp_rerr = (rerr *) data;
		if (numbytes == sizeof(rerr)) {
			return 1;
		}
		break;

	case HELLO_MESSAGE:
		tmp_hello = (hello *) data;
		if (numbytes == (sizeof(hello) + (sizeof(hello_extension)
				* tmp_hello->neigh_count))) {
			return 1;
		}
		break;

    /****************Ìí¼ÓÀàÐÍRCVP_MESSAGE*******************/
    case RCVP_MESSAGE:
        tmp_rcvp = (rcvp *) data;
        if(numbytes == sizeof(rcvp))
        return 1;
        break;

	case ETT_S_MESSAGE: //ETT small message received
		tmp_sprobe = (s_probe *) data;
		if (numbytes == sizeof(s_probe)) {
			tmp_sprobe->sec = (u_int32_t) tv.tv_sec;
			tmp_sprobe->usec= (u_int32_t) tv.tv_usec;
			return 1;
		}
		break;

	case ETT_L_MESSAGE: //ETT large message received
		tmp_lprobe = (l_probe *) data;
		if (numbytes == sizeof(l_probe)) {
			tmp_lprobe->sec = (u_int32_t) tv.tv_sec;
			tmp_lprobe->usec= (u_int32_t) tv.tv_usec;
			return 1;
		}
		break;

	case RREQ_MESSAGE:
		tmp_rreq = (rreq *) data;
		if (numbytes == sizeof(rreq))
			return 1;
		break;


	case ETT_INFO_MSG:
		tmp_ett_info = (ett_info *) data;
		if (numbytes == sizeof(ett_info))
			return 1;
		break;

	default:
		break;

	}
	return 0;
}

int packet_in(struct sk_buff *packet, struct timeval tv) {
	struct iphdr *ip;

	// Create aodv message types
	u_int8_t aodv_type;

	//The packets which come in still have their headers from the IP and UDP
	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);

	//get pointers to the important parts of the message
	ip = ip_hdr(packet);

	aodv_type = (int) packet->data[start_point];
#ifdef CaiDebug
    if( aodv_type == RCVP_MESSAGE)
        printk("It's a rcvp message\n");
#endif
#ifdef DEBUG
	if ( aodv_type != HELLO_MESSAGE )
		printk("packet_in: type: %d and of size %u from: %s\n", aodv_type, packet->len - start_point, inet_ntoa(ip->saddr));
#endif

	if (!valid_aodv_packet(packet->len - start_point, aodv_type, packet->data
			+ start_point, tv)) {

#ifdef DEBUG
		printk("Packet of type: %d and of size %u from: %s failed packet check!\n", aodv_type, packet->len - start_point, inet_ntoa(ip->saddr));
#endif
		return NF_DROP;

	}

	insert_task(aodv_type, packet);

	return NF_ACCEPT;
}

extern int initialized;
unsigned int input_handler(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *)) {
	struct timeval tv;

	u_int8_t aodv_type;
	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);
	aodv_type = (int) skb->data[start_point];

	struct iphdr *ip = ip_hdr(skb);
	struct in_device *tmp_indev = (struct in_device *) in->ip_ptr;

	void *p = (uint32_t *) ip + ip->ihl;
	struct udphdr *udp = (struct udphdr *) p;

	if (!initialized) { // this is required otherwise kernel calls this function without insmod completing the module loading process.
		return NF_ACCEPT;
	}


	char src[16];
	char dst[16];
	strcpy(src, inet_ntoa(ip->saddr));
	strcpy(dst, inet_ntoa(ip->daddr));


#ifdef DTN
	static int t = 1;
	//DTN register
	if (t && !dtn_register && udp->dest == htons(DTNREGISTER)){
		dtn_register = 1;
		t = 0;
		printk("*************DTN registered*************\n");
	}
#ifdef BLACKLIST
	int k = 0;
	if (udp->dest == htons(9556)) {
		for(k=0; k<dtn_blacksize; k++) {
			if (!strcmp("192.168.1.255",dst) && dtn_blacklist_ip[k] == ip->saddr) {
				return NF_DROP;
			}
		}
	}
#endif

#endif

#ifdef DEBUG
	if (ip->protocol == IPPROTO_ICMP) {
		printk("input_handler: Recv a ICMP from %s to %s\n", src, dst);
	} else if (aodv_type != HELLO_MESSAGE) {
		printk("input_handler: Recv a %d Packet, from %s to %s\n",ip->protocol, src, dst);
	}
#endif
	if (udp != NULL && ip->protocol == IPPROTO_UDP && skb->protocol == htons(ETH_P_IP) && udp->dest == htons(AODVPORT) && tmp_indev->ifa_list->ifa_address != ip->saddr) {

		do_gettimeofday(&tv); //MCC - capture the time of arrival needed for ett_probes
		return packet_in((skb), tv);

	}

#ifdef DEBUG
	if (aodv_type != HELLO_MESSAGE)
		printk("input_handler: input without process.\n");
#endif

	return NF_ACCEPT;

}

