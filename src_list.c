/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
//Traffic sources lists, Route tables identificators
#include "src_list.h"

extern u_int32_t g_null_ip;

src_list_entry *src_list;

extern u_int32_t g_mesh_ip;
extern u_int32_t g_null_ip;

//rwlock_t src_lock = RW_LOCK_UNLOCKED;
DEFINE_RWLOCK(src_lock);

inline void src_read_lock(void) {
	read_lock_bh(&src_lock);
}

inline void src_read_unlock(void) {
	read_unlock_bh(&src_lock);
}

inline void src_write_lock(void) {
	write_lock_bh(&src_lock);
}

inline void src_write_unlock(void) {
	write_unlock_bh(&src_lock);
}

void init_src_list() {

	src_list=NULL;
}

void flush_src_list(void) {
	src_list_entry *tmp_entry = src_list;
	src_list_entry *prev_entry = NULL;
	while (tmp_entry != NULL) {
		//Deleting associated source rules
		if (tmp_entry->ip != g_null_ip) {
			int error = rpdb_rule(RTM_DELRULE, tmp_entry->rt_table,
					tmp_entry->ip, g_null_ip);

			if (error < 0)
				printk("Error sending with rtnetlink - Add ROUTE - err no: %dn",
												error);
		}
		prev_entry = tmp_entry;
		tmp_entry = tmp_entry->next;
		kfree(prev_entry);
	}
}

void delete_src_list_entry(u_int32_t ip) {

	src_list_entry *tmp_entry = src_list;
	src_list_entry *prev_entry = NULL;

	src_write_lock(); //avoid conflicts with read_proc
	while (tmp_entry != NULL) {
		if (tmp_entry->ip == ip) {
			if (prev_entry != NULL)
				prev_entry->next = tmp_entry->next;
			else
				src_list = tmp_entry->next;

			src_write_unlock();

			if (tmp_entry->ip != g_null_ip) {
				int error = rpdb_rule(RTM_DELRULE, tmp_entry->rt_table,
						tmp_entry->ip, g_null_ip);

				if (error < 0)
					printk("Error sending with rtnetlink - Add ROUTE - err no: %dn",
													error);
			}
			kfree(tmp_entry);
			return;
		}
		prev_entry = tmp_entry;
		tmp_entry = tmp_entry->next;
	}
	src_write_unlock();
}

src_list_entry * insert_src_list_entry(u_int32_t ip) {
	int error =1;
	int n_table;
	src_list_entry *new_entry = NULL;

	new_entry = kmalloc(sizeof(src_list_entry), GFP_ATOMIC);
	if (new_entry == NULL) {
		printk("SRC_LIST: Can't allocate new entry\n");

		return NULL;
	}

	src_write_lock(); //avoid conflicts with read_proc

	new_entry->ip = ip;
	new_entry->num_routes =  0;
	new_entry->next = NULL;

	if (ip == g_mesh_ip)
		new_entry->num_routes = 1;

	if (new_entry->ip != g_null_ip) {

		n_table = find_first_rttable();

		if (n_table > MAX_ROUTE_TABLES) {
			printk("There are no more empty route tables! New Traffic can't be routed\n");
			src_write_unlock();
			kfree(new_entry);
			return NULL;
		}
		new_entry->rt_table = n_table;
		src_write_unlock();

		error = rpdb_rule(RTM_NEWRULE, new_entry->rt_table, new_entry->ip,
				g_null_ip);
		if (error < 0) {
			printk("Error sending with rtnetlink - Add ROUTE - err no: %dn",
											error);
			kfree(new_entry);
			return NULL;
		}
		src_write_lock();

	} else {
		new_entry->rt_table = DEFAULT_ROUTE_TABLE;
	
	}

	if (src_list == NULL) {
		src_list = new_entry;
	} else {
		new_entry->next = src_list;
		src_list = new_entry;
	}
	
	src_write_unlock();
	
#ifdef DEBUG
	printk("inserting %s in src_table\n", inet_ntoa(ip));
#endif

	return new_entry;
}

src_list_entry * find_src_list_entry(u_int32_t ip) {
	src_list_entry *tmp_entry = src_list;

	while (tmp_entry != NULL) {
		if (tmp_entry->ip == ip) {
			return tmp_entry;
		}
		tmp_entry = tmp_entry->next;
	}

	return NULL;
}

int find_empty_rttable(int index) {
	src_list_entry *tmp_entry = src_list;
	while (tmp_entry != NULL) {
		if (tmp_entry->rt_table == index)
			return 0;
		tmp_entry = tmp_entry->next;
	}
	return 1;
}

int find_first_rttable() {
	int rttable;

	for (rttable = 1; rttable <= MAX_ROUTE_TABLES; rttable++) {
		if (find_empty_rttable(rttable))
			return rttable;
	}
	return rttable;
}

int read_src_list_proc(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data) {
	src_list_entry *tmp_entry;
	static char *my_buffer;
	char temp_buffer[400];
	char src[16];
	int len;
	tmp_entry = src_list;
	my_buffer = buffer;

	src_read_lock();
	sprintf(
			my_buffer,
			"\n Sources Table \n"
				"-------------------------------------------------------------------------------------------\n");
	sprintf(temp_buffer,
			"	IP	   | ROUTE TABLE | TOTAL ROUTES | QoS-D ROUTES | BG ROUTES |\n");
	strcat(my_buffer, temp_buffer);
	sprintf(
			temp_buffer,
			"-------------------------------------------------------------------------------------------\n");

	strcat(my_buffer, temp_buffer);

	while (tmp_entry != NULL) {

		strcpy(src, inet_ntoa(tmp_entry->ip));

		sprintf(
				temp_buffer,
				"  %-16s       %3d           %3d\n",
				src, tmp_entry->rt_table, tmp_entry->num_routes);

		strcat(my_buffer, temp_buffer);
		tmp_entry = tmp_entry->next;
	}

	strcat(
			my_buffer,
			"\n-------------------------------------------------------------------------------------------\n\n");

	len = strlen(my_buffer);
	*buffer_location = my_buffer + offset;
	len -= offset;
	if (len > buffer_length)
		len = buffer_length;
	else if (len < 0)
		len = 0;
	src_read_unlock();
	return len;
}
