# QMedia Android Application Package
PRODUCT_PACKAGES += QMedia
# umd-daemon application to access camera and audio frames
# and pass it on to UMD-Gadget module
PRODUCT_PACKAGES += umd-daemon
PRODUCT_PACKAGES += ai_director_test

# SNPE wrapper library
PRODUCT_PACKAGES += libai_snpe_wrapper

# SNPE libraries
# SNPE_WRAPPER_COMPILATION := true
ifeq ($(SNPE_WRAPPER_COMPILATION),true)
PRODUCT_PACKAGES += libSNPE
endif