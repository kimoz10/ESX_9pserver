#!/bin/bash
umount /mnt/vmdk
kpartx -dv /dev/loop0
losetup -d /dev/loop0
