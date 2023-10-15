# dedup-domains
A solution to deduplicate domains in pfBlockerNG purified blacklists

pfBlockerNG does a great job parsing and purifying blocklists from a wide range of sources. It also does perform deduplication on those lists. What it does not currently do is deduplicate the aggregate set of blacklists.

This solution aims to fill that niche.

A patch to make use of this solution in pfBlockerNG is in https://github.com/babilon/FreeBSD-ports/tree/pfblockerng_devel_prune_duplicates.
