/***************************************************************************
                          socket.c  -  description
                             -------------------
    begin                : Wed Aug 13 2003
    copyright            : (C) 2003 by Luke Klein-Berndt
    email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "socket.h"

#include <linux/ip.h>

static struct sockaddr_in sin;
extern u_int32_t g_broadcast_ip;
extern u_int32_t g_mesh_ip;
extern aodv_dev *g_mesh_dev;

int init_sock(struct socket *sock, u_int32_t ip, char *dev_name)
{
    int error;
    struct ifreq interface;
    mm_segment_t oldfs;

    //set the address we are sending from
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ip;
    sin.sin_port = htons(AODVPORT);

	/*sock->sk->reuse = 1;
    sock->sk->allocation = GFP_ATOMIC;
    sock->sk->priority = GFP_ATOMIC;*/

    sock->sk->sk_reuse = 1;
    sock->sk->sk_allocation = GFP_ATOMIC;
    sock->sk->sk_priority = GFP_ATOMIC;

    error = sock->ops->bind(sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));
    strncpy(interface.ifr_ifrn.ifrn_name, dev_name, IFNAMSIZ);

    oldfs = get_fs();
    set_fs(KERNEL_DS);          //thank to Soyeon Anh and Dinesh Dharmaraju for spotting this bug!
    error = sock_setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (char *) &interface, sizeof(interface)) < 0;
    set_fs(oldfs);



    if (error < 0)
    {
        printk("Error, %d  binding socket. This means that some other \n", error);
        printk("daemon is (or was a short time axgo) using port %i.\n", AODVPORT);
        return 0;
    }

    return 0;
}

void close_sock()
{
        sock_release(g_mesh_dev->sock);
        kfree(g_mesh_dev);
}


int local_broadcast(u_int8_t ttl, void *data,const size_t datalen)
{
	aodv_dev *tmp_dev;
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int len = 0;

    if (ttl < 1)
    {
        return 0;
    }
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = g_broadcast_ip;
    sin.sin_port = htons((unsigned short) AODVPORT);

    //define the message we are going to be sending out
    msg.msg_name = (void *) &(sin);
    msg.msg_namelen = sizeof(sin);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
//    msg.msg_flags = MSG_NOSIGNAL;
    msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
    msg.msg_iov->iov_len = (__kernel_size_t) datalen;
    msg.msg_iov->iov_base = (char *) data;

    tmp_dev = g_mesh_dev;

    if ((tmp_dev) && (tmp_dev->sock) && (sock_wspace(tmp_dev->sock->sk) >= datalen))
    {
    	//tmp_dev->sock->sk->broadcast = 1;
		sock_set_flag( tmp_dev->sock->sk,SOCK_BROADCAST);
	#ifdef NOMIPS
	//tmp_dev->sock->sk->protinfo.af_inet.uc_ttl = ttl;
    inet_sk(tmp_dev->sock->sk)->uc_ttl = ttl;
    #else
        //tmp_dev->sock->sk->protinfo.af_inet.ttl = ttl;
        inet_sk(tmp_dev->sock->sk)->uc_ttl = ttl;
    #endif
		   oldfs = get_fs();
        set_fs(KERNEL_DS);

        len = sock_sendmsg(tmp_dev->sock, &msg,(size_t) datalen);

        if (len < 0)
            printk("Error sending! err no: %d,on interface: %s\n", len, tmp_dev->dev->name);
        set_fs(oldfs);
   }

    return len;
}

#ifdef DTN

static int bind_to_device(struct socket *sock, char *ifname)//get local ip,bind with socket
{
    struct net *net;
    struct net_device *dev;
    __be32 addr;
    struct sockaddr_in sin;
    int err;
    net = sock_net(sock->sk);
    dev = __dev_get_by_name(net, ifname);

    if (!dev) {
        printk(KERN_ALERT "No such device named %s\n", ifname);
        return -ENODEV;
    }
    addr = inet_select_addr(dev, 0, RT_SCOPE_UNIVERSE);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = addr;
    sin.sin_port = 0;
    err = sock->ops->bind(sock, (struct sockaddr*)&sin, sizeof(sin));
    if (err < 0) {
        printk(KERN_ALERT "sock bind err, err=%d\n", err);
        return err;
    }
    return 0;
}
static int connect_to_addr(struct socket *sock, u_int32_t dstip)//get aim ip and port, connect it with socket
{
    struct sockaddr_in daddr;
    int err;
    daddr.sin_family = AF_INET;
    daddr.sin_addr.s_addr = dstip;

//    daddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
//	inet_aton(ser_ip, &(daddr.sin_addr.s_addr));
//    printk("0x%lx\n",INADDR_BROADCAST);
    daddr.sin_port = DTNPORT;
    err = sock->ops->connect(sock, (struct sockaddr*)&daddr,
            sizeof(struct sockaddr), 0);
    if (err < 0) {
        printk("sock connect err, err=%d\n", err);
        return err;
    }
    return 0;
}
/**
 * send RRER info to DTN
 * @dst_ip:断路的目的地
 * @last_avail_ip:
 */
/*
int send2dtn(void * data){

	u_int32_t dst_ip = ((u_int32_t *)data)[0];
	u_int32_t last_avail_ip = ((u_int32_t *)data)[1];
	int datalen = 12;
//#ifdef DEBUG
	char s1[16],s2[16];
	strcpy(s1, inet_ntoa(dst_ip));
	strcpy(s2, inet_ntoa(last_avail_ip));
	printk("send RRER info to DTN, dst: %s, last_avail: %s\n", s1, s2);
//#endif



    struct msghdr msg;
    struct iovec iov;
    aodv_dev *tmp_dev;
    mm_segment_t oldfs;
    u_int32_t space;
    int len;

    aodv_neigh *tmp_neigh;


    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = g_mesh_ip;
    sin.sin_port = htons((unsigned short) DTNPORT);

    //define the message we are going to be sending out
    msg.msg_name = (void *) &(sin);
    msg.msg_namelen = sizeof(sin);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
    msg.msg_iov->iov_len = (__kernel_size_t) datalen;
    msg.msg_iov->iov_base = data;
#ifdef DEBUG
    printk("%x\n", ((u_int32_t *)data)[0]);
    printk("%x\n", ((u_int32_t *)data)[1]);
#endif
	tmp_dev = g_mesh_dev;

    if (tmp_dev == NULL)
    {
        printk("Error sending! Unable to find interface!\n");
        return 0;
    }

    space = sock_wspace(tmp_dev->sock->sk);

    if (space < datalen)
    {
        printk("Space: %d, Data: %d \n", space, (int)datalen);
        return 0;
    }

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    len = sock_sendmsg(tmp_dev->sock, &msg,(size_t) datalen);
    if (len < 0)
    {
        printk("Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(dst_ip));
    }
    set_fs(oldfs);
    return 0;
}
*/
/*
//overwrite the above function to a function with three paras
int send2dtn(void * data){

	u_int32_t dst_ip = ((u_int32_t *)data)[0];
	u_int32_t last_avail_ip = ((u_int32_t *)data)[1];
	u_int32_t src_ip = ((u_int32_t *)data)[2];
	int datalen = 18;
//#ifdef DEBUG
	char s1[16],s2[16],s3[16];
	strcpy(s1, inet_ntoa(dst_ip));
	strcpy(s2, inet_ntoa(last_avail_ip));
	strcpy(s3, inet_ntoa(src_ip));
	printk("send RRER info to DTN, dst: %s, last_avail: %s,src: %s\n", s1, s2, s3);
//#endif



    struct msghdr msg;
    struct iovec iov;
    aodv_dev *tmp_dev;
    mm_segment_t oldfs;
    u_int32_t space;
    int len;

    aodv_neigh *tmp_neigh;



    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    sin.sin_addr.s_addr = g_mesh_ip;
    sin.sin_port = htons((unsigned short) DTNPORT);


    //define the message we are going to be sending out
    msg.msg_name = (void *) &(sin);
    msg.msg_namelen = sizeof(sin);
    msg.msg_iov = &iov;

    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;

    msg.msg_iov->iov_len = (__kernel_size_t) datalen;
    msg.msg_iov->iov_base = data;
#ifdef DEBUG

    printk("%x\n", ((u_int32_t *)data)[0]);
    printk("%x\n", ((u_int32_t *)data)[1]);
    printk("%x\n", ((u_int32_t *)data)[2]);
#endif
	tmp_dev = g_mesh_dev;


    if (tmp_dev == NULL)
    {
        printk("Error sending! Unable to find interface!\n");

        return 0;
    }

    space = sock_wspace(tmp_dev->sock->sk);


    if (space < datalen)
    {

        printk("Space: %d, Data: %d \n", space, (int)datalen);
        return 0;
    }


    oldfs = get_fs();
    set_fs(KERNEL_DS);

    len = sock_sendmsg(tmp_dev->sock, &msg,(size_t) datalen);

    if (len < 0)
    {
        printk("Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(dst_ip));

    }
    set_fs(oldfs);
    return 0;
}*/

//Cai Bingying : overwrite the above function to a function with four paras
//it's used in sending rerr and rcvp to DTN
int send2dtn(void * data){

    	u_int32_t src_ip = ((u_int32_t *)data)[0];
	u_int32_t dst_ip = ((u_int32_t *)data)[1];
	u_int32_t last_avail_ip = ((u_int32_t *)data)[2];
	u_int32_t type = ((u_int32_t *)data)[3];
	//int datalen = 18;
	int datalen = 24;
//#ifdef DEBUG
	char s1[16],s2[16],s3[16];
	strcpy(s1, inet_ntoa(dst_ip));
	strcpy(s2, inet_ntoa(last_avail_ip));
	strcpy(s3, inet_ntoa(src_ip));
	printk("send RRER or RCVP info to DTN, dst: %s, last_avail: %s,src: %s,type:%s\n", s1, s2, s3,inet_ntoa(type));
//#endif



    struct msghdr msg;
    struct iovec iov;
    aodv_dev *tmp_dev;
    mm_segment_t oldfs;
    u_int32_t space;
    int len;

    aodv_neigh *tmp_neigh;



    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    sin.sin_addr.s_addr = g_mesh_ip;
    sin.sin_port = htons((unsigned short) DTNPORT);


    //define the message we are going to be sending out
    msg.msg_name = (void *) &(sin);
    msg.msg_namelen = sizeof(sin);
    msg.msg_iov = &iov;

    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;

    msg.msg_iov->iov_len = (__kernel_size_t) datalen;
    msg.msg_iov->iov_base = data;
#ifdef DEBUG

    printk("%x\n", ((u_int32_t *)data)[0]);
    printk("%x\n", ((u_int32_t *)data)[1]);
    printk("%x\n", ((u_int32_t *)data)[2]);
#endif
	tmp_dev = g_mesh_dev;


    if (tmp_dev == NULL)
    {
        printk("Error sending! Unable to find interface!\n");

        return 0;
    }

    space = sock_wspace(tmp_dev->sock->sk);


    if (space < datalen)
    {

        printk("Space: %d, Data: %d \n", space, (int)datalen);
        return 0;
    }


    oldfs = get_fs();
    set_fs(KERNEL_DS);

    len = sock_sendmsg(tmp_dev->sock, &msg,(size_t) datalen);

    if (len < 0)
    {
        printk("Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(dst_ip));

    }
    set_fs(oldfs);
    return 0;
}
#endif

int send_message(u_int32_t dst_ip, u_int8_t ttl, void *data, const size_t datalen)
{
    struct msghdr msg;
    struct iovec iov;
    aodv_dev *tmp_dev;
    mm_segment_t oldfs;
    u_int32_t space;
    int len;

    aodv_neigh *tmp_neigh;


    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = dst_ip;
    sin.sin_port = htons((unsigned short) AODVPORT);

    //define the message we are going to be sending out
    msg.msg_name = (void *) &(sin);
    msg.msg_namelen = sizeof(sin);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
    msg.msg_iov->iov_len = (__kernel_size_t) datalen;
    msg.msg_iov->iov_base = (char *) data;

    if (ttl == 0)
        return 0;

		tmp_neigh = find_aodv_neigh(dst_ip); //dst_ip must be a neighbor

		if (tmp_neigh == NULL)
    		{
       	 	printk("Send_Message: Can't find neigh %s \n", inet_ntoa(dst_ip));
       	 	return 0;

    		}
	tmp_dev = g_mesh_dev;

    if (tmp_dev == NULL)
    {
        printk("Error sending! Unable to find interface!\n");
        return 0;
    }

    space = sock_wspace(tmp_dev->sock->sk);

    if (space < datalen)
    {
        printk("Space: %d, Data: %d \n", space, (int)datalen);
        return 0;
    }

    //tmp_dev->sock->sk->broadcast = 0;
    sock_reset_flag( tmp_dev->sock->sk,SOCK_BROADCAST);
    #ifdef NOMIPS
    //tmp_dev->sock->sk->protinfo.af_inet.uc_ttl = ttl;
   	 inet_sk(tmp_dev->sock->sk)->uc_ttl = ttl;
    #else
    //tmp_dev->sock->sk->protinfo.af_inet.ttl = ttl;
    inet_sk(tmp_dev->sock->sk)->uc_ttl = ttl;
    #endif
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    len = sock_sendmsg(tmp_dev->sock, &msg,(size_t) datalen);
    if (len < 0)
    {
        printk("Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(dst_ip));
    }
    set_fs(oldfs);
    return 0;
}


int send_ett_probe(u_int32_t dst_ip, void *data1, const size_t datalen1, void *data2, const size_t datalen2){

    struct msghdr msg1, msg2;
    struct iovec iov1, iov2;
    aodv_dev *tmp_dev;
    mm_segment_t oldfs;
    u_int32_t space;
    int len;

    aodv_neigh *tmp_neigh;


    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = dst_ip;
    sin.sin_port = htons((unsigned short) AODVPORT);

    //define the message we are going to be sending out
    msg1.msg_name = (void *) &(sin);
    msg1.msg_namelen = sizeof(sin);
    msg1.msg_iov = &iov1;
    msg1.msg_iovlen = 1;
    msg1.msg_control = NULL;
    msg1.msg_controllen = 0;
    msg1.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
    msg1.msg_iov->iov_len = (__kernel_size_t) datalen1;
    msg1.msg_iov->iov_base = (char *) data1;

    msg2.msg_name = (void *) &(sin);
    msg2.msg_namelen = sizeof(sin);
    msg2.msg_iov = &iov2;
    msg2.msg_iovlen = 1;
    msg2.msg_control = NULL;
    msg2.msg_controllen = 0;
    msg2.msg_flags = MSG_DONTWAIT | MSG_NOSIGNAL;
    msg2.msg_iov->iov_len = (__kernel_size_t) datalen2;
    msg2.msg_iov->iov_base = (char *) data2;

	tmp_neigh = find_aodv_neigh(dst_ip); //dst_ip must be a neighbor

	if (tmp_neigh == NULL)
    	{
       	 printk("Send_Message: Can't find neigh %s \n", inet_ntoa(dst_ip));
       	 return 0;

    	}
	tmp_dev = g_mesh_dev;

    if (tmp_dev == NULL)
    {
        printk("Error sending! Unable to find interface!\n");
        return 0;
    }

    space = sock_wspace(tmp_dev->sock->sk);

    if ((space < datalen1) || (space < datalen2))
    {
        printk("Space: %d, Data: %d \n", space, (int)datalen1);
	printk("Space: %d, Data: %d \n", space, (int)datalen2);
        return 0;
    }


    sock_reset_flag( tmp_dev->sock->sk,SOCK_BROADCAST);
    #ifdef NOMIPS
    	//tmp_dev->sock->sk->sk_protinfo.af_inet.uc_ttl = 1;
    	inet_sk(tmp_dev->sock->sk)->uc_ttl = 1;

    #else
    //tmp_dev->sock->sk->protinfo.af_inet.ttl = 1;
    inet_sk(tmp_dev->sock->sk)->uc_ttl = 1;
    #endif
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    len = sock_sendmsg(tmp_dev->sock, &msg1,(size_t) datalen1);
    if (len < 0)
    {
        printk("Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(dst_ip));
	set_fs(oldfs);
        return 0;
    }

    len = sock_sendmsg(tmp_dev->sock, &msg2,(size_t) datalen2);
    if (len < 0)
    {
        printk("Error sending! err no: %d, Dst: %s\n", len, inet_ntoa(dst_ip));
    }

    set_fs(oldfs);
    return 0;
}














