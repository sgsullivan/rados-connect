# ceph-rados-connect
Verify connectivity to a given Ceph cluster via specified cephx user/pass.
This is useful for example, if you want to verify you have proper access
setup before you ever even try something like:

virsh attach-device $uid $xmlfile

Or if you simply wish to verify the hypervisor parent can connect to the
given cluster before you try booting a virtual machine off a RBD volume
on that cluster.

Installation
=============

You need to have rbd/rados libaries installed on your system for the compile
to succeed (gcc -lrbd -lrados). Obviously, you also need a C compiler. Once
you have these things, you can just run make

	make

Removal
=============

To remove, go back into the directory you ran make from originally. Then:

	make uninstall

Usage
=============

First, here's the usage help:

	USAGE: ./rados_connect [OPTIONS]
 
	NOTE: All switches below are _required_ unless stated optional.
 	       -m -- MONS                      (ex: -m10.30.177.4:6789,10.30.177.5:6789,10.30.177.6:6789)
 	       -u -- CEPHX_USER                (ex: -umycephxuser)
	       -p -- CEPH_POOL                 (ex: -ppool0)
 	       -k -- CEPHX_KEY [optional]      (ex: -kAQDJ+gtScGVSBRAA0QenPqsxWaml1jr9C1647w==)
	       -l -- LIBVIR_UUID [optional]    (ex: -l04a8f230-1bd0-4536-a101-3bbd6253ce5c)
 
	*** Options -l and -k are optional, however at least one _must_ be defined.
 
## To check cluster connectivity via cephx user and cephx key:

	rados_connect -m${monip}:${monport},${monip}:${monport},${monip}:${monport} -u${cephxuser} -p${pool} -k${cephxkey}

If the supplied information is good, you will see something like this for output:

	Verifying cluster connectivity and permissions via given cephx key...
	        GOOD: Able to get pool ID. Pool [pool0] ID is [5]
	        GOOD: Able to write [rados_connect test object] to object [9f32300b-6fc0-4f12-8266-a562afef0fc7] on pool [pool0]
	        GOOD: Able to read object [9f32300b-6fc0-4f12-8266-a562afef0fc7]. Contents: [rados_connect test object]
	        GOOD: Able to remove object [9f32300b-6fc0-4f12-8266-a562afef0fc7].
	No errors were encountered!

If there was an error, you will see something like this (and non-zero exit):

	Verifying cluster connectivity and permissions via given cephx key...
	        BAD: Cannot connect to cluster: [Operation not permitted]
	FATAL: Authentication error or insufficient permissions.

## To check cluster connectivity via cephx user and libvirt secret:

	rados_connect -m${monip}:${monport},${monip}:${monport},${monip}:${monport} -u${cephxuser} -p${pool} -l${libvirt_secret}
	
On success or failure, output will be like above. As always, on error, the exit code is non-zero.
