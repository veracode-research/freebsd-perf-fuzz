
# FreeBSD Port

**Work in Progress**

The `latest.patch` will patch the sys/ path in the FreeBSD
12.0-RELEASE source tree. I plan to pull in a copy of the
sys directory here so that changes are easier to revert.

You can add the patch to your FreeBSD 12.0-RELEASE tree by:

```
$ patch -d /path/to/src/sys -p1 < latest.patch
```


The latest.patch as of August 28, 2019 provides the basics
of the snapshot() system call and implements a naive means
to handle file descriptors and reseting them.

Note that if you are picking bits and parts to pull into your
tree, ensure that your `KERNCONF` contains `options SNAPSHOT`
and, if desired, `options SNAPSHOT_DEBUG`.

Some reminders on kernel building:

```
make -C sys/kern/ sysent
make -C sys/i386/linux sysent
make -C sys/amd64/linux sysent
make -C sys/amd64/linux32  sysent
make -C sys/compat/freebsd32 sysent

make buildkernel KERNCONF=${YOURCONFIG}
make installkernel KERNCONF=${YOURCONFIG}
```


