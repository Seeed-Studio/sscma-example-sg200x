include $(SRCTREE)/build/base.mk

TARGET   = $(TARGET_OUT_DIR)/$(PROJECT)/$(PROJECT)
SEXT	:= c
SDIR	+= $(CURDIR)/src

MW_LIB  := $(MW_PATH)/lib
MW_INC  := $(MW_PATH)/include
MW_3RD_LIB := $(MW_PATH)/lib/3rd

INCS-y								+= -I$(CURDIR)/include
INCS-$(CONFIG_MODULE_COMMON)		+= -I$(SRCTREE)/modules/common/include
INCS-$(CONFIG_MODULE_OSD)			+= -I$(SRCTREE)/modules/osd/include
INCS-$(CONFIG_MODULE_PARAMPARSE)	+= -I$(SRCTREE)/modules/paramparse/include
INCS-$(CONFIG_MODULE_VIDEO)			+= -I$(SRCTREE)/modules/video/include
INCS-$(CONFIG_MODULE_AUDIO)			+= -I$(SRCTREE)/modules/audio/include
INCS-$(CONFIG_MODULE_AI)			+= -I$(SRCTREE)/modules/ai/include
INCS-$(CONFIG_MODULE_MSG)			+= -I$(SRCTREE)/modules/msg/include
INCS-$(CONFIG_MODULE_NETWORK)		+= -I$(SRCTREE)/modules/network/include
INCS-$(CONFIG_MODULE_IRCUT)			+= -I$(SRCTREE)/modules/peripheral/ircut/include
INCS-$(CONFIG_MODULE_GPIO)			+= -I$(SRCTREE)/modules/peripheral/gpio/include
INCS-$(CONFIG_MODULE_RECORD)		+= -I$(SRCTREE)/modules/record/include
INCS += $(INCS-y)

SUBDIRS := \
	$(SRCTREE)/modules \

all : $(SUBDIRS) $(TARGET)

$(SUBDIRS) :
	$(MAKE) -C $(SRCTREE)/modules $(MAKECMDGOALS)

install:
	@mkdir -p $(APP_INSTALL_DIR)
	@cp -rf $(TARGET) $(APP_INSTALL_DIR)
	@cp -rf $(APP_RESOURCE_DIR)/www $(APP_INSTALL_DIR)
	@cp -rf $(APP_RESOURCE_DIR)/parameter/fbm/$(CONFIG_INI_FILE) $(APP_INSTALL_DIR)/

include $(SRCTREE)/build/build.mk

.PHONY: $(SUBDIRS)