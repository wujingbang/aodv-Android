#AODV for kernel 3.4

#FLAGS
#DEBUG: show debug messages
#NOMIPS: used for non-based mipsel systems

CC 		= $(CROSS_COMPILE)gcc
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

obj-m := fbaodv_panda.o
fbaodv_panda-objs := fbaodv_protocol.o aodv_dev.o aodv_neigh.o aodv_route.o aodv_thread.o hello.o packet_in.o packet_out.o rerr.o rrep.o rreq.o socket.o task_queue.o timer_queue.o utils.o gw_list.o probe_ett.o rpdb.o src_list.o aodv_neigh_2h.o route_alg.o st_rreq.o tos.o packet_queue.o brk_list.o rcvp.o
#obj-m := fbaodv_intel.o
#fbaodv_intel-objs := fbaodv_protocol.o aodv_dev.o aodv_neigh.o aodv_route.o aodv_thread.o hello.o packet_in.o packet_out.o rerr.o rrep.o rreq.o socket.o task_queue.o timer_queue.o utils.o gw_list.o probe_ett.o rpdb.o src_list.o aodv_neigh_2h.o route_alg.o st_rreq.o tos.o packet_queue.o brk_list.o rcvp.o

KDIR :=/home/cai/lab/pandaboard_kernel/linaro-kernel
#KDIR :=/home/cai/lab/intel_kernel/kernel

PWD := $(shell pwd)

EXTRA_CFLAGS += -DNOMIPS -fno-pic

default:
	$(MAKE)	-C $(KDIR) SUBDIRS=$(PWD) modules 
clean:
	rm *.o *.ko 
	rm *.mod.c
