/* -*- mode:C++; indent-tabs-mode:t; tab-width:8; c-basic-offset: 8 -*- */
/*
 *
 * Copyright (c) 2003 by Karsten Wiese <annabellesgarden@yahoo.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifdef __cplusplus
#include <string.h>
#include <math.h>
#include <stdlib.h>
extern int verbose;
extern int positive_gain;
#endif

enum E_In84 {
	eFader0 = 0,
	eFader1,
	eFader2,
	eFader3,
	eFader4,
	eFader5,
	eFader6,
	eFader7,
	eFaderM,
	eTransport,
	eModifier = 10,
	eFilterSelect,
	eSelect,
	eMute,

	eSwitch   = 15,
	eWheelGain,
	eWheelFreq,
	eWheelQ,
	eWheelPan,
	eWheel    = 20
};

#define T_RECORD   1
#define T_PLAY     2
#define T_STOP     4
#define T_F_FWD    8
#define T_REW   0x10
#define T_SOLO  0x20
#define T_REC   0x40
#define T_NULL  0x80


struct us428_ctls{
	unsigned char   Fader[9];
	unsigned char 	Transport;
	unsigned char 	Modifier;
	unsigned char 	FilterSelect;
	unsigned char 	Select;
	unsigned char   Mute;
	unsigned char   UNKNOWN;
	unsigned char   Switch;
	unsigned char   Wheel[5];
};

typedef struct us428_ctls us428_ctls_t;

typedef struct us428_setByte{
	unsigned char Offset,
		Value;
}us428_setByte_t;

enum {
	eLT_Volume = 0,
	eLT_Light
};

typedef struct usX2Y_volume {
	unsigned char	Channel,
		LH,
		LL,
		RH,
		RL;
	unsigned char	Slider;
	char		Pan,
		Mute;
#ifdef __cplusplus
public:
	void init(unsigned char _Channel) {
		memset(this, 0, sizeof(*this));
		Channel = _Channel;
	}
	int Scale(){return 0x40;}

	void calculate() {
		int Val;
		int ScaledL, ScaledR;
		float PanLeft, PanRight;
		float Norm;
		float dBL, dBR;

		// Subtract 1 because the sliders usually don't reach 0 due to sampling inaccuracy
		Val = (Slider - 1);

		if(Val < 0) {
			Val = 0;
		}

		// This is a balance knob-style pan law
		PanLeft = (float)(Pan + 127) / 127.0;
		PanRight = 2.0 - PanLeft;

		if(PanLeft < 0.0) {
			PanLeft = 0.0;
		}
		if(PanLeft > 1.0) {
			PanLeft = 1.0;
		}
		if(PanRight < 0.0) {
			PanRight = 0.0;
		}
		if(PanRight > 1.0) {
			PanRight = 1.0;
		}

		Norm = (float)Val / 254.0;

		if(positive_gain) {
			// -55dB to +12dB
			dBL = 67.0 * (Norm - 1.0) + 12.0;
			dBR = 67.0 * (Norm - 1.0) + 12.0;
		} else {
			// -55dB to 0dB
			dBL = 55.0 * (Norm - 1.0);
			dBR = 55.0 * (Norm - 1.0);
		}

		// 16384 == 0dB (unity) gain
		ScaledL = 16384.0 * pow(10.0, dBL / 20.0) * PanLeft;
		ScaledR = 16384.0 * pow(10.0, dBR / 20.0) * PanRight;

		// Make sure the sound stops completely when the slider is at the bottom
		if(ScaledL < 30) {
			ScaledL = 0;
		}
		if(ScaledR < 30) {
			ScaledR = 0;
		}

		// Prevent overflow of the 16-bit gain value
		if(positive_gain) {
			if(ScaledL > 65535) {
				ScaledL = 65535;
			}
			if(ScaledR > 65535) {
				ScaledR = 65535;
			}
		} else {
			if(ScaledL > 16384) {
				ScaledL = 16384;
			}
			if(ScaledR > 16384) {
				ScaledR = 16384;
			}
		}

		LH = ScaledL >> 8;
		LL = ScaledL;
		RH = ScaledR >> 8;
		RL = ScaledR;
		if (2 < verbose)
			printf("Slider=% 3i, Pan=% 4i, Val=% 3i, dBL=% 6.2f, dBR=% 6.2f, ScaledL=%05i, ScaledR=%05i\n", (int)Slider, (int)Pan, Val, dBL, dBR, ScaledL, ScaledR);
	}

	void SetTo(unsigned char _Channel, int RawValue){
		Slider = RawValue;
		Channel = eFaderM == _Channel ? 4 : _Channel;
		calculate();
	}
	void PanTo(int RawValue, bool Grob) {
		int NewPan = 0;
		if (Grob) {
			static int GrobVals[] = {-128, -64, 0, 64, 127};
			int i = 4;
			while (i >= 0 && GrobVals[i] > Pan)
				i--;
			if (GrobVals[i] != Pan  &&  RawValue < 0)
				i++;

			if (i >= 0) {
				if ((i += RawValue) >= 0  &&  i < 5)
					NewPan = GrobVals[i];
				else
					return;
			}

		} else {
			NewPan = Pan + RawValue;
		}
		
		// Since the step is now 13, make sure 0 can still be reached
		if (abs(NewPan) < 7) {
			NewPan = 0;
		}

		// If we went past the limit, but we weren't at the limit, clamp to the limit
		if (NewPan < -128 && Pan > -128)
			NewPan = -128;
		else if (NewPan > 127 && Pan < 127)
			NewPan = 127;

		// If we were already at the limit, then clamping didn't occur above, so ignore the new value
		if (NewPan < -128  ||  NewPan > 127)
			return;

		Pan = NewPan;
		calculate();
	}
#endif
} usX2Y_volume_t;

struct us428_lights{
	us428_setByte_t Light[7];
#ifdef __cplusplus
public:
	enum eLight{
		eL_Select0 = 0,
		eL_Rec0 = 8,
		eL_Mute0 = 16,
		eL_Solo = 24,
		eL_InputMonitor = 25,
		eL_BankL = 26,
		eL_BankR = 27,
		eL_Rew = 28,
		eL_FFwd = 29,
		eL_Play = 30,
		eL_Record = 31,
		eL_AnalogDigital = 32,
		eL_Null = 34,
		eL_Low = 36,
		eL_LowMid = 37,
		eL_HiMid = 38,
		eL_High = 39
	};
	bool LightIs(int L){
		return Light[L / 8].Value & (1 << (L % 8));
	}
	void LightSet(int L, bool Value){
		if (Value)
			Light[L / 8].Value |= (1 << (L % 8));
		else
			Light[L / 8].Value &= ~(1 << (L % 8));
	}
	void init_us428_lights();
#endif
};
typedef struct us428_lights us428_lights_t;

typedef struct {
	char type;
	union {
		usX2Y_volume_t	vol;
		us428_lights_t  lights;
	};
} us428_p4out_t;

#define N_us428_ctl_BUFS 16
#define N_us428_p4out_BUFS 16
struct us428ctls_sharedmem{
	us428_ctls_t	CtlSnapShot[N_us428_ctl_BUFS];
	int		CtlSnapShotDiffersAt[N_us428_ctl_BUFS];
	int		CtlSnapShotLast, CtlSnapShotRed;
	us428_p4out_t	p4out[N_us428_p4out_BUFS];
	int		p4outLast, p4outSent;
};
typedef struct us428ctls_sharedmem us428ctls_sharedmem_t;
