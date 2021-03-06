#include "nat64/mod/stateful/bib/db.h"

#include "nat64/mod/stateful/bib/table.h"
#include "nat64/mod/stateful/bib/port_allocator.h"

/** The BIB table for TCP connections. */
static struct bib_table bib_tcp;
/** The BIB table for UDP connections. */
static struct bib_table bib_udp;
/** The BIB table for ICMP connections. */
static struct bib_table bib_icmp;

/**
 * One-liner to get the BIB table corresponding to the "proto" protocol.
 */
static struct bib_table *get_table(const l4_protocol proto)
{
	switch (proto) {
	case L4PROTO_TCP:
		return &bib_tcp;
	case L4PROTO_UDP:
		return &bib_udp;
	case L4PROTO_ICMP:
		return &bib_icmp;
	case L4PROTO_OTHER:
		break;
	}

	WARN(true, "Unsupported transport protocol: %u.", proto);
	return NULL;
}

/**
 * Initializes the three tables (TCP, UDP and ICMP).
 * Call during initialization for the remaining functions to work properly.
 */
int bibdb_init(void)
{
	int error;

	error = bibentry_init();
	if (error)
		return error;

	bibtable_init(&bib_tcp);
	bibtable_init(&bib_udp);
	bibtable_init(&bib_icmp);

	return 0;
}

/**
 * Empties the BIB tables, freeing any memory being used by them.
 * Call during destruction to avoid memory leaks.
 */
void bibdb_destroy(void)
{
	log_debug("Emptying the BIB tables...");

	bibtable_destroy(&bib_udp);
	bibtable_destroy(&bib_tcp);
	bibtable_destroy(&bib_icmp);

	bibentry_destroy();
}

/**
 * Makes "result" point to the BIB entry you'd expect from the "tuple" tuple.
 *
 * That is, when we're translating from IPv6 to IPv4, "result" will point to the
 * BIB enrty whose IPv6 address is "tuple"'s source address.
 * When we're translating from IPv4 to IPv6, "result" will point to the entry
 * whose IPv4 address is "tuple"'s destination address.
 *
 * It increases "result"'s refcount. Make sure you call bibdb_return() on it
 * when you're done.
 *
 * @param[in] tuple summary of the packet. Describes the BIB you need.
 * @param[out] the BIB entry you'd expect from the "tuple" tuple.
 * @return error status.
 */
int bibdb_get(const struct tuple *tuple, struct bib_entry **result)
{
	if (WARN(!tuple, "tuple is NULL."))
		return -EINVAL;

	switch (tuple->l3_proto) {
	case L3PROTO_IPV6:
		return bibdb_get6(&tuple->src.addr6, tuple->l4_proto, result);
	case L3PROTO_IPV4:
		return bibdb_get4(&tuple->dst.addr4, tuple->l4_proto, result);
	}

	WARN(true, "Unsupported network protocol: %u.", tuple->l3_proto);
	return -EINVAL;
}

/**
 * Makes "result" point to the BIB entry from the "l4_proto" table whose IPv4
 * side (address and port) is "addr".
 *
 * It increases "result"'s refcount. Make sure you call bibdb_return() on it
 * when you're done.
 *
 * @param[in] address address and port you want the BIB entry for.
 * @param[in] l4_proto identifier of the table to retrieve the entry from.
 * @param[out] the BIB entry from the table will be placed here.
 * @return error status.
 */
int bibdb_get4(const struct ipv4_transport_addr *addr, const l4_protocol proto,
		struct bib_entry **result)
{
	struct bib_table *table = get_table(proto);
	return table ? bibtable_get4(table, addr, result) : -EINVAL;
}

bool bibdb_contains4(const struct ipv4_transport_addr *addr,
		const l4_protocol proto)
{
	struct bib_table *table = get_table(proto);
	return table ? bibtable_contains4(table, addr) : false;
}

/**
 * Makes "result" point to the BIB entry from the "l4_proto" table whose IPv6
 * side (address and port) is "addr".
 *
 * It increases "result"'s refcount. Make sure you call bibdb_return() on it
 * when you're done.
 *
 * @param[in] address address and port you want the BIB entry for.
 * @param[in] l4_proto identifier of the table to retrieve the entry from.
 * @param[out] the BIB entry from the table will be placed here.
 * @return error status.
 */
int bibdb_get6(const struct ipv6_transport_addr *addr, const l4_protocol proto,
		struct bib_entry **result)
{
	struct bib_table *table = get_table(proto);
	return table ? bibtable_get6(table, addr, result) : -EINVAL;
}

void bibdb_return(struct bib_entry *bib)
{
	struct bib_table *table;
	bool delete;

	if (unlikely(!bib))
		return;

	delete = bibentry_return(bib);
	if (!delete)
		return;

	table = get_table(bib->l4_proto);
	if (table)
		bibtable_rm(table, bib);

	bibentry_kfree(bib);
}

/**
 * Adds "entry" to the BIB table it belongs. Make sure you initialized "entry"
 * using bib_create(), please.
 *
 * The table's references are not supposed to count towards the entries'
 * refcounts. Do free your reference if your entry made it into the table;
 * do not assume you're transferring it.
 *
 * @param entry row to be added to the table.
 * @param l4_proto identifier of the table to add "entry" to.
 * @return whether the entry could be inserted or not.
 */
int bibdb_add(struct bib_entry *entry)
{
	struct bib_table *table = get_table(entry->l4_proto);
	return table ? bibtable_add(table, entry) : -EINVAL;
}

/**
 * Runs "func" on every BIB entry after "offset".
 */
int bibdb_foreach(const l4_protocol proto,
		int (*func)(struct bib_entry *, void *), void *arg,
		const struct ipv4_transport_addr *offset)
{
	struct bib_table *table = get_table(proto);
	return table ? bibtable_foreach(table, func, arg, offset) : -EINVAL;
}

/**
 * Sets in the value pointed by "result" the number of entries in the database
 * whose protocol is "proto".
 */
int bibdb_count(const l4_protocol proto, __u64 *result)
{
	struct bib_table *table = get_table(proto);
	return table ? bibtable_count(table, result) : -EINVAL;
}

/**
 * Removes the fake users of all the BIB entries whose local IPv4 address is
 * "addr4".
 */
void bibdb_delete_taddr4s(const struct ipv4_prefix *prefix,
		struct port_range *ports)
{
	bibtable_delete_taddr4s(&bib_tcp, prefix, ports);
	bibtable_delete_taddr4s(&bib_udp, prefix, ports);
	bibtable_delete_taddr4s(&bib_icmp, prefix, ports);
}

/**
 * Removes all the fake users of all the BIB entries in the DB.
 */
void bibdb_flush(void)
{
	log_debug("Emptying the BIB tables...");

	bibtable_flush(&bib_tcp);
	bibtable_flush(&bib_icmp);
	bibtable_flush(&bib_udp);
}
