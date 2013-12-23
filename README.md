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


