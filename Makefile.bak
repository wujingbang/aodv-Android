#AODV for kernel 2.6

#FLAGS
#DEBUG: show debug messages
#NOMIPS: used for non-based mipsel systems
#KERNEL2_6_26: kernel 2.6.26

CC := gcc

obj-m := fbaodv_proto.o
fbaodv_proto-objs := fbaodv_protocol.o aodv_dev.o aodv_neigh.o aodv_route.o aodv_thread.o hello.o packet_in.o packet_out.o rerr.o rrep.o rreq.o socket.o task_queue.o timer_queue.o utils.o gw_list.o probe_ett.o rpdb.o src_list.o aodv_neigh_2h.o route_alg.o st_rreq.o tos.o packet_queue.o

KDIR :=  /lib/modules/2.6.32.10/build/

PWD := $(shell pwd)

EXTRA_CFLAGS += -DNOMIPS -DDEBUG

default:
	$(MAKE)	-C $(KDIR) SUBDIRS=$(PWD) modules 
clean:
	rm *.o *.ko 
	rm *.mod.c
