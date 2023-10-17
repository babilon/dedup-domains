# dedup-domains
A solution to deduplicate domains in pfBlockerNG purified blacklists

pfBlockerNG does a great job parsing and purifying blocklists from a wide range
of sources. It also does perform deduplication on those lists. What it does not
currently do is deduplicate the aggregate set of blacklists.

This solution aims to fill that niche.

A patch to make use of this solution in pfBlockerNG is in
https://github.com/babilon/FreeBSD-ports/tree/pfblockerng_devel_prune_duplicates.

The deduplication is optimally used with the regex support @andrebrait has added
to pfBlockerNG. With pfBlockerNG's support of Adblock Plus style lists, the
previous necessity to specify every known subdomain is obsolete and to a small
degree wasteful of resources. Feeding pfBlockerNG large lists that have a fair
amount of overlap results in Unbound using slightly more memory than a
deduplicated set of lists.

The deduplication process used about 430MB of memory and completed in about 40
seconds on an Intel Atom E3845 equipped device. More rough details on timing are
on the commit for the deduplication.
