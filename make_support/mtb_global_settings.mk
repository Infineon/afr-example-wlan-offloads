
# Resolve toolchain name and add linker flag for Freertos OpenOCD debug
ifeq ($(TOOLCHAIN),GCC_ARM)

CY_AFR_TOOLCHAIN=GCC
CY_AFR_TOOLCHAIN_LS_EXT=ld
LDFLAGS+=-Wl,--undefined=uxTopUsedPriority

else ifeq ($(TOOLCHAIN),IAR)

CY_AFR_TOOLCHAIN=$(TOOLCHAIN)
CY_AFR_TOOLCHAIN_LS_EXT=icf
LDFLAGS+=--keep uxTopUsedPriority

else ifeq ($(TOOLCHAIN),ARM)

CY_AFR_TOOLCHAIN=GCC
CY_AFR_TOOLCHAIN_LS_EXT=sct
LDFLAGS+=--undefined=uxTopUsedPriority

endif #GCC_ARM

# Explicitly set the linker script
LINKER_SCRIPT=$(wildcard $(CY_AFR_BOARD_APP_PATH)/COMPONENT_$(CORE)/TOOLCHAIN_$(TOOLCHAIN)/*.$(CY_AFR_TOOLCHAIN_LS_EXT))

ifeq ($(TOOLCHAIN),IAR)
CFLAGS+=--dlib_config=full
CXXFLAGS+=--dlib_config=full --guard_calls
LDFLAGS+=--threaded_lib
DEFINES+=LWIP_ERRNO_INCLUDE=\"cy_lwip_errno_iar.h\"
SOURCES+=$(CY_AFR_ROOT)/freertos_kernel/portable/IAR/ARM_CM4F/portasm.s
endif

ifeq ($(TOOLCHAIN),ARM)
DEFINES+=LWIP_ERRNO_INCLUDE=\"cy_lwip_errno_armcc.h\"
# As of ARM Compiler 6.12, <assert.h> does not define static_assert (ISO/IEC 9899:2011).
# This define may interfere with <cassert> or future versions of the ARM C library.
DEFINES+=static_assert=_Static_assert
endif

ifeq ($(CY_AFR_BUILD), aws_tests)
CY_AFR_IS_TESTING=1
else
CY_AFR_IS_TESTING=0
endif
