# Periodic scan maintenance

The private operator command processes one due, previously reviewed release:

```sh
python tools/mods/mod-workshop.py rescan-next --repository /private/workshop/repository --clamscan /path/to/clamscan --clamav-database /private/scanner/database
```

It uses the same serial OS worker lock as `scan-next`. The default interval is
six hours, below the24-hour scan expiry. New uploads still require their first
clean scan and explicit moderator review; this command cannot approve them.

A due reviewed release becomes temporarily unavailable while its immutable ZIP
is hash checked, structurally validated and scanned through the existing isolated
ClamAV adapter. Fresh official definitions at most two days old and the current
isolation policy remain required. A fresh clean result renews that existing
review. Detection revokes the release permanently. A scanner error withholds
availability and retries after five minutes; changed stored bytes require manual
repair, scan and review. No database writer transaction is held during the slow
scan, so an operator can revoke content immediately. A concurrent manual action
wins over the older scan result.

Admission and retry state live in `periodic_scans`; the worker lock prevents
concurrent queue/periodic scans. A controller crash leaves availability withheld;
the next worker reclaims the interrupted attempt under that lock. Backups refuse
an active periodic lease. Restore clears these historical renewal records, so
restored approvals cannot resume automatically without fresh manual review.

At most1000 reviewed releases are considered per cycle, matching the public
catalog budget. Periodic audit history stops at100,000 records for operator
archival rather than silently deleting history. Results are recorded in the
private repository event history. These bounds need an operational capacity and
archival plan for larger deployments.

This command is not yet an installed persistent supervisor. Automatic scheduling,
external memory supervision, service-account provisioning, definition updates,
publication refresh and host restart/cancellation acceptance remain required.
Existing exported snapshots are separate immutable files: replace or stop them
when a rescan detects content, and publish signed revocation metadata. A private
database revocation alone does not remove bytes from an already exported tree.
The host/public workshop is not production ready until this is automated and
verified. See [creator service](mod-creator-service.md),
[Mac hosting](mac-mod-hosting.md) and [recovery](mod-host-recovery.md).

Native Mac acceptance now includes actual ClamAV clean renewal, missing-database
failure/retry, and harmless EICAR detection/revocation in private fixtures. The
EICAR test explicitly simulates an inconsistent old approval flag; it never
creates a publicly eligible release. See STATUS for evidence and remaining gates.
