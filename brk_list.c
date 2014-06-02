/***************************************************************************
                          brk_list.h  -  description
                             -------------------
    begin                : Thu May 15 2014
    copyright            : (C) 2014 by Cai Bingying
    email                :
 ***************************************************************************/

 //manage the brk_list
//处理断路表，添加条目，删除条目，寻找指定条目等
 #include "brk_list.h"


brk_link *brk_list;
extern u_int32_t g_broadcast_ip;
extern u_int32_t g_mesh_ip;
extern u_int8_t g_routing_metric;
extern aodv_dev *g_mesh_dev;

DEFINE_RWLOCK(brk_list_lock);

//read & write lock & unlock -- 读写锁
inline void brk_list_read_lock(void) {
	read_lock_bh(&brk_list_lock);
}

inline void brk_list_read_unlock(void) {
	read_unlock_bh(&brk_list_lock);
}

inline void brk_list_write_lock(void) {
	write_lock_bh(&brk_list_lock);
}

inline void brk_list_write_unlock(void) {

	write_unlock_bh(&brk_list_lock);
}

//find first brk entry
brk_link *first_brk_link() {
	return brk_list;
}

//initiate the brk list--empty
void init_brk_list(void) {
	brk_list = NULL;
}

//expire some brk link -- 使某个断路条目过期
void expire_brk_link(brk_link * tmp_link) {

	brk_list_write_lock();
	tmp_link->lifetime = (getcurrtime() + DELETE_PERIOD);
	tmp_link->state = INVALID;
	brk_list_write_unlock();
}

//remove brk link -- 删除某个断路条目
void remove_brk_link(brk_link *dead_link) {

	brk_list_write_lock(); //to avoid conflicts

//对此有疑问，dead_link的后继与表中的是一致的么？奇怪
	if (brk_list == dead_link) {
		brk_list = dead_link->next;
	}
	if (dead_link->prev != NULL) {
		dead_link->prev->next = dead_link->next;
	}
	if (dead_link->next!=NULL) {
		dead_link->next->prev = dead_link->prev;
	}

	brk_list_write_unlock();

	kfree(dead_link);
}

//find brk link by dst--记得测试一下返回后其下一跳长什么样
brk_link *find_first_brk_link_with_dst(u_int32_t dst_ip){

    brk_link *tmp_link;
    brk_list_read_lock();

    tmp_link = brk_list;

    while((tmp_link != NULL) && (tmp_link->dst_ip < dst_ip)){
        tmp_link = tmp_link->next;
    }

    if((tmp_link == NULL) || (tmp_link->dst_ip != dst_ip) ){//not found
       brk_list_read_unlock();
       return NULL;
    }
    else
    {
        brk_list_read_unlock();
        return tmp_link;
    }
}

//find brk link by src & dst -- 寻找指定的断路条目
brk_link *find_brk_link(u_int32_t src_ip, u_int32_t dst_ip) {

	brk_link *tmp_link;

	/*lock list */
	brk_list_read_lock();

	tmp_link = brk_list;

	while ((tmp_link != NULL) && (tmp_link->dst_ip < dst_ip)) { //serching(sorted by dst_ip in insert)
		tmp_link = tmp_link->next;
	}


	if ((tmp_link == NULL) || (tmp_link->dst_ip != dst_ip)) { //not found!
		brk_list_read_unlock();
		return NULL;
	}
	else {
		while ((tmp_link != NULL) && (tmp_link->dst_ip == dst_ip)) {
			if (tmp_link->src_ip == src_ip) {
				brk_list_read_unlock();
				//found it
				return tmp_link;
			}
			tmp_link = tmp_link->next;
		}
	}

	brk_list_read_unlock();
	return NULL;//not found

}


//clean up the brk list -- 此功能保留，brk_list的清空应无需与内核交互
int cleanup_brk_list() {
	brk_link *dead_link, *tmp_link;

	int error;

	tmp_link = brk_list;
	//如果断路表不为空，直接一个一个删除，无需像路由表一样跟内核交互
	while (tmp_link != NULL) {

	    dead_link = tmp_link;
		tmp_link = tmp_link->next;
		kfree(dead_link);

	}

	brk_list = NULL;

	return 0;
}

//创建新的断路条目
brk_link *create_brk_link(u_int32_t src_ip, u_int32_t dst_ip,
                          u_int32_t lasthop,u_int32_t lastavail) {

    brk_link *tmp_entry;
#ifdef CaiDebug
	char src[20];
	char dst[20];
	char lhop[20];
	char lavail[20];
	strcpy(src, inet_ntoa(src_ip));
	strcpy(dst, inet_ntoa(dst_ip));
    strcpy(lhop, inet_ntoa(lasthop));
	strcpy(lavail, inet_ntoa(lastavail));

	printk ("create_brk_link: Creating %s to %s via %s,last DTN is %s\n", src,dst,lhop,lavail);
#endif

	/* Allocate memory for new entry */

	if ((tmp_entry = (brk_link *) kmalloc(sizeof(brk_link), GFP_ATOMIC)) == NULL) {

		printk("Error getting memory for new brk link\n");
		return NULL;
	}

	tmp_entry->dst_ip = dst_ip;
	tmp_entry->dst_id = htonl(dst_ip);
	tmp_entry->src_ip = src_ip;
	tmp_entry->last_hop = lasthop;
	tmp_entry->state = ACTIVE;
	//tmp_entry->last_avail_ip = NULL;

//#ifdef DTN
    tmp_entry->last_avail_ip = lastavail;//若没有则为空，该字段不能缺少
//#endif

	tmp_entry->prev = NULL;
	tmp_entry->next = NULL;

	tmp_entry->lifetime = getcurrtime() + BRK_LINK_TIMEOUT;//该删除时间是否应该调整到稍微大一点？？？

	insert_brk_link(tmp_entry);
#ifdef CaiDebug
	if (brk_list == NULL) // if the brk_list is empty
	{
		printk ("brk list still NULL after insert!! ");
	}
#endif
	return tmp_entry;
}


//insert a new brk link entry,and sort it first by dst
//插入新的断路条目，并先按dst排序
void insert_brk_link(brk_link *new_link) { //ordenamos por dst_ip

    brk_link *tmp_link;
    brk_link *prev_link = NULL;

	brk_list_write_lock(); //to avoid conflicts

	tmp_link = brk_list;

	while ((tmp_link != NULL) && (tmp_link->dst_ip < new_link->dst_ip)) //entries ordered by dst_ip
	{
		prev_link = tmp_link;
		tmp_link = tmp_link->next;
	}

	if (brk_list && (tmp_link == brk_list)) // if it goes in the first spot in the list
	{
		brk_list->prev = new_link;
		new_link->next = brk_list;
		brk_list = new_link;
#ifdef CaiDebug
printk("New link should be put in the head of the list in brk_list.c\n");
#endif
		brk_list_write_unlock();
		return ;
	}

	if (brk_list == NULL) // if the brk_list is empty
	{
		brk_list = new_link;
		brk_list_write_unlock();
#ifdef CaiDebug
printk("The list is empty in brk_list.c\n");
#endif
		return;
	}

	if (tmp_link == NULL) //i'm going in next spot in the list and it is empty
	{
		new_link->next = NULL;
		new_link->prev = prev_link;
		prev_link->next = new_link;
		brk_list_write_unlock();
#ifdef CaiDebug
printk("New link should be put in the last of the list in brk_list.c\n");
#endif
		return;
	}
    //found it in somewhere of the list
	tmp_link->prev = new_link;
	new_link->next = tmp_link;
	new_link->prev = prev_link;
	prev_link->next = new_link;
	brk_list_write_unlock();
	return;
}


int flush_brk_list() {
	u_int64_t currtime = getcurrtime();
	brk_link *dead_link, *tmp_link, *prev_link=NULL;

	tmp_link = brk_list;

	while (tmp_link!=NULL) {
		prev_link = tmp_link;

		if (time_before((unsigned long) tmp_link->lifetime,
				(unsigned long) currtime)) {//生命周期在当前时间之前

			if (tmp_link->state != INVALID) {//未过期则使其过期
				expire_brk_link(tmp_link);
				tmp_link = tmp_link->next;
			}
			else {//已标志过期，则删除。

				}

				dead_link = tmp_link;
				prev_link = tmp_link->prev;
				tmp_link = tmp_link->next;
				remove_brk_link(dead_link);
			}

			tmp_link = tmp_link->next;
		} 
			
	//insert_timer_simple(TASK_CLEANUP, HELLO_INTERVAL, g_mesh_ip);
	//update_timer_queue();
	return 1;
}

