/***************************************************************************
 aodv.h  -  description
 -------------------
 begin                : Mon Jul 1 2003
 copyright            : (C) 2003 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/

/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#ifndef AODV_H
#define AODV_H

#include <linux/types.h>
#include <linux/string.h>

/* LINUX_VERSION_CODE */
#include <linux/version.h>

#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/socket.h>
#include <linux/fs.h>
#include <linux/wireless.h>
//#include <linux/smp_lock.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/udp.h>
#include <linux/init.h>
#include <linux/notifier.h>

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/netfilter/nf_queue.h>

#include <linux/spinlock.h>
#include <linux/sysctl.h>

#include <linux/in.h>
#include <linux/time.h>
#include <linux/ctype.h>
//#include <bits/sockaddr.h>
#include <linux/socket.h>
//#include <bits/sysctl.h>
#include <linux/sysctl.h>

#include <asm/uaccess.h>
#include <asm/types.h>
#include <asm/div64.h>
#include <asm/byteorder.h>
#include <asm/ioctl.h>
#include <net/iw_handler.h>
#include <net/sock.h>
#include <net/icmp.h>
#include <net/route.h>

#define DTN
//#define DEBUG 0
#define CaiDebug                666
#define DTNREGISTER		9999
#define DTNPORT			10000
#define AODVPORT		5002
#define TRUE			1
#define FALSE 			0
#define BLACKLIST               111
#define REPAIR			333
// See section 10 of the AODV draft
// Times in milliseconds
#define ACTIVE_ROUTE_TIMEOUT 	3000
#define ALLOWED_HELLO_LOSS 	10 //MCC - Changed to increment the possibility of having a bad ETX metric
#define BLACKLIST_TIMEOUT 	RREQ_RETRIES * NET_TRAVERSAL_TIME
#define DELETE_PERIOD         ALLOWED_HELLO_LOSS * HELLO_INTERVAL
#define HELLO_INTERVAL        1000
#define LOCAL_ADD_TTL         2
#define MAX_REPAIR_TTL        0.3 * NET_DIAMETER
#define MY_ROUTE_TIMEOUT      ACTIVE_ROUTE_TIMEOUT
#define NET_DIAMETER          10
#define NODE_TRAVERSAL_TIME   40
#define NET_TRAVERSAL_TIME 	2 * NODE_TRAVERSAL_TIME * NET_DIAMETER
#define NEXT_HOP_WAIT         NODE_TRAVERSAL_TIME + 10
#define PATH_DISCOVERY_TIME   2 * NET_TRAVERSAL_TIME
#define RERR_RATELIMIT        10
#define RING_TRAVERSAL_TIME   2 * NODE_TRAVERSAL_TIME * ( TTL_VALUE + TIMEOUT_BUFFER)
#define RREQ_RETRIES 		2
#define RREQ_RATELIMIT 		10
#define TIMEOUT_BUFFER 		 2
#define TTL_START 		 2
#define TTL_INCREMENT 		 2
#define TTL_THRESHOLD         7
#define TTL_VALUE             3

//ST Gateway Definitions
#define ST_INTERVAL           10000
#define ACTIVE_GWROUTE_TIMEOUT 	(ST_INTERVAL * 2) + 1000
#define TIMEOUT_DELAY_ROUTE_UPDATE 1000 // 1 second //MCC - Unused

//ETT Definitions
#define ETT_INTERVAL	60000 //1 minute by default
#define ETT_FAST_INTERVAL 20000 //20 sec for first estimation
#define ETT_PROBES_MAX 	10 //at the latest, 10 minutes to have a valid estimation
#define ETT_PROBES_MIN  3  //minimum 3 in range values to have an estimation
#define ETT_RANGE	50 //Usecs Range [-50, +50] for a valid estimation
#define ALLOWED_ETT_LOSS 2 //Consecutive Losses allowed
#define PROBE_PACKET 11776 //(Microsecs per Symbol)
#define DEFAULT_ETT_METRIC 1000000 //1 second
#define PROBE_PACKET_SIZE 61740 //(Microsecs per Symbol)
#define RATE_1 10 //1Mbps
#define RATE_2 20
#define RATE_5 55
#define RATE_11 110
#define RATE_6 60
#define RATE_9 90
#define RATE_12 120
#define RATE_18 180
#define RATE_24 240
#define RATE_36 360
#define RATE_48 480
#define RATE_54 540

//MAC parameters
//802.11a
#define TPROC_LOW 100 //Process time in estimation (6Mbps - 18Mbps)
#define TPROC_HIGH 150 //Process time in estimation (24Mbps - 54Mbps)
#define PHY_TIMERS 90 //PHY preambles + DIFS + SIFS (802.11a)
//[MAC_HEADER (30,75bytes) + IP&UDP_HEADERS(28bytes) + ACK(16,75bytes) + PROBE PACKET(1468 bytes)]xOFDM_SYMBOLx10(40microsec)
//802.11b

//802.11g


//FB-AODV & WCIM Definitions
//Routing metrics supported

#define DHCP1 67 //DHCP ports
#define DHCP2 68 //DHCP ports
#define HOPS 0 //Hop count metric
#define ETT 1 // ETT metric
#define WCIM 2 //WCIM metric
#define NUM_TOS 9
#define NO_TOS 0x00 
#define ZERO_LOAD 0
#define ZERO_SIZE 0
#define DEFAULT_ROUTE_TABLE 254 //MAIN ROUTE TABLE - Used for default routes
#define MAX_ROUTE_TABLES 252	//Number of not used kernel route tables (tables from 1 to 252, both included)
#define TTL_LOAD_MSG	2
#define NEIGHBOR_2H_TIMEOUT 10000  //ten seconds - If a 2Hops Neighbour is moving, there is the possibility that another neighbour discovers it - Wait a longer time to delete load_information
#define SMALL_LOAD_TIMER 100
#define LARGE_LOAD_TIMER 25000
#define LOAD_JITTER 10
#define RREP_TIMER 100 //100 msec - Wait a little for new RREQ (better metric?) before sending a RREP
#define DEFAULT_ROUTE_TIMEOUT 60000 //One Minute - Timeout for Internet Routes
#define UNDEFINED_LOAD 200
/***MESSAGES AND TASKS****/
//CLASSIC AODV
#define TASK_HELLO		  102
#define TASK_NEIGHBOR		  103
#define TASK_CLEANUP		104
#define TASK_ROUTE_CLEANUP  105

#define RREQ_MESSAGE 116
#define TASK_RECV_RREQ 116
#define TASK_RESEND_RREQ 	  119
#define RERR_MESSAGE 126
#define TASK_RECV_RERR 126
#define HELLO_MESSAGE	129
#define TASK_RECV_HELLO	129
#define RREP_MESSAGE	130
#define TASK_RECV_RREP	130
//ST-AODV
#define TASK_ST             106
#define TASK_GW_CLEANUP     107
#define ST_RREQ_MESSAGE 128
#define TASK_RECV_STRREQ 128

//FB-AODV
#define TASK_SEND_ETT	109  //Sending a pair of probe packets
#define ETT_S_MESSAGE	110
#define TASK_RECV_S_ETT	110 //Received a small probe packet
#define ETT_L_MESSAGE 	111
#define TASK_RECV_L_ETT	111  //Received a large probe packet
#define TASK_ETT_CLEANUP 112 //Cleanup ETT-METRIC

#define TASK_NEIGHBOR_2H 113
#define ETT_INFO_MSG 124
#define TASK_ETT_INFO 124
#define TASK_SEND_RREP 125

#define TASK_UPDATE_LOAD 131
//Packet Definitions
/////////////////////////////
//ST- RREQ
//////////////////////////////
typedef struct {
	u_int8_t type;

#ifdef __BIG_ENDIAN_BITFIELD
	u_int8_t j:1;
	u_int8_t r:1;
	u_int8_t g:1;
	u_int8_t d:1;
	u_int8_t u:1;
	u_int8_t reserved:3;
#elif defined __LITTLE_ENDIAN_BITFIELD
	u_int8_t reserved :3;
	u_int8_t u :1;
	u_int8_t d :1;
	u_int8_t g :1;
	u_int8_t r :1;
	u_int8_t j :1;
#else
	//#error "Please fix <asm/byteorder.h>"
#endif
	u_int8_t second_reserved;
	u_int8_t num_hops;
	u_int32_t gw_ip;
	u_int32_t dst_id;
	u_int32_t path_metric;

} rreq_st;
//

//////////////////////////////
//WCIM RREQ -	//
//////////////////////////////
typedef struct {
	u_int8_t type;
#ifdef __BIG_ENDIAN_BITFIELD
	u_int8_t j:1;
	u_int8_t r:1;
	u_int8_t g:1;
	u_int8_t d:1;
	u_int8_t u:1;
	u_int8_t reserved:3;
#elif defined __LITTLE_ENDIAN_BITFIELD
	u_int8_t reserved :3;
	u_int8_t u :1;
	u_int8_t d :1;
	u_int8_t g :1;
	u_int8_t r :1;
	u_int8_t j :1;
#else
	//#error "Please fix <asm/byteorder.h>"
#endif
	unsigned char tos;
	u_int8_t num_hops;
	u_int32_t gateway;
	u_int32_t dst_ip;
	u_int32_t src_ip;
	u_int32_t dst_id;
	u_int32_t path_metric;

} rreq;
//

//////////////////////////////
//RREP Message				      //
//////////////////////////////
typedef struct {
	u_int8_t type;

#ifdef __BIG_ENDIAN_BITFIELD
	unsigned int a:1;
	unsigned int q:1;
	unsigned int reserved1:6;
#elif defined __LITTLE_ENDIAN_BITFIELD
	unsigned int reserved1 :6;
	unsigned int q :1;
	unsigned int a :1;
#else
	//#error "Please fix <asm/byteorder.h>"
#endif
	unsigned char tos;
	u_int8_t num_hops;

	u_int32_t dst_ip;
	u_int32_t src_ip;
	u_int32_t dst_id;
	u_int32_t gateway;
	u_int32_t path_metric;
	u_int32_t src_id;

} rrep;
//


//////////////////////////////
//RRER Message		    //
//////////////////////////////
typedef struct {
	u_int8_t type;
	u_int8_t num_hops;
#ifdef __BIG_ENDIAN_BITFIELD
	unsigned int n:1;
	unsigned int reserved:7;
#elif defined __LITTLE_ENDIAN_BITFIELD
	unsigned int reserved :7;
	unsigned int n :1;
#else
	//#error "Please fix <asm/byteorder.h>"
#endif
	unsigned int dst_count :8;
	u_int32_t dst_ip;
	u_int32_t dst_id;
#ifdef DTN
	u_int32_t last_avail_ip;
	u_int32_t src_ip;
#endif

} rerr;
//


//////////////////////////////
//Hello Message		    //
//////////////////////////////
typedef struct {
	u_int32_t ip;
	u_int8_t count;
	u_int8_t load; //New version: Do not add extra overhead using load messages!
	u_int16_t load_seq; //New version: Do not add extra overhead using load messages!
} hello_extension;

struct _helloext_struct {
	u_int32_t ip;
	u_int8_t count;
	u_int8_t load; //New version: Do not add extra overhead using load messages!
	u_int16_t load_seq; //New version: Do not add extra overhead using load messages!
	struct _helloext_struct *next;
};
typedef struct _helloext_struct helloext_struct;

typedef struct {
	u_int8_t type;

#ifdef __BIG_ENDIAN_BITFIELD
	unsigned int a:1;
	unsigned int reserved1:7;
#elif defined __LITTLE_ENDIAN_BITFIELD
	unsigned int reserved1 :7;
	unsigned int a :1;
#else
	//#error "Please fix <asm/byteorder.h>"
#endif
	u_int8_t neigh_count;
	u_int8_t num_hops;
	u_int8_t reserved2;
	u_int8_t load; 
	u_int16_t load_seq; 

} hello;
//


//MCC - ETT Metric Implementation Messages
//Stuffing of 24 bytes - Necessary to form the probe packets
typedef struct {
	u_int32_t fill1;
	u_int32_t fill2;
	u_int32_t fill3;
	u_int32_t fill4;
	u_int32_t fill5;
	u_int32_t fill6;
} stuff;
//


//////////////////////////////
//Small ETT Pair Packet		//
//////////////////////////////
//MCC Implementation: 20bytes + 96 padding = 116 bytes
typedef struct {
	u_int8_t type;
	u_int16_t reserved1;
	u_int8_t n_probe;
	u_int32_t dst_ip;
	u_int32_t src_ip;
	u_int32_t sec;
	u_int32_t usec;
	stuff fill[4];

} s_probe;
//

//////////////////////////////
//Large ETT Pair Packet		  //
//////////////////////////////
//MCC Implementation: 28bytes + 1440 padding  = 1468 bytes
typedef struct {
	u_int8_t type;
	u_int8_t n_probe;
	u_int16_t my_rate;
	u_int32_t dst_ip;
	u_int32_t src_ip;
	u_int32_t sec;
	u_int32_t usec;
	u_int32_t last_meas_delay;
	stuff fill[60];
} l_probe;
//

//////////////////////////////
//ETT-Info Message	     		//
//////////////////////////////
//MCC Implementation: 8bytes
typedef struct {
	u_int8_t type;
	u_int8_t fixed_rate;
	u_int16_t my_rate;
	u_int32_t last_meas_delay;
} ett_info;
//


//////
//Structure Definitions
//////
//TASK
struct _task {

	int type;
	//u_int32_t id;
	u_int64_t time;
	u_int32_t dst_ip;
	u_int32_t src_ip;
	struct net_device *dev;
	u_int8_t ttl;
	u_int16_t retries;

	//MCC - Implementation
	unsigned char tos;
	
	unsigned char src_hw_addr[ETH_ALEN];
	unsigned int data_len;
	void *data;

	struct _task *next;
	struct _task *prev;

};
typedef struct _task task;
//

#define INVALID	0
#define REPLIED	1
#define ACTIVE	2

//AODV_ROUTE
struct _aodv_route {

	u_int32_t netmask;
	u_int8_t num_hops;
	u_int32_t next_hop;
	u_int64_t lifetime;
	u_int32_t last_hop;

	u_int32_t dst_ip;
	u_int32_t src_ip;
	u_int32_t dst_id;

	u_int32_t path_metric;
	unsigned char tos;
	u_int8_t load;

	u_int8_t neigh_route :1;
	u_int8_t self_route :1;
	u_int8_t state :2;

	struct _aodv_dev *dev;

	struct _aodv_route *next;
	struct _aodv_route *prev;
};
typedef struct _aodv_route aodv_route;
//


//AODV_DEV
struct _aodv_dev {
	struct net_device *dev;
	int index;
	u_int32_t ip;
	u_int32_t netmask;
	char name[IFNAMSIZ];
	struct socket *sock;
	u_int16_t load_seq;
	u_int8_t load;
};
typedef struct _aodv_dev aodv_dev;
//


typedef struct _etx_params {
	u_int8_t count;
	u_int8_t send_etx;
	u_int8_t recv_etx;
} etx_params;

typedef struct _ett_params {
	u_int8_t count_rcv;
	u_int8_t last_count;
	u_int8_t ett_window;
	u_int64_t sec;
	u_int64_t usec;
	u_int32_t meas_delay;
	u_int32_t recv_delays[ETT_PROBES_MAX];
	u_int8_t reset;
	u_int16_t interval;
	u_int16_t fixed_rate;
} ett_params;

//Node Load Tables
typedef struct _node_load {
	u_int32_t ip;
	u_int16_t load_seq;
	u_int8_t load;
	struct _node_load *next;
} node_load;
//

#define NEIGH_TABLESIZE 32
typedef struct _load_params{
	u_int16_t load_seq;
	u_int8_t load;
	u_int32_t neigh_tx[NEIGH_TABLESIZE];
}load_params;

//AODV_NEIGH
struct _aodv_neigh {
	u_int32_t ip;
	u_int64_t lifetime;
	struct net_device *dev;
	
	//MCC - Implementation
	etx_params etx;
	ett_params ett;
	load_params load_metric;
    u_int16_t recv_rate;
    u_int16_t send_rate;
    u_int8_t etx_metric;
	struct _aodv_neigh *next; 
};
typedef struct _aodv_neigh aodv_neigh;
//


//GW List
typedef struct _gw_entry {
	u_int32_t ip;
	u_int64_t lifetime;
	u_int32_t metric;
	//MCC - Implementation
	u_int8_t num_hops;
	u_int32_t dst_id;
	struct _gw_entry *next;
//

} gw_entry;
//

//2Hops Neigh
typedef struct _aodv_neigh_2h {
	u_int32_t ip;
	u_int64_t lifetime;
	u_int8_t load;
	u_int16_t load_seq;
	struct _aodv_neigh_2h *next;
} aodv_neigh_2h;
//



//Traffic Sources Tables
typedef struct _src_list_entry {
	u_int32_t ip;
	int rt_table;
	int num_routes;
	struct _src_list_entry *next;
} src_list_entry;
//


//Services available - Definition of ToS
typedef struct services flow_type;
struct services {
    unsigned char tos;
	u_int16_t avg_size;
	u_int16_t avg_rate;
	struct services *next;
};
//

#include "aodv_route.h"
#include "aodv_dev.h"
#include "aodv_neigh.h"
#include "aodv_neigh_2h.h"
#include "aodv_thread.h"
#include "gw_list.h"
#include "hello.h"
#include "fbaodv_protocol.h"
#include "packet_in.h"
#include "packet_out.h"
#include "packet_queue.h"
#include "probe_ett.h"
#include "rerr.h"
#include "route_alg.h"
#include "rpdb.h"
#include "rrep.h"
#include "rreq.h"
#include "socket.h"
#include "src_list.h"
#include "st_rreq.h"
#include "task_queue.h"
#include "timer_queue.h"
#include "utils.h"
#include "tos.h"

#endif
