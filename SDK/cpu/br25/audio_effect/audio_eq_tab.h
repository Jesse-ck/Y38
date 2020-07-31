#ifndef _AUDIO_EQ_TAB_H_
#define _AUDIO_EQ_TAB_H_

#include "system/includes.h"
#include "app_config.h"
#include "audio_eq.h"

#if (TCFG_EQ_ENABLE == 1)

static const struct eq_seg_info eq_tab_normal[EQ_SECTION_MAX] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,    0 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,    0 << 20, (int)(0.7f * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,   0 << 20, (int)(0.7f * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,   0 << 20, (int)(0.7f * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   0 << 20, (int)(0.7f * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  0 << 20, (int)(0.7f * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  0 << 20, (int)(0.7f * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,  0 << 20, (int)(0.7f * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,  0 << 20, (int)(0.7f * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f * (1 << 24))},


};

static const struct eq_seg_info eq_tab_rock[EQ_SECTION_MAX] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,    -2 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     0 << 20, (int)(0.7f * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    2 << 20, (int)(0.7f * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,    4 << 20, (int)(0.7f * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   -2 << 20, (int)(0.7f * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  -2 << 20, (int)(0.7f * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,   0 << 20, (int)(0.7f * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   0 << 20, (int)(0.7f * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   4 << 20, (int)(0.7f * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  4 << 20, (int)(0.7f * (1 << 24))},


};

static const struct eq_seg_info eq_tab_pop[EQ_SECTION_MAX] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,     3 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     1 << 20, (int)(0.7f * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    0 << 20, (int)(0.7f * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,   -2 << 20, (int)(0.7f * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   -4 << 20, (int)(0.7f * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  -4 << 20, (int)(0.7f * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  -2 << 20, (int)(0.7f * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   0 << 20, (int)(0.7f * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   1 << 20, (int)(0.7f * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  2 << 20, (int)(0.7f * (1 << 24))},


};

static const struct eq_seg_info eq_tab_classic[EQ_SECTION_MAX] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,     0 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     8 << 20, (int)(0.7f * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    8 << 20, (int)(0.7f * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,    4 << 20, (int)(0.7f * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,    0 << 20, (int)(0.7f * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,   0 << 20, (int)(0.7f * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,   0 << 20, (int)(0.7f * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   0 << 20, (int)(0.7f * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   2 << 20, (int)(0.7f * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  2 << 20, (int)(0.7f * (1 << 24))},

};

static const struct eq_seg_info eq_tab_country[EQ_SECTION_MAX] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,     -2 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     0 << 20, (int)(0.7f * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    0 << 20, (int)(0.7f * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,    2 << 20, (int)(0.7f * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,    2 << 20, (int)(0.7f * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,   0 << 20, (int)(0.7f * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,   0 << 20, (int)(0.7f * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   0 << 20, (int)(0.7f * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   4 << 20, (int)(0.7f * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  4 << 20, (int)(0.7f * (1 << 24))},

};

static const struct eq_seg_info eq_tab_jazz[EQ_SECTION_MAX] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,     0 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     0 << 20, (int)(0.7f * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    0 << 20, (int)(0.7f * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,    4 << 20, (int)(0.7f * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,    4 << 20, (int)(0.7f * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,   4 << 20, (int)(0.7f * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,   0 << 20, (int)(0.7f * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   2 << 20, (int)(0.7f * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   3 << 20, (int)(0.7f * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  4 << 20, (int)(0.7f * (1 << 24))},


};


static struct eq_seg_info eq_tab_custom[EQ_SECTION_MAX] = {
	{0, EQ_IIR_TYPE_BAND_PASS, 31,    0<<20, (int)(0.7f * (1<<24))},
	{1, EQ_IIR_TYPE_BAND_PASS, 62,    0<<20, (int)(0.7f * (1<<24))},
	{2, EQ_IIR_TYPE_BAND_PASS, 125,   0<<20, (int)(0.7f * (1<<24))},
	{3, EQ_IIR_TYPE_BAND_PASS, 250,   0<<20, (int)(0.7f * (1<<24))},
	{4, EQ_IIR_TYPE_BAND_PASS, 500,   0<<20, (int)(0.7f * (1<<24))},
	{5, EQ_IIR_TYPE_BAND_PASS, 1000,  0<<20, (int)(0.7f * (1<<24))},
	{6, EQ_IIR_TYPE_BAND_PASS, 2000,  0<<20, (int)(0.7f * (1<<24))},
	{7, EQ_IIR_TYPE_BAND_PASS, 4000,  0<<20, (int)(0.7f * (1<<24))},
	{8, EQ_IIR_TYPE_BAND_PASS, 8000,  0<<20, (int)(0.7f * (1<<24))},
	{9, EQ_IIR_TYPE_BAND_PASS, 16000, 0<<20, (int)(0.7f * (1<<24))},	

};


#endif /* (TCFG_EQ_ENABLE == 1) */

#if (TCFG_PHONE_EQ_ENABLE == 1)
/*upstream high-pass(200) for 8k*/
/* const int phone_eq_filt_08000[] = { */
/* 1863147, -837795, 937379, -2097152, 1048576, // seg 0 */
/* 1956339, -916484, 1048576, -1956339, 916484, // seg 1 */
/* 1831863, -812791, 1048576, -1831863, 812791, // seg 2 */
/* }; */
const int phone_eq_filt_08000[] = {
    // c2			// c1			// c4           // c0			// c3
    (937379 << 2), 	-(837795 << 2),	(1048576 << 2),	(1863147 << 2), 	-(2097152 << 2), 	// seg 0
    (1048576 << 2), 	-(916484 << 2),	(916484 << 2),	(1956339 << 2), 	-(1956339 << 2), 	// seg 1
    (1048576 << 2), 	-(812791 << 2),	(812791 << 2),	(1831863 << 2), 	-(1831863 << 2), 	// seg 2
};

/*upstream high-pass(200) for 16k*/
/* const int phone_eq_filt_16000[] = { */
/* 1979738, -937284, 991400, -2097152, 1048576, // seg 0 */
/* 2026633, -980309, 1048576, -2026633, 980309, // seg 1 */
/* 1963940, -923193, 1048576, -1963940, 923193, // seg 2 */
/* }; */
const int phone_eq_filt_16000[] = {
    // c2			// c1			// c4           // c0			// c3
    (991400 << 2), 	-(937284 << 2),	(1048576 << 2),	(1979738 << 2), 	-(2097152 << 2), 	// seg 0
    (1048576 << 2), 	-(980309 << 2),	(980309 << 2),	(2026633 << 2), 	-(2026633 << 2), 	// seg 1
    (1048576 << 2), 	-(923193 << 2),	(923193 << 2),	(1963940 << 2), 	-(1963940 << 2), 	// seg 2
};
#endif /* (TCFG_PHONE_EQ_ENABLE == 1) */

//////////////////////////= aec =//////////////////////////////////////
/*upstream high-pass(200) for 8k*/
/* const int aec_coeff_8k_highpass_ul[] = { */
/* 1863147, -837795, 937379, -2097152, 1048576, // seg 0 */
/* 1956339, -916484, 1048576, -1956339, 916484, // seg 1 */
/* 1831863, -812791, 1048576, -1831863, 812791, // seg 2 */
/* }; */
const int aec_coeff_8k_highpass_ul[] = {
    // c2			// c1			// c4           // c0			// c3
    (937379 << 2), 	-(837795 << 2),	(1048576 << 2),	(1863147 << 2), 	-(2097152 << 2), 	// seg 0
    (1048576 << 2), 	-(916484 << 2),	(916484 << 2),	(1956339 << 2), 	-(1956339 << 2), 	// seg 1
    (1048576 << 2), 	-(812791 << 2),	(812791 << 2),	(1831863 << 2), 	-(1831863 << 2), 	// seg 2
};

/*upstream high-pass(200) for 16k*/
/* const int aec_coeff_16k_highpass_ul[] = { */
/* 1979738, -937284, 991400, -2097152, 1048576, // seg 0 */
/* 2026633, -980309, 1048576, -2026633, 980309, // seg 1 */
/* 1963940, -923193, 1048576, -1963940, 923193, // seg 2 */
/* }; */
const int aec_coeff_16k_highpass_ul[] = {
    // c2			// c1			// c4           // c0			// c3
    (991400 << 2), 	-(937284 << 2),	(1048576 << 2),	(1979738 << 2), 	-(2097152 << 2), 	// seg 0
    (1048576 << 2), 	-(980309 << 2),	(980309 << 2),	(2026633 << 2), 	-(2026633 << 2), 	// seg 1
    (1048576 << 2), 	-(923193 << 2),	(923193 << 2),	(1963940 << 2), 	-(1963940 << 2), 	// seg 2
};

/*upstream high-pass(200) and high-shelf(4000,-8db) for 16k*/
/* const int aec_coeff_16k_hp_and_hs_ul[] = { */
/* 1979738, -937284, 991400, -2097152, 1048576, // seg 0 */
/* 258395, -93381, 661607, 258395, 93381, // seg 1 */
/* 1963940, -923193, 1048576, -1963940, 923193, // seg 2 */
/* }; */
const int aec_coeff_16k_hp_and_hs_ul[] = {
    // c2			// c1			// c4           // c0			// c3
    (991400 << 2), 	-(937284 << 2),	(1048576 << 2),	(1979738 << 2), 	-(2097152 << 2), 	// seg 0
    (661607 << 2), 	-(93381 << 2),	(93381  << 2),	(258395 << 2), (258395 << 2), 	// seg 1
    (1048576 << 2), 	-(923193 << 2),	(923193 << 2),	(1963940 << 2), 	-(1963940 << 2), 	// seg 2
};


#endif
