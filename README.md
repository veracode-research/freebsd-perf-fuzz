# Freebsd Perf-Fuzz

FreeBSD port of the [perf-fuzz](https://github.com/sslab-gatech/perf-fuzz)
OS optimized fuzzer.

This is a **work-in-progress** but we will do our best to keep some notes
available as to the state of things.

There are two pieces to this puzzle:

- Modifications to the FreeBSD kernel (12.0-RELEASE), including the addition
of the snapshot() system call.
- Modifications to the AFL tooling to support the use of the snapshot() 
system call.

The `patches` directory contains patches to `src/sys/*` in the 12.0-RELEASE
source tree. It would be a module, but the changes are enough to warrant
modification to the static code. In the near future I will likely pull a
12.0-RELEASE tree into this repo so as to remove the patch step.

## Status

This is a work in progress. The latest patch should be reviewed for your
system first! So... you are warned. That being said, the key parts of the
patch should work in their naive way, but this is totally incomplete. All
that is there is the saving of opened file descriptors and the brute-reset
of them upon a certain system call is invoked. It can be tested with something
like

```
        ret = syscall(syscallno, 1, &nfiles, &ofiles); // Save fd state
        printf("output: %d %d %d\n", ret, nfiles, ofiles);

        fd = open("tsnap.c", O_RDONLY);
        fd2 = open("tsnap", O_RDONLY);

        ret = syscall(syscallno, 5, &nfiles, &ofiles); // List out state direct from kernel
        printf("output: %d %d %d\n", ret, nfiles, ofiles);

        ret = syscall(syscallno, 3, &nfiles, &ofiles); // Reset fd's to earlier state
        printf("output: %d %d %d\n", ret, nfiles, ofiles);

        ret = syscall(syscallno, 5, &nfiles, &ofiles); // List out state direct from kernel
        printf("output: %d %d %d\n", ret, nfiles, ofiles);

        ret = syscall(syscallno, 2, &nfiles, &ofiles); // End this snapshot
        printf("output: %d %d %d\n", ret, nfiles, ofiles);
```

