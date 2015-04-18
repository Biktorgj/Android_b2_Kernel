/* linux/drivers/video/exynos/s6e63j0x03_dimming.c
 *
 * MIPI-DSI based S6E63J0X03 AMOLED lcd 1.63 inch panel driver.
 *
 * YoungJun Cho, <yj44.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

#include "s6e63j0x03_dimming.h"

#define	MAX_CANDELA		300
#define	CANDELA_RESOLUTION	10

#define	MULTIPLE_VREGOUT	4301

#define	TBL_IDX_V0		0
#define	TBL_IDX_V5		5
#define	TBL_IDX_V15		15
#define	TBL_IDX_V31		31
#define	TBL_IDX_V63		63
#define	TBL_IDX_V127		127
#define	TBL_IDX_V191		191
#define	TBL_IDX_V255		255

struct v_constant {
	int	nu;
	int	de;
};

static const short vt_trans_volt[VMAX] = {
	4301,
	4137,
	3974,
	3810,
	3178,
	3107,
	3035,
	2971,
};

static const int vreg_element_max[VMAX] = {
	0x07,
	0x7f,
	0x7f,
	0x3f,
	0x3f,
	0x3f,
	0x3f,
	0x1ff,
};

static const short v5_trans_volt[128] = {
	0,	7,	14,	21,	28,	36,	43,	50,
	57,	64,	71,	78,	85,	92,	100,	107,
	114,	121,	128,	135,	142,	149,	156,	164,
	171,	178,	185,	192,	199,	206,	213,	220,
	228,	235,	242,	249,	256,	263,	270,	277,
	284,	292,	299,	306,	313,	320,	327,	334,
	341,	348,	356,	363,	370,	377,	384,	391,
	398,	405,	412,	420,	427,	434,	441,	448,
	455,	462,	469,	476,	484,	491,	498,	505,
	512,	519,	526,	533,	540,	548,	555,	562,
	569,	576,	583,	590,	597,	604,	612,	619,
	626,	633,	640,	647,	654,	661,	668,	676,
	683,	690,	697,	704,	711,	718,	725,	732,
	740,	747,	754,	761,	768,	775,	782,	789,
	796,	804,	811,	818,	825,	832,	839,	846,
	853,	860,	868,	875,	882,	889,	896,	903,
};

static const short v15_trans_volt[128] = {
	114,	121,	128,	135,	142,	149,	156,	164,
	171,	178,	185,	192,	199,	206,	213,	220,
	228,	235,	242,	249,	256,	263,	270,	277,
	284,	292,	299,	306,	313,	320,	327,	334,
	341,	348,	356,	363,	370,	377,	384,	391,
	398,	405,	412,	420,	427,	434,	441,	448,
	455,	462,	469,	476,	484,	491,	498,	505,
	512,	519,	526,	533,	540,	548,	555,	562,
	569,	576,	583,	590,	597,	604,	612,	619,
	626,	633,	640,	647,	654,	661,	668,	676,
	683,	690,	697,	704,	711,	718,	725,	732,
	740,	747,	754,	761,	768,	775,	782,	789,
	796,	804,	811,	818,	825,	832,	839,	846,
	853,	860,	868,	875,	882,	889,	896,	903,
	910,	917,	924,	932,	939,	946,	953,	960,
	967,	974,	981,	988,	996,	1003,	1010,	1017,
};

static const short v31_trans_volt[64] = {
	512,	520,	528,	536,	544,	552,	560,	568,
	576,	584,	592,	600,	608,	616,	624,	632,
	640,	648,	656,	664,	672,	680,	688,	696,
	704,	712,	720,	728,	736,	744,	752,	760,
	768,	776,	784,	792,	800,	808,	816,	824,
	832,	840,	848,	856,	864,	872,	880,	888,
	896,	904,	912,	920,	928,	936,	944,	952,
	960,	968,	976,	984,	992,	1000,	1008,	1016,
};

static const short v63_trans_volt[64] = {
	512,	520,	528,	536,	544,	552,	560,	568,
	576,	584,	592,	600,	608,	616,	624,	632,
	640,	648,	656,	664,	672,	680,	688,	696,
	704,	712,	720,	728,	736,	744,	752,	760,
	768,	776,	784,	792,	800,	808,	816,	824,
	832,	840,	848,	856,	864,	872,	880,	888,
	896,	904,	912,	920,	928,	936,	944,	952,
	960,	968,	976,	984,	992,	1000,	1008,	1016,
};

static const short v127_trans_volt[64] = {
	512,	520,	528,	536,	544,	552,	560,	568,
	576,	584,	592,	600,	608,	616,	624,	632,
	640,	648,	656,	664,	672,	680,	688,	696,
	704,	712,	720,	728,	736,	744,	752,	760,
	768,	776,	784,	792,	800,	808,	816,	824,
	832,	840,	848,	856,	864,	872,	880,	888,
	896,	904,	912,	920,	928,	936,	944,	952,
	960,	968,	976,	984,	992,	1000,	1008,	1016,
};

static const short v191_trans_volt[64] = {
	512,	520,	528,	536,	544,	552,	560,	568,
	576,	584,	592,	600,	608,	616,	624,	632,
	640,	648,	656,	664,	672,	680,	688,	696,
	704,	712,	720,	728,	736,	744,	752,	760,
	768,	776,	784,	792,	800,	808,	816,	824,
	832,	840,	848,	856,	864,	872,	880,	888,
	896,	904,	912,	920,	928,	936,	944,	952,
	960,	968,	976,	984,	992,	1000,	1008,	1016,
};

static const short v255_trans_volt[512] = {
	4301,	4294,	4287,	4279,	4272,	4265,	4258,	4251,
	4244,	4237,	4230,	4223,	4215,	4208,	4201,	4194,
	4187,	4180,	4173,	4166,	4159,	4152,	4144,	4137,
	4130,	4123,	4116,	4109,	4102,	4095,	4088,	4080,
	4073,	4066,	4059,	4052,	4045,	4038,	4031,	4024,
	4016,	4009,	4002,	3995,	3988,	3981,	3974,	3967,
	3960,	3952,	3945,	3938,	3931,	3924,	3917,	3910,
	3903,	3896,	3888,	3881,	3874,	3867,	3860,	3853,
	3846,	3839,	3832,	3825,	3817,	3810,	3803,	3796,
	3789,	3782,	3775,	3768,	3761,	3753,	3746,	3739,
	3732,	3725,	3718,	3711,	3704,	3697,	3689,	3682,
	3675,	3668,	3661,	3654,	3647,	3640,	3633,	3625,
	3618,	3611,	3604,	3597,	3590,	3583,	3576,	3569,
	3561,	3554,	3547,	3540,	3533,	3526,	3519,	3512,
	3505,	3498,	3490,	3483,	3476,	3469,	3462,	3455,
	3448,	3441,	3434,	3426,	3419,	3412,	3405,	3398,
	3391,	3384,	3377,	3370,	3362,	3355,	3348,	3341,
	3334,	3327,	3320,	3313,	3306,	3298,	3291,	3284,
	3277,	3270,	3263,	3256,	3249,	3242,	3234,	3227,
	3220,	3213,	3206,	3199,	3192,	3185,	3178,	3171,
	3163,	3156,	3149,	3142,	3135,	3128,	3121,	3114,
	3107,	3099,	3092,	3085,	3078,	3071,	3064,	3057,
	3050,	3043,	3035,	3028,	3021,	3014,	3007,	3000,
	2993,	2986,	2979,	2971,	2964,	2957,	2950,	2943,
	2936,	2929,	2922,	2915,	2907,	2900,	2893,	2886,
	2879,	2872,	2865,	2858,	2851,	2844,	2836,	2829,
	2822,	2815,	2808,	2801,	2794,	2787,	2780,	2772,
	2765,	2758,	2751,	2744,	2737,	2730,	2723,	2716,
	2708,	2701,	2694,	2687,	2680,	2673,	2666,	2659,
	2652,	2644,	2637,	2630,	2623,	2616,	2609,	2602,
	2595,	2588,	2580,	2573,	2566,	2559,	2552,	2545,
	2538,	2531,	2524,	2517,	2509,	2502,	2495,	2488,
	2481,	2474,	2467,	2460,	2453,	2445,	2438,	2431,
	2424,	2417,	2410,	2403,	2396,	2389,	2381,	2374,
	2367,	2360,	2353,	2346,	2339,	2332,	2325,	2317,
	2310,	2303,	2296,	2289,	2282,	2275,	2268,	2261,
	2253,	2246,	2239,	2232,	2225,	2218,	2211,	2204,
	2197,	2189,	2182,	2175,	2168,	2161,	2154,	2147,
	2140,	2133,	2126,	2118,	2111,	2104,	2097,	2090,
	2083,	2076,	2069,	2062,	2054,	2047,	2040,	2033,
	2026,	2019,	2012,	2005,	1998,	1990,	1983,	1976,
	1969,	1962,	1955,	1948,	1941,	1934,	1926,	1919,
	1912,	1905,	1898,	1891,	1884,	1877,	1870,	1862,
	1855,	1848,	1841,	1834,	1827,	1820,	1813,	1806,
	1799,	1791,	1784,	1777,	1770,	1763,	1756,	1749,
	1742,	1735,	1727,	1720,	1713,	1706,	1699,	1692,
	1685,	1678,	1671,	1663,	1656,	1649,	1642,	1635,
	1628,	1621,	1614,	1607,	1599,	1592,	1585,	1578,
	1571,	1564,	1557,	1550,	1543,	1535,	1528,	1521,
	1514,	1507,	1500,	1493,	1486,	1479,	1472,	1464,
	1457,	1450,	1443,	1436,	1429,	1422,	1415,	1408,
	1400,	1393,	1386,	1379,	1372,	1365,	1358,	1351,
	1344,	1336,	1329,	1322,	1315,	1308,	1301,	1294,
	1287,	1280,	1272,	1265,	1258,	1251,	1244,	1237,
	1230,	1223,	1216,	1208,	1201,	1194,	1187,	1180,
	1173,	1166,	1159,	1152,	1145,	1137,	1130,	1123,
	1116,	1109,	1102,	1095,	1088,	1081,	1073,	1066,
	1059,	1052,	1045,	1038,	1031,	1024,	1017,	1009,
	1002,	995,	988,	981,	974,	967,	960,	953,
	945,	938,	931,	924,	917,	910,	903,	896,
	889,	881,	874,	867,	860,	853,	846,	839,
	832,	825,	818,	810,	803,	796,	789,	782,
	775,	768,	761,	754,	746,	739,	732,	725,
	718,	711,	704,	697,	690,	682,	675,	668,
};

static const short int_tbl_v0_v5[4] = {
	205,	410,	614,	819,
};

static const short int_tbl_v5_v15[9] = {
	128,	256,	384,	512,	640,	717,	794,	870,
	947,
};

static const short int_tbl_v15_v31[15] = {
	64,	128,	192,	256,	320,	384,	448,	512,
	576,	640,	704,	768,	832,	896,	960,
};

static const short int_tbl_v31_v63[31] = {
	32,	64,	96,	128,	160,	192,	224,	256,
	288,	320,	352,	384,	416,	448,	480,	512,
	544,	576,	608,	640,	672,	704,	736,	768,
	800,	832,	864,	896,	928,	960,	992,
};

static const short int_tbl_v63_v127[63] = {
	16,	32,	48,	64,	80,	96,	112,	128,
	144,	160,	176,	192,	208,	224,	240,	256,
	272,	288,	304,	320,	336,	352,	368,	384,
	400,	416,	432,	448,	464,	480,	496,	512,
	528,	544,	560,	576,	592,	608,	624,	640,
	656,	672,	688,	704,	720,	736,	752,	768,
	784,	800,	816,	832,	848,	864,	880,	896,
	912,	928,	944,	960,	976,	992,	1008,
};

static const short int_tbl_v127_v191[63] = {
	16,	32,	48,	64,	80,	96,	112,	128,
	144,	160,	176,	192,	208,	224,	240,	256,
	272,	288,	304,	320,	336,	352,	368,	384,
	400,	416,	432,	448,	464,	480,	496,	512,
	528,	544,	560,	576,	592,	608,	624,	640,
	656,	672,	688,	704,	720,	736,	752,	768,
	784,	800,	816,	832,	848,	864,	880,	896,
	912,	928,	944,	960,	976,	992,	1008,
};

static const short int_tbl_v191_v255[63] = {
	16,	32,	48,	64,	80,	96,	112,	128,
	144,	160,	176,	192,	208,	224,	240,	256,
	272,	288,	304,	320,	336,	352,	368,	384,
	400,	416,	432,	448,	464,	480,	496,	512,
	528,	544,	560,	576,	592,	608,	624,	640,
	656,	672,	688,	704,	720,	736,	752,	768,
	784,	800,	816,	832,	848,	864,	880,	896,
	912,	928,	944,	960,	976,	992,	1008,
};

static const unsigned int vref_idx[VMAX] = {
	TBL_IDX_V0,
	TBL_IDX_V5,
	TBL_IDX_V15,
	TBL_IDX_V31,
	TBL_IDX_V63,
	TBL_IDX_V127,
	TBL_IDX_V191,
	TBL_IDX_V255,
};

static const struct v_constant fix_const[VMAX] = {
	{.nu = 0,	.de = 0},
	{.nu = 0,	.de = 144},
	{.nu = 16,	.de = 144},
	{.nu = 64,	.de = 128},
	{.nu = 64,	.de = 128},
	{.nu = 64,	.de = 128},
	{.nu = 64,	.de = 128},
	{.nu = 0,	.de = 605},
};

static const char ref_shift[MAX_CANDELA / CANDELA_RESOLUTION + 1][VMAX] = {
	{0, 0, 0, 0, 0, 0, 0, 0},	/*  0 cd */
	{0, 7, 6, 7, 2, 4, 5, 5},	/* 10 cd */
	{0, 7, 5, 5, 2, 3, 3, 4},	/* 20 cd */
	{0, 5, 4, 4, 2, 3, 3, 3},	/* 30 cd */
	{0, 5, 4, 3, 3, 3, 3, 2},	/* 40 cd */
	{0, 5, 3, 1, 3, 3, 3, 2},
	{0, 5, 3, 2, 2, 3, 4, 0},
	{0, 3, 3, 2, 3, 2, 3, 0},	/* 70 cd */
	{0, 3, 3, 2, 2, 3, 2, 1},
	{0, 3, 3, 2, 3, 3, 2, 0},	/* 90 cd */
	{0, 2, 3, 2, 4, 4, 3, 1},	/* 100 cd */
	{0, 2, 3, 2, 3, 4, 2, 1},
	{0, 2, 3, 3, 4, 5, 2, 2},
	{0, 2, 3, 3, 4, 4, 2, 2},	/* 130 cd */
	{0, 1, 2, 2, 3, 4, 2, 2},
	{0, 0, 2, 2, 4, 4, 3, 1},	/* 150 cd */
	{0, 0, 3, 3, 4, 4, 2, 1},
	{0, 0, 2, 3, 3, 4, 2, 1},
	{0, 0, 2, 2, 3, 3, 1, 1},	/* 180 cd */
	{0, 0, 2, 3, 3, 2, 1, 1},
	{0, 0, 2, 2, 3, 3, 2, 1},	/* 200 cd */
	{0, 0, 2, 3, 3, 3, 2, 1},
	{0, 0, 2, 3, 3, 4, 2, 1},
	{0, 0, 2, 3, 3, 3, 2, 1},
	{0, 0, 1, 3, 2, 2, 2, 1},
	{0, 0, 1, 2, 2, 2, 2, 1},
	{0, 0, 1, 3, 3, 3, 3, 1},
	{0, 0, 2, 3, 3, 2, 3, 2},
	{0, 0, 1, 3, 2, 2, 2, 1},
	{0, 0, 1, 2, 1, 2, 2, 1},
	{0, 0, 0, 0, 0, 0, 0, 0},
};

static const int gamma_tbl[256] = {
	0,	5,	24,	60,	112,	184,	274,	385,
	516,	669,	844,	1041,	1260,	1503,	1769,	2059,
	2373,	2711,	3075,	3463,	3877,	4316,	4781,	5272,
	5790,	6334,	6905,	7503,	8128,	8780,	9460,	10167,
	10903,	11667,	12458,	13279,	14128,	15006,	15912,	16848,
	17813,	18808,	19832,	20885,	21969,	23082,	24226,	25400,
	26604,	27838,	29104,	30399,	31726,	33084,	34473,	35893,
	37344,	38827,	40342,	41888,	43465,	45075,	46717,	48391,
	50097,	51835,	53605,	55408,	57244,	59113,	61014,	62948,
	64915,	66915,	68948,	71014,	73114,	75247,	77414,	79614,
	81848,	84116,	86418,	88753,	91123,	93526,	95964,	98436,
	100942,	103483,	106058,	108668,	111313,	113992,	116706,	119455,
	122239,	125057,	127911,	130800,	133725,	136684,	139679,	142710,
	145776,	148877,	152014,	155187,	158396,	161640,	164921,	168237,
	171590,	174978,	178403,	181864,	185361,	188895,	192465,	196072,
	199715,	203395,	207111,	210864,	214654,	218481,	222345,	226245,
	230183,	234158,	238170,	242219,	246306,	250429,	254590,	258789,
	263025,	267299,	271610,	275959,	280345,	284770,	289232,	293732,
	298270,	302846,	307459,	312111,	316802,	321530,	326296,	331101,
	335944,	340826,	345746,	350704,	355701,	360737,	365811,	370924,
	376076,	381266,	386496,	391764,	397071,	402417,	407802,	413226,
	418689,	424192,	429733,	435314,	440935,	446594,	452293,	458031,
	463809,	469627,	475484,	481380,	487316,	493292,	499308,	505364,
	511459,	517594,	523769,	529984,	536240,	542535,	548870,	555245,
	561661,	568117,	574613,	581149,	587726,	594343,	601001,	607699,
	614437,	621216,	628036,	634896,	641797,	648739,	655721,	662745,
	669809,	676914,	684060,	691246,	698474,	705743,	713053,	720404,
	727796,	735230,	742704,	750220,	757777,	765375,	773015,	780697,
	788419,	796183,	803989,	811836,	819725,	827655,	835628,	843641,
	851697,	859794,	867933,	876114,	884337,	892602,	900909,	909258,
	917648,	926081,	934556,	943073,	951632,	960234,	968877,	977563,
	986291,	995062,	1003875,	1012730,	1021628,
	1030568,	1039551,	1048576,
};

const int gamma_multi_tbl[256] = {
	0,	2,	7,	17,	33,	54,	80,	113,
	151,	196,	247,	305,	369,	440,	518,	603,
	695,	794,	901,	1015,	1136,	1264,	1401,	1545,
	1696,	1856,	2023,	2198,	2381,	2572,	2771,	2979,
	3194,	3418,	3650,	3890,	4139,	4396,	4662,	4936,
	5219,	5510,	5810,	6119,	6436,	6762,	7097,	7441,
	7794,	8156,	8526,	8906,	9295,	9693,	10099,	10516,
	10941,	11375,	11819,	12272,	12734,	13206,	13687,	14177,
	14677,	15186,	15705,	16233,	16771,	17318,	17875,	18442,
	19018,	19604,	20200,	20805,	21420,	22045,	22680,	23325,
	23979,	24643,	25318,	26002,	26696,	27400,	28114,	28839,
	29573,	30317,	31072,	31836,	32611,	33396,	34191,	34997,
	35812,	36638,	37474,	38320,	39177,	40044,	40922,	41809,
	42708,	43616,	44535,	45465,	46405,	47356,	48317,	49288,
	50270,	51263,	52267,	53280,	54305,	55340,	56386,	57443,
	58510,	59588,	60677,	61777,	62887,	64008,	65140,	66283,
	67436,	68601,	69776,	70963,	72160,	73368,	74587,	75817,
	77058,	78310,	79573,	80847,	82132,	83429,	84736,	86054,
	87384,	88724,	90076,	91439,	92813,	94198,	95595,	97002,
	98421,	99851,	101293,	102745,	104209,	105685,	107171,	108669,
	110178,	111699,	113231,	114775,	116329,	117896,	119473,	121062,
	122663,	124275,	125898,	127533,	129180,	130838,	132508,	134189,
	135882,	137586,	139302,	141029,	142768,	144519,	146282,	148056,
	149841,	151639,	153448,	155269,	157101,	158946,	160802,	162670,
	164549,	166440,	168344,	170259,	172185,	174124,	176074,	178037,
	180011,	181997,	183995,	186005,	188027,	190060,	192106,	194163,
	196233,	198315,	200408,	202514,	204631,	206761,	208902,	211056,
	213222,	215399,	217589,	219791,	222005,	224231,	226469,	228720,
	230982,	233257,	235544,	237843,	240154,	242477,	244813,	247161,
	249521,	251893,	254277,	256674,	259083,	261505,	263938,	266384,
	268842,	271313,	273796,	276291,	278799,	281318,	283851,	286395,
	288953,	291522,	294104,	296698,	299305,	301924,	304556,	307200,
};

const unsigned char lookup_tbl[MAX_CANDELA + 1] = {
	0,	19,	26,	31,	36,	40,	43,	46,
	49,	52,	54,	57,	59,	61,	63,	65,
	67,	69,	71,	73,	74,	76,	78,	79,
	81,	82,	84,	85,	87,	88,	90,	91,
	92,	93,	95,	96,	97,	98,	100,	101,
	102,	103,	104,	105,	107,	108,	109,	110,
	111,	112,	113,	114,	115,	116,	117,	118,
	119,	120,	121,	122,	123,	124,	125,	125,
	126,	127,	128,	129,	130,	131,	132,	132,
	133,	134,	135,	136,	137,	137,	138,	139,
	140,	141,	141,	142,	143,	144,	145,	145,
	146,	147,	148,	148,	149,	150,	150,	151,
	152,	153,	153,	154,	155,	155,	156,	157,
	158,	158,	159,	160,	160,	161,	162,	162,
	163,	164,	164,	165,	166,	166,	167,	167,
	168,	169,	169,	170,	171,	171,	172,	173,
	173,	174,	174,	175,	176,	176,	177,	177,
	178,	179,	179,	180,	180,	181,	182,	182,
	183,	183,	184,	184,	185,	186,	186,	187,
	187,	188,	188,	189,	189,	190,	191,	191,
	192,	192,	193,	193,	194,	194,	195,	195,
	196,	196,	197,	198,	198,	199,	199,	200,
	200,	201,	201,	202,	202,	203,	203,	204,
	204,	205,	205,	206,	206,	207,	207,	208,
	208,	209,	209,	210,	210,	211,	211,	212,
	212,	213,	213,	214,	214,	214,	215,	215,
	216,	216,	217,	217,	218,	218,	219,	219,
	220,	220,	221,	221,	221,	222,	222,	223,
	223,	224,	224,	225,	225,	226,	226,	226,
	227,	227,	228,	228,	229,	229,	230,	230,
	230,	231,	231,	232,	232,	233,	233,	233,
	234,	234,	235,	235,	236,	236,	236,	237,
	237,	238,	238,	239,	239,	239,	240,	240,
	241,	241,	241,	242,	242,	243,	243,	243,
	244,	244,	245,	245,	246,	246,	246,	247,
	247,	248,	248,	248,	249,	249,	250,	250,
	250,	251,	251,	251,	252,	252,	253,	253,
	253,	254,	254,	255,	255,
};

static const char color_shift_0[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_10[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	-6,	0,	3,
	-20,	-20,	-16,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	-1,	0
};

static const char color_shift_20[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	-20,	-6,	1,
	-19,	-16,	-10,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	-1,	0,
};

static const char color_shift_30[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	-6,	-4,	-1,
	-11,	-9,	-6,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	-1,	0,
};

static const char color_shift_40[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	-3,	-3,	-3,
	-5,	-5,	-4,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_50[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	-4,	-7,	-3,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_60[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	3,	-3,	-3,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_70[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	6,	-1,	-1,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_80[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	6,	1,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_90[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	4,	-1,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_100[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	6,	0,	-1,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_110[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	6,	0,	-1,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_120[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	4,	0,	-1,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_130[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	3,	0,	1,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_140[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	4,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_150[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	4,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_160[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	3,	0,	1,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_170[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	3,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_180[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	2,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};
static const char color_shift_190[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	2,	0,	2,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_200[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	4,	1,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_210[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_220[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_230[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_240[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_250[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_260[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_270[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_280[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_290[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char color_shift_300[VMAX * MAX_COLOR_CNT] = {
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
	0,	0,	0,
};

static const char *color_shift[MAX_CANDELA / CANDELA_RESOLUTION + 1] = {
	color_shift_0,
	color_shift_10,
	color_shift_20,
	color_shift_30,
	color_shift_40,
	color_shift_50,
	color_shift_60,
	color_shift_70,
	color_shift_80,
	color_shift_90,
	color_shift_100,
	color_shift_110,
	color_shift_120,
	color_shift_130,
	color_shift_140,
	color_shift_150,
	color_shift_160,
	color_shift_170,
	color_shift_180,
	color_shift_190,
	color_shift_200,
	color_shift_210,
	color_shift_220,
	color_shift_230,
	color_shift_240,
	color_shift_250,
	color_shift_260,
	color_shift_270,
	color_shift_280,
	color_shift_290,
	color_shift_300,
};

static int calc_volt_vt(int gamma)
{
	int max = sizeof(vt_trans_volt) >> 2;

	if (gamma > max)
		gamma = max;

	return (int)vt_trans_volt[gamma];
}

static int calc_v0_volt(struct smart_dimming *dimming, int color)
{
	return MULTIPLE_VREGOUT;
}

static int calc_v5_volt(struct smart_dimming *dimming, int color)
{
	int ret, vt, v15, gamma;

	gamma = dimming->gamma[V5][color];
	if (gamma >= vreg_element_max[V5])
		gamma = vreg_element_max[V5];

	vt = dimming->volt_vt[color];
	v15 = dimming->volt[TBL_IDX_V15][color];
	ret = (vt << 10) - ((vt - v15) * (int)v5_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v15_volt(struct smart_dimming *dimming, int color)
{
	int ret, vt, v31, gamma;

	gamma = dimming->gamma[V15][color];
	if (gamma >= vreg_element_max[V15])
		gamma = vreg_element_max[V15];

	vt = dimming->volt_vt[color];
	v31 = dimming->volt[TBL_IDX_V31][color];
	ret = (vt << 10) - ((vt - v31) * (int)v15_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v31_volt(struct smart_dimming *dimming, int color)
{
	int ret, vt, v63, gamma;

	gamma = dimming->gamma[V31][color];
	if (gamma >= vreg_element_max[V31])
		gamma = vreg_element_max[V31];

	vt = dimming->volt_vt[color];
	v63 = dimming->volt[TBL_IDX_V63][color];
	ret = (vt << 10) - ((vt - v63) * (int)v31_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v63_volt(struct smart_dimming *dimming, int color)
{
	int ret, vt, v127, gamma;

	gamma = dimming->gamma[V63][color];
	if (gamma >= vreg_element_max[V63])
		gamma = vreg_element_max[V63];

	vt = dimming->volt_vt[color];
	v127 = dimming->volt[TBL_IDX_V127][color];
	ret = (vt << 10) - ((vt - v127) * (int)v63_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v127_volt(struct smart_dimming *dimming, int color)
{
	int ret, vt, v191, gamma;

	gamma = dimming->gamma[V127][color];
	if (gamma >= vreg_element_max[V127])
		gamma = vreg_element_max[V127];

	vt = dimming->volt_vt[color];
	v191 = dimming->volt[TBL_IDX_V191][color];
	ret = (vt << 10) - ((vt - v191) * (int)v127_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v191_volt(struct smart_dimming *dimming, int color)
{
	int ret, vt, v255, gamma;

	gamma = dimming->gamma[V191][color];
	if (gamma >= vreg_element_max[V191])
		gamma = vreg_element_max[V191];

	vt = dimming->volt_vt[color];
	v255 = dimming->volt[TBL_IDX_V255][color];
	ret = (vt << 10) - ((vt - v255) * (int)v191_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v255_volt(struct smart_dimming *dimming, int color)
{
	int gamma;

	gamma = dimming->gamma[V255][color];
	if (gamma >= vreg_element_max[V255])
		gamma = vreg_element_max[V255];

	return (int)v255_trans_volt[gamma];
}

static int calc_inter_v0_v5(struct smart_dimming *dimming, int gray, int color)
{
	int ret, v0, v5;

	v0 = dimming->volt[TBL_IDX_V0][color];
	v5 = dimming->volt[TBL_IDX_V5][color];

	ret = (v0 << 10) - ((v0 - v5) * (int)int_tbl_v0_v5[gray]);

	return ret >> 10;
}

static int calc_inter_v5_v15(struct smart_dimming *dimming, int gray, int color)
{
	int ret, v5, v15;

	v5 = dimming->volt[TBL_IDX_V5][color];
	v15 = dimming->volt[TBL_IDX_V15][color];

	ret = (v5 << 10) - ((v5 - v15) * (int)int_tbl_v5_v15[gray]);

	return ret >> 10;
}

static int calc_inter_v15_v31(struct smart_dimming *dimming, int gray,
								int color)
{
	int ret, v15, v31;

	v15 = dimming->volt[TBL_IDX_V15][color];
	v31 = dimming->volt[TBL_IDX_V31][color];

	ret = (v15 << 10) - ((v15 - v31) * (int)int_tbl_v15_v31[gray]);

	return ret >> 10;
}

static int calc_inter_v31_v63(struct smart_dimming *dimming, int gray,
								int color)
{
	int ret, v31, v63;

	v31 = dimming->volt[TBL_IDX_V31][color];
	v63 = dimming->volt[TBL_IDX_V63][color];

	ret = (v31 << 10) - ((v31 - v63) * (int)int_tbl_v31_v63[gray]);

	return ret >> 10;
}

static int calc_inter_v63_v127(struct smart_dimming *dimming, int gray,
								int color)
{
	int ret, v63, v127;

	v63 = dimming->volt[TBL_IDX_V63][color];
	v127 = dimming->volt[TBL_IDX_V127][color];

	ret = (v63 << 10) - ((v63 - v127) * (int)int_tbl_v63_v127[gray]);

	return ret >> 10;
}

static int calc_inter_v127_v191(struct smart_dimming *dimming, int gray,
								int color)
{
	int ret, v127, v191;

	v127 = dimming->volt[TBL_IDX_V127][color];
	v191 = dimming->volt[TBL_IDX_V191][color];

	ret = (v127 << 10) - ((v127 - v191) * (int)int_tbl_v127_v191[gray]);

	return ret >> 10;
}

static int calc_inter_v191_v255(struct smart_dimming *dimming, int gray,
								int color)
{
	int ret, v191, v255;

	v191 = dimming->volt[TBL_IDX_V191][color];
	v255 = dimming->volt[TBL_IDX_V255][color];

	ret = (v191 << 10) - ((v191 - v255) * (int)int_tbl_v191_v255[gray]);

	return ret >> 10;
}

static int calc_reg_v5(struct smart_dimming *dimming, int color)
{
	int t1, t2;

	t1 = dimming->look_volt[V5][color] - MULTIPLE_VREGOUT;
	t2 = dimming->look_volt[V15][color] - dimming->volt_vt[color];

	return (t1 * fix_const[V5].de) / t2 - fix_const[V5].nu;
}

static int calc_reg_v15(struct smart_dimming *dimming, int color)
{
	int t1, t2;

	t1 = dimming->look_volt[V15][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V31][color] - dimming->volt_vt[color];

	return (t1 * fix_const[V15].de) / t2 - fix_const[V15].nu;
}

static int calc_reg_v31(struct smart_dimming *dimming, int color)
{
	int t1, t2;

	t1 = dimming->look_volt[V31][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V63][color] - dimming->volt_vt[color];

	return (t1 * fix_const[V31].de) / t2 - fix_const[V31].nu;
}

static int calc_reg_v63(struct smart_dimming *dimming, int color)
{
	int t1, t2;

	t1 = dimming->look_volt[V63][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V127][color] - dimming->volt_vt[color];

	return (t1 * fix_const[V63].de) / t2 - fix_const[V63].nu;
}

static int calc_reg_v127(struct smart_dimming *dimming, int color)
{
	int t1, t2;

	t1 = dimming->look_volt[V127][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V191][color] - dimming->volt_vt[color];

	return (t1 * fix_const[V127].de) / t2 - fix_const[V127].nu;
}

static int calc_reg_v191(struct smart_dimming *dimming, int color)
{
	int t1, t2;

	t1 = dimming->look_volt[V191][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V255][color] - dimming->volt_vt[color];

	return (t1 * fix_const[V191].de) / t2 - fix_const[V191].nu;
}

static int calc_reg_v255(struct smart_dimming *dimming, int color)
{
	int t1;

	t1 = MULTIPLE_VREGOUT - dimming->look_volt[V255][color];

	return (t1 * fix_const[V255].de) / MULTIPLE_VREGOUT -
							fix_const[V255].nu;
}

void panel_read_gamma(struct smart_dimming *dimming, const unsigned char *data)
{
	int i, j, idx = 0;

	for (i = 0; i < VMAX; i++)
		for (j = 0; j < MAX_COLOR_CNT; j++)
			if (i == V255) {
				dimming->gamma[i][j] =
						data[idx] << 8 | data[idx + 1];
				idx += 2;
			} else {
				dimming->gamma[i][j] = data[idx];
				idx++;
			}
}

void panel_generate_volt_tbl(struct smart_dimming *dimming)
{
	int i, j, seq, idx, gray;
	int calc_seq[VMAX] = {
		V0,
		V255,
		V191,
		V127,
		V63,
		V31,
		V15,
		V5,
	};
	int (*calc_volt_point[VMAX])(struct smart_dimming *, int) = {
		calc_v0_volt,
		calc_v5_volt,
		calc_v15_volt,
		calc_v31_volt,
		calc_v63_volt,
		calc_v127_volt,
		calc_v191_volt,
		calc_v255_volt,
	};
	int (*calc_inter_volt[VMAX])(struct smart_dimming *, int, int) = {
		NULL,
		calc_inter_v0_v5,
		calc_inter_v5_v15,
		calc_inter_v15_v31,
		calc_inter_v31_v63,
		calc_inter_v63_v127,
		calc_inter_v127_v191,
		calc_inter_v191_v255,
	};

	for (i = 0; i < MAX_COLOR_CNT; i++)
		dimming->volt_vt[i] = calc_volt_vt(dimming->gamma[0][i]);

	for (i = 0; i < VMAX; i++) {
		seq = calc_seq[i];
		idx = vref_idx[seq];

		for (j = 0; j < MAX_COLOR_CNT; j++)
			dimming->volt[idx][j] =
					calc_volt_point[seq](dimming, j);
	}

	idx = 0;
	for (i = 0; i < MAX_GRADATION; i++) {
		if (i == vref_idx[idx]) {
			idx++;
			continue;
		}

		gray = (i - vref_idx[idx - 1]) - 1;
		for (j = 0; j < MAX_COLOR_CNT; j++)
			if (calc_inter_volt[idx])
				dimming->volt[i][j] =
					calc_inter_volt[idx](dimming, gray, j);
	}
}

static int panel_lookup_volt_idx(struct smart_dimming *dimming, int gray)
{
	int ret, tmp, idx, exit, i, idx_l, idx_h, cnt_l, cnt_h, delta, n_delta;

	tmp = gray >> 20;
	idx = (int)lookup_tbl[tmp];

	exit = 1;
	i = 0;
	while (exit) {
		idx_l = tmp - i;
		if (idx_l < 0)
			idx_l = 0;

		idx_h = tmp + i;
		if (idx_h > MAX_CANDELA)
			idx_h = MAX_CANDELA;

		cnt_l = (int)lookup_tbl[idx] - (int)lookup_tbl[idx_l];
		cnt_h = (int)lookup_tbl[idx_h] - (int)lookup_tbl[idx];

		if (cnt_l + cnt_h)
			exit = 0;

		i++;
	}

	idx = (int)lookup_tbl[idx_l];
	ret = idx;

	tmp = gamma_multi_tbl[idx] << CANDELA_RESOLUTION;
	if (gray > tmp)
		delta = gray - tmp;
	else
		delta = tmp - gray;

	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		tmp = gamma_multi_tbl[idx + i] << CANDELA_RESOLUTION;
		if (gray > tmp)
			n_delta = gray - tmp;
		else
			n_delta = tmp - gray;

		if (n_delta < delta) {
			delta = n_delta;
			ret = idx + i;
		}
	}

	return ret;
}

void panel_get_gamma(struct smart_dimming *dimming, int candela,
							unsigned char *target)
{
	int cndl, i, j, gray, idx, c_shift;
	int gamma_int[VMAX][MAX_COLOR_CNT];
	int (*calc_reg[VMAX])(struct smart_dimming *, int) = {
		NULL,
		calc_reg_v5,
		calc_reg_v15,
		calc_reg_v31,
		calc_reg_v63,
		calc_reg_v127,
		calc_reg_v191,
		calc_reg_v255,
	};

	if (candela > MAX_CANDELA)
		candela = MAX_CANDELA;
	cndl = candela / CANDELA_RESOLUTION;

	for (i = V5; i < VMAX; i++) {
		gray = gamma_tbl[vref_idx[i]] * candela;
		idx = (int)ref_shift[cndl][i] +
					panel_lookup_volt_idx(dimming, gray);

		for (j = 0; j < MAX_COLOR_CNT; j++)
			dimming->look_volt[i][j] = dimming->volt[idx][j];
	}

	for (i = 0; i < MAX_COLOR_CNT; i++)
		gamma_int[0][i] = dimming->gamma[0][i];

	for (i = V5; i < VMAX; i++)
		for (j = 0; j < MAX_COLOR_CNT; j++) {
			c_shift = (int)color_shift[cndl][i * MAX_COLOR_CNT + j];
			if (c_shift & 0x80)
				gamma_int[i][j] = calc_reg[i](dimming, j) -
							(0xff - c_shift + 1);
			else
				gamma_int[i][j] = calc_reg[i](dimming, j) +
									c_shift;

			if (gamma_int[i][j] >= vreg_element_max[i])
				gamma_int[i][j] = vreg_element_max[i];
			else if (gamma_int[i][j] < 0)
				gamma_int[i][j] = 0;
		}

	idx = 0;
	for (i = 0; i < VMAX; i++)
		for (j = 0; j < MAX_COLOR_CNT; j++)
			if (i == V255) {
				target[idx++] = gamma_int[i][j] > 0xff ? 1 : 0;
				target[idx++] = gamma_int[i][j] & 0xff;
			} else
				target[idx++] = (unsigned char)gamma_int[i][j];
}
