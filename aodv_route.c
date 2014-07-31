/***************************************************************************
 aodv_route.c  -  description
 -------------------
 begin                : Mon Jul 29 2002
 copyright            : (C) 2002 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "aodv_route.h"

aodv_route *aodv_route_table;
extern u_int32_t g_broadcast_ip;
extern u_int32_t g_mesh_ip;
extern u_int8_t g_routing_metric;
extern aodv_dev *g_mesh_dev;

//rwlock_t route_lock = RW_LOCK_UNLOCKED;
DEFINE_RWLOCK(route_lock);

inline void route_read_lock(void) {
	read_lock_bh(&route_lock);
}

inline void route_read_unlock(void) {
	read_unlock_bh(&route_lock);
}

inline void route_write_lock(void) {
	write_lock_bh(&route_lock);
}

inline void route_write_unlock(void) {

	write_unlock_bh(&route_lock);
}

aodv_route *first_aodv_route() {
	return aodv_route_table;
}

void init_aodv_route_table(void) {
	aodv_route_table = NULL;
}

void update_my_load() {

	aodv_route *tmp_entry;
	u_int8_t new_load;

	new_load = 0;
	tmp_entry = first_aodv_route();
	while (tmp_entry!=NULL) {

		if (tmp_entry->state == ACTIVE) //if valid and replied
			new_load += tmp_entry->load;

		tmp_entry = tmp_entry->next;

	}

	if (g_mesh_dev->load != new_load) {
		g_mesh_dev->load = new_load;
		if (g_mesh_dev->load_seq == 65535)
			g_mesh_dev->load_seq = 1;
		else
			g_mesh_dev->load_seq++;
	}

	return;

}

u_int8_t compute_load(u_int16_t rate_link, unsigned char tos) {

	u_int8_t activity;
	u_int16_t rate;
	u_int16_t size;
	u_int8_t coef = 0;
	flow_type *tmp_flow;

	if (rate_link == 0) {
#ifdef DEBUG
		printk("Wait till ETT works\n");
#endif
		return UNDEFINED_LOAD;
	}

	tmp_flow = find_flow_type(tos);
	if (tmp_flow == NULL) { //NO_TOS
		rate = ZERO_LOAD;
		size = ZERO_SIZE;
	} else {
		rate = tmp_flow->avg_rate;
		size = tmp_flow->avg_size;
	}

	coef = compute_coef(size, rate_link);
	activity = (u_int8_t)(rate*100/(rate_link*coef));

	return activity;
}

void expire_aodv_route(aodv_route * tmp_route) {

	route_write_lock(); //lifetime and route_valid are modified or read in packet_out.c - avoid conflicts
	tmp_route->lifetime = (getcurrtime() + DELETE_PERIOD);
	tmp_route->state = INVALID;
	route_write_unlock();
}

void remove_aodv_route(aodv_route * dead_route) {

	route_write_lock(); //to avoid conflicts with read_route_table_proc and packet_out.c (uncontrolled interruption)

	if (aodv_route_table == dead_route) {
		aodv_route_table = dead_route->next;
	}
	if (dead_route->prev != NULL) {
		dead_route->prev->next = dead_route->next;
	}
	if (dead_route->next!=NULL) {
		dead_route->next->prev = dead_route->prev;
	}

	route_write_unlock();

	kfree(dead_route);
}

int cleanup_aodv_route_table() {
	aodv_route *dead_route, *tmp_route;
	src_list_entry *tmp_entry;
	int error;

	tmp_route = aodv_route_table;
	while (tmp_route!=NULL) {

		tmp_entry = find_src_list_entry(tmp_route->src_ip);

		if (tmp_entry) {
			if (!(tmp_route->neigh_route) && (!tmp_route->self_route)) { //Don't try to delete virtual routes

				error = rpdb_route(RTM_DELROUTE, tmp_entry->rt_table,
						tmp_route->tos, tmp_route->src_ip, tmp_route->dst_ip,
						tmp_route->next_hop, tmp_route->dev->index,
						tmp_route->num_hops);
				if (error < 0)
					printk(
							"Error sending with rtnetlink - Delete Route - err no: %d\n",
							error);

			}

		}
		dead_route = tmp_route;
		tmp_route = tmp_route->next;
		kfree(dead_route);

	}
	tmp_route = find_aodv_route(g_mesh_ip, g_mesh_ip, 0);
	tmp_entry = find_src_list_entry(tmp_route->src_ip);

	if (tmp_route && tmp_entry)
		rpdb_route(RTM_DELROUTE, tmp_entry->rt_table, tmp_route->tos,
				tmp_route->dst_ip, tmp_route->dst_ip, tmp_route->next_hop,
				tmp_route->dev->index, 0);
	aodv_route_table = NULL;

	return 0;
}

int flush_aodv_route_table() {
	u_int64_t currtime = getcurrtime();
	aodv_route *dead_route, *tmp_route, *prev_route=NULL;
	src_list_entry *tmp_entry;

	tmp_route = aodv_route_table;
	while (tmp_route!=NULL) {
		prev_route = tmp_route;

		if (time_before((unsigned long) tmp_route->lifetime,
				(unsigned long) currtime) && (!tmp_route->self_route)) {

			if (tmp_route->state != INVALID) {
				expire_aodv_route(tmp_route);
				tmp_route = tmp_route->next;
			}

			else {

				tmp_entry = find_src_list_entry(tmp_route->src_ip);

				if (tmp_entry) {
					tmp_entry->num_routes--;
					if (!(tmp_route->neigh_route)) {

						int error = rpdb_route(RTM_DELROUTE,
								tmp_entry->rt_table, tmp_route->tos,
								tmp_route->src_ip, tmp_route->dst_ip,
								tmp_route->next_hop, tmp_route->dev->index,
								tmp_route->num_hops);
						if (error < 0)
							printk(
									"Error sending with rtnetlink - Delete Route - err no: %d\n",
									error);

					}
					//if (tmp_entry->num_routes <= 0)
						//delete_src_list_entry(tmp_route->src_ip);
					/*CaiBingying:When the src_list_entry is local node,do not delete it 							however for it having a self route any time,delete it may 							cause failure of new neighbors setting up*/
					if ((tmp_entry->num_routes <= 0) && (tmp_entry->ip!=g_mesh_ip))
						delete_src_list_entry(tmp_route->src_ip);
					else if((tmp_entry->num_routes <= 0) && (tmp_entry->ip==g_mesh_ip))
						tmp_entry->num_routes = 1;

				}

				dead_route = tmp_route;
				prev_route = tmp_route->prev;
				tmp_route = tmp_route->next;
				remove_aodv_route(dead_route);
			}

		} else 
			tmp_route = tmp_route->next;
		

	}

	if (g_routing_metric == WCIM)
		update_my_load();

	insert_timer_simple(TASK_CLEANUP, HELLO_INTERVAL, g_mesh_ip);
	update_timer_queue();
	return 0;
}

void insert_aodv_route(aodv_route * new_route) { //ordenamos por dst_ip

	aodv_route *tmp_route;
	aodv_route *prev_route = NULL;

	route_write_lock(); //to avoid conflicts with read_route_table_proc and packet_out.c (uncontrolled interruption)

	tmp_route = aodv_route_table;

	while ((tmp_route != NULL) && (tmp_route->dst_ip < new_route->dst_ip)) //entries ordered by dst_ip
	{
		prev_route = tmp_route;
		tmp_route = tmp_route->next;
	}

	if (aodv_route_table && (tmp_route == aodv_route_table)) // if it goes in the first spot in the table
	{
		aodv_route_table->prev = new_route;
		new_route->next = aodv_route_table;
		aodv_route_table = new_route;
		route_write_unlock();
		return;
	}

	if (aodv_route_table == NULL) // if the routing table is empty
	{
		aodv_route_table = new_route;
		route_write_unlock();
		return;
	}

	if (tmp_route == NULL) //i'm going in next spot in the table and it is empty
	{
		new_route->next = NULL;
		new_route->prev = prev_route;
		prev_route->next = new_route;
		route_write_unlock();
		return;
	}
	tmp_route->prev = new_route;
	new_route->next = tmp_route;
	new_route->prev = prev_route;
	prev_route->next = new_route;
	route_write_unlock();
	return;
}

//wujingbang : tmp_entry->next_hop
aodv_route *create_aodv_route(u_int32_t src_ip, u_int32_t dst_ip,
		unsigned char tos, u_int32_t dst_id) {
	aodv_route *tmp_entry;
#ifdef DEBUG0
	char src[20];
	char dst[20];
	strcpy(src, inet_ntoa(src_ip));
	strcpy(dst, inet_ntoa(dst_ip));
	
	printk ("create_aodv_route: Creating %s to %s\n", src,dst);
#endif

	/* Allocate memory for new entry */

	if ((tmp_entry = (aodv_route *) kmalloc(sizeof(aodv_route), GFP_ATOMIC))
			== NULL) {
		printk("Error getting memory for new route\n");
		return NULL;
	}
	tmp_entry->dst_ip = dst_ip;
	tmp_entry->next_hop = NULL;
	tmp_entry->src_ip = src_ip;
	tmp_entry->tos = tos;
	tmp_entry->dst_id = dst_id;
	tmp_entry->last_hop = src_ip;

	tmp_entry->self_route = FALSE;
	tmp_entry->num_hops = 0;
	tmp_entry->path_metric = 0;
	tmp_entry->dev = g_mesh_dev;
	tmp_entry->state = INVALID;
	tmp_entry->netmask = g_broadcast_ip;
	tmp_entry->prev = NULL;
	tmp_entry->next = NULL;

	tmp_entry->load = 0;
	tmp_entry->neigh_route = FALSE;

	insert_aodv_route(tmp_entry);
#ifdef DEBUG0
	if (aodv_route_table == NULL) // if the routing table is empty
	{
		printk ("routing table still NULL after insert!! ");
	}
#endif
	return tmp_entry;
}

int rreq_aodv_route(u_int32_t src_ip, u_int32_t dst_ip, unsigned char tos,
		aodv_neigh *next_hop, u_int8_t hops, u_int32_t dst_id,
		struct net_device *dev, u_int32_t path_metric) {
#ifdef DEBUG0
	char src[16];
	char dst[16];
	char nex[16];
	strcpy(src, inet_ntoa(src_ip));
	strcpy(dst, inet_ntoa(dst_ip));
	strcpy(nex, inet_ntoa(next_hop->ip));
	printk ("rreq_aodv_route: src is %s dst is %s via %s\n", src,	dst, nex);
#endif

	aodv_route *tmp_route;
	int error;
	src_list_entry *tmp_src_entry;
	int previous_state = INVALID;
	
	tmp_route = find_aodv_route(src_ip, dst_ip, tos);
	
	 if (tmp_route == NULL) {
#ifdef DEBUG0
		printk ("Creating route, %s to %s VIA %s\n", src, dst, nex);
#endif
		//wujingbang
//		tmp_route = create_aodv_route(src_ip, dst_ip, tos, dst_id);
		tmp_route = create_aodv_route(src_ip, dst_ip, tos, dst_id);

		if (tmp_route == NULL) {
#ifdef DEBUG0
			printk ("rreq_aodv_route: tem_route is STILL NULL!\n");
#endif
			return 0;
		}

		tmp_src_entry = find_src_list_entry(src_ip);

		if (tmp_src_entry == NULL) {
			tmp_src_entry = insert_src_list_entry(src_ip);
			if (tmp_src_entry == NULL) {
				return 0;
			}
		}

		tmp_src_entry->num_routes++;
	}
	 
	 else { //Route exists!
#ifdef DEBUG0
		strcpy(src, inet_ntoa(tmp_route->src_ip));
		strcpy(dst, inet_ntoa(tmp_route->dst_ip));
		strcpy(nex, inet_ntoa(tmp_route->next_hop));
		printk ("We already have route, %s to %s, via %s \n", src, dst, nex);
#endif
	 	if (tmp_route->dst_id > dst_id) //older route
		{
#ifdef DEBUG0
			printk(" tmp_route->dst_id > dst_id \n");
#endif
	 		return 0;
		}
	 	if ((dst_id == tmp_route->dst_id) && (path_metric
	 					>= tmp_route->path_metric)) //worse metric
		{
#ifdef DEBUG0
			printk(" worse metric \n");
#endif
	 		return 0;
		}
/*	 	tmp_src_entry = find_src_list_entry(src_ip);
	 	if (tmp_src_entry == NULL) {
#ifdef DEBUG0
			printk(" tmp_src_entry == NULL \n");
#endif
	 		return 0;
	 	}
*/	 		
	 	previous_state = tmp_route->state;
	 		
	 	if (previous_state != INVALID){
#ifdef DEBUG0
		 	printk("Replacing Route\n");
	#endif


			
			 error = rpdb_route(RTM_DELROUTE, /*tmp_src_entry->rt_table*/0,
			 				tmp_route->tos, tmp_route->src_ip, tmp_route->dst_ip,
			 				tmp_route->next_hop, tmp_route->dev->index,
			 				tmp_route->num_hops);
			 if (error < 0) {
			 	printk ("Error sending with rtnetlink - Delete Route - err no: %d\n", error);
			 	return 0;
		 	}
		}
	 }

	/* Update values in the RT entry */
	tmp_route->dev = g_mesh_dev;
	tmp_route->path_metric = path_metric;
	tmp_route->dst_id = dst_id;	
	tmp_route->num_hops = hops;
	tmp_route->next_hop = next_hop->ip;
	route_write_lock(); //lifetime and route_valid are modified or read in packet_out.c - avoid conflicts
	tmp_route->lifetime = getcurrtime() + DELETE_PERIOD;
	//wujingbang
//	tmp_route->state = INVALID;
	tmp_route->state = ACTIVE;

	route_write_unlock();
			
	if (g_routing_metric == WCIM){
		tmp_route->load = compute_load(next_hop->send_rate, tmp_route->tos);
		if (previous_state == ACTIVE)
			update_my_load();
	}		
	return 1;
}
int rrep_aodv_route(aodv_route *rep_route){
	int error = 0;
	src_list_entry * tmp_src_entry;
#ifdef DEBUG
	char src[16];
	char dst[16];
#endif
	
	tmp_src_entry = find_src_list_entry(rep_route->src_ip);
	if (tmp_src_entry == NULL) {
#ifdef DEBUG
		printk ("rrep_aodv_route: tmp_src_entry is NULL!!\n");
#endif
		return 0;
	}
	

	error = rpdb_route(RTM_NEWROUTE, tmp_src_entry->rt_table,
			rep_route->tos, rep_route->src_ip, rep_route->dst_ip,
			rep_route->next_hop, rep_route->dev->index,
			rep_route->num_hops);
	
	if (error < 0) {
		printk("Error sending with rtnetlink - Delete Route - err no: %d\n",error);
		return 0;
	}

	
	route_write_lock(); //lifetime and route_valid are modified or read in packet_out.c - avoid conflicts
	rep_route->state = REPLIED;
	rep_route->lifetime = getcurrtime() + DELETE_PERIOD;
	route_write_unlock();
	ipq_send_ip(rep_route->src_ip, rep_route->dst_ip, rep_route->tos);
	
#ifdef DEBUG
	strcpy(src, inet_ntoa(rep_route->src_ip));
	strcpy(dst, inet_ntoa(rep_route->dst_ip));

	printk("Installing route from %s ", src);
	printk("to %s with ToS %u - Next Hop: %s - path_metric: %u\n", dst, rep_route->tos, inet_ntoa(rep_route->next_hop),rep_route->path_metric);
#endif
	return 1;
}

aodv_route *find_aodv_route(u_int32_t src_ip, u_int32_t dst_ip,
		unsigned char tos) {
	aodv_route *tmp_route;

	/*lock table */
	route_read_lock(); //to avoid conflicts in packet_out.c (uncontrolled interruption)

	tmp_route = aodv_route_table;

	while ((tmp_route != NULL) && (tmp_route->dst_ip < dst_ip)) { //serching GW_IP (sorted by dst_ip)
		tmp_route = tmp_route->next;
	}

	if ((tmp_route == NULL) || (tmp_route->dst_ip != dst_ip)) { //not found!
		route_read_unlock();
		return NULL;
	}

	else {
		while ((tmp_route != NULL) && (tmp_route->dst_ip == dst_ip)) {
			if ((tmp_route->src_ip == src_ip) && (tmp_route->tos == tos)) {
				route_read_unlock();
				return tmp_route;
			}
			tmp_route = tmp_route->next;
		}
	}
	route_read_unlock();
	return NULL;

}

aodv_route *find_aodv_route_by_id(u_int32_t dst_ip, u_int32_t dst_id) {
	aodv_route *tmp_route;

	route_read_lock(); //to avoid conflicts in packet_out.c (uncontrolled interruption)

	tmp_route = aodv_route_table;

	while ((tmp_route != NULL) && (tmp_route->dst_ip < dst_ip)) { //serching IP (sorted by dst_ip)
		tmp_route = tmp_route->next;
	}

	if ((tmp_route == NULL) || (tmp_route->dst_ip != dst_ip)) { //not found!
		route_read_unlock();
		return NULL;
	}

	else {
		while ((tmp_route != NULL) && (tmp_route->dst_ip == dst_ip)) {
			if (tmp_route->dst_id == dst_id) {
				route_read_unlock();
				return tmp_route;
			}
			tmp_route = tmp_route->next;
		}
	}
	route_read_unlock();
	return NULL;
}

#ifdef RECOVERYPATH
int is_overlapped_with_route(brk_link *tmp_link) {

	aodv_route *tmp_route;
	/*lock table */
	route_read_lock(); //to avoid conflicts in packet_out.c (uncontrolled interruption)

	tmp_route = aodv_route_table;

	while ((tmp_route != NULL) && (tmp_route->dst_ip < tmp_link->last_avail_ip)) { //serching GW_IP (sorted by dst_ip)
		tmp_route = tmp_route->next;
	}

	if ((tmp_route == NULL) || (tmp_route->dst_ip != tmp_link->last_avail_ip)) { //not found!
		route_read_unlock();
		return 0;
	}

	else {
		while ((tmp_route != NULL) && (tmp_route->dst_ip == tmp_link->last_avail_ip)) {
			if ( tmp_route->src_ip == tmp_link->src_ip ) {
				route_read_unlock();
				return 1;
			}
			tmp_route = tmp_route->next;
		}
	}
	route_read_unlock();
	return 0;
}
#endif

int read_route_table_proc(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data) {

	static char *my_buffer;
	char temp_buffer[200];
	char temp[100];
	aodv_route *tmp_entry;
	u_int64_t remainder, numerator;
	u_int64_t tmp_time;
	int len;
	u_int64_t currtime;
	char dst[15];
	char hop[15];
	char src[15];
	char lhop[15];
	char tos[12];

	currtime=getcurrtime();

	/*lock table*/
	route_read_lock();

	tmp_entry = aodv_route_table;

	my_buffer=buffer;

	sprintf(
			my_buffer,
			"\nRoute Table \n---------------------------------------------------------------------------------------------------------------------------\n");
	sprintf(
			temp_buffer,
			"   SRC  IP    |    DST  IP    |      ToS      |RLoad|   Next Hop   |   Prev Hop    | DST_ID|Hops|   Path Metric  \n");
	strcat(my_buffer, temp_buffer);
	sprintf(
			temp_buffer,
			"----------------------------------------------------------------------------------------------------------------------------\n");
	strcat(my_buffer, temp_buffer);

	while (tmp_entry!=NULL) {
		strcpy(hop, inet_ntoa(tmp_entry->next_hop));
		strcpy(lhop, inet_ntoa(tmp_entry->last_hop));
		strcpy(dst, inet_ntoa(tmp_entry->dst_ip));
		strcpy(src, inet_ntoa(tmp_entry->src_ip));

		switch (tmp_entry->tos) {

		case 0x02:
			sprintf(tos, "0x02  ");
			strcat(tos, "  -- ");
			break;
		case 0x04:
			sprintf(tos, "0x04  ");
			strcat(tos, "  -- ");
			break;
		case 0x08:
			sprintf(tos, "0x08  ");
			strcat(tos, "  -- ");
			break;
		case 0x10:
			sprintf(tos, "0x10  ");
			strcat(tos, "  -- ");
			break;
		case 0x0c:
			sprintf(tos, "0x0c  ");
			strcat(tos, " 0x03");
			break;
		case 0x14:
			sprintf(tos, "0x14  ");
			strcat(tos, " 0x05");
			break;
		case 0x18:
			sprintf(tos, "0x18  ");
			strcat(tos, " 0x06");
			break;
		case 0x1c:
			sprintf(tos, "0x1c  ");
			strcat(tos, " 0x07");
			break;
		default:
			sprintf(tos, "NO_TOS ");
			strcat(tos, "  -- ");
			break;
		}

		sprintf(temp_buffer, "%-16s %-16s %-12s %4u %16s %16s %5u  %3d %9u",
				src, dst, tos, tmp_entry->load, hop, lhop, tmp_entry->dst_id,
				tmp_entry->num_hops, tmp_entry->path_metric);
		strcat(my_buffer, temp_buffer);

		if (tmp_entry->self_route) {
			strcat(my_buffer, " Self Route\n");
		} else {
			if (tmp_entry->state == ACTIVE) {
				strcat(my_buffer, " ACT:");

				tmp_time=tmp_entry->lifetime-currtime;
				numerator = (tmp_time);
				remainder = do_div(numerator, 1000);
				if (time_before((unsigned long) tmp_entry->lifetime,
						(unsigned long) currtime) ) {
					sprintf(temp, " Expired!\n");
				} else {
					sprintf(temp, "%lu/%lu\n", (unsigned long)numerator,
							(unsigned long)remainder);
				}
				strcat(my_buffer, temp);
			} else {
				strcat(my_buffer, " Del:");

				tmp_time=tmp_entry->lifetime-currtime;
				numerator = (tmp_time);
				remainder = do_div(numerator, 1000);
				if (time_before((unsigned long) tmp_entry->lifetime,
						(unsigned long) currtime) ) {
					sprintf(temp, " Deleted!\n");
				} else {
					sprintf(temp, " %lu/%lu\n", (unsigned long)numerator,
							(unsigned long)remainder);
				}
				strcat(my_buffer, temp);
			}
		}

		tmp_entry=tmp_entry->next;

	}

	/*unlock table*/
	route_read_unlock();

	strcat(
			my_buffer,
			"----------------------------------------------------------------------------------------------------------------------------\n\n");

	len = strlen(my_buffer);
	*buffer_location = my_buffer + offset;
	len -= offset;
	if (len > buffer_length)
		len = buffer_length;
	else if (len < 0)
		len = 0;
	return len;
}
