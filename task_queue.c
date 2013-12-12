/***************************************************************************
 task_queue.c  -  description
 -------------------
 begin                : Tue Jul 8 2003
 copyright            : (C) 2003 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/

#include "task_queue.h"

//spinlock_t task_queue_lock = SPIN_LOCK_UNLOCKED;
DEFINE_SPINLOCK(task_queue_lock);
task *task_q;
task *task_end;

void queue_lock(void) {
	spin_lock_bh(&task_queue_lock);
}

void queue_unlock(void) {
	spin_unlock_bh(&task_queue_lock);
}

void init_task_queue() {
	task_q=NULL;
	task_end=NULL;
}

void cleanup_task_queue() {
	task *dead_task, *tmp_task;

	queue_lock();
	tmp_task = task_q;
	task_q=NULL;

	while (tmp_task) {
		dead_task = tmp_task;
		tmp_task = tmp_task->next;
		kfree(dead_task->data);
		kfree(dead_task);
	}
	queue_unlock();
}

task *create_task(int type) {
	task *new_task;

	new_task = (task *) kmalloc(sizeof(task), GFP_ATOMIC);

	if (new_task == NULL) {
		printk("Not enough memory to create Event Queue Entry\n");
		return NULL;
	}

	new_task->time = getcurrtime();
	new_task->type = type;
	new_task->src_ip = 0;
	new_task->dst_ip = 0;
	new_task->ttl = 0;
	new_task->retries = 0;
	new_task->data = NULL;
	new_task->data_len = 0;
	new_task->next = NULL;
	new_task->prev = NULL;
	new_task->dev = NULL;

	new_task->tos = 0;

	return new_task;

}

int queue_aodv_task(task * new_entry) {

	/*lock table */
	queue_lock();

	//Set all the variables
	new_entry->next = task_q;
	new_entry->prev = NULL;

	if (task_q != NULL) {
		task_q->prev = new_entry;
	}

	if (task_end == NULL) {
		task_end = new_entry;
	}

	task_q = new_entry;

	//unlock table
	queue_unlock();

	//wake up the AODV thread
	kick_aodv();

	return 0;
}

task *get_task() {
	task *tmp_task = NULL;

	queue_lock();
	if (task_end) {
		tmp_task = task_end;
		if (task_end == task_q) {
			task_q = NULL;
			task_end = NULL;
		} else {
			task_end = task_end->prev;
		}
		queue_unlock();
		return tmp_task;
	}
	if (task_q != NULL) {
		printk("TASK_QUEUE: Error with task queue\n");
	}
	queue_unlock();
	return NULL;

}

int insert_task(int type, struct sk_buff *packet) {
	task *new_task;
	struct iphdr *ip;

	int start_point = sizeof(struct udphdr) + sizeof(struct iphdr);

	new_task = create_task(type);

	if (!new_task) {
		printk("Not enough memory to create Task\n");
		return -ENOMEM;
	}

	ip = ip_hdr(packet);
	new_task->src_ip = ip->saddr;
	new_task->dst_ip = ip->daddr;

	new_task->ttl = ip->ttl;
	new_task->dev = packet->dev;
	new_task->data_len = packet->len - start_point;

	//create space for the data and copy it there
	new_task->data = kmalloc(new_task->data_len, GFP_ATOMIC);
	if (!new_task->data) {
		kfree(new_task);
		printk("Not enough memory to create Event Queue Data Entry\n");
		return -ENOMEM;
	}

	memcpy(new_task->data, packet->data + start_point, new_task->data_len);



	queue_aodv_task(new_task);
	return 0;
}

int insert_task_from_timer(task * timer_task) {

	if (!timer_task) {
		printk("Passed a Null task Task\n");
		return -ENOMEM;
	}

	queue_aodv_task(timer_task);
	return 1;
}
