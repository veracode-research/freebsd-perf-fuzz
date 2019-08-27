#!/usr/local/bin/bash
make -C sys/kern/ sysent
make -C sys/i386/linux sysent
make -C sys/amd64/linux sysent
make -C sys/amd64/linux32  sysent
make -C sys/compat/freebsd32 sysent
