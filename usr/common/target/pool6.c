#include "nat64/usr/pool6.h"
#include "nat64/common/config.h"
#include "nat64/common/str_utils.h"
#include "nat64/usr/types.h"
#include "nat64/usr/netlink.h"
#include <errno.h>


#define HDR_LEN sizeof(struct request_hdr)
#define PAYLOAD_LEN sizeof(union request_pool6)


struct display_args {
	unsigned int row_count;
	union request_pool6 *request;
	bool csv;
};

static int pool6_display_response(struct nl_msg *response, void *arg)
{
	struct nlmsghdr *hdr;
	struct ipv6_prefix *prefixes;
	unsigned int prefix_count, i;
	char prefix_str[INET6_ADDRSTRLEN];
	struct display_args *args = arg;

	hdr = nlmsg_hdr(response);
	prefixes = nlmsg_data(hdr);
	prefix_count = nlmsg_datalen(hdr) / sizeof(*prefixes);

	if (args->row_count == 0 && args->csv)
		printf("Prefix\n");

	for (i = 0; i < prefix_count; i++) {
		inet_ntop(AF_INET6, &prefixes[i].address, prefix_str, INET6_ADDRSTRLEN);
		printf("%s/%u\n", prefix_str, prefixes[i].len);
	}

	args->row_count += prefix_count;
	args->request->display.prefix_set = hdr->nlmsg_flags & NLM_F_MULTI;
	if (prefix_count > 0)
		args->request->display.prefix = prefixes[prefix_count - 1];
	return 0;
}

int pool6_display(bool csv)
{
	unsigned char request[HDR_LEN + PAYLOAD_LEN];
	struct request_hdr *hdr = (struct request_hdr *) request;
	union request_pool6 *payload = (union request_pool6 *) (request + HDR_LEN);
	struct display_args args;
	int error;

	init_request_hdr(hdr, sizeof(request), MODE_POOL6, OP_DISPLAY);
	payload->display.prefix_set = false;
	memset(&payload->display.prefix, 0, sizeof(payload->display.prefix));
	args.row_count = 0;
	args.request = payload;
	args.csv = csv;

	do {
		error = netlink_request(&request, hdr->length, pool6_display_response, &args);
		if (error)
			return error;
	} while (args.request->display.prefix_set);

	if (!csv) {
		if (args.row_count > 0)
			log_info("  (Fetched %u entries.)", args.row_count);
		else
			log_info("  (empty)");
	}

	return 0;
}

static int pool6_count_response(struct nl_msg *msg, void *arg)
{
	__u64 *conf = nlmsg_data(nlmsg_hdr(msg));
	printf("%llu\n", *conf);
	return 0;
}

int pool6_count(void)
{
	struct request_hdr request;
	init_request_hdr(&request, sizeof(request), MODE_POOL6, OP_COUNT);
	return netlink_request(&request, request.length, pool6_count_response, NULL);
}

static bool get_ubit(struct ipv6_prefix *prefix)
{
	return prefix->address.s6_addr[8];
}

int pool6_add(struct ipv6_prefix *prefix, bool force)
{
	unsigned char request[HDR_LEN + PAYLOAD_LEN];
	struct request_hdr *hdr = (struct request_hdr *) request;
	union request_pool6 *payload = (union request_pool6 *) (request + HDR_LEN);

	if (!force && get_ubit(prefix)) {
		log_err("Warning: The u-bit is nonzero; see https://github.com/NICMx/NAT64/issues/174.");
		log_err("Will cancel the operation. Use --force to override this.");
		return -EINVAL;
	}

	init_request_hdr(hdr, sizeof(request), MODE_POOL6, OP_ADD);
	payload->add.prefix = *prefix;

	return netlink_request(request, hdr->length, NULL, NULL);
}

int pool6_remove(struct ipv6_prefix *prefix, bool quick)
{
	unsigned char request[HDR_LEN + PAYLOAD_LEN];
	struct request_hdr *hdr = (struct request_hdr *) request;
	union request_pool6 *payload = (union request_pool6 *) (request + HDR_LEN);

	init_request_hdr(hdr, sizeof(request), MODE_POOL6, OP_REMOVE);
	payload->rm.prefix = *prefix;
	payload->rm.quick = quick;

	return netlink_request(request, hdr->length, NULL, NULL);
}

int pool6_flush(bool quick)
{
	unsigned char request[HDR_LEN + PAYLOAD_LEN];
	struct request_hdr *hdr = (struct request_hdr *) request;
	union request_pool6 *payload = (union request_pool6 *) (request + HDR_LEN);

	init_request_hdr(hdr, sizeof(request), MODE_POOL6, OP_FLUSH);
	payload->flush.quick = quick;

	return netlink_request(&request, hdr->length, NULL, NULL);
}
