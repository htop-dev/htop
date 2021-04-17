NetBSD support in htop(1)
===

This implementation makes NetBSD use htop(1) without the need of mount_procfs(8).

The implementation has been copied over and modified from the OpenBSD
implementation in htop(1).

Current implementation mimics the procfs based implementation in stats
collection.

Make NetBSD no longer masquerade as Linux.

What works
---

* Builds in NetBSD without any additional patch
* Uses sysctl(3) and kvm(3) to get basic information
* Shows basic meters CPU / Memory / Swap
* Shows basic process listing

What needs to be tested
---

* Memory being split into used/buffers/cache
* Basic features of htop(1) like kill / nice / sort / search works as expected
* If the meter displays are working as expected
* If the process states are displayed correctly
* If thread information and count are displayed correctly
