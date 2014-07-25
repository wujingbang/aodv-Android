/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "tos.h"

flow_type *flow_type_table;

void insert_flow_type(int count){

	flow_type *new_type = NULL;
	flow_type *prev_type = NULL;

	if ((new_type = kmalloc(sizeof(flow_type), GFP_ATOMIC)) == NULL) {
	#ifdef DEBUG
			printk("TOS LIST: Can't allocate new entry\n");
	#endif
			return;
	}

	switch (count){

		case 0:
		prev_type = flow_type_table;
		new_type->tos = 0x02;
		new_type->avg_rate = 1000; //1Mbps
		new_type->avg_size = 1472; //bytes
		new_type->next = prev_type;
		flow_type_table = new_type;
		return;

		case 1:
		prev_type = flow_type_table;
		new_type->tos = 0x04;
		new_type->avg_rate = 750; //750kbps
		new_type->avg_size = 1472;
		new_type->next = flow_type_table;
		flow_type_table = new_type;
		return;

		case 2:
			prev_type = flow_type_table;
			new_type->tos = 0x08;
			new_type->avg_rate = 500;
			new_type->avg_size = 1472;
			new_type->next = flow_type_table;
			flow_type_table = new_type;
			return;

		case 3:
			prev_type = flow_type_table;
			new_type->tos = 0x10;
			new_type->avg_rate = 250;
			new_type->avg_size = 1472;
			new_type->next = flow_type_table;
			flow_type_table = new_type;
			return;

		case 4:
			prev_type = flow_type_table;
			new_type->tos = 0x0c;
			new_type->avg_rate = 100;
			new_type->avg_size = 172;
			new_type->next = flow_type_table;
			flow_type_table = new_type;
			return;

		case 5:
			prev_type = flow_type_table;
			new_type->tos = 0x14;
			new_type->avg_rate = 300;
			new_type->avg_size = 972;
			new_type->next = flow_type_table;
			flow_type_table = new_type;
			return;

		case 6:
			prev_type = flow_type_table;
			new_type->tos = 0x18;
			new_type->avg_rate = 250;
			new_type->avg_size = 1472;
			new_type->next = flow_type_table;
			flow_type_table = new_type;
			return;

		case 7:
			prev_type = flow_type_table;
			new_type->tos = 0x1c;
			new_type->avg_rate = 500;
			new_type->avg_size = 1472;
			new_type->next = flow_type_table;
			flow_type_table = new_type;
			return;

		default:
			return;

	}


}

flow_type *find_flow_type(unsigned char tos) {
	flow_type *tmp_flow;

	tmp_flow = flow_type_table;
//	if ( tmp_flow == NULL )
//	{
//		printk("flow_type_table is NULL!\n");
//	}
//	printk ("tos is %c\n", tos);
	while (tmp_flow != NULL) {
		if (tmp_flow->tos == tos)
			return tmp_flow;

		tmp_flow = tmp_flow->next;

	}
	return NULL;

}



void init_flow_type_table(void){

	int i=0;
	flow_type_table = NULL;
	for (i=0; i<NUM_TOS; i++)
		insert_flow_type(i);
}

int read_flow_type_table_proc(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data) {
	flow_type *tmp_entry;
	static char *my_buffer;
	char temp_buffer[400];
	int len;
	char tos[16];
	tmp_entry = flow_type_table;
	if (tmp_entry == NULL)
		return 0;
	my_buffer = buffer;


	sprintf(my_buffer, "\nAvailable ToS Table \n"
		"----------------------------------------------------------------\n");
	sprintf(temp_buffer,
			" TOS | DSCP |  AVG_RATE | AVG_SIZE |\n");
	strcat(my_buffer, temp_buffer);
	sprintf(temp_buffer,
			"----------------------------------------------------------------\n");

	strcat(my_buffer, temp_buffer);

	while (tmp_entry != NULL) {

		switch (tmp_entry->tos){

			case 0x02:
				sprintf(tos, "0x02  ");
				strcat(tos, "  -- ");
				break;
			case 0x04:
				sprintf(tos, "0x04  ");
				strcat(tos,  "  -- ");
				break;
			case 0x08:
				sprintf(tos, "0x08  ");
				strcat(tos,  "  -- ");
				break;
			case 0x10:
				sprintf(tos, "0x10  ");
				strcat(tos,  "  -- ");
				break;
			case 0x0c:
				sprintf(tos, "0x0c  ");
				strcat(tos,  " 0x03");
				break;
			case 0x14:
				sprintf(tos, "0x14  ");
				strcat(tos,  " 0x05");
				break;
			case 0x18:
				sprintf(tos, "0x18  ");
				strcat(tos,  " 0x06");
				break;
			case 0x1c:
				sprintf(tos, "0x1c  ");
				strcat(tos,  " 0x07");
				break;
		}



		sprintf(temp_buffer, "%-16s    %-8d     %-8d \n", tos, tmp_entry->avg_rate, tmp_entry->avg_size);

		strcat(my_buffer, temp_buffer);

		tmp_entry = tmp_entry->next;
	}



	strcat(my_buffer,
			"\n----------------------------------------------------------------\n\n");

	len = strlen(my_buffer);
	*buffer_location = my_buffer + offset;
	len -= offset;
	if (len > buffer_length)
		len = buffer_length;
	else if (len < 0)
		len = 0;

	return len;
}

void flush_flow_type_table(void){

	flow_type *tmp_entry = flow_type_table;
	flow_type *prev_entry = NULL;
	while (tmp_entry != NULL) {
		prev_entry = tmp_entry;
		tmp_entry = tmp_entry->next;
		kfree(prev_entry);
	}


}
