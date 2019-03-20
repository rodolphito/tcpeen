#include <stdlib.h>

#include "pcap.h"

#include "tn/error.h"
#include "tn/log.h"

static int ifprint(pcap_if_t *d)
{
	pcap_addr_t *a;
	char ipv4_buf[INET_ADDRSTRLEN];
	char ipv6_buf[INET6_ADDRSTRLEN];
	const char *sep;
	int status = 1; /* success */

	printf("%s\n", d->name);
	if (d->description)
		printf("\tDescription: %s\n", d->description);
	printf("\tFlags: ");
	sep = "";
 	if (d->flags & PCAP_IF_UP) {
		printf("%sUP", sep);
		sep = ", ";
	}
	if (d->flags & PCAP_IF_RUNNING) {
		printf("%sRUNNING", sep);
		sep = ", ";
	}
	if (d->flags & PCAP_IF_LOOPBACK) {
		printf("%sLOOPBACK", sep);
		sep = ", ";
	}
	if (d->flags & PCAP_IF_WIRELESS) {
		printf("%sWIRELESS", sep);
		switch (d->flags & PCAP_IF_CONNECTION_STATUS) {

		case PCAP_IF_CONNECTION_STATUS_UNKNOWN:
			printf(" (association status unknown)");
			break;

		case PCAP_IF_CONNECTION_STATUS_CONNECTED:
			printf(" (associated)");
			break;

		case PCAP_IF_CONNECTION_STATUS_DISCONNECTED:
			printf(" (not associated)");
			break;

		case PCAP_IF_CONNECTION_STATUS_NOT_APPLICABLE:
			break;
		}
	} else {
		switch (d->flags & PCAP_IF_CONNECTION_STATUS) {

		case PCAP_IF_CONNECTION_STATUS_UNKNOWN:
			printf(" (connection status unknown)");
			break;

		case PCAP_IF_CONNECTION_STATUS_CONNECTED:
			printf(" (connected)");
			break;

		case PCAP_IF_CONNECTION_STATUS_DISCONNECTED:
			printf(" (disconnected)");
			break;

		case PCAP_IF_CONNECTION_STATUS_NOT_APPLICABLE:
			break;
		}
	}
	sep = ", ";
	printf("\n");

	for (a = d->addresses; a; a = a->next) {
		if (a->addr != NULL)
			switch (a->addr->sa_family) {
			case AF_INET:
				printf("\tAddress Family: AF_INET\n");
				if (a->addr)
					printf("\t\tAddress: %s\n",
						inet_ntop(AF_INET,
							&((struct sockaddr_in *)(a->addr))->sin_addr,
							ipv4_buf, sizeof ipv4_buf));
				if (a->netmask)
					printf("\t\tNetmask: %s\n",
						inet_ntop(AF_INET,
							&((struct sockaddr_in *)(a->netmask))->sin_addr,
							ipv4_buf, sizeof ipv4_buf));
				if (a->broadaddr)
					printf("\t\tBroadcast Address: %s\n",
						inet_ntop(AF_INET,
							&((struct sockaddr_in *)(a->broadaddr))->sin_addr,
							ipv4_buf, sizeof ipv4_buf));
				if (a->dstaddr)
					printf("\t\tDestination Address: %s\n",
						inet_ntop(AF_INET,
							&((struct sockaddr_in *)(a->dstaddr))->sin_addr,
							ipv4_buf, sizeof ipv4_buf));
				break;
			case AF_INET6:
				printf("\tAddress Family: AF_INET6\n");
				if (a->addr)
					printf("\t\tAddress: %s\n",
						inet_ntop(AF_INET6,
						((struct sockaddr_in6 *)(a->addr))->sin6_addr.s6_addr,
							ipv6_buf, sizeof ipv6_buf));
				if (a->netmask)
					printf("\t\tNetmask: %s\n",
						inet_ntop(AF_INET6,
						((struct sockaddr_in6 *)(a->netmask))->sin6_addr.s6_addr,
							ipv6_buf, sizeof ipv6_buf));
				if (a->broadaddr)
					printf("\t\tBroadcast Address: %s\n",
						inet_ntop(AF_INET6,
						((struct sockaddr_in6 *)(a->broadaddr))->sin6_addr.s6_addr,
							ipv6_buf, sizeof ipv6_buf));
				if (a->dstaddr)
					printf("\t\tDestination Address: %s\n",
						inet_ntop(AF_INET6,
						((struct sockaddr_in6 *)(a->dstaddr))->sin6_addr.s6_addr,
							ipv6_buf, sizeof ipv6_buf));
				break;
			default:
				printf("\tAddress Family: Unknown (%d)\n", a->addr->sa_family);
				break;
			} else {
				fprintf(stderr, "\tWarning: a->addr is NULL, skipping this address.\n");
				status = 0;
			}
	}
	printf("\n");
	return status;
}


/* From tcptraceroute */
#define IPTOSBUFFERS	12
static char *iptos(bpf_u_int32 in)
{
	static char output[IPTOSBUFFERS][3 * 4 + 3 + 1];
	static short which;
	u_char *p;

	p = (u_char *)&in;
	which = (which + 1 == IPTOSBUFFERS ? 0 : which + 1);
	sprintf(output[which], "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	return output[which];
}

static char *program_name;

/* Forwards */
static void countme(u_char *, const struct pcap_pkthdr *, const u_char *);

static pcap_t *pd;


int main(void)
{
	char *cmdbuf = "";
	char *device = NULL;
	int timeout = 1000;
	int immediate = 0;
	int nonblock = 0;
	pcap_if_t *devlist;
	bpf_u_int32 localnet, netmask;
	struct bpf_program fcode;
	char ebuf[PCAP_ERRBUF_SIZE];
	int status;
	int packet_count;

	//immediate = 1;
	//nonblock = 1;

	if (device == NULL) {
		if (pcap_findalldevs(&devlist, ebuf) == -1)
			tn_log_error("%s", ebuf);
		if (devlist == NULL)
			tn_log_error("no interfaces available for capture");
		device = strdup(devlist->name);
		pcap_freealldevs(devlist);
	}
	*ebuf = '\0';
	pd = pcap_create(device, ebuf);
	if (pd == NULL)
		tn_log_error("%s", ebuf);
	status = pcap_set_snaplen(pd, 65535);
	if (status != 0)
		tn_log_error("%s: pcap_set_snaplen failed: %s",
			device, pcap_statustostr(status));
	//if (immediate) {
	//	status = pcap_set_immediate_mode(pd, 1);
	//	if (status != 0)
	//		tn_log_error("%s: pcap_set_immediate_mode failed: %s",
	//			device, pcap_statustostr(status));
	//}
	status = pcap_set_timeout(pd, timeout);
	if (status != 0)
		tn_log_error("%s: pcap_set_timeout failed: %s",
			device, pcap_statustostr(status));
	status = pcap_activate(pd);
	if (status < 0) {
		/*
		 * pcap_activate() failed.
		 */
		tn_log_error("%s: %s\n(%s)", device,
			pcap_statustostr(status), pcap_geterr(pd));
	} else if (status > 0) {
		/*
		 * pcap_activate() succeeded, but it's warning us
		 * of a problem it had.
		 */
		tn_log_warning("%s: %s\n(%s)", device,
			pcap_statustostr(status), pcap_geterr(pd));
	}
	if (pcap_lookupnet(device, &localnet, &netmask, ebuf) < 0) {
		localnet = 0;
		netmask = 0;
		tn_log_warning("%s", ebuf);
	}
	//cmdbuf = copy_argv(&argv[optind]);

	if (pcap_compile(pd, &fcode, cmdbuf, 1, netmask) < 0) {
		tn_log_error("%s", pcap_geterr(pd));
	}

	if (pcap_setfilter(pd, &fcode) < 0)
		tn_log_error("%s", pcap_geterr(pd));
	if (pcap_setnonblock(pd, nonblock, ebuf) == -1)
		tn_log_error("pcap_setnonblock failed: %s", ebuf);
	printf("Listening on %s\n", device);
	for (;;) {
		packet_count = 0;
		status = pcap_dispatch(pd, -1, countme,
			(u_char *)&packet_count);
		if (status < 0)
			break;
		if (status != 0) {
			printf("%d packets seen, %d packets counted after pcap_dispatch returns\n",
				status, packet_count);
		}
	}
	if (status == -2) {
		/*
		 * We got interrupted, so perhaps we didn't
		 * manage to finish a line we were printing.
		 * Print an extra newline, just in case.
		 */
		putchar('\n');
	}
	(void)fflush(stdout);
	if (status == -1) {
		/*
		 * Error.  Report it.
		 */
		(void)fprintf(stderr, "%s: pcap_loop: %s\n",
			program_name, pcap_geterr(pd));
	}
	pcap_close(pd);
	pcap_freecode(&fcode);
	free(cmdbuf);
	exit(status == -1 ? 1 : 0);
}

static void countme(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	int *counterp = (int *)user;

	(*counterp)++;
}

const char *pcap_statustostr(int errnum)
{
	static char ebuf[15 + 10 + 1];

	switch (errnum) {

	case PCAP_WARNING:
		return("Generic warning");

	case PCAP_WARNING_TSTAMP_TYPE_NOTSUP:
		return ("That type of time stamp is not supported by that device");

	case PCAP_WARNING_PROMISC_NOTSUP:
		return ("That device doesn't support promiscuous mode");

	case PCAP_ERROR:
		return("Generic error");

	case PCAP_ERROR_BREAK:
		return("Loop terminated by pcap_breakloop");

	case PCAP_ERROR_NOT_ACTIVATED:
		return("The pcap_t has not been activated");

	case PCAP_ERROR_ACTIVATED:
		return ("The setting can't be changed after the pcap_t is activated");

	case PCAP_ERROR_NO_SUCH_DEVICE:
		return ("No such device exists");

	case PCAP_ERROR_RFMON_NOTSUP:
		return ("That device doesn't support monitor mode");

	case PCAP_ERROR_NOT_RFMON:
		return ("That operation is supported only in monitor mode");

	case PCAP_ERROR_PERM_DENIED:
		return ("You don't have permission to capture on that device");

	case PCAP_ERROR_IFACE_NOT_UP:
		return ("That device is not up");

	case PCAP_ERROR_CANTSET_TSTAMP_TYPE:
		return ("That device doesn't support setting the time stamp type");

	case PCAP_ERROR_PROMISC_PERM_DENIED:
		return ("You don't have permission to capture in promiscuous mode on that device");

	case PCAP_ERROR_TSTAMP_PRECISION_NOTSUP:
		return ("That device doesn't support that time stamp precision");
	}
	snprintf(ebuf, sizeof ebuf, "Unknown error: %d", errnum);
	return(ebuf);
}