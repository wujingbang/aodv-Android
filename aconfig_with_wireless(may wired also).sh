#!/bin/bash
usage(){
       	echo "usage: `basename $0` ip-addrs"
}
IP=$1
if [ $# -ne 1 ]; then
        usage
        exit 1
fi

del_adhoc0=0
del_wlan0=0
add_adhoc0=0
adhoc0_up=0
adhoc0_join=0

ifconfig adhoc0 $IP
if [ $? -eq 0 ];then
	iw dev adhoc0 del
	if [ $? -eq 0 ];then
		del_adhoc0=1
		echo "adhoc0 del done!"
	else
		exit 1;
	fi
fi

ifconfig wlan0 $IP
if [ $? -eq 0 ];then
	iw dev wlan0 del
	if [ $? -eq 0 ];then
		del_wlan0=1
		echo "wlan0 del done!"
	else
		exit 1;
	fi
fi

iw phy phy0 interface add adhoc0 type ibss
if [ $? -eq 0 ];then
	add_adhoc0=1
	echo "iw phy phy0 interface add adhoc0 done!"
else
	exit 1;
fi

if [ $add_adhoc0 -eq 1 ];then
	ifconfig adhoc0 up
	if [ $? -eq 0 ];then
		adhoc0_up=1
		echo "ifconfig adhoc0 up done!"
	else
		exit 1;
	fi
fi

if [ $adhoc0_up -eq 1 ];then
	iw dev adhoc0 ibss join wujingbang 2412
	if [ $? -eq 0 ];then
		adhoc0_join=1
		echo "iw dev adhoc0 ibss join done!"
	else
		exit 1;
	fi
fi

ifconfig adhoc0 $IP
ifconfig adhoc0

#fbaodv config
MESH_DEV="mesh_dev=adhoc0"

AODV_NET="network_ip=$IP/255.255.255.0"
GATEWAY="aodv_gateway=0"
#METRIC="routing_metric=ETT"
METRIC="routing_metric=HOPS"
RATE="nominal_rate=60"
echo "1" > /proc/sys/net/ipv4/ip_forward
echo "Running AODV-MCC"
rmmod fbaodv_panda
echo "clean log!"
echo > /system/log
if [ $adhoc0_join -eq 1 ];then
	insmod /system/fbaodv_panda.ko $MESH_DEV $AODV_NET $METRIC $RATE $GATEWAY aodv_blacklist="192.168.1.12" dtn_blacklist="192.168.1.12"
fi

echo "change iSerial"
echo -n pan-$IP > /sys/class/android_usb/android0/iSerial