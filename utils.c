/***************************************************************************
 utils.c  -  description
 -------------------
 begin                : Wed Jul 30 2003
 copyright            : (C) 2003 by Luke Klein-Berndt
 email                : kleinb@nist.gov
 ***************************************************************************/
/***************************************************************************
 Modified by Miguel Catalan Cid - miguel.catcid@gmail.com - Version: Mon Jan 1 2010
 ***************************************************************************/
//MCC - New utils: internet addresses, client access adresses...

#include "utils.h"

extern aodv_dev *g_mesh_dev;
extern u_int32_t g_mesh_ip;
extern u_int32_t g_mesh_netmask;

// KR: 08/23/05
int is_internal(u_int32_t dest_ip) {

	if ( (g_mesh_ip & g_mesh_netmask) == (dest_ip & g_mesh_netmask))
		return 1;

	return 0;
}


u_int64_t getcurrtime() {
	struct timeval tv;
	u_int64_t result;

	do_gettimeofday(&tv);

	//This is a fix for an error that occurs on ARM Linux Kernels because they do 64bits differently
	//Thanks to S. Peter Li for coming up with this fix!

	result = (u_int64_t) tv.tv_usec;
	do_div(result, 1000);
	return ((u_int64_t) tv.tv_sec) * 1000 + result;
}

char *inet_ntoa(u_int32_t ina) {
	static char buf[4 * sizeof "123"];
	unsigned char *ucp = (unsigned char *) &ina;
	sprintf(buf, "%d.%d.%d.%d", ucp[0] & 0xff, ucp[1] & 0xff, ucp[2] & 0xff,
			ucp[3] & 0xff);
	return buf;
}

int inet_aton(const char *cp, u_int32_t * addr) {
	unsigned int val;
	int base, n;
	char c;
	u_int parts[4];
	u_int *pp = parts;

	for (;;) {

		//Collect number up to ``.''. Values are specified as for C:
		// 0x=hex, 0=octal, other=decimal.

		val = 0;
		base = 10;
		if (*cp == '0') {
			if (*++cp == 'x' || *cp == 'X')
				base = 16, cp++;
			else
				base = 8;
		}
		while ((c = *cp) != '\0') {
			if (isascii(c) && isdigit(c)) {
				val = (val * base) + (c - '0');
				cp++;
				continue;

			}
			if (base == 16 && isascii(c) && isxdigit(c)) {
				val = (val << 4) + (c + 10 - (islower(c) ? 'a' : 'A'));
				cp++;
				continue;
			}
			break;
		}
		if (*cp == '.') {
			// Internet format: a.b.c.d a.b.c       (with c treated as
			// 16-bits) a.b         (with b treated as 24 bits)

			if (pp >= parts + 3 || val > 0xff)
				return (0);
			*pp++ = val, cp++;
		} else
			break;
	}

	// Check for trailing characters.

	if (*cp && (!isascii(*cp) || !isspace(*cp)))
		return (0);

	// Concoct the address according to the number of parts specified.

	n = pp - parts + 1;
	switch (n) {

	case 1: // a -- 32 bits
		break;

	case 2: //a.b -- 8.24 bits
		if (val > 0xffffff)
			return (0);
		val |= parts[0] << 24;
		break;

	case 3: //a.b.c -- 8.8.16 bits
		if (val > 0xffff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 4: // a.b.c.d -- 8.8.8.8 bits
		if (val > 0xff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}
	if (addr)
		*addr = htonl(val);
	return (1);
}


int aodv_subnet_test(u_int32_t tmp_ip) {

	if ((g_mesh_dev->ip & g_mesh_dev->netmask)
			== (g_mesh_dev->netmask & tmp_ip ))
		return 1;

	return 0;
}

u_int32_t calculate_netmask(int t) {
	int i;
	uint32_t final = 0;

	for (i = 0; i < 32 - t; i++) {
		final = final + 1;
		final = final << 1;
	}

	final = final + 1;
	final = ~final;
	return final;

}

int calculate_prefix(u_int32_t t) {
	int i = 1;

	while (t != 0) {
		t = t << 1;
		i++;
	}
	return i;
}

int seq_less_or_equal(u_int32_t seq_one, u_int32_t seq_two) {
	int *comp_seq_one = &seq_one;
	int *comp_seq_two = &seq_two;

	if ( ( *comp_seq_one - *comp_seq_two ) > 0) {
		return 0;
	} else
		return 1;
}

int seq_greater(u_int32_t seq_one, u_int32_t seq_two) {
	int *comp_seq_one = &seq_one;
	int *comp_seq_two = &seq_two;

	if ( ( *comp_seq_one - *comp_seq_two ) < 0)
		return 0;
	else
		return 1;
}

int char_compar(unsigned char tos1, unsigned char tos2) {

	int int_tos1 = (int)tos1;
	int int_tos2 = (int)tos2;

	if (int_tos1 > int_tos2)
		return 1;
	else if (int_tos1 == int_tos2)
		return 0;
	else
		return -1;
}

int get_nominalrate(int rate) {

	switch (rate) {
	case RATE_1:
		return RATE_1;
	case RATE_2:
		return RATE_2;
	case RATE_5:
		return RATE_5;
	case RATE_11:
		return RATE_11;
	case RATE_6:
		return RATE_6;
	case RATE_9:
		return RATE_9;
	case RATE_12:
		return RATE_12;
	case RATE_18:
		return RATE_18;
	case RATE_24:
		return RATE_24;
	case RATE_36:
		return RATE_36;
	case RATE_48:
		return RATE_48;
	case RATE_54:
		return RATE_54;
	default:
		return 0;
	}

}
