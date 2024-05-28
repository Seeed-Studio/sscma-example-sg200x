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
INCS-$(CONFIG_MODULE_IPC)			+= -I$(SRCTREE)/modules/ipc/include
INCS += $(INCS-y)

SUBDIRS := \
	$(SRCTREE)/modules \

all : $(SUBDIRS) $(TARGET)

$(SUBDIRS) :
	$(MAKE) -C $(SRCTREE)/modules $(MAKECMDGOALS)

install:
	@mkdir -p $(APP_INSTALL_DIR)

	@echo "INFO: installing bin..."
	$(eval OUT_DIR := $(APP_INSTALL_DIR)/mnt/system/usr/bin)
	@mkdir -p $(OUT_DIR)
	@cp -rf $(TARGET) $(OUT_DIR)/

	@echo "INFO: installing www..."
	$(eval OUT_DIR := $(APP_INSTALL_DIR)/mnt)
	@mkdir -p $(OUT_DIR)
	@cp -rf $(SRCTREE)/solutions/$(PROJECT)/www/ $(OUT_DIR)/

	@echo "INFO: installing ini..."
	$(eval OUT_DIR := $(APP_INSTALL_DIR)/mnt/data)
	@mkdir -p $(OUT_DIR)
	@cp -rf $(APP_RESOURCE_DIR)/parameter/fbm/$(CONFIG_INI_FILE) $(OUT_DIR)/
	ln -s $(CONFIG_INI_FILE) $(OUT_DIR)/param_config.ini

	@echo "INFO: installing PQ_BINs..."
	$(eval OUT_DIR := $(APP_INSTALL_DIR)/mnt/data)
	@mkdir -p $(OUT_DIR)
	@cp -rf $(APP_RESOURCE_DIR)/isp_tuning/$(CHIP_ARCH)/$(CONFIG_SENSOR_NAME)/$(CONFIG_PQ_BIN_SDR) $(OUT_DIR)/
	ln -s $(CONFIG_PQ_BIN_SDR) $(OUT_DIR)/cvi_sdr_bin
	$(eval OUT_DIR := $(APP_INSTALL_DIR)/mnt/cfg/param)
	@mkdir -p $(OUT_DIR)
	ln -s ../../data/cvi_sdr_bin $(OUT_DIR)/cvi_sdr_bin

	@echo "INFO: packing..."
	@tar -zcf $(TARGET_OUT_DIR)/$(PROJECT).tar.gz -C $(APP_INSTALL_DIR) .

include $(SRCTREE)/build/build.mk

.PHONY: $(SUBDIRS)