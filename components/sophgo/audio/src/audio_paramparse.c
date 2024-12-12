#include "audio_paramparse.h"

#if 0
int load_audio_param(const char *file, APP_PARAM_AUDIO_CFG_T *Auido)
{
    int enum_num = 0;
    int ret = 0;
    char tmp_section[32] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading audio config ------------------> start \n");

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "audio_config");

    Auido->astAudioCfg.enSamplerate = ini_getl(tmp_section, "sample_rate", 0, file);
    Auido->astAudioCfg.enReSamplerate = ini_getl(tmp_section, "resample_rate", 0, file);
    Auido->astAudioCfg.Cal_DB_Enable = ini_getl(tmp_section, "Cal_DB_Enable", 0, file);
    Auido->astAudioCfg.u32ChnCnt = ini_getl(tmp_section, "chn_cnt", 0, file);

    ini_gets(tmp_section, "sound_mode", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = Convert_StrName_to_EnumNum(str_name, audio_sound_mode, AUDIO_SOUND_MODE_BUTT, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][sound_mode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][sound_mode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Auido->astAudioCfg.enSoundmode = enum_num;
    }

    ini_gets(tmp_section, "bit_width", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = Convert_StrName_to_EnumNum(str_name, audio_bit_width, AUDIO_BIT_WIDTH_BUTT, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][bit_width] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][bit_width] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Auido->astAudioCfg.enBitwidth = enum_num;
    }

    ini_gets(tmp_section, "work_mode", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = Convert_StrName_to_EnumNum(str_name, aio_mode, AIO_MODE_BUTT, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][work_mode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][work_mode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Auido->astAudioCfg.enWorkmode = enum_num;
    }

    Auido->astAudioCfg.u32EXFlag      = ini_getl(tmp_section, "ex_flag", 0, file);
    Auido->astAudioCfg.u32FrmNum      = ini_getl(tmp_section, "frm_num", 0, file);
    Auido->astAudioCfg.u32PtNumPerFrm = ini_getl(tmp_section, "ptnum_per_frm", 0, file);
    Auido->astAudioCfg.u32ClkSel      = ini_getl(tmp_section, "clk_sel", 0, file);

    ini_gets(tmp_section, "i2s_type", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = Convert_StrName_to_EnumNum(str_name, aio_i2stype, AIO_I2STYPE_BUTT, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][i2s_type] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][i2s_type] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Auido->astAudioCfg.enI2sType = enum_num;
    }

    Auido->astAudioCfg.u32AiDevId = ini_getl(tmp_section, "ai_dev_id", 0, file);
    Auido->astAudioCfg.u32AiChn   = ini_getl(tmp_section, "ai_chn", 0, file);
    Auido->astAudioCfg.u32AoDevId = ini_getl(tmp_section, "ao_dev_id", 0, file);
    Auido->astAudioCfg.u32AoChn   = ini_getl(tmp_section, "ao_chn", 0, file);
    Auido->astAudioCfg.u32AeChn   = ini_getl(tmp_section, "ae_chn", 0, file);
    Auido->astAudioCfg.u32AdChn   = ini_getl(tmp_section, "ad_chn", 0, file);

    ini_gets(tmp_section, "en_type", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = Convert_StrName_to_EnumNum(str_name, payload_type, PT_BUTT, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][en_type] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][en_type] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Auido->astAudioCfg.enAencType = enum_num;
    }

    Auido->astAudioVol.iDacLVol = ini_getl(tmp_section, "daclvol", 0, file);
    Auido->astAudioVol.iDacRVol = ini_getl(tmp_section, "dacrvol", 0, file);
    Auido->astAudioVol.iAdcLVol = ini_getl(tmp_section, "adclvol", 0, file);
    Auido->astAudioVol.iAdcRVol = ini_getl(tmp_section, "adcrvol", 0, file);

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "audio_vqe_agc");

    Auido->astAudioVqe.bAiAgcEnable                   = ini_getl(tmp_section, "ai_bEnable", 0, file);
    Auido->astAudioVqe.mAiAgcCfg.para_agc_max_gain    = ini_getl(tmp_section, "ai_max_gain", 0, file);
    Auido->astAudioVqe.mAiAgcCfg.para_agc_target_high = ini_getl(tmp_section, "ai_target_high", 0, file);
    Auido->astAudioVqe.mAiAgcCfg.para_agc_target_low  = ini_getl(tmp_section, "ai_target_low", 0, file);

    Auido->astAudioVqe.bAoAgcEnable                   = ini_getl(tmp_section, "ao_bEnable", 0, file);
    Auido->astAudioVqe.mAoAgcCfg.para_agc_max_gain    = ini_getl(tmp_section, "ao_max_gain", 0, file);
    Auido->astAudioVqe.mAoAgcCfg.para_agc_target_high = ini_getl(tmp_section, "ao_target_high", 0, file);
    Auido->astAudioVqe.mAoAgcCfg.para_agc_target_low  = ini_getl(tmp_section, "ao_target_low", 0, file);

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "audio_vqe_anr");

    Auido->astAudioVqe.bAiAnrEnable                   = ini_getl(tmp_section, "ai_bEnable", 0, file);
    Auido->astAudioVqe.mAiAnrCfg.para_nr_snr_coeff    = ini_getl(tmp_section, "ai_snr_coeff", 0, file);

    Auido->astAudioVqe.bAoAnrEnable                   = ini_getl(tmp_section, "ao_bEnable", 0, file);
    Auido->astAudioVqe.mAoAnrCfg.para_nr_snr_coeff    = ini_getl(tmp_section, "ao_snr_coeff", 0, file);

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "audio_vqe_aec");

    Auido->astAudioVqe.bAiAecEnable                   = ini_getl(tmp_section, "ai_bEnable", 0, file);
    Auido->astAudioVqe.mAiAecCfg.para_aec_filter_len  = ini_getl(tmp_section, "ai_filter_len", 0, file);
    Auido->astAudioVqe.mAiAecCfg.para_aes_std_thrd    = ini_getl(tmp_section, "ai_std_thrd", 0, file);
    Auido->astAudioVqe.mAiAecCfg.para_aes_supp_coeff  = ini_getl(tmp_section, "ai_supp_coeff", 0, file);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading audio config ------------------> done \n\n");

    return CVI_SUCCESS;
}
#endif