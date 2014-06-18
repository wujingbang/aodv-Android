aodv-Android
============

Based on the FBAODV,  I have transplanted it to the Android 4 with linux kernel 3.4

Change the makefile varible 'KDIR', redirect it to your kernel file.

Then, export ARCH & CROSS_COMPILE.

	with x86, this can be:
	
		export ARCH=i386
		
		export CROSS_COMPILE=i686-android-linux-
		
	with arm, this can be:
	
		export ARCH=arm
		
		export CROSS_COMPILE=arm-none-linux-gnueabi-
		
After environment variable was set, 'make' can start the compile.

wait and *.ko is what you want.

transport the .ko adhoc-config.sh file to your device, run the adhoc-config.sh to start.

like:

	sh adhoc-config.sh 192.168.1.1
	
the IP address is your device's IP.

PLUS: Android has block the linux kernel package forwarding, you can use 

	iptables –P FORWARD ACCEPT
	
to open that function.(untest)

OR, you can change linux kernel source code:

	ip_forward， /net/ipv4/ip_forward.c:55
	
	change
	
	return NF_HOOK(NFPROTO_IPV4, NF_INET_FORWARD, skb, skb->dev,
		       rt->dst.dev, ip_forward_finish);

	to
	
	return ip_forward_finish(skb);


#脚本说明：
1.将设备重新挂载，使其可以修改的权限
adb -s "Medfield0B0D835F" shell mount -o remount rw /system
adb -s "Medfield3FC1ECA3" shell mount -o remount rw /system
adb -s "Medfield1CF7A8ED" shell mount -o remount rw /system

2.将change.sh传到相应的设备中，这个是用来修改设备的名字的，需要重启adb shell才行
adb -s "Medfield0B0D835F" push e:\MyScript\change3.sh /system/change.sh
adb -s "Medfield3FC1ECA3" push e:\MyScript\change4.sh /system/change.sh
adb -s "Medfield1CF7A8ED" push e:\MyScript\change5.sh /system/change.sh

3.把adhoc-config-intel.sh脚本放进相应设备，该脚本是用来完成设备的adhoc模式打开
adb -s "Medfield0B0D835F" push e:\MyScript\adhoc-config-intel3.sh /system/adhoc-config-intel.sh
adb -s "Medfield3FC1ECA3" push e:\MyScript\adhoc-config-intel4.sh /system/adhoc-config-intel.sh
adb -s "Medfield1CF7A8ED" push e:\MyScript\adhoc-config-intel5.sh /system/adhoc-config-intel.sh

4.运行设备中的change.sh脚本来修改设备名字
adb -s "Medfield0B0D835F" shell sh /system/change.sh
adb -s "Medfield3FC1ECA3" shell sh /system/change.sh
adb -s "Medfield1CF7A8ED" shell sh /system/change.sh
adb kill-server
adb devices

5.清空设备中的DTN相关的文件，如果只是调试AODV可以不需要这段
adb -s "med-192.168.1.3" shell rm /sdcard/dtn/storage/*
adb -s "med-192.168.1.4" shell rm /sdcard/dtn/storage/*
adb -s "med-192.168.1.5" shell rm /sdcard/dtn/storage/*
adb -s "med-192.168.1.3" shell rm /sdcard/dtn_test_data/*
adb -s "med-192.168.1.4" shell rm /sdcard/dtn_test_data/*
adb -s "med-192.168.1.5" shell rm /sdcard/dtn_test_data/*

6.启动adhoc-config-intel.sh脚本，完成对设备自组网的设置
adb -s "med-192.168.1.3" shell sh /system/adhoc-config-intel.sh 192.168.1.3
adb -s "med-192.168.1.4" shell sh /system/adhoc-config-intel.sh 192.168.1.4
adb -s "med-192.168.1.5" shell sh /system/adhoc-config-intel.sh 192.168.1.5

7.这个是卸载aodv模块，对于DTN处单跳试验时，可以加上这段，后续多跳实验删掉这段即可。
adb -s "med-192.168.1.3" shell rmmod fbaodv_intel
adb -s "med-192.168.1.4" shell rmmod fbaodv_intel
adb -s "med-192.168.1.5" shell rmmod fbaodv_intel
pause

