/***************************************************************************
 Created by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
//Send and receive ETT messages
#include "probe_ett.h"

extern u_int32_t g_mesh_ip;
extern u_int16_t g_fixed_rate;

u_int8_t count_send=0;

//Initiate the delay vector
void delay_vector_init(u_int32_t *r_delays) {
	int i;
	for (i=0; i<ETT_PROBES_MAX; i++)
		r_delays[i]=DEFAULT_ETT_METRIC;

}

void reset_ett(aodv_neigh *reset_neigh) {
	u_int64_t jitter;
	int rand;

	delete_timer(reset_neigh->ip, reset_neigh->ip, NO_TOS, TASK_SEND_ETT);
	reset_neigh->ett.last_count = reset_neigh->ett.count_rcv
			= reset_neigh->ett.sec = reset_neigh->ett.usec
					= reset_neigh->ett.reset = 0;
	reset_neigh->ett.meas_delay = DEFAULT_ETT_METRIC;
	reset_neigh->ett.ett_window = ETT_PROBES_MIN+1; //Initially to MIN value + 1
	reset_neigh->ett.interval = ETT_FAST_INTERVAL;
	reset_neigh->ett.fixed_rate = 0;
	delay_vector_init(&(reset_neigh->ett.recv_delays[0]));
	reset_neigh->send_rate = 0;

	get_random_bytes(&rand, sizeof (rand));
	jitter = (u_int64_t)((rand*reset_neigh->ip)%2000);
	insert_timer_simple(TASK_SEND_ETT, 1000 + 10*jitter, reset_neigh->ip);
	insert_timer_simple(TASK_ETT_CLEANUP, ETT_INTERVAL * (1 + ALLOWED_ETT_LOSS)
			+ 100, reset_neigh->ip);
	update_timer_queue();

#ifdef DEBUG
	printk("Reseting the ETT metric of %s\n",inet_ntoa(reset_neigh->ip));
#endif
	gen_rerr(reset_neigh->ip);

	return;

}

u_int16_t compute_rate(u_int32_t estimation) {

	u_int16_t bps;
	bps = PROBE_PACKET_SIZE/(u_int16_t)estimation;

	if (bps < 38) //bps around 3 - rate = 6Mbps
		return RATE_6;
	else if (bps < 53) //bps around 4,5 - rate = 9Mbps
		return RATE_9;
	else if (bps < 78) //bps around 6 - rate = 12Mbps
		return RATE_12;
	else if (bps < 105) //bps around 9 - rate = 18Mbps
		return RATE_18;
	else if (bps < 150) //bps around 12 - rate = 24Mbps
		return RATE_24;
	else if (bps < 210) //bps around 18 - rate = 36Mbps
		return RATE_36;
	else if (bps < 255) //bps around 24 - rate = 48Mbps
		return RATE_48;
	else
		//bps around 27 - rate = 54Mbps
		return RATE_54;

}

//The idea is to discard incorrect values, i.e. delay measurements which are isolated
//This function searchs in the vector the range of values [MIN, MIN+50 USECS] with the highest quantity of measurements
int find_delay_range(u_int32_t *r_delay, u_int32_t delay, int pos,
		u_int8_t num_probes) {
	int j;
	int count=0;
	u_int32_t difference;

	for (j=pos+1; j<num_probes; j++) {
		difference = (r_delay[j] - delay);
		if (difference <= ETT_RANGE)
			count++;
		else {
			return count;
		}
	}
	return count;
}

//Computes the average delay of the founded range of values
u_int32_t compute_delay(aodv_neigh *tmp_neigh) {
	int count = 0;
	int i, j;
	int max_count = 0;
	u_int32_t fin_delay = 0;
	int max_pos=0;

	for (i=0; i<tmp_neigh->ett.ett_window; i++) {
		count = find_delay_range(&(tmp_neigh->ett.recv_delays[0]),
				tmp_neigh->ett.recv_delays[i], i, tmp_neigh->ett.ett_window);
		if (count > max_count) {
			max_count = count;
			max_pos = i;
		}
	}
	if (max_count < 2) { //It's possible that two incorrect values are in the range, but three it's no so probable

		return 0;
	}

	for (j = max_pos; j <= max_pos + max_count; j++)
		fin_delay = (tmp_neigh->ett.recv_delays[j] + fin_delay);

	fin_delay= fin_delay/(u_int32_t)(max_count+1);
	delay_vector_init(&(tmp_neigh->ett.recv_delays[0]));

	/* Only for 4G ACCESSCUBES
	 if (fin_delay > 2000)
	 fin_delay = fin_delay;
	 else if (fin_delay > 800)
	 fin_delay = fin_delay - TPROC_LOW;
	 else if (fin_delay > TPROC_HIGH)
	 fin_delay = fin_delay - TPROC_HIGH;
	 else
	 fin_delay = fin_delay;*/
	return fin_delay;

}

//Add a new delay entry in the delay-vector - Arranged by value (Lowest delay in the firs position)
void delay_vector_add(aodv_neigh *tmp_neigh, u_int32_t delay) {
	int i, j;
	int count;

	count = tmp_neigh->ett.count_rcv;

	for (i=0; i<= count; i++) {
		if (delay < tmp_neigh->ett.recv_delays[i]) {
			for (j=count; j>i; j--)
				tmp_neigh->ett.recv_delays[j]=tmp_neigh->ett.recv_delays[j-1];
			tmp_neigh->ett.recv_delays[i]=delay;
			return;

		}
	}

}

void compute_estimation(aodv_neigh *tmp_neigh) {

	int recv_delay;
	//Nominal rate computation
	//Add last own transmission delay measured by our neighbour
	tmp_neigh->ett.count_rcv++;
	
	if (tmp_neigh->ett.count_rcv == tmp_neigh->ett.ett_window) { //Let's compute the estimation!
		recv_delay = compute_delay(tmp_neigh);
		
		if (recv_delay == 0 && tmp_neigh->ett.ett_window == ETT_PROBES_MAX) {
			if (tmp_neigh->ett.reset) { //Second consectutive round without an estimation!
#ifdef DEBUG
				printk(
						"Capacity Estimation with %s is not possible - Second: Resetting\n",
						inet_ntoa(tmp_neigh->ip));
#endif
				reset_ett(tmp_neigh);

			} else { //First round without an estimation!
#ifdef DEBUG
				printk(
						"Capacity Estimation with %s is not possible - First: Using last measured values\n",
						inet_ntoa(tmp_neigh->ip));
#endif
				//lets start a new measurements round
				delay_vector_init(&(tmp_neigh->ett.recv_delays[0]));
				tmp_neigh->ett.count_rcv = 0;
				tmp_neigh->ett.ett_window = ETT_PROBES_MIN;
				tmp_neigh->ett.reset = 1;

			}
			send_ett_info(tmp_neigh, tmp_neigh->send_rate, FALSE);
			return;
		}

		if (recv_delay == 0) {
			//Increase the size of the estimation window
			tmp_neigh->ett.ett_window += 2; //lets do two more measurements
			if (tmp_neigh->ett.ett_window > ETT_PROBES_MAX)
				tmp_neigh->ett.ett_window=ETT_PROBES_MAX;
			return;
		}

		if (recv_delay == DEFAULT_ETT_METRIC) {
#ifdef DEBUG
			printk(
					"%s does not receive my ETT_PROBES: Capacity Estimation is not possible - Resetting ETT...\n",
					inet_ntoa(tmp_neigh->ip));
#endif
			reset_ett(tmp_neigh);
			return;
		}
		//lets start a new measurement round
		delay_vector_init(&(tmp_neigh->ett.recv_delays[0]));
		tmp_neigh->ett.count_rcv = 0;
		tmp_neigh->ett.ett_window = ETT_PROBES_MIN;
		tmp_neigh->ett.reset = 0;
		tmp_neigh->send_rate = compute_rate(recv_delay);
		send_ett_info(tmp_neigh, tmp_neigh->send_rate, FALSE);
	}

}

void send_probe(u_int32_t ip) {

	s_probe *tmp_sprobe;
	l_probe *tmp_lprobe;

	aodv_neigh *tmp_neigh = find_aodv_neigh(ip);
	if (tmp_neigh==NULL) {
#ifdef DEBUG
		printk ("ERROR - Sending a ETT PROBE : %s is no more a neighbour\n", inet_ntoa(ip));
#endif
		return;
	}

	count_send++;

	if (count_send == 100) {
		count_send=0;
	}

	if (g_fixed_rate != 0) //Fixed rate - No estimation is needed! Send a ETT info message!
		send_ett_info(tmp_neigh, g_fixed_rate, TRUE);

	else {
		if ((tmp_sprobe = (s_probe *) kmalloc(sizeof(s_probe), GFP_ATOMIC))
				== NULL) {
#ifdef DEBUG
			printk(" Can't allocate new ETT-PROBE\n");
#endif
			return;
		}
		if ((tmp_lprobe = (l_probe *) kmalloc(sizeof(l_probe), GFP_ATOMIC))
				== NULL) {
#ifdef DEBUG
			printk(" Can't allocate new ETT-PROBE\n");
#endif
			kfree(tmp_sprobe);
			return;
		}
		tmp_sprobe->type = ETT_S_MESSAGE;
		tmp_sprobe->n_probe = count_send; // only to assure that the small and large packets recieved were sended at the same time
		tmp_sprobe->src_ip = g_mesh_ip;
		tmp_sprobe->dst_ip = ip;
		tmp_sprobe->reserved1 = 0;

		tmp_lprobe->type = ETT_L_MESSAGE;
		tmp_lprobe->n_probe = count_send;
		tmp_lprobe->src_ip = g_mesh_ip;
		tmp_lprobe->dst_ip = ip;
		tmp_lprobe->last_meas_delay = htonl(tmp_neigh->ett.meas_delay);
		tmp_lprobe->my_rate = htons(tmp_neigh->send_rate);

		//estimations are based on Unicast messages, so the transmission rate will be the data one
		send_ett_probe(ip, tmp_sprobe, sizeof(s_probe), tmp_lprobe,
				sizeof(l_probe));

		kfree(tmp_lprobe);
		kfree(tmp_sprobe);
	}

	insert_timer_simple(TASK_SEND_ETT, tmp_neigh->ett.interval, ip);
	update_timer_queue();
	return;

}

void send_ett_info(aodv_neigh *tmp_neigh, u_int16_t my_rate, int fixed) {

	ett_info *tmp_ett_info;
	if ((tmp_ett_info = (ett_info *) kmalloc(sizeof(tmp_ett_info), GFP_ATOMIC))
			== NULL) {
#ifdef DEBUG
		printk("Can't allocate new ETT_INFO_MSG\n");
#endif
		return;
	}
	tmp_ett_info->type = ETT_INFO_MSG;
	tmp_ett_info->fixed_rate = fixed;
	tmp_ett_info->my_rate = htons(my_rate);
	tmp_ett_info->last_meas_delay = htonl(tmp_neigh->ett.meas_delay);
	send_message(tmp_neigh->ip, NET_DIAMETER, tmp_ett_info, sizeof(ett_info));

	kfree(tmp_ett_info);

	return;

}

void recv_sprobe(task * tmp_packet) {

	aodv_neigh *tmp_neigh;
	s_probe *tmp_sprobe;

	tmp_sprobe = tmp_packet->data;
	tmp_neigh = find_aodv_neigh(tmp_packet->src_ip);

	if (tmp_neigh == NULL) {
#ifdef DEBUG
		printk ("Error: Source %s of an incoming small probe-packet belongs to any neighbour\n", inet_ntoa(tmp_packet->src_ip));
#endif
		return;
	}

	tmp_neigh->ett.last_count = tmp_sprobe->n_probe; //to compare with next large packet received
	tmp_neigh->ett.sec = tmp_sprobe->sec; //the arrival time is saved
	tmp_neigh->ett.usec = tmp_sprobe->usec;

	return;
}

void recv_lprobe(task * tmp_packet) {
	aodv_neigh *tmp_neigh;
	l_probe *tmp_lprobe;
	u_int32_t res_sec, res_usec, sec, usec;

	tmp_lprobe = tmp_packet->data;

	tmp_neigh = find_aodv_neigh(tmp_packet->src_ip);
	if (tmp_neigh == NULL) {
#ifdef DEBUG
		printk ("Error: Source %s of an incoming large probe-packet belongs to any neighbour\n", inet_ntoa(tmp_packet->src_ip));
#endif
		return;
	}

	
	//Update neighbor timelife
	delete_timer(tmp_neigh->ip, tmp_neigh->ip, NO_TOS, TASK_NEIGHBOR);
	insert_timer_simple(TASK_NEIGHBOR, HELLO_INTERVAL
			* (1 + ALLOWED_HELLO_LOSS) + 100, tmp_neigh->ip);
	update_timer_queue();
	tmp_neigh->lifetime = HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 20
			+ getcurrtime();

	if (tmp_lprobe->n_probe != tmp_neigh->ett.last_count) //if not equal, the don't belong to the same pair.
	{
#ifdef DEBUG
		printk("Error: Invalid pair of probe-packets from %s\n",inet_ntoa(tmp_packet->src_ip) );
#endif
		return;
	}

	//NEW ETT-PAIR received - Timer is reloaded
	delete_timer(tmp_neigh->ip, tmp_neigh->ip, NO_TOS, TASK_ETT_CLEANUP);
	insert_timer_simple(TASK_ETT_CLEANUP, ETT_INTERVAL * (1 + ALLOWED_ETT_LOSS)
			+ 100, tmp_neigh->ip);
	update_timer_queue();

	sec = tmp_lprobe->sec;
	usec = tmp_lprobe->usec;
	res_sec = sec - tmp_neigh->ett.sec;
	if (res_sec == 0)
		//large packet still received  on same the second-value to the small one, only usec was incremented
		res_usec = usec - tmp_neigh->ett.usec;
	else if (res_sec == 1) //large packet received in a new second-value
		res_usec=1000000 + usec - tmp_neigh->ett.sec;
	else {
#ifdef DEBUG
		printk ("Too high delay between the probe-packets, difference in seconds of %d\n", res_sec);
#endif
		res_usec = DEFAULT_ETT_METRIC;
	}
	tmp_neigh->ett.meas_delay = res_usec;
	tmp_neigh->recv_rate = ntohs(tmp_lprobe->my_rate);

	if (g_fixed_rate == 0) {
		delay_vector_add(tmp_neigh, ntohl(tmp_lprobe->last_meas_delay));
#ifdef DEBUG
		printk("Received Delay to %s: %d Usecs\n",inet_ntoa(tmp_neigh->ip) ,ntohl(tmp_lprobe->last_meas_delay));
#endif
		compute_estimation(tmp_neigh);
	}

	if (tmp_neigh->recv_rate == 0 || tmp_neigh->send_rate == 0) //Estimation is still undone! Fast_interval!
		tmp_neigh->ett.interval = ETT_FAST_INTERVAL;
	else
		tmp_neigh->ett.interval = ETT_INTERVAL;

	return;

}

void recv_ett_info(task * tmp_packet) {

	aodv_neigh *tmp_neigh;
	ett_info *tmp_ett_info;

	tmp_neigh = find_aodv_neigh(tmp_packet->src_ip);
	if (tmp_neigh == NULL) {
#ifdef DEBUG
		printk ("Error: Source %s of an incoming ett_info message belongs to any neighbour\n", inet_ntoa(tmp_packet->src_ip));
#endif
		return;
	}

	//Update neighbor timelife
	delete_timer(tmp_neigh->ip, tmp_neigh->ip, NO_TOS, TASK_NEIGHBOR);
	insert_timer_simple(TASK_NEIGHBOR, HELLO_INTERVAL
			* (1 + ALLOWED_HELLO_LOSS) + 100, tmp_neigh->ip);
	update_timer_queue();
	tmp_neigh->lifetime = HELLO_INTERVAL * (1 + ALLOWED_HELLO_LOSS) + 20
			+ getcurrtime();

	tmp_ett_info =tmp_packet->data;

	tmp_neigh->recv_rate = ntohs(tmp_ett_info->my_rate);
	
	if (tmp_ett_info->fixed_rate==TRUE){
		tmp_neigh->ett.fixed_rate = TRUE;
		delete_timer(tmp_neigh->ip, tmp_neigh->ip, NO_TOS, TASK_ETT_CLEANUP);
					insert_timer_simple(TASK_ETT_CLEANUP, ETT_INTERVAL * (1 + ALLOWED_ETT_LOSS)
							+ 100, tmp_neigh->ip);
					update_timer_queue();
				delay_vector_add(tmp_neigh, ntohl(tmp_ett_info->last_meas_delay));
	

	if (g_fixed_rate == 0) { //this neigh does not send pair-packets - Lets use the estimation in ETT_INFO
		//NEW ETT-PAIR received - Timer is reloaded
			
#ifdef DEBUG
		printk("Received Delay to %s: %d Usecs\n",inet_ntoa(tmp_neigh->ip) ,ntohl(tmp_ett_info->last_meas_delay));
#endif
		compute_estimation(tmp_neigh);
	}
	}
	
	if ((tmp_neigh->recv_rate == 0) || (tmp_neigh->send_rate == 0))
		tmp_neigh->ett.interval = ETT_FAST_INTERVAL;
	else
		tmp_neigh->ett.interval = ETT_INTERVAL;

	return;
}

