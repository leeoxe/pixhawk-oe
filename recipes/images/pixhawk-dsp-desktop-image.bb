# PIXHAWK console image
# Includes all packages and setting for the PIXHAWK system

require pixhawk-desktop-image.bb

IMAGE_INSTALL += " \
   ti-dsplink-module \
   ti-cmem-module \
   ti-codec-engine "


export IMAGE_BASENAME = "pixhawk-dsp-desktop-image"
