
#这里必须要指定 CROSS_COMPILE
#指定CLFAGS
#指定CXXFLAGS
#指定交叉编译链
#Makefile.param 里面指定交叉编译
include $(BUILD_PATH)/.config
#获取sensor配置
include $(SRCTREE)/build/sensor.mk
#获取默认配置
include $(SRCTREE)/build/module_config/$(PROJECT).mk

APP_MODULE_PATH  := $(SRCTREE)/modules
APP_PREBUILT_DIR := $(SRCTREE)/prebuilt
APP_RESOURCE_DIR := $(SRCTREE)/resource
APP_INSTALL_DIR  := $(TARGET_OUT_DIR)/install

MW_PATH := $(TOP_DIR)/middleware/$(MW_VER)

CHIP_ARCH_LOWER := $(shell echo $(CHIP_ARCH) | tr A-Z a-z)
ISP_INC := $(MW_PATH)/modules/isp/include/$(CHIP_ARCH_LOWER)

ifeq ($(KERNEL_DIR), )
KERNEL_PATH ?= $(ROOT_DIR)/$(KERNEL_SRC)
ifeq ($(PROJECT_FULLNAME), )
  $(error PROJECT_FULLNAME not defined! Please check!)
else
  KERNEL_DIR = $(KERNEL_PATH)/build/$(PROJECT_FULLNAME)
endif
endif

ifeq ($(SDK_VER), glibc_riscv64)
  KERNEL_INC := $(KERNEL_DIR)/riscv/usr/include
else ifeq ($(SDK_VER), musl_riscv64)
  KERNEL_INC := $(KERNEL_DIR)/riscv/usr/include
else
  $(error UNKNOWN SDK VER - $(SDK_VER))
endif

ifeq ($(SDK_VER), 32bit)
CROSS_COMPILE ?= arm-linux-gnueabihf-
else ifeq ($(SDK_VER),uclibc)
CROSS_COMPILE ?= arm-cvitek-linux-uclibcgnueabihf-
else ifeq ($(SDK_VER),musl_riscv64)
CROSS_COMPILE ?= riscv64-unknown-linux-musl-
else ifeq ($(SDK_VER),glibc_riscv64)
CROSS_COMPILE ?= riscv64-unknown-linux-gnu-
else
CROSS_COMPILE ?= aarch64-linux-gnu-
endif

## Needed first find libs about network form the path of WEB_SOCKET_LIB_DIR
LIBS-$(CONFIG_MODULE_NETWORK) += -L$(WEB_SOCKET_LIB_DIR)

INCS-y += -I$(MW_PATH)/include -I$(ISP_INC) -I$(MW_PATH)/include/isp/$(CHIP_ARCH_LOWER)
LIBS-y += -L$(MW_PATH)/lib -lcvi_bin -lcvi_bin_isp -lvpu -lvenc -lvdec -losdc -lsns_full -lisp -lawb -lae -laf -lisp_algo -lsys 
LIBS-$(CONFIG_PROJECT_SUPPORT_ATOMIC) += -latomic
#for dual_os
LIBS-$(CONFIG_MODULE_MSG) += -lmsg -lcvilink
LIBS-y += -L$(MW_PATH)/lib/3rd

#AUDIO
DEFS-$(CONFIG_MODULE_AUDIO) += -DAUDIO_SUPPORT
ifeq ($(CONFIG_PROJECT_STATIC), y)
DEFS-$(CONFIG_MODULE_AUDIO) += -DCVIAUDIO_STATIC
endif
LIBS-$(CONFIG_MODULE_AUDIO) += -lcvi_audio -ltinyalsa -lcvi_vqe -lcvi_ssp -lcvi_RES1 -lcvi_VoiceEngine -ldnvqe 
LIBS-$(CONFIG_MODULE_AUDIO) += -laacdec2 -laacenc2 -laacsbrdec2 -laacsbrenc2 -laaccomm2
LIBS-$(CONFIG_MODULE_AUDIO) += -lcli
DEFS-$(CONFIG_MODULE_AUDIO_MP3) += -DMP3_SUPPORT
LIBS-$(CONFIG_MODULE_AUDIO_MP3) += -lcvi_mp3 -lmad

#AI
ifeq ($(SDK_VER), musl_riscv64)
LIBS-$(CONFIG_MODULE_AI) += -L$(APP_PREBUILT_DIR)/jpegturbo/musl_lib
else ifeq ($(SDK_VER), glibc_riscv64)
LIBS-$(CONFIG_MODULE_AI) += -L$(APP_PREBUILT_DIR)/jpegturbo/glibc_lib
else
$(error "SDK_VER = $(SDK_VER) not match??")
endif

INCS-$(CONFIG_MODULE_AI) += -I$(APP_PREBUILT_DIR)/jpegturbo/include
DEFS-$(CONFIG_MODULE_AI) += -DAI_SUPPORT
INCS-$(CONFIG_MODULE_AI) += -I$(MW_PATH)/include/cviai
INCS-$(CONFIG_MODULE_AI) += -I$(OUTPUT_DIR)/tpu_$(SDK_VER)/cvitek_ai_sdk/include/cviai
JPEG-TUBRO = -lturbojpeg
LIBS-$(CONFIG_MODULE_AI) += -L$(OUTPUT_DIR)/tpu_$(SDK_VER)/cvitek_ai_sdk/lib/
LIBS-$(CONFIG_MODULE_AI) += -L$(OUTPUT_DIR)/tpu_$(SDK_VER)/cvitek_tpu_sdk/lib/

ifeq ($(CONFIG_PROJECT_STATIC), y) # AI libs static link
AISDK = -lcviai-static
ifeq ($(CHIP_ARCH), CV180X)
  LIBS-$(CONFIG_MODULE_AI) += -L$(OUTPUT_DIR)/tpu_$(SDK_VER)/cvitek_ive_sdk/lib/
  IVE = -lcvi_ive_tpu-static
else
  #INCS-$(CONFIG_MODULE_AI) += -I$(OUTPUT_DIR)/tpu_$(SDK_VER)/cvitek_tpu_sdk/include
  DEFS-$(CONFIG_MODULE_AI_FD_FACE) += -DFACE_SUPPORT
  AISDK += -lcviai_app-static
  IVE = -lcvi_ive
  OPENCV = -lopencv_core -lopencv_imgproc
endif
TPU = -lcvikernel-static -lcviruntime-static -lcnpy -lcvimath-static -lz
LIBS-$(CONFIG_MODULE_AI) += -Wl,--start-group $(AISDK) $(IVE) $(TPU) $(OPENCV) $(JPEG-TUBRO) -Wl,--end-group
else
AISDK = -lcviai -lcviai_app
JPEG-TUBRO = -lturbojpeg
IVE = -lcvi_ive
TPU = -lcnpy  -lcvikernel  -lcvimath  -lcviruntime  -lz
LIBS-$(CONFIG_MODULE_AI) += $(AISDK) $(TPU) $(IVE) $(JPEG-TUBRO)
endif

#AI_IR_FACE
DEFS-$(CONFIG_MODULE_AI_IRFAECE) += -DIR_FACE_SUPPORT

#AI_BABY_CRY
DEFS-$(CONFIG_MODULE_AI_BABYCRY) += -DAI_BABYCRY_SUPPORT

#RTSP
ifeq ($(SDK_VER), musl_riscv64)
RTSP_LIB_DIR = $(APP_PREBUILT_DIR)/rtsp/musl_riscv64
else ifeq ($(SDK_VER), glibc_riscv64)
RTSP_LIB_DIR = $(APP_PREBUILT_DIR)/rtsp/glibc_riscv64
else
$(error "SDK_VER = $(SDK_VER) not match??")
endif

INCS-$(CONFIG_MODULE_RTSP) += -I$(RTSP_LIB_DIR)/include
ifeq ($(CONFIG_PROJECT_STATIC), y)
LIBS-$(CONFIG_MODULE_RTSP) += -L$(RTSP_LIB_DIR)/lib -Wl,--start-group -lcvi_rtsp -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment -Wl,--end-group
else
LIBS-$(CONFIG_MODULE_RTSP) += -L$(RTSP_LIB_DIR)/lib -lcvi_rtsp
endif

#PQTOOL
ifeq ($(CONFIG_PROJECT_STATIC), y)
LIBS-$(CONFIG_MODULE_PQTOOL) += -Wl,-Bdynamic -ldl -Wl,-Bstatic -lcvi_ispd2 -lsys -ljson-c -lvpu
else
LIBS-$(CONFIG_MODULE_PQTOOL) += -ldl -ljson-c
endif
DEFS-$(CONFIG_MODULE_PQTOOL) += -DSUPPORT_ISP_PQTOOL

#IRCUT
DEFS-$(CONFIG_MODULE_IRCUT) += -DIRCUT_SUPPORT

# MULTI_PROCESS_SUPPORT
LIBS-$(CONFIG_PROJECT_MULTI_PROCESS_SUPPORT) += -lnanomsg
DEFS-$(CONFIG_PROJECT_MULTI_PROCESS_SUPPORT) += -DRPC_MULTI_PROCESS

#NET
ifeq ($(SDK_VER), musl_riscv64)
WEB_SOCKET_LIB_DIR = $(APP_PREBUILT_DIR)/libwebsockets/libmusl_riscv64
THTTPD_LIB_DIR = $(APP_PREBUILT_DIR)/thttpd/libmusl_riscv64
else ifeq ($(SDK_VER), glibc_riscv64)
THTTPD_LIB_DIR = $(APP_PREBUILT_DIR)/thttpd/libglibc_riscv64
WEB_SOCKET_LIB_DIR = $(APP_PREBUILT_DIR)/libwebsockets/libglibc_riscv64
else
$(error "SDK_VER = $(SDK_VER) not match??")
endif

# RECORD
FFMPEG_LIB_DIR = $(APP_PREBUILT_DIR)/ffmpeg/musl_riscv
LIBS-$(CONFIG_MODULE_RECORD) += -L$(FFMPEG_LIB_DIR) -lavformat -lavcodec -lavutil -lswresample
DEFS-$(CONFIG_MODULE_RECORD) += -DRECORD_SUPPORT
INCS-$(CONFIG_MODULE_RECORD) += -I$(APP_PREBUILT_DIR)/ffmpeg/include

# websockets
INCS-$(CONFIG_MODULE_NETWORK) += -I$(APP_PREBUILT_DIR)/thttpd/include
LIBS-$(CONFIG_MODULE_NETWORK) += -L$(THTTPD_LIB_DIR) -lthttpd
INCS-$(CONFIG_MODULE_NETWORK) += -I$(APP_PREBUILT_DIR)/libwebsockets/include
INCS-$(CONFIG_MODULE_NETWORK) += -I$(APP_PREBUILT_DIR)/libwebsockets/include/libwebsockets
INCS-$(CONFIG_MODULE_NETWORK) += -I$(APP_PREBUILT_DIR)/libwebsockets/include/libwebsockets/protocols
INCS-$(CONFIG_MODULE_NETWORK) += -I$(APP_PREBUILT_DIR)/libwebsockets/include/libwebsockets/transports
LIBS-$(CONFIG_MODULE_NETWORK) += -L$(WEB_SOCKET_LIB_DIR) -lwebsockets
DEFS-$(CONFIG_MODULE_NETWORK) += -DWEB_SOCKET
# openssl
OPENSSL_LIB_DIR = $(APP_PREBUILT_DIR)/openssl/lib64bit
INCS-$(CONFIG_MODULE_NETWORK) += -I$(APP_PREBUILT_DIR)/openssl/include/openssl

# kernel include
INCS-$(CONFIG_PROJECT_SUPPORT_INCLUDE_KERNEL) += -I$(KERNEL_INC)

# OSD
DEFS-$(CONFIG_MODULE_OSD) += -DOSD_SUPPORT

#这里指定全局的GLOBAL CFLAGS
DEFS-y += $(KBUILD_DEFINES)
DEFS-y += -D_MIDDLEWARE_V2_
GDB_DEBUG = 1
ifeq ($(GDB_DEBUG), 1)
CFLAGS += -g -O0
endif

INCS += $(INCS-y)
TARGETFLAGS += $(INCS)
TARGETFLAGS += -Os -Wl,--gc-sections -rdynamic

ifeq ($(SDK_VER), musl_riscv64)
CFLAGS      += -MMD -Os -mcpu=c906fdv -march=rv64imafdcv0p7xthead -mcmodel=medany -mabi=lp64d
TARGETFLAGS += -mcpu=c906fdv -march=rv64imafdcv0p7xthead -mcmodel=medany -mabi=lp64d
else ifeq ($(SDK_VER), glibc_riscv64)
CFLAGS      += -MMD -Os -mcpu=c906fdv -march=rv64imafdcv0p7xthead -mcmodel=medany -mabi=lp64d
TARGETFLAGS += -mcpu=c906fdv -march=rv64imafdcv0p7xthead -mcmodel=medany -mabi=lp64d
else
$(error "SDK_VER = $(SDK_VER) not match??")
endif

CFLAGS += -DARCH_$(CHIP_ARCH)
ifeq ("$(CVIARCH)", "CV181X")
CFLAGS += -D__CV181X__
endif
ifeq ("$(CVIARCH)", "CV180X")
CFLAGS += -D__CV180X__
endif

CFLAGS += -std=gnu11 -g -Wall -Wextra -Werror -fPIC -ffunction-sections -fdata-sections -Wl,--gc-sections -mno-ldd
CFLAGS += $(DEFS-y)
CFLAGS += -Wno-unused-parameter

LIBS += $(LIBS-y)
ifeq ($(CONFIG_PROJECT_STATIC), y)
LIBS += -static
endif
