
includes += \
	-I$(ROOT)/apps/$(APP_CASE)/include/ \
	-I$(ROOT)/apps/common/ \
	-I$(ROOT)/apps/common/ble/include/ \
	-I$(ROOT)/apps/common/edr/include/ \
	-I$(ROOT)/apps/common/charge/include/ \
	-I$(ROOT)/apps/common/third_party_profile/include/tm_gma_protocol/ \
	-I$(ROOT)/apps/common/third_party_profile/include/dma_deal/ \
	-I$(ROOT)/apps/common/third_party_profile/include/common/ \
	-I$(ROOT)/apps/common/third_party_profile/include/JL_rcsp/ \
	-I$(ROOT)/apps/common/include/ \
	-I$(ROOT)/apps/common/key/ \
	-I$(ROOT)/apps/config/include/ \
	-I$(ROOT)/apps/common/ble/include/ \
	-I$(ROOT)/apps/$(APP_CASE)/board/$(CPU)/ \
	-I$(ROOT)/cpu/$(CPU)/ \
	-I$(ROOT)/cpu/$(CPU)/audio_common \
	-I$(ROOT)/cpu/$(CPU)/audio_dec \
	-I$(ROOT)/cpu/$(CPU)/audio_enc \
	-I$(ROOT)/cpu/$(CPU)/audio_effect \

# LIBS = \
--start-group \
   $(AR_DIR)/cpu.a \
   $(AR_DIR)/system.a \
--whole-archive \
   $(AR_DIR)/btstack.a \
--no-whole-archive \
   $(AR_DIR)/btctrler.a \
   $(AR_DIR)/media.a \
   $(AR_DIR)/aes_lib_pi32.a \
--end-group


#     $(AR_DIR)/libc.a \


objs += \
	$(ROOT)/apps/common/norflash/norflash.o \


objs += \
	$(ROOT)/apps/common/key/iokey.o \
	$(ROOT)/apps/common/key/irkey.o \
	$(ROOT)/apps/common/key/adkey.o \
	$(ROOT)/apps/common/key/touch_key.o \
	$(ROOT)/apps/common/key/ctmu_touch_key.o \
	$(ROOT)/apps/common/key/adkey_rtcvdd.o \
	$(ROOT)/apps/common/key/rdec_key.o \
	$(ROOT)/apps/common/key/slidekey.o \
	$(ROOT)/apps/common/key/key_driver.o \
	$(ROOT)/apps/common/debug_lite/debug_lite.o \
	$(ROOT)/apps/common/sine_make.o \
	$(ROOT)/apps/common/update.o \
	$(ROOT)/apps/common/uartPcmSender.o \
	$(ROOT)/apps/common/detection.o \
	$(ROOT)/apps/common/gSensor/da230.o \
    $(ROOT)/apps/common/gSensor/SC7A20.o \
	$(ROOT)/apps/common/gSensor/gSensor_manage.o \
	$(ROOT)/apps/common/ble/le_streamer_demo.o \
	$(ROOT)/apps/common/edr/spp_user.o \
	$(ROOT)/apps/common/edr/spp_trans_data.o \
	$(ROOT)/apps/common/ble/le_trans_data.o \
	$(ROOT)/apps/common/ble/le_rcsp_demo.o \
	$(ROOT)/apps/common/third_party_profile/tm_gma_protocol/tm_frame_mg/tm_frame_mg.o \
	$(ROOT)/apps/common/third_party_profile/tm_gma_protocol/tm_gma/tm_gma.o \
	$(ROOT)/apps/common/third_party_profile/tm_gma_protocol/tm_gma_app/gma.o \
	$(ROOT)/apps/common/third_party_profile/tm_gma_protocol/tm_gma_deal/tm_gma_deal.o \
	$(ROOT)/apps/common/third_party_profile/tm_gma_protocol/tm_gma_hw_driver/tm_gma_hw_driver.o \
	$(ROOT)/apps/common/third_party_profile/dma_deal/dma_deal.o \
	$(ROOT)/apps/common/third_party_profile/common/3th_profile_api.o \
	$(ROOT)/apps/common/third_party_profile/common/custom_cfg.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp/rcsp_bluetooth.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp/rcsp_bs_api.o \
	$(ROOT)/apps/common/audio_digital_vol.o \
	$(ROOT)/apps/common/charge/app_charge.o \
	$(ROOT)/apps/common/charge/app_chargestore.o \
	$(ROOT)/apps/common/power_manage/app_power_manage.o \
	$(ROOT)/apps/$(APP_CASE)/app_main.o \
	$(ROOT)/apps/$(APP_CASE)/fs_test.o \
	$(ROOT)/apps/$(APP_CASE)/version.o \
	$(ROOT)/apps/$(APP_CASE)/idle.o \
	$(ROOT)/apps/$(APP_CASE)/user_cfg.o \
	$(ROOT)/apps/$(APP_CASE)/ui_manage.o \
	$(ROOT)/apps/$(APP_CASE)/app_action.o \
	$(ROOT)/apps/$(APP_CASE)/dev_manager.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/log_config/lib_btctrler_config.o \
	$(ROOT)/apps/$(APP_CASE)/log_config/lib_driver_config.o \
	$(ROOT)/apps/$(APP_CASE)/log_config/lib_system_config.o \
	$(ROOT)/apps/$(APP_CASE)/log_config/lib_update_config.o \
	$(ROOT)/apps/$(APP_CASE)/log_config/app_config.o \


-include $(ROOT)/apps/common/usb/Makefile.inc

#------------- EQ config-----------------#
# objs += \
	$(ROOT)/apps/$(APP_CASE)/app_eq.o \

include $(ROOT)/apps/$(APP_CASE)/bt/key/Makefile
include $(ROOT)/apps/$(APP_CASE)/music/key/Makefile
include $(ROOT)/apps/$(APP_CASE)/fm/key/Makefile
include $(ROOT)/apps/$(APP_CASE)/pc/key/Makefile
include $(ROOT)/apps/$(APP_CASE)/linein/key/Makefile
include $(ROOT)/apps/$(APP_CASE)/rtc/key/Makefile
include $(ROOT)/apps/$(APP_CASE)/spdif/key/Makefile
include $(ROOT)/apps/$(APP_CASE)/record/key/Makefile
include $(ROOT)/apps/$(APP_CASE)/common/key/Makefile

objs += \
	$(ROOT)/apps/$(APP_CASE)/common/app_common.o \
	$(ROOT)/apps/$(APP_CASE)/common/app_common_key.o \
	$(ROOT)/apps/$(APP_CASE)/common/power_on.o \
	$(ROOT)/apps/$(APP_CASE)/common/power_off.o \
	$(ROOT)/apps/$(APP_CASE)/common/fm_emitter_led7_ui.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/app_api/vm_api.o \
	$(ROOT)/apps/$(APP_CASE)/app_api/app_debug_api.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/bt/bt.o \
	$(ROOT)/apps/$(APP_CASE)/bt/bt_ble.o \
	$(ROOT)/apps/$(APP_CASE)/bt/bt_decode.o \
	$(ROOT)/apps/$(APP_CASE)/bt/bt_key.o \
	$(ROOT)/apps/$(APP_CASE)/bt/bt_tws.o \
	$(ROOT)/apps/$(APP_CASE)/bt/key_event_deal.o \
	$(ROOT)/apps/$(APP_CASE)/bt/media_sync.o \
	$(ROOT)/apps/$(APP_CASE)/bt/vol_sync.o \
	$(ROOT)/apps/$(APP_CASE)/bt/bt_led7_ui.o \
	$(ROOT)/apps/$(APP_CASE)/bt/bt_emitter.o \

objs += \
	$(ROOT)/apps/config/app_config.o \
	$(ROOT)/apps/config/ci_transport_uart.o \
	$(ROOT)/apps/config/bt_profile_config.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/music/music.o \
	$(ROOT)/apps/$(APP_CASE)/music/music_player.o \
	$(ROOT)/apps/$(APP_CASE)/music/music_player_api.o \
	$(ROOT)/apps/$(APP_CASE)/music/music_key.o \
	$(ROOT)/apps/$(APP_CASE)/music/music_led7_ui.o \
	$(ROOT)/apps/common/storage_dev/storage_dev.o \
	$(ROOT)/apps/common/storage_dev/dev_preprocessor.o \
	$(ROOT)/apps/common/file_operate/file_operate.o \
	$(ROOT)/apps/common/file_operate/file_bs_deal.o \
	$(ROOT)/apps/common/file_operate/file_api.o \
	$(ROOT)/apps/common/music/music_decrypt.o \
	$(ROOT)/apps/common/music/music_id3.o \
	$(ROOT)/apps/common/encode/encode_write_file.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/linein/linein_dev.o \
	$(ROOT)/apps/$(APP_CASE)/linein/linein.o \
	$(ROOT)/apps/$(APP_CASE)/linein/linein_key.o \
	$(ROOT)/apps/$(APP_CASE)/linein/linein_led7_ui.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/fm/fm.o \
	$(ROOT)/apps/$(APP_CASE)/fm/fm_key.o \
	$(ROOT)/apps/$(APP_CASE)/fm/fm_rw.o \
	$(ROOT)/apps/$(APP_CASE)/fm/fm_led7_ui.o \
	$(ROOT)/apps/$(APP_CASE)/fm/fm_server.o \
	$(ROOT)/apps/common/fm/fm_inside/fm_inside.o \
	$(ROOT)/apps/common/fm/bk1080/Bk1080.o \
	$(ROOT)/apps/common/fm/qn8035/QN8035.o \
	$(ROOT)/apps/common/fm/rda5807/RDA5807.o \
	$(ROOT)/apps/common/fm/fm_manage.o \

objs += \
	$(ROOT)/apps/common/fm_emitter/fm_inside/fm_emitter_inside.o \
	$(ROOT)/apps/common/fm_emitter/ac3433/ac3433.o \
	$(ROOT)/apps/common/fm_emitter/qn8007/qn8007.o \
	$(ROOT)/apps/common/fm_emitter/qn8027/qn8027.o \
	$(ROOT)/apps/common/fm_emitter/fm_emitter_manage.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/pc/usb_audio.o \
	$(ROOT)/apps/$(APP_CASE)/pc/pc.o \
	$(ROOT)/apps/$(APP_CASE)/pc/pc_key.o \
	$(ROOT)/apps/$(APP_CASE)/pc/pc_led7_ui.o \
	$(ROOT)/apps/$(APP_CASE)/pc/key/ac696x_demo_pc_key.o \
	$(ROOT)/apps/$(APP_CASE)/pc/key/ac6969d_demo_pc_key.o \

objs += \
	$(ROOT)/apps/common/ui/ui_api.o \
	$(ROOT)/apps/common/ui/ui_server.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/rtc/rtc.o \
	$(ROOT)/apps/$(APP_CASE)/rtc/rtc_key.o \
	$(ROOT)/apps/$(APP_CASE)/rtc/alarm.o \
	$(ROOT)/apps/$(APP_CASE)/rtc/rtc_led7_ui.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/record/record.o \
	$(ROOT)/apps/$(APP_CASE)/record/record_key.o \
	$(ROOT)/apps/$(APP_CASE)/record/record_led7_ui.o \

objs += \
	$(ROOT)/apps/$(APP_CASE)/spdif/spdif.o \
	$(ROOT)/apps/$(APP_CASE)/spdif/spdif_key.o \
	$(ROOT)/apps/$(APP_CASE)/spdif/hdmi_cec_drv.o \

objs += \
    $(ROOT)/apps/$(APP_CASE)/charge_box/chargeIc_manage.o \
    $(ROOT)/apps/$(APP_CASE)/charge_box/ic_self.o \
    $(ROOT)/apps/$(APP_CASE)/charge_box/charge_box.o \
    $(ROOT)/apps/$(APP_CASE)/charge_box/charge_ctrl.o \
    $(ROOT)/apps/$(APP_CASE)/charge_box/charge_det.o \
    $(ROOT)/apps/$(APP_CASE)/charge_box/wireless.o \
    $(ROOT)/apps/$(APP_CASE)/charge_box/box.o \
    $(ROOT)/apps/$(APP_CASE)/charge_box/box_key.o \
    $(ROOT)/apps/$(APP_CASE)/charge_box/charge_box_ui.o \

includes += \
	-I$(ROOT)/apps/common/third_party_profile/include/JL_rcsp_adv/ \
	-I$(ROOT)/apps/common/third_party_profile/include/JL_rcsp_adv/rcsp_adv_setting/ \

objs += \
	$(ROOT)/apps/common/ble/le_rcsp_adv_module.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_bluetooth.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_user_update.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_setting/adv_bt_name_setting.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_setting/adv_key_setting.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_setting/adv_led_setting.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_setting/adv_mic_setting.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_setting/rcsp_adv_tws_sync.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_setting/adv_time_stamp_setting.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_setting/adv_work_setting.o \
	$(ROOT)/apps/common/third_party_profile/JL_rcsp_adv/rcsp_adv_setting/rcsp_adv_opt.o \


