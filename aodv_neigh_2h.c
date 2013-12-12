/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "aodv_neigh_2h.h"

aodv_neigh_2h *aodv_neigh_list_2h;

void init_aodv_neigh_list_2h(void) {
	aodv_neigh_list_2h=NULL;
}

void delete_aodv_neigh_list_2h(void) {
	aodv_neigh_2h *tmp_entry = aodv_neigh_list_2h;
	aodv_neigh_2h *prev_entry = NULL;
	while (tmp_entry != NULL) {
		prev_entry = tmp_entry;
		tmp_entry = tmp_entry->next;
		kfree(prev_entry);
	}
	
}

aodv_neigh_2h *find_aodv_neigh_2h(u_int32_t target_ip) {

	aodv_neigh_2h *tmp_neigh_2h;

	//neigh_read_lock(); //unnecessary
	tmp_neigh_2h = aodv_neigh_list_2h;

	while ((tmp_neigh_2h != NULL) && (tmp_neigh_2h->ip <= target_ip)) {
		if (tmp_neigh_2h->ip == target_ip) {

			//	neigh_read_unlock();
			return tmp_neigh_2h;
		}
		tmp_neigh_2h = tmp_neigh_2h->next;
	}

	//neigh_read_unlock();
	return NULL;
}

aodv_neigh_2h *create_aodv_neigh_2h(u_int32_t ip) {
	aodv_neigh_2h *new_neigh_2h;
	aodv_neigh_2h *prev_neigh_2h = NULL;
	aodv_neigh_2h *tmp_neigh_2h = NULL;

	if ((new_neigh_2h = kmalloc(sizeof(aodv_neigh_2h), GFP_ATOMIC)) == NULL) {
#ifdef DEBUG
		printk("NEIGHBOR_LIST_2H: Can't allocate new entry\n");
#endif
		return NULL;
	}

	tmp_neigh_2h = aodv_neigh_list_2h;
	while ((tmp_neigh_2h != NULL) && (tmp_neigh_2h->ip < ip)) {
		prev_neigh_2h = tmp_neigh_2h;
		tmp_neigh_2h = tmp_neigh_2h->next;
	}

	if (tmp_neigh_2h && (tmp_neigh_2h->ip == ip)) {
#ifdef DEBUG
		printk("NEIGHBOR_LIST_2H: Creating a duplicate 2h neighbor entry\n");
#endif
		kfree(new_neigh_2h);
		return NULL;
	}

	neigh_write_lock(); //to avoid conflicts with read_neigh_proc (uncontrolled interruption)

	printk("NEW NEIGHBOR_2HOPS DETECTED: %s\n", inet_ntoa(ip));

	new_neigh_2h->ip = ip;
	new_neigh_2h->lifetime = getcurrtime() + NEIGHBOR_2H_TIMEOUT;
	new_neigh_2h->next = NULL;
	new_neigh_2h->load = 0;
	new_neigh_2h->load_seq = 0;
	
	if (prev_neigh_2h == NULL) {
		new_neigh_2h->next = aodv_neigh_list_2h;
		aodv_neigh_list_2h = new_neigh_2h;
	} else {
		new_neigh_2h->next = prev_neigh_2h->next;
		prev_neigh_2h->next = new_neigh_2h;
	}

	neigh_write_unlock();

	insert_timer_simple(TASK_NEIGHBOR_2H, NEIGHBOR_2H_TIMEOUT + 100, ip);
	update_timer_queue();

	return new_neigh_2h;
}

int delete_aodv_neigh_2h(u_int32_t ip) {

	aodv_neigh_2h *tmp_neigh_2h;
	aodv_neigh_2h *prev_neigh_2h = NULL;
	neigh_write_lock(); //to avoid conflicts with read_neigh_proc (uncontrolled interruption)
	tmp_neigh_2h = aodv_neigh_list_2h;

	while (tmp_neigh_2h != NULL) {
		if (tmp_neigh_2h->ip == ip) {

			if (prev_neigh_2h != NULL) {
				prev_neigh_2h->next = tmp_neigh_2h->next;
			} else {
				aodv_neigh_list_2h = tmp_neigh_2h->next;
			}

			neigh_write_unlock();
			kfree(tmp_neigh_2h);

			update_timer_queue();
			return 1;
		}
		prev_neigh_2h = tmp_neigh_2h;
		tmp_neigh_2h = tmp_neigh_2h->next;

	}
	neigh_write_unlock();

	return 0;
}

