include $(SRCTREE)/build/base.mk
TARGET_DIR := $(TARGET_OUT_DIR)/lib
TARGET_A := $(TARGET_DIR)/libapp_ipc.a

SEXT	:= cpp

SDIR	+= $(CURDIR)/src

#DEPEND
INCS									+= -I$(CURDIR)/include
INCS-$(CONFIG_MODULE_COMMON)			+= -I$(SRCTREE)/modules/common/include
INCS-$(CONFIG_MODULE_VIDEO)				+= -I$(SRCTREE)/modules/video/include
INCS-$(CONFIG_MODULE_OTA)				+= -I$(SRCTREE)/modules/ota/include
INCS-$(CONFIG_MODULE_AI)				+= -I$(SRCTREE)/modules/ai/include
INCS-$(CONFIG_MODULE_OSD)				+= -I$(SRCTREE)/modules/osd/include
INCS-$(CONFIG_MODULE_AUDIO)				+= -I$(SRCTREE)/modules/audio/include
INCS-$(CONFIG_MODULE_IRCUT)				+= -I$(SRCTREE)/modules/peripheral/ircut/include
INCS-$(CONFIG_MODULE_GPIO)				+= -I$(SRCTREE)/modules/peripheral/gpio/include
INCS-$(CONFIG_MODULE_RECORD)			+= -I$(SRCTREE)/modules/record/include
INCS += $(INCS-y)

include $(SRCTREE)/build/build.mk