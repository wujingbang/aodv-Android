/***************************************************************************
 module.c  -  description
 -------------------
 begin                : Wed Aug 20 2003
 copyright            : (C) 2003 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "fbaodv_protocol.h"

//General definitions 
//MESH
u_int32_t g_mesh_ip;
u_int32_t g_mesh_netmask;
u_int32_t g_broadcast_ip;
u_int8_t g_aodv_gateway;
u_int8_t g_routing_metric;
u_int32_t g_fixed_rate;

// KR: 08/23/05
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Miguel Catalan Cid");
MODULE_DESCRIPTION("A AODV ad-hoc routing kernel module: Flow-based and WCIM routing metric");

extern struct timer_list aodv_timer;
extern struct aodv_route *aodv_route_table;

struct nf_hook_ops input_filter;
struct nf_hook_ops output_filter;

char *mesh_dev;
char *aodv_subnet;
char *routing_metric;
int nominal_rate;
char *network_ip;
unsigned int aodv_gateway;
char g_aodv_dev[8];
u_int32_t g_null_ip;

static struct proc_dir_entry *aodv_dir, *route_table_proc, *neigh_proc,
		*gw_proc, *timers_proc, *node_load_proc, *sources_proc,
		*reliability_proc, *ett_proc,
		*tos_proc;

module_param(mesh_dev, charp, 0);
module_param(aodv_gateway, uint, 0);
module_param(network_ip, charp, 0);
module_param(routing_metric, charp,0);
module_param(nominal_rate,uint,0);

/****************************************************
 init_module
 ----------------------------------------------------
 This is called by insmod when things get
 started up!
 ****************************************************/

static int __init init_fbaodv_module(void) {
	char *tmp_str = NULL;
	inet_aton("0.0.0.0", &g_null_ip);

	if (mesh_dev==NULL)
	{
		printk("You need to specify the mesh network device ie:\n");
		printk("    insmod fbaodv_protocol mesh_dev=wlan0    \n");
		return(-1);
	}
	
	strcpy(g_aodv_dev, mesh_dev);
	
	if(aodv_gateway) {
		g_aodv_gateway = (u_int8_t)aodv_gateway;
		printk("\n\nSet as AODV-ST gateway\n");
	} else
	g_aodv_gateway = 0;

	if (network_ip == NULL) {
		printk("You need to specify aodv network ip range, ie: \n");
		printk("network_ip=192.168.0.12/255.255.255.0\n");
		return(-1);
	}
	inet_aton("255.255.255.255",&g_broadcast_ip);
	printk("Miguel Catalan Cid\nImplementation built on top of aodv-st by Krishna Ramachandran, UC Santa Barbara\n");
	printk("---------------------------------------------\n");

	
	/*ROUTING METRICS*/
	if (routing_metric == NULL || strcmp(routing_metric, "HOPS") == 0){
		g_routing_metric = HOPS;
		printk("Hop count routing\n\n");
	}

	else if (strcmp(routing_metric, "ETT") == 0) {
		g_routing_metric = ETT;
		printk("ETT routing: Expected Transmission Time\n\n");
		
		if (nominal_rate && get_nominalrate(nominal_rate))
			g_fixed_rate = nominal_rate;
		
		if (!nominal_rate || g_fixed_rate == 0) {
				g_fixed_rate = 0;
				printk("Nominal rate estimation using packet-pair probe messages: ACTIVE\n\n");
		}
		else
			printk("Nominal rate estimation using packet-pair probe messages: INACTIVE\n\n");

	}
	else if (strcmp(routing_metric, "WCIM") == 0) {
		g_routing_metric = WCIM;
		printk("WCIM routing: Weighted Contention and Interference Model\n\n");
		
		if (nominal_rate && get_nominalrate(nominal_rate))
					g_fixed_rate = nominal_rate;
				
		if (!nominal_rate || g_fixed_rate == 0) {
					g_fixed_rate = 0;
					printk("Nominal rate estimation using packet-pair probe messages: ACTIVE\n\n");
		}
		else
					printk("Nominal rate estimation using packet-pair probe messages: INACTIVE\n\n");


	}

	
	if (network_ip!=NULL)
	{
		tmp_str = strchr(network_ip,'/');
		if (tmp_str)
		{	(*tmp_str)='\0';
			tmp_str++;
			printk("* Network IP - %s/%s\n", network_ip, tmp_str);
			inet_aton(network_ip, &g_mesh_ip);
			inet_aton(tmp_str, &g_mesh_netmask);
		}
	}
		
	
	if (init_packet_queue() < 0) {
			printk("Netlink queue error!!!\n");
			printk("Shutdown complete!\n");
			return(-1);
	}
	

	init_aodv_route_table();
	init_task_queue();
	init_timer_queue();
	init_aodv_neigh_list();
	init_aodv_neigh_list_2h();
	init_src_list();
	init_gw_list();
	init_flow_type_table();

	printk("\n*Initialicing mesh device  - %s\n", mesh_dev);	
	if (init_aodv_dev(mesh_dev))
		goto out1;
	

	startup_aodv();


	if (g_aodv_gateway) {
		printk("initiating st_rreq dissemination\n");
		insert_timer_simple(TASK_ST, HELLO_INTERVAL, g_mesh_ip); // start first st_rreq immediately
	}

	//MCC 21/07/2008- UPDATE TIMER ahora en insert_timer
	insert_timer_simple(TASK_HELLO, HELLO_INTERVAL,g_mesh_ip );
	insert_timer_simple(TASK_GW_CLEANUP, ACTIVE_GWROUTE_TIMEOUT, g_mesh_ip);
	insert_timer_simple(TASK_CLEANUP, HELLO_INTERVAL+HELLO_INTERVAL/2, g_mesh_ip);
	update_timer_queue();

	aodv_dir = proc_mkdir("fbaodv",NULL);
	if (aodv_dir == NULL) {
		goto out2;
	}
///*
#ifdef KERNEL2_6_26
	aodv_dir->owner = THIS_MODULE;
#endif

	route_table_proc=create_proc_read_entry("routes", 0, aodv_dir, read_route_table_proc, NULL);
#ifdef KERNEL2_6_26
	route_table_proc->owner=THIS_MODULE;
#endif

	if (route_table_proc == NULL) goto out2;

	neigh_proc=create_proc_read_entry("neigh", 0, aodv_dir, read_neigh_proc, NULL);
	if (neigh_proc == NULL) goto out2;
#ifdef KERNEL2_6_26
	neigh_proc->owner=THIS_MODULE;
#endif

	timers_proc=create_proc_read_entry("timers", 0, aodv_dir, read_timer_queue_proc, NULL);
	if (timers_proc == NULL) goto out2;
#ifdef KERNEL2_6_26
	timers_proc->owner=THIS_MODULE;
#endif

	reliability_proc=create_proc_read_entry("reliability", 0, aodv_dir, read_rel_list_proc, NULL);
	if (reliability_proc == NULL) goto out2;
#ifdef KERNEL2_6_26
	reliability_proc->owner=THIS_MODULE;
#endif

	ett_proc=create_proc_read_entry("bw_estimation", 0, aodv_dir, read_ett_list_proc, NULL);
	if (ett_proc == NULL) goto out2;
#ifdef KERNEL2_6_26
	ett_proc->owner=THIS_MODULE;
#endif

	sources_proc=create_proc_read_entry("sources", 0, aodv_dir, read_src_list_proc, NULL);
	if (sources_proc == NULL) goto out2;
#ifdef KERNEL2_6_26
	sources_proc->owner=THIS_MODULE;
#endif

	node_load_proc=create_proc_read_entry("node_load", 0, aodv_dir, read_node_load_proc, NULL);
	if (node_load_proc == NULL) goto out2;
#ifdef KERNEL2_6_26
	node_load_proc->owner=THIS_MODULE;
#endif

	gw_proc=create_proc_read_entry("gateways", 0, aodv_dir, read_gw_list, NULL);
	if (gw_proc == NULL) goto out2;
#ifdef KERNEL2_6_26
	gw_proc->owner=THIS_MODULE;
#endif

	tos_proc=create_proc_read_entry("tos_available", 0, aodv_dir, read_flow_type_table_proc, NULL);
	if (ett_proc == NULL) goto out2;
#ifdef KERNEL2_6_26
	ett_proc->owner=THIS_MODULE;
#endif
//*/
	//netfilter stuff
	// input hook
	input_filter.list.next = NULL;
	input_filter.list.prev = NULL;
	input_filter.hook = input_handler;
	input_filter.pf = PF_INET; // IPv4
	input_filter.hooknum = NF_INET_LOCAL_IN;

	//MCC- output hook
	output_filter.list.next = NULL;
	output_filter.list.prev = NULL;
	output_filter.hook = output_handler;
	output_filter.pf = PF_INET; // IPv4
	output_filter.hooknum = NF_INET_POST_ROUTING;

	nf_register_hook(&output_filter);
	nf_register_hook(&input_filter);

	printk("\n\n****FBAODV_PROTO module initialization complete****\n\n");
	return 0;

	out1:
	close_sock();

	del_timer(&aodv_timer);
	printk("Removed timer...\n");

	cleanup_packet_queue();
	printk("Cleaned up AODV Queues...\n");

	if (aodv_route_table != NULL)
	cleanup_aodv_route_table();

	flush_src_list();
	printk("Cleaned up Route and Rule Tables...\n");

	printk("Shutdown complete!\n");

	return(-1);

	out2:

	remove_proc_entry("fbaodv/routes",NULL);
	remove_proc_entry("fbaodv/timers",NULL);
	remove_proc_entry("fbaodv/neigh",NULL);
	remove_proc_entry("fbaodv/reliability", NULL);
	remove_proc_entry("fbaodv/sources", NULL);
	remove_proc_entry("fbaodv/node_load", NULL);

	remove_proc_entry("fbaodv/gateways", NULL);

	remove_proc_entry("fbaodv/user_gateway", NULL);
	remove_proc_entry("fbaodv/gateways_assigned", NULL);
	remove_proc_entry("fbaodv/bw_estimation", NULL);
	remove_proc_entry("fbaodv/tos_available", NULL);
	remove_proc_entry("fbaodv", NULL);

	printk("\n\nShutting down...\n");

	printk("Unregistered NetFilter hooks...\n");

	close_sock();
	printk("Closed sockets...\n");

	del_timer(&aodv_timer);
	printk("Removed timer...\n");

	kill_aodv();
	printk("Killed router thread...\n");

	cleanup_task_queue();
	cleanup_packet_queue();
	printk("Cleaned up AODV Queues...\n");

	cleanup_neigh_routes();
	cleanup_aodv_route_table();
	flush_src_list();
	printk("Cleaned up Route and Rule Tables...\n");

	//Eliminando listas
	delete_aodv_neigh_list();
	delete_aodv_neigh_list_2h();
	delete_gw_list();
	flush_flow_type_table();
	printk("Cleaned up Lists...\n");

	printk("Shutdown complete!\n");

	return(-1);
}

/****************************************************

 cleanup_module
 ----------------------------------------------------
 cleans up the module. called by rmmod
 ****************************************************/
static void __exit cleanup_fbaodv_module(void)
{
	printk("\n\nShutting down fbaodv PROTOCOL\n");

	remove_proc_entry("fbaodv/routes",NULL);
	remove_proc_entry("fbaodv/timers",NULL);
	remove_proc_entry("fbaodv/neigh",NULL);
	remove_proc_entry("fbaodv/reliability", NULL);
	remove_proc_entry("fbaodv/sources", NULL);
	remove_proc_entry("fbaodv/node_load", NULL);

	remove_proc_entry("fbaodv/gateways", NULL);

	remove_proc_entry("fbaodv/user_gateway", NULL);
	//remove_proc_entry("fbaodv/gateways_assigned", NULL);
	remove_proc_entry("fbaodv/bw_estimation", NULL);
	remove_proc_entry("fbaodv/tos_available", NULL);

	remove_proc_entry("fbaodv", NULL);

	nf_unregister_hook(&input_filter);
	nf_unregister_hook(&output_filter);
	printk("Unregistered NetFilter hooks...\n");

	close_sock();
	printk("Closed sockets...\n");

	del_timer(&aodv_timer);
	printk("Removed timer...\n");

	kill_aodv();
	printk("Killed router thread...\n");

	cleanup_task_queue();
	cleanup_packet_queue();
	printk("Cleaned up AODV Queues...\n");

	cleanup_neigh_routes();
	cleanup_aodv_route_table();
	flush_src_list();
	printk("Cleaned up Route and Rule Tables...\n");

	//Eliminando listas
	delete_aodv_neigh_list();
	delete_aodv_neigh_list_2h();
	delete_gw_list();
	flush_flow_type_table();
	printk("Cleaned up Lists...\n");

	printk("Shutdown complete!\n");
}

/*
 * ADDED JL kernel 2.6:
 * */
module_init(init_fbaodv_module);
module_exit(cleanup_fbaodv_module);
