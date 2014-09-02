#include "nat64/unit/skb_generator.h"
#include "nat64/unit/types.h"
#include "nat64/comm/str_utils.h"

#include <linux/if_ether.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/icmp.h>


#define IPV4_HDR_LEN sizeof(struct iphdr)
int init_ipv4_hdr(void *l3_hdr, u16 payload_len, u8 nexthdr, void *arg,
		bool df, bool mf, u16 frag_offset)
{
	struct iphdr *hdr = l3_hdr;
	struct ipv4_pair *pair4 = arg;

	hdr->version = 4;
	hdr->ihl = 5;
	hdr->tos = 0;
	hdr->tot_len = cpu_to_be16(sizeof(*hdr) + payload_len);
	hdr->id = cpu_to_be16(1234);
	hdr->frag_off = build_ipv4_frag_off_field(df, mf, frag_offset);
	hdr->ttl = 32;
	hdr->protocol = nexthdr;
	hdr->saddr = pair4->remote.address.s_addr;
	hdr->daddr = pair4->local.address.s_addr;

	hdr->check = 0;
	hdr->check = ip_fast_csum(hdr, hdr->ihl);

	return 0;
}

#define IPV6_HDR_LEN sizeof(struct ipv6hdr)
int init_ipv6_hdr(void *l3_hdr, u16 payload_len, u8 nexthdr, void *arg,
		bool df, bool mf, u16 frag_offset)
{
	struct ipv6hdr *hdr = l3_hdr;
	struct ipv6_pair *pair6 = arg;

	hdr->version = 6;
	hdr->priority = 0;
	hdr->flow_lbl[0] = 0;
	hdr->flow_lbl[1] = 0;
	hdr->flow_lbl[2] = 0;
	hdr->payload_len = cpu_to_be16(payload_len);
	hdr->nexthdr = nexthdr;
	hdr->hop_limit = 32;
	hdr->saddr = pair6->remote.address;
	hdr->daddr = pair6->local.address;

	return 0;
}

#define FRAG_HDR_LEN sizeof(struct frag_hdr)
static int init_ipv6_and_frag_hdr(void *l3_hdr, u16 payload_len, u8 nexthdr, void *arg,
		bool df, bool mf, u16 frag_offset)
{
	struct ipv6hdr *hdr6 = l3_hdr;
	struct frag_hdr *frag_hdr = (struct frag_hdr *) (hdr6 + 1);
	int error;

	error = init_ipv6_hdr(hdr6, FRAG_HDR_LEN + payload_len, NEXTHDR_FRAGMENT, arg,
			df, mf, frag_offset);
	if (error != 0)
		return error;

	frag_hdr->nexthdr = nexthdr;
	frag_hdr->reserved = 0;
	frag_hdr->frag_off = build_ipv6_frag_off_field(frag_offset, mf);
	frag_hdr->identification = cpu_to_be32(4321);

	return 0;
}

#define UDP_HDR_LEN sizeof(struct udphdr)
static int init_udp_hdr(void *l4_hdr, int l3_hdr_type, u16 datagram_len, void *arg)
{
	struct udphdr *hdr = l4_hdr;
	struct ipv6_pair *pair6;
	struct ipv4_pair *pair4;

	switch (l3_hdr_type) {
	case ETH_P_IPV6:
		pair6 = arg;
		hdr->source = cpu_to_be16(pair6->remote.l4_id);
		hdr->dest = cpu_to_be16(pair6->local.l4_id);
		break;
	case ETH_P_IP:
		pair4 = arg;
		hdr->source = cpu_to_be16(pair4->remote.l4_id);
		hdr->dest = cpu_to_be16(pair4->local.l4_id);
		break;
	default:
		log_err("Unsupported network protocol: %d.", l3_hdr_type);
		return -EINVAL;
	}

	hdr->len = cpu_to_be16(datagram_len);
	hdr->check = 0;

	return 0;
}

#define TCP_HDR_LEN sizeof(struct tcphdr)
int init_tcp_hdr(void *l4_hdr, int l3_hdr_type, u16 datagram_len, void *arg)
{
	struct tcphdr *hdr = l4_hdr;
	struct ipv6_pair *pair6;
	struct ipv4_pair *pair4;

	switch (l3_hdr_type) {
	case ETH_P_IPV6:
		pair6 = arg;
		hdr->source = cpu_to_be16(pair6->remote.l4_id);
		hdr->dest = cpu_to_be16(pair6->local.l4_id);
		break;
	case ETH_P_IP:
		pair4 = arg;
		hdr->source = cpu_to_be16(pair4->remote.l4_id);
		hdr->dest = cpu_to_be16(pair4->local.l4_id);
		break;
	default:
		log_err("Unsupported network protocol: %d.", l3_hdr_type);
		return -EINVAL;
	}

	hdr->seq = cpu_to_be32(4669);
	hdr->ack_seq = cpu_to_be32(6576);
	hdr->doff = sizeof(*hdr) / 4;
	hdr->res1 = 0;
	hdr->cwr = 0;
	hdr->ece = 0;
	hdr->urg = 0;
	hdr->ack = 0;
	hdr->psh = 0;
	hdr->rst = 0;
	hdr->syn = 0;
	hdr->fin = 0;
	hdr->window = cpu_to_be16(3233);
	hdr->check = 0;
	hdr->urg_ptr = cpu_to_be16(9865);

	return 0;
}

#define ICMP4_HDR_LEN sizeof(struct icmphdr)
static int init_icmp4_hdr_info(void *l4_hdr, int l3_hdr_type, u16 datagram_len, void *arg)
{
	struct icmphdr *hdr = l4_hdr;
	struct ipv4_pair *pair4 = arg;

	hdr->type = ICMP_ECHO;
	hdr->code = 0;
	hdr->checksum = 0;
	hdr->un.echo.id = cpu_to_be16(pair4->remote.l4_id);
	hdr->un.echo.sequence = cpu_to_be16(2000);

	return 0;
}

static int init_icmp4_hdr_error(void *l4_hdr, int l3_hdr_type, u16 datagram_len, void *arg)
{
	struct icmphdr *hdr = l4_hdr;

	hdr->type = ICMP_DEST_UNREACH;
	hdr->code = ICMP_FRAG_NEEDED;
	hdr->checksum = 0;
	hdr->un.frag.mtu = cpu_to_be16(1300);
	hdr->un.frag.__unused = cpu_to_be16(0);

	return 0;
}

#define ICMP6_HDR_LEN sizeof(struct icmp6hdr)
static int init_icmp6_hdr_info(void *l4_hdr, int l3_hdr_type, u16 datagram_len, void *arg)
{
	struct icmp6hdr *hdr = l4_hdr;
	struct ipv6_pair *pair6 = arg;

	hdr->icmp6_type = ICMPV6_ECHO_REQUEST;
	hdr->icmp6_code = 0;
	hdr->icmp6_cksum = 0;
	hdr->icmp6_dataun.u_echo.identifier = cpu_to_be16(pair6->remote.l4_id);
	hdr->icmp6_dataun.u_echo.sequence = cpu_to_be16(4000);

	return 0;
}

static int init_icmp6_hdr_error(void *l4_hdr, int l3_hdr_type, u16 datagram_len, void *arg)
{
	struct icmp6hdr *hdr = l4_hdr;

	hdr->icmp6_type = ICMPV6_PKT_TOOBIG;
	hdr->icmp6_code = 0;
	hdr->icmp6_cksum = 0;
	hdr->icmp6_mtu = cpu_to_be32(3100);

	return 0;
}

static int init_empty_hdr(void *l4_hdr, int l3_hdr_type, u16 datagram_len, void *arg)
{
	return 0;
}

int init_payload_normal(void *target, u16 payload_len)
{
	unsigned char *payload = target;
	u16 i;

	for (i = 0; i < payload_len; i++)
		payload[i] = i;

	return 0;
}

static int init_payload_inner_ipv6(void *target, u16 payload_len)
{
	struct ipv6hdr *hdr_ipv6;
	struct tcphdr *hdr_tcp;
	struct ipv6hdr tmp_hdr_ipv6;
	struct tcphdr tmp_hdr_tcp;
	unsigned char *inner_payload;
	struct ipv6_pair pair6;
	int error;

	if (payload_len <= 0)
		return 0; /* Nothing to do here. */

	error = init_pair6(&pair6, "2::2", 22, "6::6", 66);
	if (error)
		return error;

	hdr_ipv6 = target;
	hdr_tcp = (struct tcphdr *) (hdr_ipv6 + 1);
	inner_payload = (unsigned char *) (hdr_tcp + 1);

	error = init_ipv6_hdr(&tmp_hdr_ipv6, 1300, NEXTHDR_TCP, &pair6, true, false, 0);
	if (error)
		return error;

	if (payload_len >= IPV6_HDR_LEN) {
		memcpy(hdr_ipv6, &tmp_hdr_ipv6, IPV6_HDR_LEN);
		payload_len -= IPV6_HDR_LEN;
	} else {
		memcpy(hdr_ipv6, &tmp_hdr_ipv6, payload_len);
		goto end;
	}

	error = init_tcp_hdr(&tmp_hdr_tcp, ETH_P_IPV6, 1300, &pair6);
	if (error)
		return error;

	if (payload_len >= TCP_HDR_LEN) {
		memcpy(hdr_tcp, &tmp_hdr_tcp, TCP_HDR_LEN);
		payload_len -= TCP_HDR_LEN;
	} else {
		memcpy(hdr_tcp, &tmp_hdr_tcp, payload_len);
		goto end;
	}

	error = init_payload_normal(inner_payload, payload_len);
	if (error)
		return error;

end:
	return 0;
}

static int init_payload_inner_ipv4(void *target, u16 payload_len)
{
	struct iphdr *hdr_ipv4;
	struct tcphdr *hdr_tcp;
	struct iphdr tmp_hdr_ipv4;
	struct tcphdr tmp_hdr_tcp;
	unsigned char *inner_payload;
	struct ipv4_pair pair4;
	int error;

	if (payload_len <= 0)
		return 0; /* Nothing to do here. */

	error = init_pair4(&pair4, "2.2.2.2", 222, "6.6.6.6", 666);
	if (error)
		return error;

	hdr_ipv4 = target;
	hdr_tcp = (struct tcphdr *) (hdr_ipv4 + 1);
	inner_payload = (unsigned char *) (hdr_tcp + 1);

	error = init_ipv4_hdr(&tmp_hdr_ipv4, 1300, IPPROTO_TCP, &pair4, true, false, 0);
	if (error)
		return error;

	if (payload_len >= IPV4_HDR_LEN) {
		memcpy(hdr_ipv4, &tmp_hdr_ipv4, IPV4_HDR_LEN);
		payload_len -= IPV4_HDR_LEN;
	} else {
		memcpy(hdr_ipv4, &tmp_hdr_ipv4, payload_len);
		goto end;
	}

	error = init_tcp_hdr(&tmp_hdr_tcp, ETH_P_IP, 1300, &pair4);
	if (error)
		return error;

	if (payload_len >= TCP_HDR_LEN) {
		memcpy(hdr_tcp, &tmp_hdr_tcp, TCP_HDR_LEN);
		payload_len -= TCP_HDR_LEN;
	} else {
		memcpy(hdr_tcp, &tmp_hdr_tcp, payload_len);
		goto end;
	}

	error = init_payload_normal(inner_payload, payload_len);
	if (error)
		return error;

end:
	return 0;
}

static int empty_post(void *l4_hdr, u16 datagram_len, void *arg)
{
	return 0;
}

int ipv4_tcp_post(void *l4_hdr, u16 datagram_len, void *arg)
{
	struct tcphdr *hdr = l4_hdr;
	struct ipv4_pair *pair4 = arg;

	hdr->check = csum_tcpudp_magic(pair4->remote.address.s_addr, pair4->local.address.s_addr,
			datagram_len, IPPROTO_TCP, csum_partial(l4_hdr, datagram_len, 0));

	return 0;
}

static int ipv4_udp_post(void *l4_hdr, u16 datagram_len, void *arg)
{
	struct udphdr *hdr = l4_hdr;
	struct ipv4_pair *pair4 = arg;

	hdr->check = csum_tcpudp_magic(pair4->remote.address.s_addr, pair4->local.address.s_addr,
			datagram_len, IPPROTO_UDP, csum_partial(l4_hdr, datagram_len, 0));

	return 0;
}

static int ipv4_icmp_post(void *l4_hdr, u16 datagram_len, void *arg)
{
	struct icmphdr *hdr = l4_hdr;
	hdr->checksum = ip_compute_csum(hdr, datagram_len);
	return 0;
}

int ipv6_tcp_post(void *l4_hdr, u16 datagram_len, void *arg)
{
	struct tcphdr *hdr = l4_hdr;
	struct ipv6_pair *pair6 = arg;

	hdr->check = csum_ipv6_magic(&pair6->remote.address, &pair6->local.address, datagram_len,
			NEXTHDR_TCP, csum_partial(l4_hdr, datagram_len, 0));

	return 0;
}

static int ipv6_udp_post(void *l4_hdr, u16 datagram_len, void *arg)
{
	struct udphdr *hdr = l4_hdr;
	struct ipv6_pair *pair6 = arg;

	hdr->check = csum_ipv6_magic(&pair6->remote.address, &pair6->local.address, datagram_len,
			NEXTHDR_UDP, csum_partial(l4_hdr, datagram_len, 0));

	return 0;
}

static int ipv6_icmp_post(void *l4_hdr, u16 datagram_len, void *arg)
{
	struct icmp6hdr *hdr = l4_hdr;
	struct ipv6_pair *pair6 = arg;

	hdr->icmp6_cksum = csum_ipv6_magic(&pair6->remote.address, &pair6->local.address, datagram_len,
			NEXTHDR_ICMP, csum_partial(l4_hdr, datagram_len, 0));

	return 0;
}

static int create_skb(int (*l3_hdr_fn)(void *, u16, u8, void *, bool, bool, u16),
		int l3_hdr_type, int l3_hdr_len, bool df, bool mf, u16 frag_offset,
		int (*l4_hdr_fn)(void *, int, u16, void *), int l4_hdr_type, int l4_hdr_len, int l4_total_len,
		int (*payload_fn)(void *, u16), u16 payload_len,
		int (*l4_post_fn)(void *, u16, void *),
		struct sk_buff **result, void *arg)
{
	struct sk_buff *skb;
	int datagram_len = l4_hdr_len + payload_len;
	int error;

	skb = alloc_skb(LL_MAX_HEADER + l3_hdr_len + datagram_len, GFP_ATOMIC);
	if (!skb) {
		log_err("New packet allocation failed.");
		return -ENOMEM;
	}
	skb->protocol = htons(l3_hdr_type);

	skb_reserve(skb, LL_MAX_HEADER);
	skb_put(skb, l3_hdr_len + l4_hdr_len + payload_len);

	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb_set_transport_header(skb, l3_hdr_len);

	error = l3_hdr_fn(skb_network_header(skb), datagram_len, l4_hdr_type, arg, df, mf, frag_offset);
	if (error)
		goto failure;
	error = l4_hdr_fn(skb_transport_header(skb), l3_hdr_type, l4_total_len, arg);
	if (error)
		goto failure;

	error = payload_fn(skb_transport_header(skb) + l4_hdr_len, payload_len);
	if (error)
		goto failure;
	error = l4_post_fn(skb_transport_header(skb), datagram_len, arg);
	if (error)
		goto failure;

	switch (l3_hdr_type) {
	case ETH_P_IP:
		error = skb_init_cb_ipv4(skb);
		break;
	case ETH_P_IPV6:
		error = skb_init_cb_ipv6(skb);
		break;
	default:
		error = -EINVAL;
	}
	if (error)
		goto failure;

	*result = skb;

	return 0;

failure:
	kfree_skb(skb);
	return error;
}

int create_skb_ipv6_udp(struct ipv6_pair *pair6, struct sk_buff **result, u16 payload_len)
{
	return create_skb(init_ipv6_hdr, ETH_P_IPV6, IPV6_HDR_LEN, true, false, 0,
			init_udp_hdr, NEXTHDR_UDP, UDP_HDR_LEN, UDP_HDR_LEN + payload_len,
			init_payload_normal, payload_len,
			ipv6_udp_post,
			result, pair6);
}

int create_skb_ipv6_tcp(struct ipv6_pair *pair6, struct sk_buff **result, u16 payload_len)
{
	return create_skb(init_ipv6_hdr, ETH_P_IPV6, IPV6_HDR_LEN, true, false, 0,
			init_tcp_hdr, NEXTHDR_TCP, TCP_HDR_LEN, TCP_HDR_LEN + payload_len,
			init_payload_normal, payload_len,
			ipv6_tcp_post,
			result, pair6);
}

int create_skb_ipv6_icmp_info(struct ipv6_pair *pair6, struct sk_buff **result, u16 payload_len)
{
	return create_skb(init_ipv6_hdr, ETH_P_IPV6, IPV6_HDR_LEN, true, false, 0,
			init_icmp6_hdr_info, NEXTHDR_ICMP, ICMP6_HDR_LEN, ICMP6_HDR_LEN + payload_len,
			init_payload_normal, payload_len,
			ipv6_icmp_post,
			result, pair6);
}

int create_skb_ipv6_icmp_error(struct ipv6_pair *pair6, struct sk_buff **result, u16 payload_len)
{
	return create_skb(init_ipv6_hdr, ETH_P_IPV6, IPV6_HDR_LEN, true, false, 0,
			init_icmp6_hdr_error, NEXTHDR_ICMP, ICMP6_HDR_LEN, ICMP6_HDR_LEN + payload_len,
			init_payload_inner_ipv6, payload_len,
			ipv6_icmp_post,
			result, pair6);
}

int create_skb_ipv4_udp(struct ipv4_pair *pair4, struct sk_buff **result, u16 payload_len)
{
	return create_skb_ipv4_udp_frag(pair4, result, payload_len,
			UDP_HDR_LEN + payload_len, true, false, 0);
}

int create_skb_ipv4_tcp(struct ipv4_pair *pair4, struct sk_buff **result, u16 payload_len)
{
	return create_skb_ipv4_tcp_frag(pair4, result, payload_len,
			TCP_HDR_LEN + payload_len, true, false, 0);
}

int create_skb_ipv4_icmp_info(struct ipv4_pair *pair4, struct sk_buff **result, u16 payload_len)
{
	return create_skb_ipv4_icmp_info_frag(pair4, result, payload_len,
			ICMP4_HDR_LEN + payload_len, true, false, 0);
}

int create_skb_ipv4_icmp_error(struct ipv4_pair *pair4, struct sk_buff **result, u16 payload_len)
{
	return create_skb(init_ipv4_hdr, ETH_P_IP, IPV4_HDR_LEN, true, false, 0,
			init_icmp4_hdr_error, IPPROTO_ICMP, ICMP4_HDR_LEN, ICMP4_HDR_LEN + payload_len,
			init_payload_inner_ipv4, payload_len,
			ipv4_icmp_post,
			result, pair4);
}

int create_skb_ipv4_udp_frag(struct ipv4_pair *pair4, struct sk_buff **result, u16 payload_len,
		u16 total_l4_len, bool df, bool mf, u16 frag_offset)
{
	if (frag_offset == 0)
		return create_skb(init_ipv4_hdr, ETH_P_IP, IPV4_HDR_LEN, df, mf, frag_offset,
				init_udp_hdr, IPPROTO_UDP, UDP_HDR_LEN, total_l4_len,
				init_payload_normal, payload_len,
				ipv4_udp_post,
				result, pair4);
	else
		return create_skb(init_ipv4_hdr, ETH_P_IP, IPV4_HDR_LEN, df, mf, frag_offset,
				init_empty_hdr, IPPROTO_UDP, 0, total_l4_len,
				init_payload_normal, payload_len,
				empty_post,
				result, pair4);
}

int create_skb_ipv4_tcp_frag(struct ipv4_pair *pair4, struct sk_buff **result,
		u16 payload_len, u16 total_l4_len,bool df, bool mf, u16 frag_offset)
{
	if (frag_offset == 0)
		return create_skb(init_ipv4_hdr, ETH_P_IP, IPV4_HDR_LEN, df, mf, frag_offset,
				init_tcp_hdr, IPPROTO_TCP, TCP_HDR_LEN, total_l4_len,
				init_payload_normal, payload_len,
				ipv4_tcp_post,
				result, pair4);
	else
		return create_skb(init_ipv4_hdr, ETH_P_IP, IPV4_HDR_LEN, df, mf, frag_offset,
				init_empty_hdr, IPPROTO_TCP, 0, total_l4_len,
				init_payload_normal, payload_len,
				empty_post,
				result, pair4);
}

int create_skb_ipv4_icmp_info_frag(struct ipv4_pair *pair4, struct sk_buff **result,
		u16 payload_len, u16 total_l4_len, bool df, bool mf, u16 frag_offset)
{
	if (frag_offset == 0)
		return create_skb(init_ipv4_hdr, ETH_P_IP, IPV4_HDR_LEN, df, mf, frag_offset,
				init_icmp4_hdr_info, IPPROTO_ICMP, ICMP4_HDR_LEN, total_l4_len,
				init_payload_normal, payload_len,
				ipv4_icmp_post,
				result, pair4);
	else
		return create_skb(init_ipv4_hdr, ETH_P_IP, IPV4_HDR_LEN, df, mf, frag_offset,
				init_empty_hdr, IPPROTO_ICMP, 0, total_l4_len,
				init_payload_normal, payload_len,
				empty_post,
				result, pair4);
}

int create_skb_ipv6_udp_frag(struct ipv6_pair *pair6, struct sk_buff **result,
		u16 payload_len, u16 total_l4_len, bool mf, u16 frag_offset)
{
	if (frag_offset == 0)
		return create_skb(init_ipv6_and_frag_hdr, ETH_P_IPV6, IPV6_HDR_LEN + FRAG_HDR_LEN,
				true, mf, frag_offset,
				init_udp_hdr, NEXTHDR_UDP, UDP_HDR_LEN, total_l4_len,
				init_payload_normal, payload_len,
				ipv6_udp_post,
				result, pair6);
	else
		return create_skb(init_ipv6_and_frag_hdr, ETH_P_IPV6, IPV6_HDR_LEN + FRAG_HDR_LEN,
				true, mf, frag_offset,
				init_empty_hdr, NEXTHDR_UDP, 0, total_l4_len,
				init_payload_normal, payload_len,
				empty_post,
				result, pair6);
}

int create_skb_ipv6_tcp_frag(struct ipv6_pair *pair6, struct sk_buff **result,
		u16 payload_len, u16 total_l4_len, bool mf, u16 frag_offset)
{
	if (frag_offset == 0)
		return create_skb(init_ipv6_and_frag_hdr, ETH_P_IPV6, IPV6_HDR_LEN + FRAG_HDR_LEN,
				true, mf, frag_offset,
				init_tcp_hdr, NEXTHDR_TCP, TCP_HDR_LEN, total_l4_len,
				init_payload_normal, payload_len,
				ipv6_tcp_post,
				result, pair6);
	else
		return create_skb(init_ipv6_and_frag_hdr, ETH_P_IPV6, IPV6_HDR_LEN + FRAG_HDR_LEN,
				true, mf, frag_offset,
				init_empty_hdr, NEXTHDR_TCP, 0, total_l4_len,
				init_payload_normal, payload_len,
				empty_post,
				result, pair6);
}

int create_skb_ipv6_icmp_info_frag(struct ipv6_pair *pair6, struct sk_buff **result,
		u16 payload_len, u16 total_l4_len, bool mf, u16 frag_offset)
{
	if (frag_offset == 0)
		return create_skb(init_ipv6_and_frag_hdr, ETH_P_IPV6, IPV6_HDR_LEN + FRAG_HDR_LEN,
				true, mf, frag_offset,
				init_icmp6_hdr_info, NEXTHDR_ICMP, ICMP6_HDR_LEN, total_l4_len,
				init_payload_normal, payload_len,
				ipv6_icmp_post,
				result, pair6);
	else
		return create_skb(init_ipv6_and_frag_hdr, ETH_P_IPV6, IPV6_HDR_LEN + FRAG_HDR_LEN,
				true, mf, frag_offset,
				init_empty_hdr, NEXTHDR_ICMP, 0, total_l4_len,
				init_payload_normal, payload_len,
				empty_post,
				result, pair6);
}

int create_packet_ipv4_udp_fragmented_disordered(struct ipv4_pair *pair4, struct sk_buff **skb_out)
{
	struct sk_buff *skb1, *skb2, *skb3;
	int error;

	error = create_skb_ipv4_udp_frag(pair4, &skb1, 8, 32, true, true, 0);
	if (error)
		return error;

	error = create_skb_ipv4_udp_frag(pair4, &skb2, 8, 32, true, true, 16);
	if (error)
		return error;

	error = create_skb_ipv4_udp_frag(pair4, &skb3, 8, 32, true, false, 24);
	if (error)
		return error;

	skb1->next = skb3;
	skb3->next = skb2;
	skb3->prev = skb1;
	skb2->prev = skb3;
	*skb_out = skb1;

	return 0;
}

int create_packet_ipv6_tcp_fragmented_disordered(struct ipv6_pair *pair6, struct sk_buff **skb_out)
{
	struct sk_buff *skb1, *skb2, *skb3;
	int error;

	error = create_skb_ipv6_tcp_frag(pair6, &skb1, 8, 32, true, 0);
	if (error)
		return false;

	error = create_skb_ipv6_tcp_frag(pair6, &skb2, 8, 32, true, 16);
	if (error)
		return false;

	error = create_skb_ipv6_tcp_frag(pair6, &skb3, 8, 32, false, 24);
	if (error)
		return false;

	skb1->next = skb2;
	skb2->next = skb3;
	skb2->prev = skb1;
	skb3->prev = skb2;
	*skb_out = skb1;

	return 0;
}

int create_tcp_packet(struct sk_buff **skb, l3_protocol l3_proto, bool syn, bool rst, bool fin)
{
	struct tcphdr *hdr_tcp;
	struct ipv6_pair pair6;
	struct ipv4_pair pair4;
	int error;

	switch (l3_proto) {
	case L3PROTO_IPV4:
		error = init_pair4(&pair4, "8.7.6.5", 8765, "5.6.7.8", 5678);
		if (error)
			return error;
		error = create_skb_ipv4_tcp(&pair4, skb, 100);
		if (error)
			return error;
		break;
	case L3PROTO_IPV6:
		error = init_pair6(&pair6, "1::2", 1212, "3::4", 3434);
		if (error)
			return error;
		error = create_skb_ipv6_tcp(&pair6, skb, 100);
		if (error)
			return error;
		break;
	}

	hdr_tcp = tcp_hdr(*skb);
	hdr_tcp->syn = syn;
	hdr_tcp->rst = rst;
	hdr_tcp->fin = fin;

	return 0;
}
