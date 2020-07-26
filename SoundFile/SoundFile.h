/* INTERNAL DEFINES AND STRUCTURES */
#include "Version.h"
//#include <QDOffscreen.h>	/* for GWorld routines */

#define WRITEBYTES			262144UL

#define	QUAD				4
#define	STEREO				2
#define	MONO				1

#define RIGHT				1
#define LEFT				0

/* Soundfile Header Types */
#define	AIFF	1
#define AIFC	2
#define AUDMED	3
#define	BICSF	4
#define DSPD	5
#define MMIX	6
#define WAVE	7
#define	NEXT	8
#define	QKTMA	9	// AIFF with QuickTime movie
#define	SDII	10
#define SUNAU	11	// new
#define TEXT	12
#define	RAW		13
// Spectral types
#define CS_PVOC	20	// Csound phase vocoder format
#define CS_HTRO	21	// Csound hetrodyne / adsyn format
#define LEMUR	22	// Lemur format
#define PICT	23	// PICT resource for spect type
#define MPEG	24	// MPEG Layer I, II or III
#define QKTMV   25	// QuickTime video sonogram
#define SDIFF	26
/* Not displayed in file save popup - read only formats */
#define SDI		32
#define TX16W	33
#define MAC		34
#define MACEXT	35
#define MACCOMP	36
#define MACTWO	37

/*Soundfile PackModes - need to be in the same order as the menu*/
#define SF_FORMAT_4_ADDVI		1
#define SF_FORMAT_4_ADIMA		2
#define	SF_FORMAT_8_LINEAR		3
#define	SF_FORMAT_8_UNSIGNED	4
#define	SF_FORMAT_8_ALAW		5
#define	SF_FORMAT_8_MULAW		6
#define	SF_FORMAT_16_LINEAR		7
#define	SF_FORMAT_16_SWAP		8
#define SF_FORMAT_24_LINEAR		9
#define SF_FORMAT_24_SWAP		10
#define SF_FORMAT_32_LINEAR		11
#define SF_FORMAT_32_SWAP		12
#define	SF_FORMAT_32_FLOAT		13
#define	SF_FORMAT_TEXT			14
#define	SF_FORMAT_MACE3			15
#define	SF_FORMAT_MACE6			16
#define SF_FORMAT_TX16W			17
#define SF_FORMAT_24_COMP		18
#define SF_FORMAT_32_COMP		19
// spectral packmodes
#define	SF_FORMAT_SPECT_AMP		101
#define	SF_FORMAT_SPECT_AMPPHS	102
#define	SF_FORMAT_SPECT_AMPFRQ	103
#define SF_FORMAT_SPECT_COMPLEX	107
#define	SF_FORMAT_SPECT_MQ		104
#define	SF_FORMAT_PICT			105
#define	SF_FORMAT_QKTM			106
// import-export only packmodes
#define	SF_FORMAT_MPEG_I		201
#define	SF_FORMAT_MPEG_II		202
#define	SF_FORMAT_MPEG_III		203
// exttra file formats
#define SF_FORMAT_3DO_CONTENT	301

// struct to hold everything neccesary for displaying signals, spectra or sonograms
// offscreen crap dwells here
typedef struct
{
	WindowPtr	windo;
	GWorldPtr	offScreen;
	GDHandle	winDevice;
	CGrafPtr	winPort;
	long		spare;	// Horizontal position of sonogram display, maybe other things
	long		spareB;	// back reference to menu which created this, maybe other stuff
	PicHandle	thePict; // for PICT > sound conversion.
}	SoundDisp;

// to deal with apple compression
typedef struct
{
	SoundConverter	sc;
	Ptr				inputPtr;
	Ptr				outputPtr;
    unsigned long	inputBytes;
    unsigned long	inputFrames;
    unsigned long	outputBytes;
    unsigned long	outputFrames;
	long init;
	long curBufPos;
	long filePosition;
}	CompStruct;
/* 
 * struct for soundfile information, all of these need to be set up on an
 * opensf or a createsf
 */
typedef struct
{
	FSSpec	sfSpec;			// name, parID and vRefNum
	double	sRate;			/* sample rate*/
	TimeScale timeScale;	/* if QT, its media timeScale */
	TimeValue timeValue;	/* if QT, the position */
	long	dataStart;		/* offset in data fork from begining of file (in bytes)*/
	long	dataEnd;		/* byte position of the last sample*/
	long	numBytes;		/* number of bytes of sound data */
	long	packMode;		/* numeric format*/
	Str255	compName;		/* string description of compressed numeric formats */
	long	nChans;			/* number of channels */
	double	frameSize;		/* size of sample in bytes */
	long	sfType;			/* type of soundfile header */
	short	rFileNum;		/* resource file number */
	short	dFileNum;		/* data file number */
    long 	spectFrameSize;			/* PVA files: size of FFT frames (2^n) */
    long 	spectFrameIncr;			/* PVA files: # new samples each fram (overlap) */
    unsigned long playPosition;		// Where we are in the playback window.
    CompStruct	comp;			// if an applecompressed file...
    int		infoUpdate;		//	should we update the info window?
    float	infoSamps[256];		// a little snippet for the infoWindow
    float	peak;
    float	peakFL;
    float	peakFR;
    float	peakRL;
    float	peakRR;
	SoundDisp	view;	/* window to display soundfile in */
	SoundDisp	sigDisp;	/* window to display signal in */
	SoundDisp	spectDisp;	/* window to display spectrum in */
	SoundDisp	sonoDisp;		/* window to display sonogram in */
	WindowPtr	infoWindow;		/* the window that describes the soundfile */
	CGrafPtr	infoPort;
	GDHandle	infoDevice;
	GWorldPtr	infoOffScreen;
	Ptr			nextSIPtr;	/* the next soundfile in the linked list (must be typdefed)*/
}	SoundInfo;

typedef SoundInfo *SoundInfoPtr;

/*Function prototypes for SoundFileIO.c*/
void	AllocateSoundIOMemory(short channels, long frames);
void 	SetSecondsPosition(SoundInfo *mySI, double seconds);
void	SetOutputScale(long packMode);
long	ReadQuadBlock(SoundInfo *mySI, long numSamples, float blockL[], float blockR[], float block3[], float block4[]);
long	ReadStereoBlock(SoundInfo *mySI, long numSamples, float blockL[], float blockR[]);
long	ReadMonoBlock(SoundInfo *mySI, long numSamples, float block[]);
long	WriteStereoBlock(SoundInfo *mySI, long numSamples, float blockL[], float blockR[]);
long	WriteMonoBlock(SoundInfo *mySI, long numSamples, float block[]);
//void 	WriteBuffered(SoundInfo *mySI, long *numBytes, char * block);
//void 	FlushWriteBuffer(SoundInfo *mySI);

/* AIFF - AIFC DEFINES AND STRUCTURES */

/* defines for AIFF files */
#define	FORMID					'FORM'
#define FORMVERID				'FVER'
#define	AIFFTYPE				'AIFF'
#define	AIFCTYPE				'AIFC'
#define	COMMONID				'COMM'
#define	SOUNDID					'SSND'
#define MARKERID				'MARK'
#define INSTRUMENTID			'INST'
#define MIDIDATAID				'MIDI'
#define JUNKID					'JUNK'
#define AUDIORECORDINGID		'AESD'
#define APPLICATIONSPECIFICID	'APPL'
#define COMMENTID				'COMT'
#define NAMEID					'NAME'
#define AUTHORID				'AUTH'
#define COPYRIGHTID				'(c) '
#define ANNOTATIONID			'ANNO'
#define PEAKID					'PEAK'
#define	AIFCVERSION1			0xA2805140

/* defines for AIFC compression types */
#define AIFC_ID_ADDVI	'ADP4'
#define AIFC_ID_FLT32	'FL32'

/* defines for uncompressed AIFF - AIFC */
#define	AIFF_FORMAT_LINEAR_8	8
#define	AIFF_FORMAT_LINEAR_16	16
#define	AIFF_FORMAT_LINEAR_24	24
#define	AIFF_FORMAT_LINEAR_32	32

/* structs for Audio IFF file format */
typedef	struct
{
	ID		ckID;
	long	ckSize;
	ID		formType;
}	FormChunk;

#pragma pack(2)
typedef	struct
{
	ID				ckID;				// 4
	long			ckSize;				// 4
	short			numChannels;		// 2
	unsigned long	numSampleFrames;	// 4
	short			sampleSize;		
	union
	{	// 2
		unsigned char	Bytes[10];		//10
		unsigned short	Shorts[5];
	}	sampleRate;
}	AIFFCommonChunk;					//26
#pragma pack()

typedef	struct
{
	ID				ckID;
	long			ckSize;
	unsigned long	offset;
	unsigned long	blockSize;
	unsigned char	soundData[];
}	AIFFSoundDataChunk;

typedef	struct
{
	ID				ckID;
	long			ckSize;
	unsigned long	offset;
	unsigned long	blockSize;
}	SoundDataChunkHeader;

typedef struct
{
	ID				ckID;
	long			ckSize;
    unsigned long version;        /* version of the PEAK chunk */
    unsigned long timeStamp;      /* secs since 1/1/1970  */
}	PeakChunkHeader;

typedef struct
{
    float         value;    /* signed value of peak */
    unsigned long position; /* the sample frame for the peak */
}   PositionPeak;

typedef	struct
{
	ID				ckID;
	long			ckSize;
	unsigned short	numComments;
}	CommentsChunkHeader;

typedef	struct
{
	unsigned long	timeStamp;
	unsigned short	MarkerID;
	unsigned short	count;
}	AIFFComment;

typedef struct
{
	ID				ckID;
	long			ckSize;
	unsigned short	numMarkers;
}	MarkerChunkHeader;

typedef struct
{
	unsigned short	MarkerID;
	unsigned long	position;
	Str255			markerName;
}	AIFFMarker;

/* Just so I don't have to deal with the variable sized text */
typedef struct
{
	unsigned short	MarkerID;
	unsigned long	position;
}	AIFFShortMarker;

typedef struct
{
	short			playMode;	/* 0 = No Loop, 1 = Forward Loop, 2 = For/Back Loop */
	unsigned short	beginLoop;
	unsigned short	endLoop;
}	AIFFLoop2;

typedef struct
{
	ID				ckID;			// 4
	long			ckSize;			// 4
	char			baseFrequency;	// 1
	char			detune;			// 1
	char			lowFrequency;	// 1
	char			highFrequency;	// 1
	char			lowVelocity;	// 1
	char			highVelocity;	// 1
	short			gain;			// 2
	AIFFLoop2		sustainLoop;	// 6
	AIFFLoop2		releaseLoop;	// 6
}	AIFFInstrumentChunk;


/* Additional structs for AIFC support. AIFC uses SoundDataChunk and FormChunk from
	above declarations */
	
#pragma pack(2)
typedef struct
{
	ID				ckID;
	long			ckSize;
	short			numChannels;
	unsigned long	numSampleFrames;
	short			sampleSize;
	union
	{	// 2
		unsigned char	Bytes[10];		//10
		unsigned short	Shorts[5];
	}	sampleRate;
	ID				compressionType;
	char			compressionName[256];
}	AIFCCommonChunk;
#pragma pack()

typedef struct
{
	ID				ckID;
	long			ckSize;
	unsigned long	timestamp;
}	AIFCFormatVersionChunk;

/* WAVE - RIFF DEFINES AND STRUCTURES - Everything needs to be byte swapped!*/

#define	WAV_ID_RIFF				'RIFF'
#define	WAV_ID_WAVE				'WAVE'
#define	WAV_ID_LIST				'LIST'
#define	WAV_ID_FORMAT			'fmt '
#define	WAV_ID_DATA				'data'
#define	WAV_ID_INFO				'INFO'
#define	WAV_ID_INAM				'INAM'
#define	WAV_ID_WAVELIST			'wavl'
#define	WAV_ID_SILENCE			'sInt'
#define	WAV_ID_INST				'inst'
#define	WAV_ID_SAMPLE			'smpl'
#define	WAV_ID_CUE				'cue '

#define	WAV_FORMAT_PCM			0x0001
#define	WAV_FORMAT_ADDVI		0x0002
#define	WAV_FORMAT_ADIMA		0x0011
#define	IBM_FORMAT_MULAW		0x0101
#define	IBM_FORMAT_ALAW			0x0102
#define	IBM_FORMAT_ADDVI		0x0103

typedef	struct
{
	ID		ckID;
	long	ckSize;
	ID		formType;
}	RIFFFormChunk;

// size  - 24 bytes
typedef struct
{
	ID		ckID;
	long	ckSize;
	short	wFormatTag;
	short	wChannels;
	long	dwSamplePerSec;
	long	dwAvgBytesPerSec;
	short	wBlockAlign;
	short	sampleSize
}	WAVEFormatChunk;

typedef struct
{
	ID				ckID;
	long			ckSize;
	unsigned char	bUnshiftedNote;
    char			chFineTune;
    char			chGain;
    unsigned char	bLowNote;
    unsigned char	bHighNote;
    unsigned char	bLowVelocity;
    unsigned char	bHighVelocity;
}	WAVEInstChunk;

typedef struct
{
	ID		ckID;
	long	ckSize;
	long	dwManufacturer;
	long	dwProduct;
	long	dwSamplePeriod;
	long	dwMIDIUnityNote;
	long	dwMIDIPitchFraction;
	long	dwSMPTEFormat;
	long	dwSMPTEOffset;
	long	cSampleLoops;
	long	cbSamplerData;
}	WAVESampleHeader;

typedef struct
{
	long	dwIdentifier;
	long	dwType;
	long	dwStart;
	long	dwEnd;
	long	dwFraction;
	long	dwPlayCount;
}	WAVESampleLoop;

typedef struct
{
	ID		ckID;
	long	ckSize;
	long	dwCuePoints;
}	WAVECueHeader;
	
typedef struct 
{
	long	dwName;	
	long	dwPosition;
	ID		fccChunk;
	long	dwChunkStart;
	long	dwBlockStart;	
	long	dwSampleOffset;
}	WAVECuePoint;
  

/* NeXT - Sun DEFINES AND STRUCTURES */
#define	NEXTMAGIC	0x2e736e64L

#define	NEXT_FORMAT_UNSPECIFIED		(0)
#define	NEXT_FORMAT_MULAW_8			(1)
#define	NEXT_FORMAT_LINEAR_8		(2)
#define	NEXT_FORMAT_LINEAR_16		(3)
#define	NEXT_FORMAT_LINEAR_24		(4)
#define	NEXT_FORMAT_LINEAR_32		(5)
#define	NEXT_FORMAT_FLOAT			(6)
#define	NEXT_FORMAT_DOUBLE			(7)
#define	NEXT_FORMAT_INDIRECT		(8)
#define	NEXT_FORMAT_NESTED			(9)
#define	NEXT_FORMAT_DSP_CORE		(10)
#define	NEXT_FORMAT_DSP_DATA_8		(11)
#define	NEXT_FORMAT_DSP_DATA_16		(12)
#define	NEXT_FORMAT_DSP_DATA_24		(13)
#define	NEXT_FORMAT_DSP_DATA_32		(14)
#define	NEXT_FORMAT_DISPLAY			(16)
#define	NEXT_FORMAT_MULAW_SQUELCH	(17)
#define	NEXT_FORMAT_EMPHASIZED		(18)
#define	NEXT_FORMAT_COMPRESSED		(19)
#define	NEXT_FORMAT_COMPRESSED_EMPHASIZED	(20)
#define	NEXT_FORMAT_DSP_COMMANDS	(21)
#define	NEXT_FORMAT_DSP_COMMANDS_SAMPLES	(22)
#define	NEXT_FORMAT_ALAW_8			(27)

/* typedef for NeXT or Sun header structure */

typedef struct
{
	long 	magic;
	long	dataLocation;
	long	dataSize;
	long	dataFormat;
	long	samplingRate;
	long	channelCount;
}	NeXTSoundInfo;


/* BICSF DEFINES AND STRUCTURES */
#define BICSFMAGIC	107364L

#define	BICSF_FORMAT_LINEAR_8		1
#define	BICSF_FORMAT_LINEAR_16		2
#define	BICSF_FORMAT_FLOAT			4
#define BICSF_CODE_END 				0	/* Code meaning "no more information" */
#define BICSF_CODE_MAXAMP			1	/* Code meaning "maxamp follows"  */
#define BICSF_CODE_COMMENT			2	/* Code for "comment line" */

typedef struct
{
	short   code;           /* Code for what information follows */
	short   bsize;          /* Total nr bytes of added information */
}	BICSFCodeHeader;

/* typedef for BICSF header structure */
typedef	struct
{
	long	magic;
	float	srate;
	long	chans;
	long	packMode;	/* 2 for short, 4 for float? */
	char	codes[1008];
}	BICSFSoundInfo;

/* Structures for Sound Designer II support */
typedef struct
{
	short	version;
	long	markerOffset;
	short	numMarkers;
}	SDMarkerHeader;

typedef struct
{
	short	markerType0;	/* 1 = numbered, 2 = text */
	short	markerType1;	/* duplicate */
	long	position;
	long	text;			/* do not use */
	short	cursorID;		/* 24430 = numeric, 3012 = text */
	short	markerID;		/* ID number */
	long	textLength;		/* length of following text, text excluded in this struct */
}	SDShortMarker;

typedef struct
{
	short	version;	/* (write 1) */
	short	hScale;	/* do not use (write 0) */
	short	vScale;	/* do not use (write 0) */
	short	numLoops;
}	SDLoopHeader;

typedef struct
{
	long	loopStart;
	long	loopEnd;
	short	loopIndex;
	short	loopSense;	/* 117 = forward, 118 = backward/forward */
	short	channel;	/* what channel it is on */
}	SDLoop;

/* typedef for DSP Designer DSPs resource */

typedef struct
{
	Byte	version;	/* Set to 1 */
	Byte	frameSize;	/* 2 - short, 4 - float, 8 - double, 10 - ext80, 12 - ext96*/
	Byte	streamOrBlock;	/* 0 - stream, 1 - block */
	Byte	complexType; /* 0 - real, 1 - complex, 2 - polar */
	Byte	domain;		/* 0 - time domain, 1 - freq. domain */
	Byte	objectType;	/* 1 - Group delay data, 0 - standard */
	Byte	dBOrLinear; /* 0 - linear, 1 - dB */
	Byte	radianOrDegree; /* radians - 0, 1 - degrees */
	Byte	unused1;	/* Null C-String, Initialize to Zero */
	Byte	unused2;	/* Null C-String, Initialize to Zero */
	Byte	unused3;	/* Null C-String, Initialize to Zero */
	Byte	unused4;	/* Null C-String, Initialize to Zero */
	char	sRateCstr[12];
	char	initialTimeCstr[2];/*Set to zero */
	char	fracScaleCstr[2]; /* Set to one */
} DSPsResource;

/* stuff for TX16W format */
#define TX16WMAGIC 'LM8953'
#define TX16W_NOLOOP 0xc9
#define TX16W_LOOP 0x49
#define TX16W_MAXLENGTH 0x3FF80



typedef struct
{
	char filetype[6];
	char nulls[10];
	char dummy_aeg[6];    /* space for the AEG (never mind this) */
	char format;          /* 0x49 = looped, 0xC9 = non-looped */
	char sample_rate;     /* 1 = 33 kHz, 2 = 50 kHz, 3 = 16 kHz */
	char atc_length[3];   /* I'll get to this... */
	char rpt_length[3];
	char unused[2];       /* set these to null, to be on the safe side */
}	TX16WHeader;

/* typedef for macintosh sfil extended header structure (format 2 'snd ' resource) */

typedef struct
{
	short format;
	short numModifiers;
	short numCommands;
	short soundCommand;
	short soundParam1;
	long soundParam2;
	long dataPointer;
	unsigned long numChannels;
    Fixed sampleRate;                       /* sample rate in Apples Fixed point representation */
    unsigned long loopStart;                /* same meaning as regular SoundHeader */
    unsigned long loopEnd;                  /* same meaning as regular SoundHeader */
    unsigned char encode;                   /* data structure used , stdSH, extSH, or cmpSH */
    unsigned char baseFrequency;            /* same meaning as regular SoundHeader */
    unsigned long numFrames;                /* length in total number of frames */
    extended80 AIFFSampleRate;                /* IEEE sample rate */
    Ptr markerChunk;                        /* sync track */
    Ptr InstrumentChunks;                   /* AIFF instrument chunks */
    Ptr AESRecording;
    unsigned short sampleSize;              /* number of bits in sample */
    unsigned short futureUse1;              /* reserved by Apple */
    unsigned long futureUse2;               /* reserved by Apple */
    unsigned long futureUse3;               /* reserved by Apple */
    unsigned long futureUse4;               /* reserved by Apple */
}	MacSoundInfo;

typedef struct MarkerType
{
	Boolean Free;   // TRUE if this marker is free for use (TRUE/1).
	long    Position;       // Byte position in file (0).
	char    Name[33];       // Name of the marker ("Untitled").
}	MarkerType;

enum SideType {kLeftSide,kRightSide};

typedef struct EditRecord
{
	long    HiAddr; // DO NOT USE (0).
	long    LoAddr; // DO NOT USE (0).
	enum SideType   ExtendSide;     // DO NOT USE (0).
} EditRecord;

enum ScaleNames {kTime,kSampleNumber,kHexSampNum,kVolts,kPercent,kdbm,kUser};
enum ModeType {kSelect,kDraw,kZoomSelect};

typedef struct ZoomType
{
	short   v;      // Vertical scale factor: positive = magnification
	                //      negative = reduction (-256).
	long    h;      // Horizontal scale factor: positive = magnification
	                //      negative = reduction (1)}
} ZoomType;


typedef struct ScaleType
{
	long    VFactor;        // Scale factor for vertical axis tick units:                           
							//     positive = magnification,
	             			//      negative = reduction (327).
	enum ScaleNames VType;  // Type of vertical axis tick mark unit (4).
	char    VString[34];    // Vertical axis tick units string ("%Scale").
	long    HFactor;        // Scale factor for horizontal axis tick units:
	      					//      positive = magnification
	              			//      negative = reduction (1).
	enum ScaleNames HType;  // Type of horizontal axis tick mark unit (0).          char    HString[34];   
							// Horizontal axis tick units string:
	         				//      µsec,msec,sec,samples ("sec").
} ScaleType;

typedef struct
{
	short   HeaderSize;     // Size in bytes of the file header (1336).
	short   Version;        // DO NOT USE (32).
	Boolean Preview;        // DO NOT USE (0).
	WindowPtr       WPtr;   // DO NOT USE (0).
	WindowPtr      WPeek;  // DO NOT USE (0).
	char	dummy[34];
	long    HInxPage,HInxLine;      // DO NOT USE (0).
	long    VInxPage,VInxLine;      // DO NOT USE (0).
	long    HCtlPage,HCtlLine;      // DO NOT USE (0).
	long    VCtlPage,VCtlLine;      // DO NOT USE (0).
	long    VOffset;        // Position of vertical center of window in quantization
	              //      units ie -32768 to 32767 (0).
	long    HOffset;        // Position of left edge of sample window in #SAMPLES (0).
	short   VOffConst;      // DO NOT USE (0).
	ZoomType        Zoom;   // See above.
	ScaleType       Scale;  // See above.
	short   VScrUpdate;     // DO NOT USE (0).
	Ptr     BufPtr; // DO NOT USE (0).
	Size    BufBytes;       // DO NOT USE (0).
	long    BufOffset;      // DO NOT USE (0).
	RgnHandle       WaveRgn;        // DO NOT USE (0).
	RgnHandle       ClipArea;       // DO NOT USE (0).
	RgnHandle       ScaleArea;      // DO NOT USE (0).
	Rect    CtlWidth;       // DO NOT USE (0).
	ControlHandle   VScroll;        // DO NOT USE (0).
	ControlHandle   HScroll;        // DO NOT USE (0).
	Size    FileSize;       // 2 * number of samples in this file ie number
	      //      of bytes of sound data.
	char    BUName[66];     // Name of edit backup file.
	char    FileName[66];   // Name of this file (Mac Filename).
	short   BURefNum;       // DO NOT USE (0).
	short   refNum;         // DO NOT USE (0).
	short   vRefNum;        // DO NOT USE (0).
	Boolean BufChanged;     // DO NOT USE (0).
	Boolean FileChanged;    // DO NOT USE (0).
	Boolean NoBackup;       // DO NOT USE (0).
	enum ModeType   Mode;   // DO NOT USE (0).
	EditRecord      Edit;   // See above.
	long    CursorPos;      // Cursor position relative to window start (0).
	RgnHandle       CursorRgn;      // Cursor region in which it can be grasped (0).
	MarkerType      MarkerData[10]; // See above.
	long    MarkerOffset;   // Offset to get relative time (0).
	long    LoopStart;      // Starting byte # of loop (-1).
	long    LoopEnd;        // Ending byte # of loop (-1).
	Boolean ZeroLineOn;     // (FALSE/0).
	Boolean CursorOn;       // (FALSE/0).
	Boolean ScalesOn;       // (FALSE/0).
	Str255  Comment;        // File comment (" ").
	long    SampRate;       // Sample rate in hertz ie 44100.
	long    SampPeriod;     // Sample period in microseconds.
	short   SampSize;       // Number of bits in a sample (16).
	char    CodeType[34];   // Type of sample data ("Linear").
	Str255  UserStr1;       // For user comments or reserved for future.
	Size    BufSize;        // Size of the RAM wave buffer in bytes.
	long    Loop2Start;     // Release loop start in bytes (-1).
	long    Loop2End;       // Release loop end in bytes (-1).
	Byte    Loop1Type;      // Type of loop: 1 = forward 2 = forward/backward.
	Byte    Loop2Type;      // Type of loop: 1 = forward 2 = forward/backward.
	short   User4;  // DO NOT USE (0).
} SDIHeader;