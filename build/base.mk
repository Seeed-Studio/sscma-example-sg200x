#please set project name
include $(SRCTREE)/build/config.mk


AS              = $(CROSS_COMPILE)as
LD              = $(CROSS_COMPILE)ld
CC              = $(CROSS_COMPILE)gcc
CXX             = $(CROSS_COMPILE)g++
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump
ARFLAGS         = rcs
LDFLAGS_SO      = -shared -fPIC



TARGETFLAGS += -L$(TARGET_OUT_DIR)/lib/

TARGET_LIB-$(CONFIG_MODULE_AUDIO)                       += -lapp_audio
TARGET_LIB-$(CONFIG_MODULE_MSG)                         += -lapp_msg
TARGET_LIB-$(CONFIG_MODULE_NETWORK)                     += -lapp_network
TARGET_LIB-$(CONFIG_MODULE_IPC)                     	+= -lapp_ipc


TARGET_LIB-$(CONFIG_MODULE_OSD)                         += -lapp_osd
TARGET_LIB-$(CONFIG_MODULE_OTA)                         += -lapp_ota
TARGET_LIB-$(CONFIG_MODULE_GPIO)                        += -lapp_gpio
TARGET_LIB-$(CONFIG_MODULE_ADC)                         += -lapp_adc
TARGET_LIB-$(CONFIG_MODULE_IRCUT)                       += -lapp_ircut
TARGET_LIB-$(CONFIG_MODULE_PWM)                         += -lapp_pwm

TARGET_LIB-$(CONFIG_MODULE_AI_MD)                      	+= -lapp_ai_md
TARGET_LIB-$(CONFIG_MODULE_AI_PD)                      	+= -lapp_ai_pd
TARGET_LIB-$(CONFIG_MODULE_AI_FD_FACE)                	+= -lapp_ai_fd_cace
TARGET_LIB-$(CONFIG_MODULE_AI_IRFAECE)                  += -lapp_ai_irface
TARGET_LIB-$(CONFIG_MODULE_AI_BABYCRY)                  += -lapp_ai_babycry

TARGET_LIB-$(CONFIG_MODULE_PARAMPARSE)                  += -lapp_paramparse
TARGET_LIB-$(CONFIG_MODULE_VIDEO)                       += -lapp_video
TARGET_LIB-$(CONFIG_MODULE_RECORD)                      += -lapp_recorder
TARGET_LIB-$(CONFIG_MODULE_RECORD)                		+= -lapp_file_recover
TARGET_LIB-$(CONFIG_MODULE_RINGBUF)                     += -lapp_ringbuffer
TARGET_LIB-$(CONFIG_MODULE_COMMON)                      += -lapp_common

TARGET_LIB += $(TARGET_LIB-y)

