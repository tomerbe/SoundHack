#include "portaudio.h"
typedef struct
{
	ParamBlockRec	io;
	long theA5;
}	ParmBlockA5;

typedef ParmBlockA5 *ParmBlockA5Ptr;

typedef struct
{
	PaError	init;
	Boolean fileRead;
	Boolean	looped;
	long bufferPos;
	long numChannels;
	long tickStart;
	long done;
	double timeFactor;
	double gain;
	PaStream *stream;
	ParmBlockA5Ptr inSParmBlk;	
	PaSampleFormat	sampleFormat;
	long	startPosition;
	long	endPosition;
	float	startTime;
	float	endTime;
	long	framesPerRead;
	long	bytesPerRead;
	char	*fileBlock;
	short	*shortBlock;
	long	*longBlock;
	float	*floatBlock;
}	PAPlayInfo;


Boolean	StartPAPlay(SoundInfo *mySI, double startTime, double endTime);
float	GetPAPlayTime();
void	StopPAPlay(Boolean wait);
int PAPlayCallback(void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
				void *userData );
pascal void DecompressBlock (ParmBlkPtr paramBlock);
