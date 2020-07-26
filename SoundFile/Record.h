#include "portaudio.h"
/* For Record Dialog display */
#define REC_RATE_MENU		6
#define	REC_SIZE_MENU		7
#define	REC_CHAN_MENU		8
#define	REC_INPUT_MENU		9
#define	REC_FILE_BUTTON		1
#define	REC_CANCEL_BUTTON	2
#define	REC_RECORD_BUTTON	3
#define	REC_STOP_BUTTON		4
#define	REC_METER_ITEM		5

void	MeterUpdate(void);
void	HandleRecordDialog(void);
void	SelectRecordInput(void);
void	HandleRecordDialogEvent(short itemHit);
int 	PARecordCallback(void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
				void *userData );


typedef struct
{
	PaStream *stream;
	ParmBlkPtr inSParmBlk;	
	const PaDeviceInfo	*deviceInfo;
	float	meterLevel;
	long	deviceID;
	long	formatSelected;
	short	choiceRate;
	short	choiceSize;
	double	sampRateSelected;
	short	sampSizeSelected;
	short	channelsSelected;
	long	bytesRecorded;
	long	framesPerBuffer;
	OSErr	error;
	Boolean inBuffer;
	Boolean	recording;
	Boolean metering;
} RecordInfo;
