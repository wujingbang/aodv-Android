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

//sub queue--control tasks
task *sub_q;
task *sub_end;
//

void queue_lock(void) {
	spin_lock_bh(&task_queue_lock);
}

void queue_unlock(void) {
	spin_unlock_bh(&task_queue_lock);
}

void init_task_queue() {
	task_q=NULL;
	task_end=NULL;
	
	sub_q=NULL;
	sub_end=NULL;
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
///
	new_task->prev_control = NULL;
///

	new_task->tos = 0;

	return new_task;

}


int is_control_task(int type){

	switch(type)

	{

		case TASK_ETT_INFO:

		case TASK_SEND_ETT:

		case TASK_RECV_S_ETT:

		case TASK_RECV_L_ETT: return 0;



		case TASK_HELLO:

	  	case TASK_NEIGHBOR:

		case TASK_CLEANUP:

		case TASK_ROUTE_CLEANUP:

		case TASK_RECV_RREQ:

		case TASK_RESEND_RREQ:

		case TASK_RECV_RERR:

		case TASK_RECV_HELLO:

		case TASK_RECV_RREP:

		case TASK_SEND_RREP:



		case TASK_ST:

		case TASK_GW_CLEANUP:

		case TASK_RECV_STRREQ:



		case TASK_ETT_CLEANUP:

		case TASK_NEIGHBOR_2H:

		case TASK_UPDATE_LOAD:
		case TASK_RECV_RCVP:
<<<<<<< HEAD
		//case TASK_RECV_RRDP:
=======
		case TASK_RECV_RRDP:
>>>>>>> addDTNneigh
		case TASK_DTN_HELLO:return 1;



		default:return 0;

	}
	return 0;

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


//#ifdef CaiDebug
	//if(new_entry->type==TASK_DTN_HELLO)
		//printk("-----------%d---------------\n",new_entry->type);
//#endif
	//add the new task already
	if(is_control_task(new_entry->type)){
		if(sub_q==NULL && sub_end==NULL){//the sub queue is empty
			sub_q = new_entry;
			sub_end = new_entry;
			
		}
		else{
			sub_q->prev_control = new_entry;
			sub_q = new_entry;
		}
#ifdef CaiDebug
			//printk("-----------%d:126---------------\n",new_entry->type);
#endif
	}

	//unlock table
	queue_unlock();

	//wake up the AODV thread
	kick_aodv();

	return 0;
}

task *get_task() {
	task *tmp_task = NULL;
	
	task *prev;
	task *next;

	queue_lock();

	//get control task first
	if(sub_end){

		tmp_task = sub_end;
		//#ifdef CaiDebug
			//printk("-----------type=%d---------------\n",tmp_task->type);
		//#endif
		
		if(sub_end == sub_q){//one element
			if( (task_end==task_q) && (task_end==sub_end) ){
				task_q = NULL;
				task_end = NULL;
			}
			else if(task_end==sub_end){//at the end of queue & subqueue
				task_end = task_end->prev;
			}
			else if(task_q==sub_q){//at the beginning of queue &subqueue
				task_q = task_q->next;
				task_q->prev = NULL;
			}
			else{//at some where in the queue
				prev = tmp_task->prev;
				next = tmp_task->next;
				prev->next = next;
				next->prev = prev;
			}

			sub_q = NULL;
			sub_end = NULL;
		}//if sub_end == sub_q
		else{
			if(task_end==sub_end){//at the end of queue & subqueue
				task_end = task_end->prev;
				
			}
			else{//at some where in the queue
				prev = tmp_task->prev;
				next = tmp_task->next;
				prev->next = next;
				next->prev = prev;
			}
			
			sub_end = sub_end->prev_control;
			
			//#ifdef CaiDebug
			///printk("-----------type=%d---------------\n",tmp_task->type);
			//#endif

			
		}//if-else
		
		queue_unlock();

//#ifdef CaiDebug
		//if(tmp_task->type==TASK_DTN_HELLO)
		//printk("-----------%d---------------\n",tmp_task->type);
//#endif
		
		return tmp_task;
	}//if sub_end
	
	#ifdef CaiDebug
			//printk("-----------data task---------------\n");
		#endif
	//if no control task ,get the end of the queue
	if (task_end) {
		tmp_task = task_end;
		if (task_end == task_q) {
			task_q = NULL;
			task_end = NULL;
		} else {
			task_end = task_end->prev;
		}
		queue_unlock();
		#ifdef CaiDebug
			//printk("-----------data task:%d---------------\n",tmp_task);
		#endif
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

	//the del neigh task should be managed first
//#ifdef CaiDebug
//printk(" Task type %d\n",timer_task->type);//103
//#endif
	//if(timer_task->type == TASK_NEIGHBOR)
		//queue_at_head(timer_task);
	//else
		queue_aodv_task(timer_task);
	
	return 1;
}

