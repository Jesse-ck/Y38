#usb slave 
ifneq ($(APP_CASE),gamebox)
objs += \
	$(ROOT)/apps/common/usb/usb_config.o \
	$(ROOT)/apps/common/usb/device/descriptor.o \
	$(ROOT)/apps/common/usb/device/usb_device.o \
	$(ROOT)/apps/common/usb/device/user_setup.o \
	$(ROOT)/apps/common/usb/device/task_pc.o \

#mass storage
objs += \
	$(ROOT)/apps/common/usb/device/msd.o \
	$(ROOT)/apps/common/usb/device/msd_upgrade.o \

#hid
objs += \
	$(ROOT)/apps/common/usb/device/hid.o \

#audio
objs += \
	$(ROOT)/apps/common/usb/device/uac1.o \
	$(ROOT)/apps/common/usb/device/uac_stream.o \

endif

#usb host 
objs += \
	$(ROOT)/apps/common/usb/usb_host_config.o \
	$(ROOT)/apps/common/usb/host/usb_bulk_transfer.o \
	$(ROOT)/apps/common/usb/host/usb_ctrl_transfer.o \
	$(ROOT)/apps/common/usb/host/usb_host.o \

#udisk    
ifneq ($(APP_CASE),gamebox)
objs += \
	$(ROOT)/apps/common/usb/host/usb_storage.o
endif

#gamepad    
ifeq ($(APP_CASE),gamebox)
objs += \
	$(ROOT)/apps/common/usb/host/adb.o \
    $(ROOT)/apps/common/usb/host/aoa.o \
    $(ROOT)/apps/common/usb/host/hid.o \

endif

includes += \
	-I$(ROOT)/apps/common/usb/ \
	-I$(ROOT)/apps/common/usb/device \
	-I$(ROOT)/apps/common/usb/host \


