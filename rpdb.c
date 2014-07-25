/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
//RTNETLINK - Routes and Rules
#include "rpdb.h"

extern u_int32_t g_mesh_ip;
extern char g_aodv_dev[8];
extern u_int32_t g_null_ip;

struct {
	struct nlmsghdr nl;
	struct rtmsg rt;
	char buf[400];
} req;

struct sockaddr_nl pa;
struct msghdr msg;
struct iovec iov;

// RTNETLINK message pointers & lengths
// used when processing messages
struct rtattr *rtap;
int rtl;

int rpdb_rule(unsigned char type, int rt_table, u_int32_t ip_src,
		u_int32_t dst_ip) {
	int len, error;
	mm_segment_t oldfs;
	struct socket *rl_socket;
	int normal = 1;
	int table = 0;

#ifdef AODV_SIGNALLING
	printk("RPDB: Adding or deleting a rule\n");
#endif

	if (dst_ip == g_null_ip) //normal rule creation
		table = rt_table + 1000;
	else {//local rule creation
		table = rt_table + 500;
		normal = 0;
	}

	error = sock_create(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE, &rl_socket);
	if (error < 0) {
		printk("Error during creation of route socket; terminating, %d\n",
				error);
		return (-1);
	}

	init_sock(rl_socket, g_mesh_ip, g_aodv_dev);
	// initalize RTNETLINK request buffer

	// compute the initial length of the
	// service request
	memset(&req, 0, sizeof(req));
	memset(&rtap, 0, sizeof(rtap));
	rtl = sizeof(struct rtmsg);

	if (type == RTM_NEWRULE) {

		if ((ip_src == g_mesh_ip) || (!normal)) {

			// set SRC IP addr - from all
			rtap = (struct rtattr *) req.buf;
			rtap->rta_type = RTA_SRC;
			rtap->rta_len = sizeof(struct rtattr) + 4;
			memcpy(((char *)rtap) + sizeof(struct rtattr), &g_null_ip, 4);
			rtl += rtap->rta_len;
			// set the network prefix size
			req.rt.rtm_src_len = 0;

			if (normal) {
				char *device = "lo";
				//set interface - dev lo
				rtap = (struct rtattr *) (((char *)rtap) + rtap->rta_len);
				rtap->rta_type = RTA_IIF;
				rtap->rta_len = sizeof(struct rtattr) + 4;
				memcpy(((char *)rtap) + sizeof(struct rtattr), device, 4);
				rtl += rtap->rta_len;

			} else {
				// set DST_IP addr
				rtap = (struct rtattr *) (((char *)rtap) + rtap->rta_len);
				rtap->rta_type = RTA_DST;
				rtap->rta_len = sizeof(struct rtattr) + 4;
				memcpy(((char *)rtap) + sizeof(struct rtattr), &dst_ip, 4);
				rtl += rtap->rta_len;
				// set the network prefix size
				req.rt.rtm_dst_len = 32;
			}

		}

		else {

			// set SRC IP addr and increment the RTNETLINK buffer size
			rtap = (struct rtattr *) req.buf;
			rtap->rta_type = RTA_SRC;
			rtap->rta_len = sizeof(struct rtattr) + 4;
			memcpy(((char *)rtap) + sizeof(struct rtattr), &ip_src, 4);
			rtl += rtap->rta_len;

			// set the network prefix size
			req.rt.rtm_src_len = 32;

		}

		//set rule priority (simply the route table)  and increment the size
		rtap = (struct rtattr *) (((char *)rtap) + rtap->rta_len);
		rtap->rta_type = RTA_PRIORITY;
		rtap->rta_len = sizeof(struct rtattr) + 4;
		memcpy(((char *)rtap) + sizeof(struct rtattr), &table, 4);
		rtl += rtap->rta_len;
		//Let first 1000 priority rules to other services
	}

	else {//for rule deletion, only use  the  priority
		//set rule priority (simply the route table)  and increment the size
		rtap = (struct rtattr *) req.buf;
		rtap->rta_type = RTA_PRIORITY;
		rtap->rta_len = sizeof(struct rtattr) + 4;
		memcpy(((char *)rtap) + sizeof(struct rtattr), &table, 4);
		rtl += rtap->rta_len;
		//Let first 1000 priority rules to other services

	}

	// setup the NETLINK header
	req.nl.nlmsg_len = NLMSG_LENGTH(rtl);
	req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
	req.nl.nlmsg_type = type;

	// setup the service header (struct rtmsg)
	req.rt.rtm_family = AF_INET;
	if (normal)
		req.rt.rtm_table = (unsigned char)rt_table;
	else
		req.rt.rtm_table = (unsigned char)DEFAULT_ROUTE_TABLE;

	req.rt.rtm_protocol = RTPROT_STATIC;
	req.rt.rtm_scope = RT_SCOPE_UNIVERSE;
	req.rt.rtm_type = RTN_UNICAST;

	// create the remote address
	// to communicate
	memset(&pa, 0, sizeof(pa));
	pa.nl_family = AF_NETLINK;

	// initialize & create the struct msghdr supplied
	// to the sendmsg() function
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *) &pa;
	msg.msg_namelen = sizeof(pa);

	// place the pointer & size of the RTNETLINK
	// message in the struct msghdr
	memset(&iov, 0, sizeof(iov));
	iov.iov_base = (void *) &req.nl;
	iov.iov_len = req.nl.nlmsg_len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	// send the RTNETLINK message to kernel
	len = sock_sendmsg(rl_socket, &msg, (size_t)NLMSG_LENGTH(rtl) );
	if (len < 0)
		printk("Error sending! err no: %d\n", len);

	set_fs(oldfs);
	sock_release(rl_socket);

	return (len);

}

int rpdb_route(unsigned char type, int rt_table, unsigned char tos,
		u_int32_t ip_src, u_int32_t ip_dst, u_int32_t ip_gw/*(next_hop)*/, int dev_index,
		int metric) {
	int len, error;
	mm_segment_t oldfs;
	struct socket *rt_socket;

//#ifdef AODV_SIGNALLING
//#ifdef DEBUG
	char src[20];
	char dst[20];
	char nex[20];
	strcpy(src, inet_ntoa(ip_src));
	strcpy(dst, inet_ntoa(ip_dst));
	strcpy(nex, inet_ntoa(ip_gw));
	printk("RPDB: Adding or deleting a route %s to %s via %s,type is %c\n", src, dst, nex,type);
//#endif
	error = sock_create(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE, &rt_socket);
	if (error < 0) {
		printk("Error during creation of route socket; terminating, %d\n",
				error);
		return (-1);
	}

	init_sock(rt_socket, g_mesh_ip, g_aodv_dev);

	memset(&req, 0, sizeof(req));
	memset(&rtap, 0, sizeof(rtap));
	// compute the initial length of the
	// service request
	rtl = sizeof(struct rtmsg);

	// add first attrib:

	// set DST IP addr and increment the RTNETLINK buffer size
	rtap = (struct rtattr *) req.buf;
	rtap->rta_type = RTA_DST;
	rtap->rta_len = sizeof(struct rtattr) + 4;
	memcpy(((char *)rtap) + sizeof(struct rtattr), &ip_dst, 4);
	rtl += rtap->rta_len;

	// set the network prefix size
	if (ip_dst != g_null_ip)
		req.rt.rtm_dst_len = 32;
	else
		req.rt.rtm_dst_len = 0;

	// add second attrib:
	// set mesh interface index and increment the size
	rtap = (struct rtattr *) (((char *)rtap) + rtap->rta_len);
	rtap->rta_type = RTA_OIF;
	rtap->rta_len = sizeof(struct rtattr) + 4;
	memcpy(((char *)rtap) + sizeof(struct rtattr), &dev_index, 4);
	rtl += rtap->rta_len;

	//add third attrib
	//set GW_IP (next_hop)  and increment the size
	rtap = (struct rtattr *) (((char *)rtap) + rtap->rta_len);
	rtap->rta_type = RTA_GATEWAY;
	rtap->rta_len = sizeof(struct rtattr) + 4;
	memcpy(((char *)rtap) + sizeof(struct rtattr), &ip_gw, 4);
	rtl += rtap->rta_len;

	//add fourth attrib
	//metric
	rtap = (struct rtattr *) (((char *)rtap) + rtap->rta_len);
	rtap->rta_type = RTA_PRIORITY;
	rtap->rta_len = sizeof(struct rtattr) + 4;
	memcpy(((char *)rtap) + sizeof(struct rtattr), &metric, 4);
	rtl += rtap->rta_len;

	// setup the NETLINK header
	req.nl.nlmsg_len = NLMSG_LENGTH(rtl);
	req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
	req.nl.nlmsg_type = type;

	// setup the service header (struct rtmsg)
	req.rt.rtm_family = AF_INET;

	//set the rt_table used and the ToS of the traffic
	req.rt.rtm_table = (unsigned char)rt_table;

	req.rt.rtm_tos = tos;

	req.rt.rtm_protocol = RTPROT_STATIC;

	if ((ip_dst == g_mesh_ip))
		req.rt.rtm_scope = RT_SCOPE_LINK; //Self-Route
	else
		req.rt.rtm_scope = RT_SCOPE_UNIVERSE;

	req.rt.rtm_type = RTN_UNICAST;

	// create the remote address
	// to communicate
	memset(&pa, 0, sizeof(pa));
	pa.nl_family = AF_NETLINK;

	// initialize & create the struct msghdr supplied
	// to the sendmsg() function
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *) &pa;
	msg.msg_namelen = sizeof(pa);

	// place the pointer & size of the RTNETLINK
	// message in the struct msghdr
	memset(&iov, 0, sizeof(iov));
	iov.iov_base = (void *) &req.nl;
	iov.iov_len = req.nl.nlmsg_len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	// send the RTNETLINK message to kernel
	len = sock_sendmsg(rt_socket, &msg, (size_t)NLMSG_LENGTH(rtl) );

	if (len < 0)
		printk("Error sending! err no: %d,on interface: %d\n", len, dev_index);

	set_fs(oldfs);

	sock_release(rt_socket);

	return (len);

}

