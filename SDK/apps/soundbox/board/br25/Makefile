LIBS = \
--start-group \
   $(AR_DIR)/cpu$(PROJECT_SUFFIX).a \
   $(AR_DIR)/system$(PROJECT_SUFFIX).a \
   $(AR_DIR)/btstack$(PROJECT_SUFFIX).a \
   $(AR_DIR)/btctrler$(PROJECT_SUFFIX).a \
   $(AR_DIR)/libepmotion.a \
   $(AR_DIR)/wtg_dec_lib.a \
   $(AR_DIR)/sbc_eng_lib.a \
   $(AR_DIR)/mp3tsy_dec_lib.a \
   $(AR_DIR)/wtg_dec_lib.a \
   $(AR_DIR)/flac_dec_lib.a \
   $(AR_DIR)/ape_dec_lib.a \
   $(AR_DIR)/m4a_dec_lib.a \
   $(AR_DIR)/amr_dec_lib.a \
   $(AR_DIR)/dts_dec_lib.a \
   $(AR_DIR)/mp3_decstream_lib.a \
   $(AR_DIR)/wma_decstream_lib.a \
   $(AR_DIR)/crypto_toolbox_Osize.a \
   $(AR_DIR)/libAptFilt_pi32v2_OnChip.a \
   $(AR_DIR)/libEchoSuppress_pi32v2_OnChip.a \
   $(AR_DIR)/libNoiseSuppress_pi32v2_OnChip.a \
   $(AR_DIR)/libSplittingFilter_pi32v2_OnChip.a \
   $(AR_DIR)/libDelayEstimate_pi32v2_OnChip.a \
   $(AR_DIR)/compressor.a \
   $(AR_DIR)/crossover_coff.a \
   $(AR_DIR)/limiter.a \
   $(AR_DIR)/lib_pitchshifter.a \
   $(AR_DIR)/lib_reverb_cal.a \
   $(AR_DIR)/lib_mp2_code.a \
   $(AR_DIR)/lib_adpcm_code.a \
   $(AR_DIR)/mp3_enc_lib.a \
   $(AR_DIR)/opus_enc_lib.a \
   $(AR_DIR)/lib_rmono2stereo.a \
   $(AR_DIR)/limiter_noiseGate.a \
   $(AR_DIR)/howling.a \
   $(AR_DIR)/lib_resample_cal.a \
   $(AR_DIR)/JL_rcsp.a \
   $(AR_DIR)/mp3_dec_lib.a \
   $(AR_DIR)/wma_dec_lib.a \
   $(AR_DIR)/wav_dec_lib.a \
# $(AR_DIR)/libOpcore_maskrom_pi32v2_OnChip.a \


ifeq ($(CONFIG_ANS_V2),y)
LIBS += \
   $(AR_DIR)/media_v2$(PROJECT_SUFFIX).a \
   #$(AR_DIR)/libNoiseSuppress_pi32v2_OnChip_v2.a \

else
LIBS += \
   $(AR_DIR)/media$(PROJECT_SUFFIX).a \
   #$(AR_DIR)/libNoiseSuppress_pi32v2_OnChip.a \

endif

ifeq ($(BFILTERFUN_LIB_IN_MASKROM),n)
LIBS += \
   $(AR_DIR)/bfilterfun_lib.a
endif

ifeq ($(libFFT_pi32v2_OnChip_IN_MASKROM),n)
LIBS += \
   $(AR_DIR)/libFFT_pi32v2_OnChip.a
endif

ifeq ($(OTA_UPDATE),y)
LIBS += \
   $(AR_DIR)/update$(PROJECT_SUFFIX).a \

endif

LIBS += \
--end-group

objs += \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac696x_demo.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac6969d_demo.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac696x_lighter.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac696x_tws.o \
	  $(ROOT)/apps/$(APP_CASE)/board/$(CPU)/board_ac696x_tws_box.o \

