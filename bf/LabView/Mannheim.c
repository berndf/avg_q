/* CIN source file */

#include "extcode.h"

/* Typedefs */

typedef struct {
	int32 dimSizes[2];
	float32 elt[1];
	} TD2;
typedef TD2 **TD2Hdl;

typedef struct {
	uInt16 S;
	uInt16 DR;
	uInt16 B;
	uInt16 P;
	uInt16 T;
	uInt16 R;
	uInt16 Time;
	uInt16 TriggerOut;
	uInt16 AUX;
	uInt32 SecTot;
	uInt32 SecDR;
	TD2Hdl Samples;
	} TD1;

typedef struct {
	int32 dimSize;
	int32 Index[1];
	} TD4;
typedef TD4 **TD4Hdl;

typedef struct {
	LStrHandle A2N;
	LVBoolean On;
	LStrHandle Type;
	LStrHandle SN;
	LStrHandle LN;
	int32 SR;
	LStrHandle Unit;
	float64 Gain;
	LStrHandle Name;
	} TD6;

typedef struct {
	int32 dimSize;
	TD6 Cluster[1];
	} TD5;
typedef TD5 **TD5Hdl;

typedef struct {
	uInt16 Subject;
	uInt32 SampleRate;
	HWAVEFORM currentTime;
	int32 Ch;
	TD4Hdl AChs;
	TD5Hdl Chs;
	} TD3;

MgErr CINRun(TD1 *Data, TD3 *recordS);

MgErr CINRun(TD1 *Data, TD3 *recordS)
	{

	/* Insert code here */

	return noErr;
	}
