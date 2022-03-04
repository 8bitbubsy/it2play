#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CHN_DISOWNED 128
#define DIR_FORWARDS 0
#define DIR_BACKWARDS 1
#define PAN_SURROUND 100
#define LOOP_PINGPONG 24
#define LOOP_FORWARDS 8

enum // 8bb: envelope flags
{
	ENVF_ENABLED = 1,
	ENVF_LOOP = 2,
	ENVF_SUSLOOP = 4,
	ENVF_CARRY = 8,
	ENVF_TYPE_FILTER = 128 // 8bb: for pitch envelope only
};

enum // 8bb: sample flags
{
	SMPF_ASSOCIATED_WITH_HEADER = 1,
	SMPF_16BIT = 2,
	SMPF_STEREO = 4,
	SMPF_COMPRESSED = 8,
	SMPF_USE_LOOP = 16,
	SMPF_USE_SUSLOOP = 32,
	SMPF_LOOP_PINGPONG = 64,
	SMPF_SUSLOOP_PINGPONG = 128
};

enum // 8bb: host channel flags
{
	HF_UPDATE_EFX_IF_CHAN_ON = 1,
	HF_ALWAYS_UPDATE_EFX = 2,
	HF_CHAN_ON = 4,
	HF_CHAN_CUT = 8, // No longer implemented
	HF_PITCH_SLIDE_ONGOING = 16,
	HF_FREEPLAY_NOTE = 32, // 8bb: Only needed for tracker. Logic removed.
	HF_ROW_UPDATED = 64,
	HF_APPLY_RANDOM_VOL = 128,
	HF_UPDATE_VOLEFX_IF_CHAN_ON = 256,
	HF_ALWAYS_VOLEFX = 512
};

enum // 8bb: slave channel flags
{
	SF_CHAN_ON = 1,
	SF_RECALC_PAN = 2,
	SF_NOTE_OFF = 4,
	SF_FADEOUT = 8,
	SF_RECALC_VOL = 16,
	SF_FREQ_CHANGE = 32,
	SF_RECALC_FINALVOL = 64,
	SF_CENTRAL_PAN = 128,
	SF_NEW_NOTE = 256,
	SF_NOTE_STOP = 512,
	SF_LOOP_CHANGED = 1024,
	SF_CHN_MUTED = 2048,
	SF_VOLENV_ON = 4096,
	SF_PANENV_ON = 8192,
	SF_PITCHENV_ON = 16384,
	SF_PAN_CHANGED = 32768
};

enum // 8bb: IT header flags
{
	ITF_STEREO = 1,
	ITF_VOL0_OPTIMIZATION = 2, // 8bb: not used in IT1.04 and later
	ITF_INSTR_MODE = 4,
	ITF_LINEAR_FRQ = 8,
	ITF_OLD_EFFECTS = 16,
	ITF_COMPAT_GXX = 32,
	ITF_USE_MIDI_PITCH_CNTRL = 64,
	ITF_REQ_MIDI_CFG = 128
};

enum // 8bb: audio driver flags
{
	DF_SUPPORTS_MIDI = 1,
	DF_USES_VOLRAMP = 2, // 8bb: aka. "hiqual"
	DF_WAVEFORM = 4, // Output waveform data available
	DF_HAS_RESONANCE_FILTER = 8 // 8bb: added this
};

// 8bb: do NOT change these, it will only mess things up!
#define MAX_PATTERNS 200
#define MAX_SAMPLES 100
#define MAX_INSTRUMENTS 100
#define MAX_ORDERS 256
#define MAX_ROWS 200
#define MAX_HOST_CHANNELS 64
#define MAX_SLAVE_CHANNELS 256
#define MAX_SONGMSG_LENGTH 8000

typedef struct pattern_t
{
	uint16_t Rows;
	uint8_t *PackedData;
} pattern_t;

typedef struct envNode_t
{
	int8_t Magnitude;
	uint16_t Tick;
} envNode_t;

typedef struct env_t
{
	uint8_t Flags, Num, LpB, LpE, SLB, SLE;
	envNode_t NodePoints[25];
} env_t;

typedef struct instrument_t
{
	char DOSFilename[12+1];
	uint8_t NNA, DCT, DCA;
	uint16_t FadeOut;
	uint8_t PPS, PPC, GbV, DfP, RV, RP;
	uint16_t TrkVers;
	uint8_t NoS;
	char InstrumentName[26];
	uint8_t IFC, IFR, MCh, MPr;
	uint16_t MIDIBnk;
	uint16_t SmpNoteTable[120];
	env_t VEnvelope;
	env_t PEnvelope;
	env_t PtEnvelope;
} instrument_t;

typedef struct smp_t
{
	char DOSFilename[12+1];
	uint8_t GvL, Flags, Vol;
	char SampleName[26];
	uint8_t Cvt, DfP;
	uint32_t Length, LoopBeg, LoopEnd, C5Speed, SusLoopBeg, SusLoopEnd, OffsetInFile;
	uint8_t ViS, ViD, ViR, ViT;
	void *Data;

	// 8bb: added this for custom HQ driver
	void *OrigData, *DataR, *OrigDataR;
} sample_t;

typedef struct hostChn_t
{
	uint16_t Flags;
	uint8_t Msk, Nte, Ins, Vol, Cmd, CmdVal, OCm, OCmVal, VCm, VCmVal, MCh, MPr, Nt2, Smp;
	uint8_t DKL, EFG, O00, I00, J00, M00, N00, P00, Q00, T00, S00, OxH, W00, VCE, GOE, SFx;
	uint8_t HCN, CUC, VSe, LTr;
	void *SCOffst;
	uint8_t PLR, PLC, PWF, PPo, PDp, PSp, LPn;
	int8_t LVi;
	uint8_t CP, CV;
	int8_t VCh;
	uint8_t TCD, Too, RTC;
	int32_t PortaFreq;
	uint8_t VWF, VPo, VDp, VSp, TWF, TPo, TDp, TSp;
	uint8_t MiscEfxData[16];
} hostChn_t;

typedef struct envState_t
{
	int32_t Value, Delta;
	int16_t Tick, CurNode, NextTick;
} envState_t;

typedef struct slaveChn_t
{
	uint16_t Flags;
	uint32_t MixOffset;
	uint8_t LpM, LpD;
	int32_t LeftVolume, RightVolume;
	int32_t Frequency, FrequencySet;
	uint8_t Bit, ViP;
	uint16_t ViDepth;
	int32_t OldLeftVolume, OldRightVolume;
	uint8_t FV, Vol, VS, CVl, SVl, FP;
	uint16_t FadeOut;
	uint8_t DCT, DCA, Pan, PS;
	instrument_t *InsOffs;
	uint8_t Nte, Ins;
	sample_t *SmpOffs;
	uint8_t Smp, FPP;
	void *HCOffst;
	uint8_t HCN, NNA, MCh, MPr;
	uint16_t MBank;
	int32_t LoopBeg, LoopEnd;
	uint32_t SmpError;
	uint16_t vol16Bit;
	int32_t SamplingPosition;
	envState_t VEnvState;
	int32_t filtera;
	envState_t PEnvState;
	int32_t filterb;
	envState_t PtEnvState;
	int32_t filterc;

	// 8bb: added these
	uint32_t Delta;
	int32_t OldSamples[2];
	int32_t DestVolL, DestVolR, CurrVolL, CurrVolR; // 8bb: ramp
	float fOldSamples[4], fFiltera, fFilterb, fFilterc;

	// 8bb: for custom HQ mixer
	float fOldLeftVolume, fOldRightVolume, fLeftVolume, fRightVolume;
	float fDestVolL, fDestVolR, fCurrVolL, fCurrVolR;
	uint64_t Frac64, Delta64;
} slaveChn_t;

typedef struct it_header_t
{
	char SongName[26];
	uint16_t OrdNum, InsNum, SmpNum, PatNum, Cwtv, Cmwt, Flags, Special;
	uint8_t GlobalVol, MixVolume, InitialSpeed, InitialTempo, PanSep;
	uint16_t MessageLength;
	uint32_t MessageOffset;
	uint8_t ChnlPan[MAX_HOST_CHANNELS], ChnlVol[MAX_HOST_CHANNELS];
} it_header_t;

typedef struct // 8bb: custom struct
{
	bool StartNoRamp; // 8bb: for "WAV writer" driver
	uint8_t Type, Flags, FilterParameters[128];
	uint32_t MixMode, MixSpeed;
	int32_t Delta;
	int64_t Delta64;
	float QualityFactorTable[128], FreqParameterMultiplier, FreqMultiplier;
} driver_t;

typedef struct song_t
{
	it_header_t Header;
	uint8_t Orders[MAX_ORDERS];
	instrument_t Ins[MAX_INSTRUMENTS];
	sample_t Smp[MAX_SAMPLES];
	pattern_t Pat[MAX_PATTERNS];
	char Message[MAX_SONGMSG_LENGTH+1]; // 8bb: +1 to fit protection-NUL

	volatile bool Playing, Loaded;
	uint8_t *PatternOffset, LastMIDIByte;
	uint16_t CurrentOrder, CurrentPattern, CurrentRow, ProcessOrder, ProcessRow;
	uint16_t BreakRow;
	uint8_t RowDelay;
	bool RowDelayOn, StopSong, PatternLooping;
	uint16_t NumberOfRows, CurrentTick, CurrentSpeed, ProcessTick;
	uint16_t Tempo, GlobalVolume;
	uint16_t DecodeExpectedPattern, DecodeExpectedRow;
} song_t;

extern hostChn_t hChn[MAX_HOST_CHANNELS];
extern slaveChn_t sChn[MAX_SLAVE_CHANNELS];
extern song_t Song;
extern driver_t Driver;
