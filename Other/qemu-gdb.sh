#!/bin/sh

qemu-system-i386 -serial mon:stdio -hdb fs.img xv6.img -smp 2 -m 256 -s -S
