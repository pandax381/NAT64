#include <linux/module.h>
#include <linux/printk.h>

#include "nf_nat64_session.h"

/********************************************
 * Structures and private variables.
 ********************************************/

// Hash table; indexes session entries by IPv4 address.
// (this code generates the "ipv4_table" structure and related functions used below).
#define HTABLE_NAME ipv4_table
#define KEY_TYPE struct ipv4_pair
#define VALUE_TYPE struct session_entry
#include "nf_nat64_hash_table.c"

// Hash table; indexes BIB entries by IPv6 address.
// (this code generates the "ipv6_table" structure and related functions used below).
#define HTABLE_NAME ipv6_table
#define KEY_TYPE struct ipv6_pair
#define VALUE_TYPE struct session_entry
#include "nf_nat64_hash_table.c"

/**
 * Session table definition.
 * Holds two hash tables, one for each indexing need (IPv4 and IPv6).
 */
struct session_table
{
	/** Indexes entries by IPv4. */
	struct ipv4_table ipv4;
	/** Indexes entries by IPv6. */
	struct ipv6_table ipv6;
};

/** The session table for UDP connections. */
static struct session_table session_table_udp;
/** The session table for TCP connections. */
static struct session_table session_table_tcp;
/** The session table for ICMP connections. */
static struct session_table session_table_icmp;

/**
 * Chains all known session entries.
 * Currently only used while looking en deleting expired ones.
 */
static LIST_HEAD(all_sessions);

/********************************************
 * Private (helper) functions.
 ********************************************/

static struct session_table *get_session_table(int l4protocol)
{
	switch (l4protocol) {
	case IPPROTO_UDP:
		return &session_table_udp;
	case IPPROTO_TCP:
		return &session_table_tcp;
	case IPPROTO_ICMP:
		return &session_table_icmp;
	}

	printk(KERN_CRIT "Error: Unknown l4 protocol (%d); no session table mapped to it.", l4protocol);
	return NULL;
}

/*******************************
 * Public functions.
 *******************************/

void nat64_session_init(void)
{
	ipv4_table_init(&session_table_udp.ipv4, ipv4_pair_equals, ipv4_pair_hash_code);
	ipv6_table_init(&session_table_udp.ipv6, ipv6_pair_equals, ipv6_pair_hash_code);

	ipv4_table_init(&session_table_tcp.ipv4, ipv4_pair_equals, ipv4_pair_hash_code);
	ipv6_table_init(&session_table_tcp.ipv6, ipv6_pair_equals, ipv6_pair_hash_code);

	ipv4_table_init(&session_table_icmp.ipv4, ipv4_pair_equals, ipv4_pair_hash_code);
	ipv6_table_init(&session_table_icmp.ipv6, ipv6_pair_equals, ipv6_pair_hash_code);
}

bool nat64_add_session_entry(struct session_entry *entry)
{
	bool inserted_to_ipv4, inserted_to_ipv6;
	struct session_table *table = get_session_table(entry->l4protocol);

	if (entry->bib == NULL)
		return false; // Because it's invalid.

	// Insert into the hash tables.
	inserted_to_ipv4 = ipv4_table_put(&table->ipv4, &entry->ipv4, entry);
	inserted_to_ipv6 = ipv6_table_put(&table->ipv6, &entry->ipv6, entry);

	if (!inserted_to_ipv4 || !inserted_to_ipv6) {
		ipv4_table_remove(&table->ipv4, &entry->ipv4, false, false);
		ipv6_table_remove(&table->ipv6, &entry->ipv6, false, false);
		return false;
	}

	// Insert into the linked lists.
	list_add(&entry->entries_from_bib, &entry->bib->session_entries);
	list_add(&entry->all_sessions, &all_sessions);

	return true;
}

struct session_entry *nat64_get_session_entry_by_ipv4(struct ipv4_tuple_address *remote,
		struct ipv4_tuple_address *local, int l4protocol)
{
	struct ipv4_pair pair = { *remote, *local };
	printk(KERN_DEBUG "Searching session entry: [%pI4#%d, %pI4#%d]...", &remote->address, remote->pi.port, &local->address, local->pi.port);
	return ipv4_table_get(&get_session_table(l4protocol)->ipv4, &pair);
}

struct session_entry *nat64_get_session_entry_by_ipv6(struct ipv6_tuple_address *local,
		struct ipv6_tuple_address *remote, int l4protocol)
{
	struct ipv6_pair pair = { *local, *remote };
	printk(KERN_DEBUG "Searching session entry: [%pI6#%d, %pI6#%d]...", &local->address, local->pi.port, &remote->address, remote->pi.port);
	return ipv6_table_get(&get_session_table(l4protocol)->ipv6, &pair);
}

void nat64_update_session_lifetime(struct session_entry *entry, unsigned int ttl)
{
	entry->dying_time = jiffies_to_msecs(jiffies) + ttl;
}

bool nat64_remove_session_entry(struct session_entry *entry)
{
	struct session_table *table;
	bool removed_from_ipv4, removed_from_ipv6;

	table = get_session_table(entry->l4protocol);

	// Free from both tables.
	removed_from_ipv4 = ipv4_table_remove(&table->ipv4, &entry->ipv4, false, false);
	removed_from_ipv6 = ipv6_table_remove(&table->ipv6, &entry->ipv6, false, false);

	if (removed_from_ipv4 && removed_from_ipv6) {
		// Remove the entry from the linked lists.
		list_del(&entry->entries_from_bib);
		list_del(&entry->all_sessions);

		// Erase the BIB. Might not happen if it has more sessions.
		if (nat64_remove_bib_entry(entry->bib, entry->l4protocol)) {
			kfree(entry->bib);
			entry->bib = NULL;
		}

		return true;
	}
	if (!removed_from_ipv4 && !removed_from_ipv6) {
		return false;
	}

	// Why was it not indexed by both tables? Programming error.
	printk(KERN_CRIT "Programming error: Weird session removal: ipv4:%d; ipv6:%d.", removed_from_ipv4, removed_from_ipv6);
	return true;
}

void nat64_clean_old_sessions(void)
{
	struct list_head *current_node, *next_node;
	struct session_entry *current_entry;
	unsigned int current_time = jiffies_to_msecs(jiffies);

	list_for_each_safe(current_node, next_node, &all_sessions) {
		current_entry = list_entry(current_node, struct session_entry, all_sessions);
		if (!current_entry->is_static && current_entry->dying_time <= current_time) {
			nat64_remove_session_entry(current_entry);
			kfree(current_entry);
		}
	}
}

void nat64_session_destroy(void)
{
	printk(KERN_DEBUG "Emptying the session tables...");

	// The keys needn't be released because they're part of the values.
	// The values need to be released only in one of the tables because both tables point to the same value.

	ipv4_table_empty(&session_table_udp.ipv4, false, false);
	ipv6_table_empty(&session_table_udp.ipv6, false, true);

	ipv4_table_empty(&session_table_tcp.ipv4, false, false);
	ipv6_table_empty(&session_table_tcp.ipv6, false, true);

	ipv4_table_empty(&session_table_icmp.ipv4, false, false);
	ipv6_table_empty(&session_table_icmp.ipv6, false, true);

	INIT_LIST_HEAD(&all_sessions);
}

bool session_entry_equals(struct session_entry *session_1, struct session_entry *session_2)
{
	if (session_1 == session_2)
		return true;
	if (session_1 == NULL || session_2 == NULL)
		return false;

	if (session_1->l4protocol != session_2->l4protocol)
		return false;
	if (!ipv6_tuple_address_equals(&session_1->ipv6.remote, &session_2->ipv6.remote))
		return false;
	if (!ipv6_tuple_address_equals(&session_1->ipv6.local, &session_2->ipv6.local))
		return false;
	if (!ipv4_tuple_address_equals(&session_1->ipv4.local, &session_2->ipv4.local))
		return false;
	if (!ipv4_tuple_address_equals(&session_1->ipv4.remote, &session_2->ipv4.remote))
		return false;

	return true;
}
