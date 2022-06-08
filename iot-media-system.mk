# QMedia Android Application Package
PRODUCT_PACKAGES += QMedia
# umd-daemon application to access camera and audio frames
# and pass it on to UMD-Gadget module
PRODUCT_PACKAGES += umd-daemon

# AI Director test & SNPE Lib
# AI_DIRECTOR := true
ifeq ($(AI_DIRECTOR), true)
PRODUCT_PACKAGES += ai_director_test
endif

# SNPE wrapper library
PRODUCT_PACKAGES += libai_snpe_wrapper

# SNPE libraries
ifeq ($(AI_DIRECTOR),true)
PRODUCT_PACKAGES += libSNPE
endif