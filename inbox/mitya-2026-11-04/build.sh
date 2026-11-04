#!/bin/bash

set -e

CMDLINE=''
CMDLINE+=' ignore_loglevel'
#CMDLINE+=' ath10k_core.debug_mask=0x20'
#CMDLINE+=' ath11k.debug_mask=0x20'
#CMDLINE+=' console=tty0'
CMDLINE+=' console=ttyMSM0,115200n8'
#CMDLINE+=' pd_ignore_unused'
CMDLINE+=' clk_ignore_unused'
#CMDLINE+=' regulator_ignore_unused'
#CMDLINE+=' earlycon'
#CMDLINE+=' pcie_pme=nomsi'
#CMDLINE+=' pcie_aspm=off'
#CMDLINE+=' initcall_blacklist=qcom_pcie_driver_init'
#CMDLINE+=' initcall_blacklist=disp_cc_sm8250_driver_init'
#CMDLINE+=' initcall_debug'
#CMDLINE+=' root=/dev/mmcblk0p1 rootwait'
CMDLINE+=' root=PARTLABEL=rootfs rootwait'
#CMDLINE+=' root=/dev/sda1 rootwait'
#CMDLINE+=' root=/dev/mmcblk0p14 rootwait'
#CMDLINE+=' drm.debug=0x2' #device
#CMDLINE+=' drm.debug=0x4' #KMS
#CMDLINE+=' drm.debug=0x5' #core + KMS
#CMDLINE+=' drm.debug=0x6' #device + KMS
#CMDLINE+=' drm.debug=0x7'
#CMDLINE+=' drm.debug=0x10' #atomic
#CMDLINE+=' drm.debug=0x12' #atomic + device
#CMDLINE+=' drm.debug=0x16' #atomic + device + KMS
#CMDLINE+=' drm.debug=0x46' #stat + device + KMS
#CMDLINE+=' drm.debug=0x100' #DP
#CMDLINE+=' drm.debug=0x106' #DP + device + KMS
#CMDLINE+=' drm.debug=0x1ff'
#CMDLINE+=' drm.debug=0x092'
#CMDLINE+=' fb.lockless_register_fb=1'
#CMDLINE+=' msm.dpu_use_virtual_planes=1'
#CMDLINE+=' msm.prefer_mdp5=false'
#CMDLINE+=' cpuidle.off=1'
#CMDLINE+=' msm.separate_gpu_kms=true'
CMDLINE+=' systemd.mask=pd-mapper.service'
CMDLINE+=' systemd.mask=qbootctl.service'
CMDLINE+=' systemd.mask=adbd.service'
CMDLINE+=' systemd.mask=android-tools-adbd.service'
#CMDLINE+=' qcom_scm.qseecom=force'
#CMDLINE+=' fw_devlink=permissive'
#CMDLINE+=' rdinit=/bin/sh'
#CMDLINE+=' adbd'
CMDLINE+=' -- '

cd `dirname $0`

if [ \! -f kernel/Makefile ]; then
	echo "Expecting a kernel source tree in kernel/"
	exit 1
fi

RAMDISK=~/Projects/RPB/build/tmp-qcom-distro/deploy/images/qcom-armv8a/initramfs-test-image-qcom-armv8a.cpio.gz
RAMDISK_TINY=~/Projects/RPB/build/tmp-qcom-distro/deploy/images/qcom-armv8a/initramfs-tiny-image-qcom-armv8a.cpio.xz
#RAMDISK=/dev/null
#RAMDISK_TINY=/dev/null

#RAMDISK=~/Projects/RPB/build/tmp-qcom-distro/deploy/images/qcom-armv8a/initramfs-tiny-image-qcom-armv8a.cpio.gz

#RAMDISK=RPB/build/tmp-qcom-distro/deploy/images/qcom-armv8a/initramfs-test-image-qcom-armv8a.cpio.gz
#RAMDISK=RPB/build/tmp-qcom-distro/deploy/images/qcom-armv8a/initramfs-rootfs-image-qcom-armv8a.cpio.gz
#RAMDISK=RPB-dunfell/build/tmp-qcom-distro/deploy/images/qcom-armv8a/initramfs-test-image-qcom-armv8a.cpio.gz

MAKECMD="make%CROSS_COMPILE=ccache aarch64-linux-gnu-%ARCH=arm64%-j48"

#export CCACHE_PREFIX=icecc
#export ICECC_VERSION=/home/lumag/Projects/Qcomm/c25786a43e76d593e8cf00a80d852185.tar.gz
#export ICECC_CARET_WORKAROUND=0
#MAKECMD+="%-j80"

#
# Kernel and modules
#
echo "Building kernel and modules"
#CFG="defconfig%qc-only.config"
#CFG="olddefconfig"

rm -rf modules
(cd build-64; IFS=%; ${MAKECMD} ${CFG} dtbs Image Image.gz modules savedefconfig modules_install -C ../kernel O=`pwd` INSTALL_MOD_PATH=`pwd`/../modules/usr)
(cd modules; find . | cpio -ov -H newc -R +0:+0 | gzip -c > ../modules.cpio.gz)

do_image_v2() {
	board=$1
	dtb=build-64/arch/arm64/boot/dts/qcom/$2.dtb
	echo "Building boot-$board.img v2"

	OPTS="--base 0x80000000 \
	      --pagesize 2048 \
	      --os_version 14.0.0 \
	      --os_patch_level 2023-10"

	IMAGE=Image.gz
	if [ "${3}" = "nogz" ] ; then
		IMAGE=Image
		shift
	fi

	if [ "${RAMDISK}" = "/dev/null" ] ; then
		> ramdisk
	else
		if [ "${3}" = "null" ] ; then
			echo -n > ramdisk
			shift
		elif [ "${3}" = "tiny" ] ; then
			cp ${RAMDISK_TINY} ramdisk
			shift
		else
			cp ${RAMDISK} ramdisk
		fi

		if [ "${3}" = "no-modules" ] ; then
			shift
		else
			cat modules.cpio.gz >> ramdisk
		fi

		if [ -n "$3" ] ; then
			cat /home/lumag/Projects/RPB/build/tmp-qcom-distro/deploy/images/qcom-armv8a/initramfs-firmware-$3-image-qcom-armv8a.cpio.gz >> ramdisk
		fi
	fi

	mkbootimg --header_version 2 \
		  --kernel build-64/arch/arm64/boot/${IMAGE} \
		  --dtb $dtb \
		  --cmdline "${CMDLINE}" \
		  --ramdisk ramdisk \
		  ${OPTS} \
		  --output boot-${board}.img
}

do_image() {
	board=$1
	dtb=build-64/arch/arm64/boot/dts/qcom/$2.dtb
	echo "Building boot-${board}.img"
	cat build-64/arch/arm64/boot/Image.gz $dtb > Image.gz-dtb
	if [ "${RAMDISK}" = "/dev/null" ] ; then
		> ramdisk
	else
		if [ "${3}" = "null" ] ; then
			echo -n > ramdisk
			shift
		elif [ "${3}" = "tiny" ] ; then
			cp ${RAMDISK_TINY} ramdisk
			shift
		else
			cp ${RAMDISK} ramdisk
		fi

		if [ "${3}" = "no-modules" ] ; then
			shift
		else
			cat modules.cpio.gz >> ramdisk
		fi

		if [ -n "$3" ] ; then
			cat /home/lumag/Projects/RPB/build/tmp-qcom-distro/deploy/images/qcom-armv8a/initramfs-firmware-$3-image-qcom-armv8a.cpio.gz >> ramdisk
		fi
	fi
	skales-mkbootimg --kernel Image.gz-dtb \
		  --cmdline "${CMDLINE}" \
		  --ramdisk ramdisk \
		  --base 0x80000000 \
		  --pagesize 2048 \
		  --output boot-${board}.img
}

#do_image sm8250-mtp sm8250-mtp
#
#do_image miix630 msm8998-lenovo-miix-630
#
##
do_image_v2 rb5 qrb5165-rb5 nogz rb5
do_image_v2 rb5-rootfs qrb5165-rb5 null rb5
#do_image rb5-snap qrb5165-rb5-snap
#do_image rb5-snap-dsi0 qrb5165-rb5-snap-dsi0
#do_image rb5-snap-dsi1 qrb5165-rb5-snap-dsi1
#do_image rb5-snap-separate qrb5165-rb5-snap-separate
##do_image rb5-ddsi qrb5165-rb5-dual-dsi
##do_image rb5-dvt sm8250-rb5-dvt
##
#do_image sdm845-mtp sdm845-mtp

do_image openq835-hdk apq8098-lantronix-openq-835-hdk tiny lantronix-openq-835-hdk
do_image openq835-hdk-rootfs apq8098-lantronix-openq-835-hdk null lantronix-openq-835-hdk

do_image sdm845-hdk sdm845-hdk tiny no-modules sdm845-hdk
do_image sdm845-hdk-rootfs sdm845-hdk null sdm845-hdk

do_image sdm845-hdk-display sdm845-hdk-display-card tiny no-modules sdm845-hdk
do_image sdm845-hdk-display-rootfs sdm845-hdk-display-card null sdm845-hdk

do_image rb3 sdm845-db845c dragonboard845c
do_image rb3-rootfs sdm845-db845c null dragonboard845c
##do_image rb3-ddsi sdm845-db845c-dual-dsi
##
do_image db410c apq8016-sbc tiny no-modules dragonboard410c
do_image db410c-usb apq8016-sbc-usb-host dragonboard410c
do_image db410c-rootfs apq8016-sbc null dragonboard410c
do_image db410c-usb-rootfs apq8016-sbc-usb-host null dragonboard410c
#do_image db410c-panel apq8016-sbc-panel dragonboard410c
##
do_image db820c apq8096-db820c dragonboard820c
do_image db820c-rootfs apq8096-db820c null dragonboard820c
#do_image db820c apq8096-db820c tiny no-modules dragonboard820c
##
do_image ifc6560 sda660-inforce-ifc6560 tiny inforce-ifc6560
do_image ifc6560-rootfs sda660-inforce-ifc6560 null #inforce-ifc6560
##
do_image sm8350-hdk sm8350-hdk tiny sm8350-hdk
do_image sm8350-hdk-rootfs sm8350-hdk null no-modules sm8350-hdk
##
##do_image msm8998-mtp msm8998-mtp
##
#do_image sm8450-qrd sm8450-qrd

do_image sm8450-hdk sm8450-hdk tiny sm8450-hdk
do_image sm8450-hdk-rootfs sm8450-hdk null sm8450-hdk

#do_image sm8450-hdk-display sm8450-hdk-display-card tiny sm8450-hdk
#do_image sm8450-hdk-display-rootfs sm8450-hdk-display-card null sm8450-hdk

#do_image sm8550-mtp sm8550-mtp
#do_image sm8550-qrd sm8550-qrd
#do_image_v2 sm8550-hdk sm8550-hdk
#
#do_image_v2 sm8650-mtp sm8650-mtp
#do_image_v2 sm8650-qrd sm8650-qrd
#do_image_v2 sm8650-hdk sm8650-hdk
#
##
#do_image qcs404 qcs404-evb-1000
##do_image qcs404-r700v3 r700v3
#
do_image sm8150-hdk sm8150-hdk tiny no-modules sm8150-hdk
do_image sm8150-hdk-rootfs sm8150-hdk null no-modules

do_image sm8150-hdk-display sm8150-hdk-display-card tiny no-modules sm8150-hdk
do_image sm8150-hdk-display-rootfs sm8150-hdk-display-card null no-modules
#do_image sm8150-mtp sm8150-mtp
#do_image sa8155p-adp sa8155p-adp
do_image rb1 qrb2210-rb1 tiny rb1
do_image rb1-rootfs qrb2210-rb1 null rb1
do_image rb2 qrb4210-rb2 tiny rb2
do_image rb2-rootfs qrb4210-rb2 null rb2

do_image_v2 rb3gen2 qcs6490-rb3gen2 nogz rb3gen2
do_image_v2 rb3gen2-rootfs qcs6490-rb3gen2 nogz null no-modules rb3gen2

# QCS8550; no rb5gen2 firmware image yet, sm8550-hdk covers the same SoC
do_image_v2 rb5gen2 qcs8550-rb5gen2 nogz sm8550-hdk
#do_image_v2 rb5gen2-rootfs qcs8550-rb5gen2 nogz null no-modules sm8550-hdk
do_image_v2 rb5gen2-rootfs qcs8550-rb5gen2 null no-modules sm8550-hdk
#
do_image_v2 primus sc8180x-primus
do_image_v2 sc8280xp-crd sc8280xp-crd nogz tiny no-modules
do_image_v2 x1e-crd x1e80100-crd nogz
do_image_v2 glymur-crd glymur-crd nogz tiny no-modules

do_image_v2 kaanapali-qrd kaanapali-qrd null no-modules
