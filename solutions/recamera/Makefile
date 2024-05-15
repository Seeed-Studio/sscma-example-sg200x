include $(SRCTREE)/build/base.mk

TARGET = $(TARGET_OUT_DIR)/$(PROJECT)/$(PROJECT)
SEXT	:= c
SDIR	+= $(CURDIR)/src

MW_LIB := $(MW_PATH)/lib
MW_INC := $(MW_PATH)/include
MW_3RD_LIB := $(MW_PATH)/lib/3rd

INCS-y									+= -I$(CURDIR)/include
INCS-$(CONFIG_MODULE_COMMON)			+= -I$(SRCTREE)/modules/common/include
INCS-$(CONFIG_MODULE_OSD)				+= -I$(SRCTREE)/modules/osd/include
INCS-$(CONFIG_MODULE_PARAMPARSE)		+= -I$(SRCTREE)/modules/paramparse/include
INCS-$(CONFIG_MODULE_VIDEO)				+= -I$(SRCTREE)/modules/video/include
INCS-$(CONFIG_MODULE_AUDIO)				+= -I$(SRCTREE)/modules/audio/include
INCS-$(CONFIG_MODULE_AI)				+= -I$(SRCTREE)/modules/ai/include
INCS-$(CONFIG_MODULE_MSG)				+= -I$(SRCTREE)/modules/msg/include
INCS-$(CONFIG_MODULE_NETWORK)			+= -I$(SRCTREE)/modules/network/include
INCS-$(CONFIG_MODULE_IRCUT)				+= -I$(SRCTREE)/modules/peripheral/ircut/include
INCS-$(CONFIG_MODULE_GPIO)				+= -I$(SRCTREE)/modules/peripheral/gpio/include
INCS-$(CONFIG_MODULE_RECORD)			+= -I$(SRCTREE)/modules/record/include
INCS += $(INCS-y)

SUBDIRS := \
	$(SRCTREE)/modules \

all : $(SUBDIRS) $(TARGET)

$(SUBDIRS) :
	$(MAKE) -C $(SRCTREE)/modules $(MAKECMDGOALS)

install:
	@mkdir -p $(SRCTREE)/install
	@cp -rf $(TARGET) $(SRCTREE)/install
ifneq ($(findstring $(CHIP_ARCH), MARS CV181X), )
	@cp -f $(APP_RESOURCE_DIR)/ai_models/181x/mobiledetv2-pedestrian-d0-ls-384.cvimodel $(APP_INSTALL_DIR)
ifeq ($(IRFACE_ENABLE), 1)
	@cp -f $(APP_RESOURCE_DIR)/ai_models/181x/ir_recogition.cvimodel $(APP_INSTALL_DIR)
	@cp -f $(APP_RESOURCE_DIR)/ai_models/181x/liveness.cvimodel $(APP_INSTALL_DIR)
	@cp -f $(APP_RESOURCE_DIR)/ai_models/181x/scrfd_320_256.cvimodel $(APP_INSTALL_DIR)
endif
else ifneq ($(findstring $(CHIP_ARCH), PHOBOS CV180X), )
	@cp -f $(APP_RESOURCE_DIR)/ai_models/180x/mobiledetv2-pedestrian-d0-ls-384.cvimodel $(APP_INSTALL_DIR)
endif
	@cp -f $(APP_RESOURCE_DIR)/bitmap/tiger.bmp $(APP_INSTALL_DIR)
	@cp -rf $(APP_RESOURCE_DIR)/www $(APP_INSTALL_DIR)
ifneq ($(wildcard $(APP_RESOURCE_DIR)/parameter/sbm/$(CHIP)_$(word 1, $(subst _, ,$(BOARD)))_gc4653.ini),)
	@cp -f $(APP_RESOURCE_DIR)/parameter/sbm/$(CHIP)_$(word 1, $(subst _, ,$(BOARD)))_gc4653.ini $(APP_INSTALL_DIR)/param_config.ini
endif

include $(SRCTREE)/build/build.mk

.PHONY: $(SUBDIRS)