/***************************************************************************
                          brk_list.h  -  description
                             -------------------
    begin                : Thu May 15 2014
    copyright            : (C) 2014 by Cai Bingying
    email                :
 ***************************************************************************/

#ifndef BRK_LIST_H
#define BRK_LIST_H

#include "aodv.h"


//find first brk entry
brk_link *first_brk_link();

//initiate the brk list--empty
void init_brk_list(void);

//expire some brk link -- 使某个断路条目过期
void expire_brk_link(brk_link * tmp_link);

//remove brk link -- 删除某个断路条目
void remove_brk_link(brk_link * dead_link);

////find brk link by dst
brk_link *find_first_brk_link_with_dst(u_int32_t dst_ip);

//find brk link by src & dst -- 寻找指定的断路条目
brk_link *find_brk_link(u_int32_t src_ip, u_int32_t dst_ip);


//clean up the brk list -- 此功能保留，brk_list的清空应无需与内核交互
int cleanup_brk_list();

//创建新的断路条目
brk_link *create_brk_link(u_int32_t src_ip, u_int32_t dst_ip, u_int32_t lasthop, u_int32_t lastavail);

//insert a new brk link entry,and sort it first by dst
//插入新的断路条目，并先按dst排序
void insert_brk_link(brk_link * new_link);

int flush_brk_list();

#endif

