#!/bin/bash
usage(){
       	echo "usage: `basename $0` ip-addrs"
}
IP=$1
if [ $# -ne 1 ]; then
        usage
        exit 1
fi
eth0_up=0

ifconfig eth0 $IP
if [ $? -eq 0 ];then
	eth0_up=1
	echo "eth0 is up now!"
fi

MESH_DEV="mesh_dev=eth0"
AODV_NET="network_ip=$IP/255.255.254.0"
GATEWAY="aodv_gateway=0"
METRIC="routing_metric=HOPS"
RATE="nominal_rate=60"
echo "1" > /proc/sys/net/ipv4/ip_forward
echo "Running AODV-MCC"
rmmod fbaodv_panda
echo "clean log!"
echo > /system/log
if [ $eth0_up -eq 1 ];then
	insmod /system/fbaodv_panda.ko $MESH_DEV $AODV_NET $METRIC $RATE $GATEWAY aodv_blacklist="192.168.1.6,192.168.1.5" dtn_blacklist="192.168.1.6,192.168.1.5"
fi

echo "change iSerial"
echo -n med-$IP > /sys/class/android_usb/android0/iSerial
