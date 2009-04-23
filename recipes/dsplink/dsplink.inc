DESCRIPTION = "DSP Link for TI ARM/DSP processors"

DEPENDS = "virtual/kernel perl-native"

LICENSE = "TI Proprietary"

inherit module

S = "${WORKDIR}/dsplink_1_60/dsplink"

# Needed for buildscripts
export DSPLINK="${S}"

require ti-paths.inc

do_configure () {
	# Clean up stale binaries
	find ${S} -name "*.ko" -exec rm {} \; || true
	find ${S} -name "*.o" -exec rm {} \; || true

	# Run perl script to create appropriate makefiles (v1.60 and up)
	(
	cd ${DSPLINK}
	perl config/bin/dsplinkcfg.pl --platform=${DSPLINKPLATFORM} --nodsp=1 --dspcfg_0=${DSPCFG} --dspos_0=DSPBIOS5XX  --gppos=${GPPOS} --comps=ponslrm
	)
}

PARALLEL_MAKE = ""

do_compile () {
	unset DISPLAY
	sed -i -e s:armv7a:armv7-a:g ${DSPLINK}/make/Linux/omap3530_2.6.mk

	# export various settings to override the defaults in the makefiles	
	export DSP_BASE_CGTOOLS=${TITOOLSDIR}/${TICGTOOLSDIR}
	export DSP_BASE_BIOS=${TITOOLSDIR}/${TIBIOSDIR}
	export DSP_BASE_RTDX=${TITOOLSDIR}/${TIBIOSDIR}/packages/ti/rtdx
	export GPPTOOL_DIR=${CROSS_DIR}
	export LINUXKERNEL_INSTALL_DIR=${STAGING_KERNEL_DIR}
	export LINK_INSTALL_DIR=${DSPLINK}
	export VARIANT=${DSPLINKSOC}
	export PLATFORM=${DSPLINKPLATFORM}
	export BASE_TOOLCHAIN=${CROSS_DIR}
	export BASE_CGTOOLS=${BASE_TOOLCHAIN}/bin
	# 'OSINC_PLATFORM' is used in both the dsp and gpp sides...	
	export OSINC_PLATFORM1=${CROSS_DIR}/lib/gcc/${TARGET_SYS}/$(${TARGET_PREFIX}gcc -dumpversion)/include
	export OSINC_TARGET=${BASE_TOOLCHAIN}/target/usr/include

	# 'ARCHIVER' is being used in the dsp side of the build as well as gpp
	export ARCHIVER_AR=${TARGET_PREFIX}ar
	export BASE_SABIOS=${DSP_BASE_BIOS}
	
	if [ ! -d ${DSPLINK}/gpp/BUILD/EXPORT/RELEASE ] ; then 
		install -d ${DSPLINK}/gpp/BUILD/EXPORT/RELEASE
	fi

#dsplinkk.ko bits
	cp ${WORKDIR}/Makefile-dsplink-gpp ${DSPLINK}/gpp/BUILD/EXPORT/RELEASE/Makefile
	cd ${DSPLINK}/gpp/BUILD/EXPORT/RELEASE/

	unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS
	export KDIR=${STAGING_KERNEL_DIR}

	make -e driver
	make -e library
	make -e samples

	make -e -f ${WORKDIR}/Makefile-dsplink-dsp
	mkdir -p ${DSPLINK}/gpp/export/BIN/Linux/${DSPLINKPLATFORM}/RELEASE
        cp -a * ${DSPLINK}/gpp/export/BIN/Linux/${DSPLINKPLATFORM}/RELEASE

#lpm bits
	if [ -e ${S}/cetools/packages/ti/bios/power/ ] ; then
		
		# Build the DSP power manager kernel module
		cd ${S}/cetools/packages/ti/bios/power/modules/${DSPPOWERSOC}/lpm

		# Still need to move this into the patch file
		if [ $(echo ${KERNEL_VERSION} | cut -c5,6) -gt 26 ] ; then
			sed -i -e s:asm/semaphore:linux/semaphore: lpm_driver.c
		fi

		#what is this for?
		rm -f *o
		
		# lpm also needs KERNEL_INSTALL_DIR and MVTOOL_PREFIX which come from ti-paths.inc
		export DSPLINK_REPO=${DSPLINK}/..
		# should move the clean step into the clean stage..
		#oe_runmake clean
		oe_runmake
	fi

#cmemk bits
	if [ -e ${S}/cetools/packages/ti/sdo/linuxutils/cmem ] ; then
                echo "MVTOOL_PREFIX=${TARGET_PREFIX}" > ${S}/Rules.make
                echo "UCTOOL_PREFIX=${TARGET_PREFIX}" >> ${S}/Rules.make
                echo "LINUXKERNEL_INSTALL_DIR=${STAGING_KERNEL_DIR}"  >> ${S}/Rules.make
                #export DSPLINK=${STAGING_DIR}/${MULTIMACH_TARGET_SYS}/dsplink

		# Build the cmem kernel module
		# We unset CFLAGS because kernel modules need different ones, this is basically a verbatim copy of kernel.bbclass and module-base.bbclass	
		unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS	

		cd ${S}/cetools/packages/ti/sdo/linuxutils/cmem
		oe_runmake clean
		oe_runmake KERNEL_PATH=${STAGING_KERNEL_DIR}   \
   	        KERNEL_SRC=${STAGING_KERNEL_DIR}    \
   	        KERNEL_VERSION=${KERNEL_VERSION}    \
   	        CC="${KERNEL_CC}" LD="${KERNEL_LD}" \
   	        AR="${KERNEL_AR}"
	fi
}

do_install () {
	install -d ${D}/${bindir}
	install ${DSPLINK}/gpp/BUILD/EXPORT/RELEASE/*gpp ${D}/${bindir} || true

	install -d ${D}/${datadir}/dsplink
	for i in $(find ${DSPLINK}/dsp/BUILD/ -name "*.out") ; do
		install ${i}  ${D}/${datadir}/dsplink
	done	

	install -d ${D}/${libdir}
	install -m 0755 ${DSPLINK}/gpp/BUILD/EXPORT/RELEASE/dsplink.lib  ${D}/${libdir}

	install -d ${D}/lib/modules/${KERNEL_VERSION}/kernel/drivers/dsp
	cp ${DSPLINK}/gpp/BUILD/EXPORT/RELEASE/dsplinkk.ko ${D}/lib/modules/${KERNEL_VERSION}/kernel/drivers/dsp/ 
	cp ${S}/cetools/packages/ti/sdo/linuxutils/cmem/src/module/cmemk.ko ${D}/lib/modules/${KERNEL_VERSION}/kernel/drivers/dsp || true
	cp ${S}/cetools/packages/ti/bios/power/${DSPPOWERSOC}/lpm/*.ko ${D}/lib/modules/${KERNEL_VERSION}/kernel/drivers/dsp || true
        cp ${S}/cetools/packages/ti/bios/power/modules/${DSPPOWERSOC}/lpm/*.ko ${D}/lib/modules/${KERNEL_VERSION}/kernel/drivers/dsp || true
}


# Codec Engine and friends need a complete tree, so stage it all
do_stage() {
	install -d ${STAGING_DIR}/${MULTIMACH_TARGET_SYS}/dsplink
	cp -pPrf ${S}/* ${STAGING_DIR}/${MULTIMACH_TARGET_SYS}/dsplink/ 
}

pkg_postinst_dsplink-module () {
	if [ -n "$D" ]; then
		exit 1
	fi
	depmod -a
	update-modules || true
}

pkg_postrm_dsplink-module () {
	update-modules || true
}

PACKAGES =+ "dsplink-apps dsplink-module ti-lpm-module ti-cmemk-module"
FILES_dsplink-module  = "${sysconfdir} /lib/modules/${KERNEL_VERSION}/kernel/drivers/dsp/dsplinkk.ko"
FILES_dsplink-apps = "${bindir}/* ${datadir}/dsplink/* ${libdir}/dsplink.lib"

FILES_ti-lpm-module = "/lib/modules/${KERNEL_VERSION}/kernel/drivers/dsp/*lpm*ko"

pkg_postinst_ti-lpm-module () {
	if [ -n "$D" ]; then
		exit 1
	fi
	depmod -a
	update-modules || true
}

pkg_postrm_ti-lpm-module () {
	update-modules || true
}

FILES_ti-cmemk-module = "${sysconfdir} /lib/modules/${KERNEL_VERSION}/kernel/drivers/dsp/cmemk.ko"

pkg_postinst_ti-cmemk-module () {
	if [ -n "$D" ]; then        
                exit 1
        fi
        depmod -a
        update-modules || true
}

pkg_postrm_ti-cmemk-module () {
        update-modules || true
}



INHIBIT_PACKAGE_STRIP = "1"

PACKAGE_ARCH = "${MACHINE_ARCH}" 
