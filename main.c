#include <pcap.h>
#include <stdio.h>
#include <ctype.h>

#include "types.h"
#include "dns.h"
#include "helper.h"

int extract_name(u8 *b, u8 *p, u8 *out) {
	int len;
	int i, k=0;

	while(k < 254) {
		len = *b++;

		if (len == 0)
			break;

		if ((len >> 6) == 3) { // compressed name packet?
			len &= ~0xc0; // clear top 2 bits
			len <<= 8; 
			len |= *b++;
		
			if (k != 0)
				*out++ = '.';

			extract_name(p + len, p, out);

			return k+2;
		}


		if (k != 0)
			*out++ = '.';

		k++;

		for(i = 0; i < len; i++) {
			*out++ = *b++;
			k++;
		}
	}

	*out = 0;

	return k+1;
}

void ipv4_to_ascii(u32 ip, u8 *out) {
	sprintf(out, "%d.%d.%d.%d", 
		ip >> 24,
		(ip >> 16)&0xff,
		(ip >>  8)&0xff,
		ip & 0xff
	);
}

void handle_packet(u8 *args, const struct pcap_pkthdr *header, const u8 *packet) {
	u16 id, flags, qcount, ancount, nscount, arcount;
	u8 *p = packet;
	char ip_buf[128];


	// Question fields
	u16 qtype, qclass, qlen;
	u32 qttl;
	
	// Answer fields
	u16 atype,aclass,alen;
	u32 attl;

	// UDP fields
	u32 src_addr, dst_addr;
	u16 udp_len;

	u16 pref;

	u8 *q;
	int i, j;
	char name[255];


	if (strstr(packet+0x36, "audioscrobbler") != NULL)
		return;
	
	if (strstr(packet+0x36, "infostorm") != NULL)
		return;

	p += 0x1a; // what hdr?

	// UDP header starts here
	src_addr = be32(p+0);
	dst_addr = be32(p+4);
	udp_len  = be16(p+10);

	ipv4_to_ascii(src_addr, ip_buf);
	printf("UDP src: %s\n", ip_buf);
	ipv4_to_ascii(dst_addr, ip_buf);
	printf("UDP dst: %s\n", ip_buf);

	p += 0x10;	

	id      = be16(p+0);
	flags   = be16(p+2);
	qcount  = be16(p+4);
	ancount = be16(p+6);
	nscount = be16(p+8);
	arcount = be16(p+10);

	hexdump(packet, header->len);
	printf("++ ID: %04x QR: %d OPCODE: %x QCOUNT: %d ANCOUNT: %d ARCOUNT: %d\n", id, 0, 0, qcount, ancount, arcount);

	q = p + 12;

	for(i = 0; i < qcount; i++) {
		q += extract_name(q, p, name);

		qtype  = be16(q+0);
		qclass = be16(q+2);

		q += 4;

		printf("  `-- Question #%d -- text: [%s] (%d) '%s' qclass:%04x\n", i, dns_record_type_name[qtype], qtype, name, qclass);
	}

	for(i = 0; i < ancount; i++) {
		q += extract_name(q, p, name);

		atype  = be16(q+0);
		aclass = be16(q+2);
		attl   = be32(q+4);
		alen   = be16(q+8);
	
		q += 10;
	
		printf("  `-- Answer #%d for [%s] (%d) '%s' -- ttl:%dsec class:%04x len:%04x -- ", i, dns_record_type_name[atype], atype, name, attl, aclass, alen);

		switch(atype) {
			case DNS_RECORD_TYPE_A:
				printf("IPv4: %d.%d.%d.%d\n",
					q[0], q[1], q[2], q[3]
				);

				q += 4;
			break;

			case DNS_RECORD_TYPE_AAAA:
				printf("IPv6: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
					be16(q+0), be16(q+2) , be16(q+4 ), be16(q+6),	
					be16(q+8), be16(q+10), be16(q+12), be16(q+14)
				);

				q += 16;
			break;

			case DNS_RECORD_TYPE_CNAME:
				q += extract_name(q, p, name);
				printf("CNAME: '%s'\n", name);
			break;

			case DNS_RECORD_TYPE_MX:
				pref = be16(q); q += 2;
				q += extract_name(q, p, name);
				printf("pref:%d name: %s\n", pref, name);
			break;

			case DNS_RECORD_TYPE_NS:
				q += extract_name(q, p, name);
				printf("NS: %s\n", name);
			break;

			default: printf("UNHANDLED RECORD_TYPE: '%02x' (%s)\n", atype, dns_record_type_name[atype]); break;
		}	
	}

	for(i = 0; i < arcount; i++) {

	}
}


int main(int argc, char *argv[]) {
	pcap_t *pcap_handle;
	char   *dev;
	char   errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program fp;
	struct pcap_pkthdr header;

	bpf_u_int32 mask;
	bpf_u_int32 net;
	
	char filter_exp[] = "udp port 53";

	dev = pcap_lookupdev(errbuf);

	if (dev == NULL) {
		fprintf(stderr, "Couldnt find default device: %s\n", errbuf);
		return -1;
	}

	printf("Found default device '%s'\n", dev);

	if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
		fprintf(stderr, "Couldnt get netmask for device %s: %s\n", dev, errbuf);
		net  = 0;
		mask = 0;
	}

	pcap_handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);

	if (pcap_handle == NULL) {
		fprintf(stderr, "Couldnt open device %s: %s\n", dev, errbuf);
		return -1;
	}

	if (pcap_compile(pcap_handle, &fp, filter_exp, 0, net) == -1) {
		fprintf(stderr, "Couldnt parse filter %s: %s\n", filter_exp, pcap_geterr(pcap_handle));
		return -1;
	}

	if (pcap_setfilter(pcap_handle, &fp) == -1) {
		fprintf(stderr, "Couldnt install filter %s: %s\n", filter_exp, pcap_geterr(pcap_handle));
		return -1;
	}

	pcap_loop(pcap_handle, -1, handle_packet, NULL);
	pcap_close(pcap_handle);

	return 0;
}
