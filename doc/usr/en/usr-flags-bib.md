---
language: en
layout: default
category: Documentation
title: --bib
---

[Documentation](documentation.html) > [Userspace Application Arguments](documentation.html#userspace-application-arguments) > \--bib

# \--bib

## Index

1. [Description](#description)
2. [Syntax](#syntax)
3. [Arguments](#arguments)
   1. [Operations](#operations)
   2. [Options](#options)
4. [Examples](#examples)

## Description

Interacts with Jool's [Binding Information Base (BIB)](bib.html).

## Syntax

	jool --bib [--tcp] [--udp] [--icmp] (
		[--display] [--numeric] [--csv]
		| --count
		| --add <IPv4-transport-address> <IPv6-transport-address>
		| --remove <IPv4-transport-address> <IPv6-transport-address>
	)

## Arguments

### Operations

* `--display`: The BIB tables are printed in standard output. This is the default operation.
* `--count`: The number of entries per BIB table are printed in standard output.
* `--add`: Combines `<IPv4-transport-address>` and `<IPv6-transport-address>` into a static BIB entry, and uploads it to Jool's tables.  
Note that the `<IPv4-transport-address>` component must be a member of Jool's [IPv4 pool](usr-flags-pool4.html), so make sure you have registered it there first.
* `--remove`: Deletes from the tables the BIB entry described by `<IPv4-transport-address>` and/or `<IPv6-transport-address>`.  
Within a BIB table, every IPv4 transport address is unique. Within a BIB table, every IPv6 transport address is also unique. Therefore, If you're removing a BIB entry, you actually only need to provide one transport address. You can still input both to make sure you're deleting exactly what you want to delete, though.

### Options

| **Flag** | **Description** |
| `--tcp` | If present, the command operates on the TCP table. |
| `--udp` | If present, the command operates on the UDP table. |
| `--icmp` | If present, the command operates on the ICMP table. |
| `--numeric` | By default, the application will attempt to resolve the name of the IPv6 node of each BIB entry. _If your nameservers aren't answering, this will slow the output down_.<br />Use `--numeric` to turn this behavior off. |
| `--csv` | Print the table in [_Comma/Character-Separated Values_ format](http://en.wikipedia.org/wiki/Comma-separated_values). This is intended to be redirected into a .csv file. |

`--tcp`, `--udp` and `--icmp` are not mutually exclusive. If neither of them are present, the records are added or removed to/from all three protocols.

## Examples

Assumptions:

* 4.4.4.4 belongs to the IPv4 pool.
* The name of 6::6 is "potato.mx".
* 6::6 already spoke to a IPv4 node recently, so the database will not start empty.

Display the entire database:

{% highlight bash %}
$ jool --bib --display
TCP:
[Dynamic] 4.4.4.4#1234 - potato.mx#1234
  (Fetched 1 entries.)
UDP:
  (empty)
ICMP:
  (empty)
{% endhighlight %}

Publish a couple of TCP services:

{% highlight bash %}
# jool --bib --add --tcp 6::6#6 4.4.4.4#4
# jool --bib --add --tcp 6::6#66 4.4.4.4#44
{% endhighlight %}

Display the TCP table:

{% highlight bash %}
$ jool --bib --display --tcp
TCP:
[Static] 4.4.4.4#4 - potato.mx#6
[Static] 4.4.4.4#44 - potato.mx#66
[Dynamic] 4.4.4.4#1234 - potato.mx#1234
  (Fetched 3 entries.)
{% endhighlight %}

Same, but do not query the DNS:

{% highlight bash %}
$ jool --bib --display --tcp --numeric
TCP:
[Static] 4.4.4.4#4 - 6::6#6
[Static] 4.4.4.4#44 - 6::6#66
[Dynamic] 4.4.4.4#1234 - 6::6#1234
  (Fetched 3 entries.)
{% endhighlight %}

Publish a UDP service:

{% highlight bash %}
# jool --bib --add --udp 6::6#6666 4.4.4.4#4444
{% endhighlight %}

Dump the database on a CSV file:

{% highlight bash %}
$ jool --bib --display --numeric --csv > bib.csv
{% endhighlight %}

[bib.csv](../obj/bib.csv)

Display the number of entries in the TCP and ICMP tables:

{% highlight bash %}
$ jool --bib --count --tcp --icmp
TCP: 3
ICMP: 0
{% endhighlight %}

Remove the UDP entry:

{% highlight bash %}
# jool --bib --remove --udp 6::6#6666
{% endhighlight %}

