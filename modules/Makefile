include $(SRCTREE)/build/base.mk

TOPTARGETS := all


module-$(CONFIG_MODULE_AI)						+= ai
module-$(CONFIG_MODULE_AUDIO)					+= audio
module-$(CONFIG_MODULE_MSG)						+= msg
module-$(CONFIG_MODULE_NETWORK)					+= network
module-$(CONFIG_MODULE_OSD)						+= osd
module-$(CONFIG_MODULE_OTA)						+= ota
module-$(CONFIG_MODULE_PARAMPARSE)				+= paramparse
module-$(CONFIG_MODULE_PERIPHERAL)				+= peripheral
module-$(CONFIG_MODULE_RECORD)					+= record
module-$(CONFIG_MODULE_RECORD)					+= file_recover
module-$(CONFIG_MODULE_RINGBUF)					+= ringbuffer
module-$(CONFIG_MODULE_VIDEO)					+= video
module-$(CONFIG_MODULE_COMMON)					+= common
module-$(CONFIG_MODULE_IPC)						+= ipc

SUBDIRS += $(module-y)

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

clean :

install :



.PHONY: $(TOPTARGETS) $(SUBDIRS)
