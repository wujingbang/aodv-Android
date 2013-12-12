/***************************************************************************
 hello.c  -  description
 -------------------
 begin                : Wed Aug 13 2003
 copyright            : (C) 2003 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
#include "hello.h"

extern u_int32_t g_mesh_ip;
extern u_int8_t g_routing_metric;
extern aodv_neigh *aodv_neigh_list;
extern aodv_dev *g_mesh_dev;

int count = 0;

void update_neigh_load(aodv_neigh *neigh, aodv_neigh_2h *neigh_2h,
		u_int8_t load, u_int16_t load_seq) {

	if (neigh) {
		if (load_seq > neigh->load_metric.load_seq) { //Load update
			neigh->load_metric.load = load;
			neigh->load_metric.load_seq = load_seq;
		} else if (load_seq < neigh->load_metric.load_seq
				&& neigh->load_metric.load_seq-load_seq > 64000) {
			//sequence number has been reinitialized 
			neigh->load_metric.load = load;
			neigh->load_metric.load_seq = load_seq;
		}

	} else if (neigh_2h) {
		if (load_seq > neigh_2h->load_seq) { //Load update
			neigh_2h->load = load;
			neigh_2h->load_seq = load_seq;
		} else if (load_seq < neigh_2h->load_seq
				&& neigh_2h->load_seq-load_seq > 64000) {
			//sequence number has been reinitialized
			neigh_2h->load = load;
			neigh_2h->load_seq = load_seq;
		}
	}

}

void update_neightx(aodv_neigh *neigh, u_int32_t neightx) {

	int i=0;
	int first_empty = -1;
	u_int32_t position;
	
	for (i=0; i<NEIGH_TABLESIZE; i++) {
		position = neigh->load_metric.neigh_tx[i];
		if (position == neightx) {
			first_empty = i;
			break;
		} 
		else if (position == 0 && first_empty == -1)
			first_empty = i;
	}
	neigh->load_metric.neigh_tx[first_empty]=neightx;
	
}

int send_hello(helloext_struct *total_ext, int num_neigh) {
	hello *tmp_hello;
	int hello_size;
	u_int64_t jitter;
	int rand;
	helloext_struct *tmp_helloext_struct = total_ext;
	helloext_struct *free_helloext = NULL;
	hello_extension *tmp_hello_extension = NULL;

	void *data;
	hello_size = sizeof(hello) + (sizeof(hello_extension) * num_neigh);
	if ((data = kmalloc(hello_size, GFP_ATOMIC)) == NULL) {
		printk("Error creating packet to send HELLO\n");
		return -ENOMEM;
	}

	tmp_hello = (hello *) data;
	tmp_hello->type = HELLO_MESSAGE;
	tmp_hello->reserved1 = 0;
	tmp_hello->num_hops = 0;
	tmp_hello->neigh_count = num_neigh;
	tmp_hello->load = g_mesh_dev->load;
	tmp_hello->load_seq = g_mesh_dev->load_seq;

	tmp_hello_extension = (hello_extension *) ((void *)data + sizeof(hello));
	while (tmp_helloext_struct) {
		tmp_hello_extension->ip = tmp_helloext_struct->ip;
		tmp_hello_extension->count = tmp_helloext_struct->count;
		tmp_hello_extension->load = tmp_helloext_struct->load;
		tmp_hello_extension->load_seq = tmp_helloext_struct->load_seq;
		free_helloext = tmp_helloext_struct;
		tmp_helloext_struct = tmp_helloext_struct->next;
		kfree(free_helloext);
		tmp_hello_extension = (void *)tmp_hello_extension
				+ sizeof(hello_extension);
	}

	local_broadcast(1, data, hello_size);
	kfree(data);
	//random jitter 
	get_random_bytes(&rand, sizeof (rand));
	jitter = (u_int64_t)(rand%NODE_TRAVERSAL_TIME); //a number between 0 and 50usecs
	insert_timer_simple(TASK_HELLO, HELLO_INTERVAL+jitter, g_mesh_ip);
	update_timer_queue();

	count++;
	if (count == 10) {
		count = 0;
		if (g_routing_metric != HOPS)
			compute_etx();
	}

	return 1;
}

int gen_hello() {

	helloext_struct *total_ext = NULL;
	helloext_struct *simple_ext = NULL;
	int neigh_count = 0;
	aodv_neigh *tmp_entry;

	if (g_routing_metric != HOPS) {
		tmp_entry = aodv_neigh_list;
		while (tmp_entry != NULL) {
			if ((simple_ext = (helloext_struct *) kmalloc(
					sizeof(helloext_struct), GFP_ATOMIC)) == NULL) {
				printk("HELLO - Can't allocate new entry\n");
				return 0;
			}
			simple_ext->next = total_ext;
			total_ext = simple_ext;
			simple_ext->ip = tmp_entry->ip;
			simple_ext->count = tmp_entry->etx.recv_etx;
			simple_ext->load = tmp_entry->load_metric.load;
			simple_ext->load_seq = tmp_entry->load_metric.load_seq;
			neigh_count++;
			tmp_entry = tmp_entry->next;
		}
	}

	send_hello(total_ext, neigh_count);

	return 0;
}

int recv_hello(task * tmp_packet) {
	aodv_neigh *hello_orig;
	hello * tmp_hello;
	hello_extension *tmp_hello_extension;
	int i;
	u_int8_t load = 0;
	u_int16_t load_seq = 0;
	
	tmp_hello= (hello *)tmp_packet->data;
	tmp_hello_extension = (hello_extension *) ((void*)tmp_packet->data
			+ sizeof(hello));

	hello_orig = find_aodv_neigh(tmp_packet->src_ip);

	if (hello_orig == NULL) {
		aodv_neigh_2h *tmp_neigh_2h = find_aodv_neigh_2h(tmp_packet->src_ip);

		if (tmp_neigh_2h) { //previous detected as a 2h_neighbor - delete it but conserve its load info
			load = tmp_neigh_2h->load;
			load_seq = tmp_neigh_2h->load_seq;
			delete_aodv_neigh_2h(tmp_neigh_2h->ip);
			printk("MOVING NEIGHBOR_2HOPS TO NEIGHBOUR_1HOP: %s\n",
					inet_ntoa(tmp_packet->src_ip));

		} else 
			printk("NEW NEIGHBOR_1HOP DETECTED: %s\n",
					inet_ntoa(tmp_packet->src_ip));

		hello_orig = create_aodv_neigh(tmp_packet->src_ip);
		if (!hello_orig) {
#ifdef DEBUG
			printk("Error creating neighbor: %s\n", inet_ntoa(tmp_packet->src_ip));
#endif
			return -1;
		}
		
		hello_orig->load_metric.load = load;
		hello_orig->load_metric.load_seq = load_seq;
			
	}

	//Update neighbor timelife
	delete_timer(hello_orig->ip, hello_orig->ip, NO_TOS, TASK_NEIGHBOR);
	insert_timer_simple(TASK_NEIGHBOR, HELLO_INTERVAL
			* (1 + ALLOWED_HELLO_LOSS) + 100, hello_orig->ip);
	update_timer_queue();
	hello_orig->lifetime = HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 20
			+ getcurrtime();

	if (g_routing_metric == HOPS) //it doesn't need metric computation
		return 0;

			hello_orig->etx.count++;

	if (g_routing_metric == WCIM)
		update_neigh_load(hello_orig, NULL, tmp_hello->load,
				tmp_hello->load_seq);

	for (i=0; i < tmp_hello->neigh_count; i++) {
		if (tmp_hello_extension->ip == g_mesh_ip) {
			hello_orig->etx.send_etx = tmp_hello_extension->count;
			if (g_routing_metric == ETT) //it only needs its etx count
			return 0;
		}

		else {
			if (g_routing_metric == WCIM) { //neighbors of my neighbors

				aodv_neigh *ext_neigh =
						find_aodv_neigh(tmp_hello_extension->ip);

				if (ext_neigh)
					update_neigh_load(ext_neigh, NULL, tmp_hello->load,
							tmp_hello->load_seq);

				else {
					aodv_neigh_2h *ext_neigh_2h =
							find_aodv_neigh_2h(tmp_hello_extension->ip);
					if (ext_neigh_2h == NULL)
						ext_neigh_2h = create_aodv_neigh_2h(tmp_hello_extension->ip);

					else {
						delete_timer(tmp_hello_extension->ip,
								tmp_hello_extension->ip, NO_TOS,
								TASK_NEIGHBOR_2H);
						insert_timer_simple(TASK_NEIGHBOR_2H,
								NEIGHBOR_2H_TIMEOUT + 100,
								tmp_hello_extension->ip);
						update_timer_queue();
						ext_neigh_2h->lifetime = getcurrtime()
								+ NEIGHBOR_2H_TIMEOUT;
					}
					update_neigh_load(NULL, ext_neigh_2h, tmp_hello->load,
							tmp_hello->load_seq);
				}

				update_neightx(hello_orig, tmp_hello_extension->ip);
			}
		}

		tmp_hello_extension = (hello_extension *) ((void*)tmp_hello_extension
				+ sizeof(hello_extension));
	}

	return 0;
}

void compute_etx() {
	aodv_neigh *tmp_neigh = aodv_neigh_list;
	while (tmp_neigh != NULL) {
		if (tmp_neigh->etx.count == 0)
			tmp_neigh = tmp_neigh->next;
		else {
			tmp_neigh->etx.recv_etx = tmp_neigh->etx.count;
			tmp_neigh->etx.count = 0;
			
		if (tmp_neigh->etx.recv_etx  > 10)
			tmp_neigh->etx.recv_etx = 10;
		if (tmp_neigh->etx.send_etx  > 10)
			tmp_neigh->etx.send_etx = 10;
				
			tmp_neigh->etx_metric = tmp_neigh->etx.recv_etx
					*tmp_neigh->etx.send_etx; //new etx


			tmp_neigh = tmp_neigh->next;
		}

	}
}

