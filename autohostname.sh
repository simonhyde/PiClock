#!/bin/sh
HOSTNAME="piclock-$(cat /proc/cpuinfo| grep ^Serial | sed 's/.*: *0*//')"
/bin/hostname "$HOSTNAME"
/bin/sed -i "s/raspberrypi\|piclock-[^. ]*/$HOSTNAME/g" /etc/hosts
