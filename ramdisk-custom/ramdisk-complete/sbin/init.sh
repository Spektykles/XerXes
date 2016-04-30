#!/sbin/busybox sh
set +x
_PATH="$PATH"
export PATH=/sbin

busybox cd /
busybox date >>boot.txt
exec >>boot.txt 2>&1
busybox rm /init

# include device specific vars
source /sbin/bootrec-device

# create directories
busybox mkdir -m 755 -p /cache
busybox mkdir -m 755 -p /dev/block
busybox mkdir -m 755 -p /dev/input
busybox mkdir -m 555 -p /proc
busybox mkdir -m 755 -p /sys

# create device nodes
busybox mknod -m 600 /dev/block/mmcblk0 b 179 0
busybox mknod -m 600 ${BOOTREC_CACHE_NODE}
busybox mknod -m 600 ${BOOTREC_EVENT_NODE}
busybox mknod -m 666 /dev/null c 1 3

# mount filesystems
busybox mount -t proc proc /proc
busybox mount -t sysfs sysfs /sys
busybox mount /dev/block/mmcblk0p24 /cache


# check /cache/recovery/boot
if [ -e /cache/recovery/boot ] || busybox grep -q warmboot=0x77665502 /proc/cmdline ; then

busybox echo "found reboot into recovery flag"  >>boot.txt

else

# trigger amber LED
busybox echo 255 > ${BOOTREC_LED_RED}
busybox echo 0 > ${BOOTREC_LED_GREEN}
busybox echo 255 > ${BOOTREC_LED_BLUE}

# trigger vibration
#busybox echo 200 > /sys/class/timed_output/vibrator/enable

# keycheck
busybox cat ${BOOTREC_EVENT} > /dev/keycheck&
busybox sleep 3

fi

# android ramdisk
load_image=/sbin/ramdisk.cpio

# boot decision
if [ -s /dev/keycheck ] || busybox grep -q warmboot=0x77665502 /proc/cmdline ; then

	busybox echo 'RECOVERY BOOT' >>boot.txt

	# orange led for recoveryboot
	busybox echo 255 > ${BOOTREC_LED_RED}
	busybox echo 100 > ${BOOTREC_LED_GREEN}
	busybox echo 0 > ${BOOTREC_LED_BLUE}

	# recovery ramdisk
	# default recovery ramdisk is TWRP
	load_image=/sbin/ramdisk-recovery.cpio


	if [ -e /cache/recovery/boot ] || busybox grep -q warmboot=0x77665502 /proc/cmdline ; then

		busybox rm /cache/recovery/boot
		
		busybox id >>boot.txt
		busybox ls -l /cache/recovery >>boot.txt

		busybox echo 'checking CWM flag' >>boot.txt
		if [ -e /cache/recovery/boot_cwm ]
		then
			busybox rm /cache/recovery/boot_cwm
			#load cwm ramdisk		
			load_image=/sbin/ramdisk-recovery.cpio
			busybox echo 'Selected CWM' >>boot.txt
		fi

		busybox echo 'checking TWRP flag' >>boot.txt
		if [ -e /cache/recovery/boot_twrp ]
		then
			busybox rm /cache/recovery/boot_twrp
			#load twrp ramdisk
			load_image=/sbin/ramdisk-recovery.cpio
			busybox echo 'Selected TWRP' >>boot.txt
		fi

		busybox echo 'checking PHILZ flag' >>boot.txt
		if [ -e /cache/recovery/boot_philz ]
		then
			busybox rm /cache/recovery/boot_philz
			#load philz ramdisk
			load_image=/sbin/ramdisk-recovery.cpio
			busybox echo 'Selected PHILZ' >>boot.txt
		fi


	else

		## DooMLoRD: handle multiple recovery ramdisks based on keypress
	
		if [ -s /dev/keycheck ]
		then
			busybox hexdump < /dev/keycheck > /dev/keycheck1

			export VOLUKEYCHECK=`busybox cat /dev/keycheck1 | busybox grep '0001 0073'`
			export VOLDKEYCHECK=`busybox cat /dev/keycheck1 | busybox grep '0001 0072'`
			export POWRKEYCHECK=`busybox cat /dev/keycheck1 | busybox grep '0001 0074'`
			export CAMRKEYCHECK=`busybox cat /dev/keycheck1 | busybox grep '0001 0210'`

			busybox rm /dev/keycheck
			busybox rm /dev/keycheck1

			if [ -n "$VOLUKEYCHECK" ]
			then
				#load cwm ramdisk
				load_image=/sbin/ramdisk-recovery.cpio
				busybox echo 'Selected CWM' >>boot.txt
			fi

			if [ -n "$VOLDKEYCHECK" ]
			then
				#load twrp ramdisk
				load_image=/sbin/ramdisk-recovery.cpio
				busybox echo 'Selected TWRP' >>boot.txt
			fi
			
			if [ -n "$POWRKEYCHECK" ]
			then
				#load FOTA ramdisk
				busybox mknod -m 600 ${BOOTREC_FOTA_NODE}
				busybox mount -o remount,rw /
				busybox ln -sf /sbin/busybox /sbin/sh
				extract_elf_ramdisk -i ${BOOTREC_FOTA} -o /sbin/ramdisk-recovery-fota.cpio -t / -c
				busybox rm /sbin/sh
				load_image=/sbin/ramdisk-recovery-fota.cpio
				busybox echo 'Selected FOTA recovery' >>boot.txt
			fi

			if [ -n "$CAMRKEYCHECK" ]
			then
				#load philz ramdisk
				load_image=/sbin/ramdisk-recovery.cpio
				busybox echo 'Selected PHILZ' >>boot.txt
			fi
		fi

	fi
	


else
	busybox echo 'ANDROID BOOT' >>boot.txt
fi
	# poweroff LED
	busybox echo 0 > ${BOOTREC_LED_RED}
	busybox echo 0 > ${BOOTREC_LED_GREEN}
	busybox echo 0 > ${BOOTREC_LED_BLUE}

# kill the keycheck process
busybox pkill -f "busybox cat ${BOOTREC_EVENT}"

# unpack the ramdisk image
busybox cpio -i < ${load_image}

busybox umount /cache
busybox umount /proc
busybox umount /sys

busybox rm -fr /dev/*
busybox date >>boot.txt
export PATH="${_PATH}"
exec /init
