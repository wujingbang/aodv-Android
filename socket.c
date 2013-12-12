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














