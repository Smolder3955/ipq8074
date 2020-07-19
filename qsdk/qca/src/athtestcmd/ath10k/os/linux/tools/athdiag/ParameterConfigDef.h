#ifndef __ParamterConfigDef
#define __ParamterConfigDef

#define MPARAMETER	20
#define PARAMETERLEN	50
#define MBUFFER		1024

#define MIN_G_BAND_FREQ		2412
#define MAX_G_BAND_FREQ		2490
#define MIN_A_BAND_FREQ		4800
#define MAX_A_BAND_FREQ		6000
#define UNDEFINE 		-1
#define DEFINE_ALL 		1000

#define MAX_CHAIN	3

#define BAND_BG		"band_BG"
#define BAND_A		"band_A"

enum BAND_enum
{
	band_undefined = -1,
	band_BG = 0,
	band_A,
	band_both,
};

enum HTmode_enum
{
	mode_undefined = -1,
	legacy_CCK=0,
	legacy_OFDM,
	HT20,
	HT40,
	HT80,
};


// for set CalPier/get CalPier
#define NUM_5G_CAL_PIERS		8
#define NUM_2G_CAL_PIERS		3

// for set_CalTGT/get_CalTGT
#define MAX_NUM_TGT_FREQ	8
#define MAX_NUM_TGT_RATES	14
#define NUM_CALTGT_FREQ_CCK		2
#define NUM_CALTGT_FREQ_2G		3
#define NUM_CALTGT_FREQ_5G		8
#define NUM_TGT_DATARATE_LEGACY	4
#define NUM_TGT_DATARATE_HT 	14
#define NUM_TGT_DATARATE_HT_ALL	24

#define MAX_NUM_PARAM_VALUE	20
// for set/get;
typedef struct SetGetParamsStruct {
	int		numInGroup;		// number of values saved in values.
    int		values[MAX_NUM_PARAM_VALUE];
	int		itemMin, itemMax;
	int		itemValueIsSet;
	char itemName[PARAMETERLEN];
} _ITEM_STRUCT;

#define MAX_NUM_PARAM_ITEM	5
// for set/get;
typedef struct SetGetParamItemStruct {
	int		numItems;		// number of values saved in itemValue.
	int		iBand;
	int		iMode;
	_ITEM_STRUCT item[MAX_NUM_PARAM_ITEM];
	char paramName[PARAMETERLEN];
	int		isHex;			// input in Hex format
} _PARAM_ITEM_STRUCT;

#define INT8BITS	108
#define BIT1		1
#define BIT2		2
#define BIT4		4
#define BIT6		6
#define UINT8BITS	8
#define UINT16BITS	16
#define UINT32BITS	32
#define IsHEX		1
#define NotHEX		0

extern void intArr2Str(int *Arr, int max, unsigned char *tValue);

#endif
