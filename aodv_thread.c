/***************************************************************************
 aodv_thread.c  -  description
 -------------------
 begin                : Tue Aug 12 2003
 copyright            : (C) 2003 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/


#include "aodv_thread.h"
#include <linux/sched.h>
//JL: Added kernel threads interface:
#include <linux/kthread.h>

//static int aodv_pid;
wait_queue_head_t aodv_wait;

static atomic_t kill_thread;
static atomic_t aodv_is_dead;
int initialized =0;

//extern u_int32_t g_aodv_gateway;
extern u_int32_t g_mesh_ip;

struct task_struct *aodv_task;

void kick_aodv() {
	//We are trying to wake up AODV!!!
	//AODV thread is an interupptible sleep state.... this interupts it
	wake_up_interruptible(&aodv_wait);
}

void kill_aodv() {

	wait_queue_head_t queue;
	init_waitqueue_head(&queue);
	//sets a flag letting the thread know it should die
	//wait for the thread to set flag saying it is dead
	//lower semaphore for the thread
	atomic_set(&kill_thread, 1);
	wake_up_interruptible(&aodv_wait);
	interruptible_sleep_on_timeout(&queue, HZ);
	//kthread_should_stop();
}

void aodv(void) {
	//The queue holding all the events to be dealt with
	task *tmp_task;

	//Initalize the variables
	init_waitqueue_head(&aodv_wait);
	atomic_set(&kill_thread, 0);
	atomic_set(&aodv_is_dead, 0);

	//Name our thread
	/*lock_kernel();
	 sprintk(current->comm, "aodv-mcc");
	 //exit_mm(current); --> equivale a: current->mm = NULL;
	 //we are in a kthread? aren't we?
	 unlock_kernel();*/

	//add_wait_queue_exclusive(event_socket->sk->sleep,&(aodv_wait));
	//	add_wait_queue(&(aodv_wait),event_socket->sk->sleep);


	//why would I ever want to stop ? :)
	for (;;) {
		//should the thread exit?
		if (atomic_read(&kill_thread)) {
			goto exit;
		}
		//goto sleep until we recieve an interupt
		interruptible_sleep_on(&aodv_wait);

		//should the thread exit?
		if (atomic_read(&kill_thread)) {
			goto exit;
		}
		//While the buffer is not empty
		while ((tmp_task = get_task()) != NULL) {

			//takes a different action depending on what type of event is recieved
			switch (tmp_task->type) {

			//RREP
			case TASK_RECV_RREP:
				recv_rrep(tmp_task);
				kfree(tmp_task->data);
				break;

				//RERR
			case TASK_RECV_RERR:
				recv_rerr(tmp_task);
				kfree(tmp_task->data);
				break;

			case TASK_RECV_HELLO:
				recv_hello(tmp_task);
				kfree(tmp_task->data);
				break;

				//Cleanup  the Route Table and Flood ID queue
			case TASK_CLEANUP:
				flush_aodv_route_table();
				break;

			case TASK_HELLO:
				gen_hello();
				break;

			case TASK_ST:
				gen_st_rreq();
				break;
			
			case TASK_GW_CLEANUP:
				update_gw_lifetimes();
				insert_timer_simple(TASK_GW_CLEANUP, ACTIVE_GWROUTE_TIMEOUT,
						g_mesh_ip);
				update_timer_queue();
				break;
			
			case TASK_NEIGHBOR:
			delete_aodv_neigh(tmp_task->src_ip);
				break;

			case TASK_ROUTE_CLEANUP:
				flush_aodv_route_table();
				break;

			case TASK_SEND_ETT:
				send_probe(tmp_task->dst_ip);
				break;
				//A small probe packet is received
			case TASK_RECV_S_ETT:
				recv_sprobe(tmp_task);
				kfree(tmp_task->data);
				break;
				//A large probe packet is received
			case TASK_RECV_L_ETT:
				recv_lprobe(tmp_task);
				kfree(tmp_task->data);
				break;
			case TASK_ETT_CLEANUP:
				reset_ett(find_aodv_neigh(tmp_task->src_ip));
				printk("Reseting ETT-Info from neighbour %s\n",
						inet_ntoa(tmp_task->src_ip));
				break;
				
			case TASK_NEIGHBOR_2H:
				delete_aodv_neigh_2h(tmp_task->src_ip);
				break;
					
			case TASK_RECV_RREQ:
				recv_rreq(tmp_task);
				kfree(tmp_task->data);
				break;
					
		
			case TASK_RESEND_RREQ:
				resend_rreq(tmp_task);
				break;
				
			
			case TASK_ETT_INFO:
				recv_ett_info(tmp_task);
				kfree(tmp_task->data);
				break;
			case TASK_SEND_RREP:
				gen_rrep(tmp_task->src_ip, tmp_task->dst_ip, tmp_task->tos);
				break;
		
			case TASK_RECV_STRREQ:
				recv_rreq_st(tmp_task);
				kfree(tmp_task->data);
				break;
				
			case TASK_UPDATE_LOAD:
				update_my_load();
				break;
	
			default:
				break;
			}

			kfree(tmp_task);

		}

	}

	exit:
	//Set the flag that shows you are dead
	atomic_set(&aodv_is_dead, 1);

}

void startup_aodv() {
	//aodv_pid = kernel_thread((void *) &aodv, NULL, 0);
	aodv_task = kthread_run((void *) &aodv, NULL, "fbaodv_protocol");
	initialized = 1;
}
