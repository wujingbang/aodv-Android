/*
 * Author : Krishna Ramachandran
 * Date created : 08/26/05
 */
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
//MCC - Change of discovery procedure - No routes are created
#include "gw_list.h"

extern u_int32_t g_null_ip;

gw_entry  *gw_list;
u_int32_t g_default_gw;

//rwlock_t gw_lock = RW_LOCK_UNLOCKED;
DEFINE_RWLOCK(gw_lock);

inline void gw_read_lock(void) {
	read_lock_bh(&gw_lock);
}

inline void gw_read_unlock(void) {
	read_unlock_bh(&gw_lock);
}

inline void gw_write_lock(void) {
	write_lock_bh(&gw_lock);
}

inline void gw_write_unlock(void) {
	write_unlock_bh(&gw_lock);
}


void init_gw_list(void){
	gw_list = NULL;
	g_default_gw = g_null_ip;
}

void delete_gw_list(void){
	gw_entry *tmp_entry = gw_list;
	gw_entry *prev_entry = NULL;
	while (tmp_entry != NULL) {
		prev_entry = tmp_entry;
		tmp_entry = tmp_entry->next;
		kfree(prev_entry);
	}

}


int update_gw(u_int32_t gw_ip, u_int32_t dst_id, u_int8_t num_hops,
		u_int32_t path_metric) {

	gw_entry *tmp_entry = gw_list;
	gw_entry *new_entry = NULL;

	while (tmp_entry != NULL) {
		if (tmp_entry->ip == gw_ip) {
			if (tmp_entry->dst_id > dst_id) { //old ST-RREQ
				return 0;
			} else if ((tmp_entry->dst_id == dst_id) && (tmp_entry->metric
					< path_metric)) { //ST-RREQ already received through a better path
				return 0;
			} else {
				tmp_entry->metric = path_metric;
				tmp_entry->num_hops = num_hops;
				tmp_entry->lifetime = getcurrtime() + ACTIVE_GWROUTE_TIMEOUT;
				tmp_entry->dst_id = dst_id;
				return 1;
			}
		}
		tmp_entry = tmp_entry->next;
	}

	
	new_entry = (gw_entry *) kmalloc(sizeof(gw_entry), GFP_ATOMIC);

	if (new_entry == NULL) {
		printk("GW_entry: Can't allocate new entry\n");
		return 1;
	}

	gw_write_lock(); //to avoid conflicts with read_proc and gateway assignation (netfilter socket) (uncontrolled interruption)
	
	new_entry->ip = gw_ip;
	new_entry->metric = path_metric;
	new_entry->num_hops = num_hops;
	new_entry->lifetime = getcurrtime() + ACTIVE_GWROUTE_TIMEOUT;
	new_entry->dst_id = dst_id;
	new_entry->next = NULL;

	if (gw_list == NULL) {
		gw_list = new_entry;
	}

	else {
		new_entry->next = gw_list;
		gw_list = new_entry;
	}
	gw_write_unlock();
	return 1;
}

void delete_gw_list_entry(u_int32_t gw_ip) {

	gw_entry *tmp_entry = gw_list;
	gw_entry *prev_entry = NULL;
	gw_write_lock();  //to avoid conflicts with read_proc and gateway assignation (netfilter socket) (uncontrolled interruption)
	while (tmp_entry != NULL) {
		if (tmp_entry->ip == gw_ip) {
			if (prev_entry != NULL) {
				prev_entry->next = tmp_entry->next;
			} else
				gw_list = tmp_entry->next;
			
			gw_write_unlock();
			kfree(tmp_entry);
			return;
		}
		prev_entry = tmp_entry;
		tmp_entry = tmp_entry->next;
	}
	gw_write_unlock();
}

int is_gw(u_int32_t ip) {

	gw_entry *tmp_entry = gw_list;
	gw_read_lock(); //called in gwuser_assignation (netlink interruption) - avoid possible conflicts
	while (tmp_entry != NULL) {
		if (tmp_entry->ip == ip) {
			gw_read_unlock();
			return 1;
		}
		tmp_entry = tmp_entry->next;
	}
	gw_read_unlock();
	return 0;
}

void update_gw_lifetimes() {
	u_int64_t currtime = getcurrtime();
	gw_entry *tmp_entry = gw_list;
	u_int32_t best_gw = g_null_ip;
	u_int32_t best_metric = DEFAULT_ETT_METRIC; 

	while (tmp_entry != NULL) {
		if (time_after( (unsigned long) currtime,
				(unsigned long) tmp_entry->lifetime)) {

			printk("timing out gateway %s\n", inet_ntoa(tmp_entry->ip));
			delete_gw_list_entry(tmp_entry->ip);

		}
		else{
			if (tmp_entry->metric < best_metric){
				best_gw = tmp_entry->ip;
				best_metric = tmp_entry->metric;
			}
		}
		tmp_entry = tmp_entry->next;
	}
	
	g_default_gw = best_gw;

}

/****************************************************

 read_gw_proc
 ----------------------------------------------------
 Writes the gw list to /proc/gateways

 MCC: Simplified version for UAMN assistance
 ****************************************************/
int read_gw_list(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data) {
	static char *my_buffer;
	char temp_buffer[200];
	int len;
	gw_entry *tmp_entry;
	my_buffer = buffer;

	gw_read_lock();

	tmp_entry = gw_list;

	sprintf(my_buffer, "\n");

	while (tmp_entry != NULL) {
		sprintf(temp_buffer, " IP: %s", inet_ntoa(tmp_entry->ip));
		strcat(my_buffer, temp_buffer);
		sprintf(temp_buffer, "    Metric: %u", tmp_entry->metric);
		strcat(my_buffer, temp_buffer);
		sprintf(temp_buffer, "    Hops: %d", tmp_entry->num_hops);
		strcat(my_buffer, temp_buffer);
		if (tmp_entry->ip == g_default_gw)
		strcat(my_buffer, "    default\n");
		else
			strcat(my_buffer, "\n");
		
		
		tmp_entry = tmp_entry->next;
		

	}
	strcat(my_buffer, "\n");

	gw_read_unlock();
	len = strlen(my_buffer);
	*buffer_location = my_buffer + offset;
	len -= offset;
	if (len > buffer_length)
		len = buffer_length;
	else if (len < 0)
		len = 0;

	return len;
}

