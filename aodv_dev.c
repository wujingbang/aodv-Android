/***************************************************************************
 aodv_dev.c  -  description
 -------------------
 begin                : Thu Aug 7 2003
 copyright            : (C) 2003 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#include "aodv_dev.h"
#include <linux/rcupdate.h>

extern char g_aodv_dev[8];
extern u_int32_t g_mesh_ip;
extern u_int32_t g_mesh_netmask;
#ifdef DEBUGC
extern u_int32_t g_node_name;
#endif

aodv_dev *g_mesh_dev;
aodv_route *g_local_route;
aodv_dev *net_dev_list;

aodv_dev *create_aodv_dev(struct net_device *dev) {
	int error;
	aodv_dev *new_dev  = NULL;
	src_list_entry *tmp_src_entry = NULL;

	if ((new_dev = (aodv_dev *) kmalloc(sizeof(aodv_dev), GFP_ATOMIC)) == NULL) {
		/* Couldn't create a new entry in the routing table */
		printk("Not enough memory to create Route Table Entry\n");
		return NULL;
	}
	
	strncpy(new_dev->name, dev->name, IFNAMSIZ);
	new_dev->ip = g_mesh_ip;
	new_dev->netmask = g_mesh_netmask;
	new_dev->dev = dev;
	new_dev->load = 0;
	new_dev->load_seq = 1;
	

	new_dev->index= dev->ifindex;
	
	if (strcmp(dev->name, g_aodv_dev) == 0){
		
		g_local_route = create_aodv_route(g_mesh_ip, g_mesh_ip, NO_TOS, 1);
		g_local_route->dst_ip =g_mesh_ip;
		//tmp_route->netmask = calculate_netmask(0); //ifa->ifa_mask;
		g_local_route->self_route = TRUE;
		g_local_route->next_hop = g_local_route->dst_ip;
		g_local_route->last_hop = g_local_route->dst_ip;
		g_local_route->lifetime = -1;
		g_local_route->state = ACTIVE;
		g_local_route->dev = new_dev;
		g_local_route->tos = 0;
		g_local_route->load = 0;
		tmp_src_entry = insert_src_list_entry(g_mesh_ip);
		if (tmp_src_entry == NULL) {
				kfree(new_dev);
				return NULL;
		}
		
	error = rpdb_route(RTM_NEWROUTE, tmp_src_entry->rt_table, g_local_route->tos,
			g_local_route->dst_ip, g_local_route->dst_ip, g_local_route->next_hop,
			new_dev->index, 0);

	if (error < 0) {
		printk(
				"Error sending with rtnetlink - Insert Route - err no: %d,on interface: %s\n",
				error, new_dev->name);
		kfree(new_dev);
		return NULL;
	}
	}
	return new_dev;

}

int insert_aodv_dev(struct net_device *dev) {
	aodv_dev *new_dev;
	int error=0;

	char netmask[16];

	new_dev = create_aodv_dev(dev);
	if (new_dev == NULL){
		printk("Error during creation of socket; terminating, %d\n", error);
		return 1;
	}
	strcpy(netmask, inet_ntoa(new_dev->netmask & new_dev->ip));

	printk(
			"Adding interface: %s  Interface Index: %d  IP: %s Subnet: %s\n",
			dev->name, dev->ifindex, inet_ntoa(g_mesh_ip), netmask);
		                                 
		error = sock_create(PF_INET, SOCK_DGRAM, 0, &(new_dev->sock));
		if (error < 0) {
			kfree(new_dev);
			printk("Error during creation of socket; terminating, %d\n", error);
			return 1;
		}
		init_sock(new_dev->sock, new_dev->ip, dev->name);
	
	g_mesh_dev = new_dev;
	return 0;
}

//extern u_int8_t g_aodv_gateway;

int init_aodv_dev(char *name) {

	struct net_device *dev;
	struct in_device *tmp_indev;
	int error = 0;

	dev = dev_get_by_name(&init_net,name); //using device name passed in module intialization
		
	if (dev != NULL) {
		//Checking if g_mesh_ip and g_mesh_netmask are correct!
		tmp_indev = (struct in_device *) dev->ip_ptr;
		if (tmp_indev && (tmp_indev->ifa_list != NULL)) {
			
			if ((tmp_indev->ifa_list->ifa_address == g_mesh_ip)
					&& (tmp_indev->ifa_list->ifa_mask == g_mesh_netmask)) {
				error = insert_aodv_dev(dev);
				dev_put(dev); //free the device!!
				return error;
			} else {
				printk("\n\nDEVICE ERROR! Device %s with ip %s", name,
						inet_ntoa(tmp_indev->ifa_list->ifa_address));
				printk(" and netmask %s\n",
						inet_ntoa(tmp_indev->ifa_list->ifa_mask));
				printk("does not match with the ip and netmask specified as module parameters\n\n");
				dev_put(dev); //free the device!!
				return 1;

			}
		}
		dev_put(dev); //free the device!!
	}
	
	printk("\n\nDEVICE ERROR! Unable to locate device: %s, Bailing!\n\n", name);
	return 1;

}

/////////////////////////////////////////////////////////

///20141013
aodv_dev *create_net_dev(struct net_device *dev) {
	int error;
	aodv_dev *new_dev  = NULL;
	aodv_dev *pdev;
	src_list_entry *tmp_src_entry = NULL;

	if ((new_dev = (aodv_dev *) kmalloc(sizeof(aodv_dev), GFP_ATOMIC)) == NULL) {
		/* Couldn't create a new entry in the routing table */
		printk("Not enough memory to create Route Table Entry\n");
		return NULL;
	}
	
	strncpy(new_dev->name, dev->name, IFNAMSIZ);
	new_dev->dev = dev;
	new_dev->load = 0;
	new_dev->load_seq = 1;
	new_dev->index= dev->ifindex;

	//different net_device have to get ip from diff way
	if(strcmp(dev->name,g_aodv_dev)==0)
	{
		//get ip & netmask from params
		new_dev->ip = g_mesh_ip;
		new_dev->netmask = g_mesh_netmask;
	}
	else{
		//eth* get from net_device
		struct in_device *ip;
		struct in_ifaddr *ifaddr;
		ip = dev->ip_ptr;
		ifaddr = ip->ifa_list;
		new_dev->ip = ifaddr->ifa_address; 
		new_dev->netmask = ifaddr->ifa_mask;
printk("-----------new dev ip %s--------\n",inet_ntoa(new_dev->ip));
	}
	//when the dev is the first dev,create g_local_route & the net_dev_list
	if(net_dev_list == NULL){
		g_local_route = create_aodv_route(new_dev->ip, new_dev->ip, NO_TOS, 1);
		g_local_route->dst_ip = new_dev->ip;
		//tmp_route->netmask = calculate_netmask(0); //ifa->ifa_mask;
		g_local_route->self_route = TRUE;
		g_local_route->next_hop = g_local_route->dst_ip;
		g_local_route->last_hop = g_local_route->dst_ip;
		g_local_route->lifetime = -1;
		g_local_route->state = ACTIVE;
		g_local_route->dev = new_dev;
		g_local_route->tos = 0;
		g_local_route->load = 0;
		
	}
	tmp_src_entry = insert_src_list_entry(new_dev->ip);
	if (tmp_src_entry == NULL) {
		kfree(new_dev);
		return NULL;
	}
	
		
	error = rpdb_route(RTM_NEWROUTE, tmp_src_entry->rt_table, g_local_route->tos,
			new_dev->ip, new_dev->ip, new_dev->ip,new_dev->index, 0);

	if (error < 0) {
		printk(
			"Error sending with rtnetlink - Insert Route - err no: %d,on interface: %s\n",
				error, new_dev->name);
		kfree(new_dev);
		return NULL;
	}
	
printk("---------create net dev  999---------\n");
	return new_dev;

}

int insert_net_dev(struct net_device *dev) {
	aodv_dev *new_dev;
	int error=0;
printk("----------i n d 1----------------\n");
	char netmask[16];

	new_dev = create_net_dev(dev);
	if (new_dev == NULL){
		printk("Error during creation of socket; terminating, %d\n", error);
		return 1;
	}
	strcpy(netmask, inet_ntoa(new_dev->netmask & new_dev->ip));
	//printk(
	//		"Adding interface: %s  Interface Index: %d  IP: %s Subnet: %s\n",
	//		dev->name, dev->ifindex, inet_ntoa(new_dev->ip), inet_ntoa(new_dev->netmask));
	                                 
		error = sock_create(PF_INET, SOCK_DGRAM, 0, &(new_dev->sock));
		if (error < 0) {
			kfree(new_dev);
			printk("Error during creation of socket; terminating, %d\n", error);
			return 1;
		}
		init_sock(new_dev->sock, new_dev->ip, dev->name);

	/////**************************////
	//need to think of                                             
	if(g_mesh_dev==NULL) {
		g_mesh_dev = new_dev;
		g_node_name = new_dev->ip;
		
#ifdef DEBUGC
		printk("=================init g_mesh_dev%s=============\n",inet_ntoa(g_node_name));
#endif
	}
	/////**************************////
	//insert the new_dev in the front of list 
	new_dev->next = net_dev_list;
	net_dev_list = new_dev;
	printk("---\ninsert_net_dev in socket:%s-----\n",new_dev->name);
	return 0;
}

//extern u_int8_t g_aodv_gateway;

int init_net_dev(char *name) {

	struct net_device *dev;
	struct in_device *tmp_indev;
	int error = 0;

	dev = dev_get_by_name(&init_net,name); //using device name passed in module intialization
		
	if ( dev != NULL ) {
		//set the ip and netmask
		tmp_indev = (struct in_device *) dev->ip_ptr;
printk("init_net_dev \n");
		if (tmp_indev && (tmp_indev->ifa_list != NULL)) {
printk("init_net_dev not null\n");
			error = insert_net_dev(dev);
			dev_put(dev); //free the device!!
			return error;
			
		}
		dev_put(dev); //free the device!!
	}
	
	printk("\n\nDEVICE ERROR! Unable to locate device: %s, Bailing!\n\n", name);
	return 1;

}

