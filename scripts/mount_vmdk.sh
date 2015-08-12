#!/bin/bash
losetup /dev/loop0 $1
kpartx -av /dev/loop0
mount /dev/mapper/loop0p1 /mnt/vmdk
