/*
 * DNS Name Lookup
 *
 * Entirely rewritten by Greg Haerr Feb 2022
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEBUG		0		/* =1 to display debug messages */

#define DEFAULT_DNS	"208.67.222.222"	/* DNS server IP */

/* flag codes */
#define QUERY		0x0000	/* DNS query (opcode 0) */
#define RESPONSE	0x8000	/* DNS response */
#define RD			0x0100	/* recursion desired */
#define RC			0x000F	/* response code */
#define NO_ERROR		0
#define FORMAT_ERROR	1	/* bad query */
#define SERVER_FAIL		2	/* server failed */
#define NAME_ERROR		3	/* name doesn't exist */
#define NOT_IMPL		4	/* not implemented */
#define REFUSED			5	/* query refused */

struct DNS_HEADER {
	__u16	len;		/* length for TCP only */
	__u16	id;
	__u16	flags;
	__u16	qdcount;	/* question count */
	__u16	ancount;	/* answer count */
	__u16	nscount;	/* name server count in auth section */
	__u16	arcount;	/* RR count in addditional section */
};

#define TYPE_A		1	/* IPv4 host address */
#define CLASS_IN	1	/* The ARPA Internet */

struct QUESTION {
	//char	name[];		/* DNS-encoded name */
	__u16	qtype;		/* always TYPE_A */
	__u16	qclass;		/* always CLASS_IN */
};

struct RR {				/* resource record */
	//char	name[];		/* DNS-encoded name */
	__u16	type;		/* always TYPE_A */
	__u16	class;		/* always CLASS_IN */
	__u32	ttl;		/* TTL in seconds to discard */
	__u16	rdlength;	/* size of rdata (always 4) */
	__u32	rdata;		/* IP address for TYPE_A */
};

/* convert e.g. www.google.com (host) to 3www6google3com (dns) */
static void format_dns(char *dns, char *host)
{
	int i, next = 0;

	for(i = 0;; i++) {
		if (host[i] == '.' || !host[i]) {
			*dns++ = i-next;
			while (next < i)
				*dns++ = host[next++];
			if (!host[next])
				break;
			next++;
		}
	}
	*dns++ = '\0';
}

/* resolve a name to an IP address, optionally use DNS 'server' */
ipaddr_t in_resolve(char *hostname, char *server)
{
	int fd, rc, len;
	struct DNS_HEADER *dns;
	struct QUESTION *qd;
	struct RR *rr;
	char *dnsname;
	struct sockaddr_in addr;
	unsigned short flags;
	char buf[256];

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	addr.sin_family = AF_INET;
	addr.sin_port = PORT_ANY;
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
		close(fd);
		return 0;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = in_aton(server? server: DEFAULT_DNS);
	addr.sin_port = htons(53);
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Can't connect to %s\n", in_ntoa(addr.sin_addr.s_addr));
		close(fd);
		return 0;
	}

	dns = (struct DNS_HEADER *)buf;
	dns->id = htons(0xABCD);
	dns->flags = htons(QUERY | RD);
	dns->qdcount = htons(1);
	dns->ancount = 0;
	dns->nscount = 0;
	dns->arcount = 0;

	dnsname = &buf[sizeof(struct DNS_HEADER)];
	format_dns(dnsname, hostname);

	len = strlen(dnsname) + 1;
	qd = (struct QUESTION*)&buf[sizeof(struct DNS_HEADER) + len];
	qd->qtype = htons(TYPE_A);
	qd->qclass = htons(CLASS_IN);

	len += sizeof(struct DNS_HEADER) + sizeof(struct QUESTION) - 2;
	dns->len = htons(len);

	write(fd, buf, len + 2);
	rc = read(fd,buf,200);
	close(fd);

#if DEBUG
	printf("DNS: %d message bytes\n", rc);
	for (int i=0;i<rc;i++) printf("%2x,",buf[i] & 0xff);
	printf("\n");
#endif
	if (rc < sizeof(struct DNS_HEADER) + sizeof(struct RR))
		return 0;

	dns = (struct DNS_HEADER *)buf;
	flags = htons(dns->flags);
#if DEBUG
	printf("response id %04x\n", htons(dns->id));
	printf("response code %04x\n", flags);
	printf("response qd count %04x\n", htons(dns->qdcount));
	printf("response an count %04x\n", htons(dns->ancount));
	printf("response ns count %04x\n", htons(dns->nscount));
	printf("response ar count %04x\n", htons(dns->arcount));
#endif
	if ((flags & RC) != NO_ERROR) {
		char *str;

		switch (flags & RC) {
		case FORMAT_ERROR:	str = "Bad format"; break;
		case NAME_ERROR:	str = "Name not found"; break;
		case REFUSED:		str = "Query refused"; break;
		default:			str = "Server error"; break;
		}
		printf("DNS: %s: %s\n", hostname, str);
		return 0;
	}

	rr = (struct RR *)&buf[rc - sizeof(struct RR)];
#if DEBUG
	printf("response type %04x\n", htons(rr->type));
	printf("response class %04x\n", htons(rr->class));
	printf("response ttl %ld\n", htonl(rr->ttl));
	printf("response rdlength %04x\n", htons(rr->rdlength));
	printf("response rdata %08lx\n", htonl(rr->rdata));
	printf("response IP %s\n", in_ntoa(rr->rdata));
#endif
	/* dns may return auth data but not answer */
	if (htons(dns->ancount) == 0)
		return 0;

	return rr->rdata;
}

int main(int ac, char **av)
{
	ipaddr_t result;
	char *server;

	if (ac < 2) {
		printf("Usage: nslookup <domain> [nameserver]\n");
		return 1;
	}
	if (ac > 2)
		server = av[2];
	else
		server = getenv("DNS");

	result = in_resolve(av[1], server);
	if (result)
		printf("%s is %s\n", av[1], in_ntoa(result));
	else printf("%s: Name not found\n", av[1]);
	return (result == 0);
}
