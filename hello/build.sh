#! /bin/env bash
gcc -isystem ../sysroot-musl-dbg/include -g -c -o hello.o hello.c
ld -o hello-musl-dbg hello.o ../sysroot-musl-dbg/lib/crt1.o ../sysroot-musl-dbg/lib/libc.a
