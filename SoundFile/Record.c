/* Stupid Record - As stupid as it gets */
#if __MC68881__
#define __FASTMATH__
#define __HYBRIDMATH__
#endif	/* __MC68881__ */
#include <math.h>
#include <string.h>

//#include <Sound.h>
//#include <SoundInput.h>
#include "Misc.h"
#include "Record.h"
#include "SoundFile.h"
#include "OpenSoundFile.h"
#include "Dialog.h"
#include "Menu.h"
#include "SoundHack.h"
#include "CarbonGlue.h"
// Used for fixed math types and operations
//#include <FixMath.h>

#ifdef powerc
SICompletionUPP	gDoRecordComplete;
#endif
extern short	gProcessEnabled;
DialogPtr	gRecordDialog;
extern MenuHandle	gSizeMenu, gRateMenu, gChanMenu, gProcessMenu, gFileMenu, gControlMenu, gInputMenu;

SoundInfo		gRecSI;
extern PixPatHandle	gRedPP;
RecordInfo		gRI;
double	gRecordRates[7]={48000.0, 44100.0, 32000.0, 22254.545454, 22050.0, 11127.272727, 11025.0};
unsigned char	gMeterTable[4096];


// Updates a level meter in the record dialog
void
MeterUpdate(void)
{
	short	itemType;
	long	level;
	Handle	itemHandle;
	Rect	itemRect, whiteRect, blackRect;
	Pattern	whitePat;
	long	meterIndex;
	RGBColor	color;


	if(gRecordDialog == nil)
		return;
#if TARGET_API_MAC_CARBON == 1
	SetPort(GetDialogPort(gRecordDialog));
#else
	SetPort(gRecordDialog);
#endif
	GetDialogItem(gRecordDialog, REC_METER_ITEM, &itemType, &itemHandle, &itemRect);
	blackRect.top = whiteRect.top = itemRect.top;
	blackRect.bottom = whiteRect.bottom = itemRect.bottom;
	blackRect.left = itemRect.left;
	whiteRect.right = itemRect.right;
	
	meterIndex = 0;
	if(gRI.sampSizeSelected == 2)
		meterIndex = (4095.0f * gRI.meterLevel) / (32768.0f);
//	else if(gRI.sampSizeSelected == 3)
//		meterIndex = (4095.0f * gRI.meterLevel) / (8388608.0f);
	else if(gRI.sampSizeSelected == 4)
		meterIndex = (4095.0f * gRI.meterLevel) / (2147483648.0f);
	gRI.meterLevel *= 0.9;	
	
	// set up some colors
	color.blue = (unsigned short)(00);
	color.green = (unsigned short)((4095 - meterIndex) * 16);
	color.red = (unsigned short)(meterIndex * 16);
	MakeRGBPat(gRedPP,&color);	

	level = gMeterTable[meterIndex];
	if(level < 0)
		whiteRect.left = blackRect.right = level = 0;
	else
		whiteRect.left = blackRect.right = level;
#if TARGET_API_MAC_CARBON == 1
	GetQDGlobalsWhite(&whitePat);
	FillRect(&whiteRect, &whitePat);
#else
	FillRect(&whiteRect, &(qd.white));
#endif
	FillCRect(&blackRect, gRedPP);
	DrawDialog(gRecordDialog);
}

void
HandleRecordDialog(void)
{
	short		returnValue=false;
	long 		i;
	long		tmpLong;
	long		numDevices, recordDevices;
	short		itemType;
	Handle	itemHandle;
	Rect	itemRect;
	
	
	Str255	deviceName;
	WindInfo	*theWindInfo;
	
	gRI.channelsSelected = 2;
	gRI.sampSizeSelected = 2;
	gRI.recording = false;
	gRI.metering = false;
	gRI.framesPerBuffer = 4096;
	
	// open the default input device for reading and writing
	numDevices = Pa_GetDeviceCount();
	recordDevices = 0;
	for(gRI.deviceID = 0; gRI.deviceID < numDevices; gRI.deviceID++)
	{
		gRI.deviceInfo = Pa_GetDeviceInfo(gRI.deviceID);
		if(gRI.deviceInfo->maxInputChannels > 0)
			recordDevices++;
	}
	if(CountMenuItems(gInputMenu) <= 1)
	{
		for(gRI.deviceID = 0; gRI.deviceID < numDevices; gRI.deviceID++)
		{
			gRI.deviceInfo = Pa_GetDeviceInfo(gRI.deviceID);
			if(gRI.deviceInfo->maxInputChannels > 0)
			{
				strcpy((char *)deviceName, gRI.deviceInfo->name);
				c2pstr((char *)deviceName);
				AppendMenuItemText(gInputMenu, deviceName);
			}
			//if(gRI.deviceInfo->maxInputChannels > 0 && (strcmp(gRI.deviceInfo->name, "Mac OS X Audio HAL") == 0 ||strcmp(gRI.deviceInfo->name, "Built-in") == 0) )
			//	break;
		}
	}
	if(recordDevices == 0)
	{
		DrawErrorMessage("\pYou don't seem to have an input device.");
		return;
	}
	
	gRecordDialog = GetNewDialog(RECORD_DLOG,NIL_POINTER,(WindowPtr)MOVE_TO_FRONT);
	
	theWindInfo = (WindInfo *)NewPtr(sizeof(WindInfo));
	theWindInfo->windType = PROCWIND;
	theWindInfo->structPtr = (Ptr)RECORD_DLOG;

#if TARGET_API_MAC_CARBON == 1
	SetWRefCon(GetDialogWindow(gRecordDialog), (long)theWindInfo);
#else
	SetWRefCon(gRecordDialog, (long)theWindInfo);
#endif

	// fill the meter lookup table
	for(i = 0; i<4096; i++)
	{
		tmpLong = (long)(52.0 *log((float)(i+1)/4096.0));
		if(tmpLong < -190)
			tmpLong = -190;
		gMeterTable[i] = (unsigned char)(190 + tmpLong);
	}


	SelectRecordInput();
	GetDialogItem(gRecordDialog, REC_INPUT_MENU, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 0);


	gProcessEnabled = METER_PROCESS;
	MenuUpdate();
#if TARGET_API_MAC_CARBON == 1
	ShowWindow(GetDialogWindow(gRecordDialog));
	SelectWindow(GetDialogWindow(gRecordDialog));
	SetPort(GetDialogPort(gRecordDialog));
#else
	ShowWindow(gRecordDialog);
	SelectWindow(gRecordDialog);
	SetPort(gRecordDialog);
#endif
}

void
SelectRecordInput(void)
{
	short		itemType;
	Handle	itemHandle;
	Rect	itemRect;
	long numDevices, recordDevices;
	short choice, i, j;
	Str255	deviceName, deviceCName;
	OSErr	status;
	long		response;
    PaStreamParameters inputParameters;
	
	SetMenuItemText(gSizeMenu, 2, "\p32 Bit Linear");	
	numDevices = Pa_GetDeviceCount();
	recordDevices = 0;
	GetDialogItem(gRecordDialog,REC_INPUT_MENU, &itemType, &itemHandle, &itemRect);
	choice = GetControlValue((ControlHandle)itemHandle);
	GetMenuItemText(gInputMenu, choice, deviceName);
	StringCopy(deviceName, deviceCName);
	p2cstr(deviceCName);
	if(strcmp("None Selected", (char *)deviceCName) == 0)
	{
		GetDialogItem(gRecordDialog, REC_RATE_MENU, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle, 255);
		GetDialogItem(gRecordDialog, REC_SIZE_MENU, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle, 255);
		GetDialogItem(gRecordDialog, REC_CHAN_MENU, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle, 255);
		GetDialogItem(gRecordDialog, REC_RECORD_BUTTON, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle, 255);
		GetDialogItem(gRecordDialog, REC_STOP_BUTTON, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle, 255);
		GetDialogItem(gRecordDialog, REC_FILE_BUTTON, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle, 255);
		GetDialogItem(gRecordDialog, REC_CANCEL_BUTTON, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle, 0);
		return;
	}

	
	for(gRI.deviceID = 0; gRI.deviceID < numDevices; gRI.deviceID++)
	{
		gRI.deviceInfo = Pa_GetDeviceInfo(gRI.deviceID);
		if(gRI.deviceInfo->maxInputChannels > 0 && strcmp(gRI.deviceInfo->name, (char *)deviceCName) == 0)
			break;
	}
	
    inputParameters.channelCount = gRI.deviceInfo->maxInputChannels;
    inputParameters.device = gRI.deviceID;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = gRI.deviceInfo->defaultLowInputLatency ;

	// enable the available sample rates and pick the highest as default	
	for(i=6; i>=0; i--)
	{
		if(paFormatIsSupported == Pa_IsFormatSupported( &inputParameters, NULL, gRecordRates[i]))
		{
			EnableItem(gRateMenu, i+1);
			gRI.choiceRate = i+1;
			gRI.sampRateSelected = gRecordRates[i];
			break;
		}
		else
			DisableItem(gRateMenu, i+1);
	}
	GetDialogItem(gRecordDialog, REC_RATE_MENU, &itemType, &itemHandle, &itemRect);
	SetControlValue((ControlHandle)itemHandle, gRI.choiceRate);
	
	// enable the available number of channels and pick the highest as default
	
	EnableItem(gChanMenu, 1);	// there will always be at least one channel
	if(gRI.deviceInfo->maxInputChannels >= STEREO)
	{
		EnableItem(gChanMenu, 2);
		gRI.channelsSelected = 2;
	}
	else
	{
		DisableItem(gChanMenu, 2);
		gRI.channelsSelected = 1;
	}
	// OS X workaround
	status = Gestalt(gestaltSystemVersion, &response);

	if((status!=noErr)||(response>=0x1000))
	{
		DisableItem(gChanMenu, 1);
		EnableItem(gChanMenu, 2);
		gRI.channelsSelected = 2;
	}
	GetDialogItem(gRecordDialog, REC_CHAN_MENU, &itemType, &itemHandle, &itemRect);
	SetControlValue((ControlHandle)itemHandle, gRI.channelsSelected);
	
	// enable the available resolutions and pick the highest as default
    inputParameters.channelCount = gRI.deviceInfo->maxInputChannels;
    inputParameters.device = gRI.deviceID;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.suggestedLatency = gRI.deviceInfo->defaultLowInputLatency ;

	DisableItem(gSizeMenu, 1);
	DisableItem(gSizeMenu, 2);
    inputParameters.sampleFormat = paInt16;
	if(paFormatIsSupported == Pa_IsFormatSupported( &inputParameters, NULL, gRI.sampRateSelected))
	{
		EnableItem(gSizeMenu, 1);
		gRI.choiceSize = 1;
		gRI.formatSelected = paInt16;
		gRI.sampSizeSelected = 2;
	}
    inputParameters.sampleFormat = paInt24;
//	if(paFormatIsSupported == Pa_IsFormatSupported( &inputParameters, NULL, gRI.sampRateSelected))
//	{
//		EnableItem(gSizeMenu, 2);
//		gRI.choiceSize = 2;
//		gRI.formatSelected = paInt24;
//		gRI.sampSizeSelected = 3;
//	}
    inputParameters.sampleFormat = paInt32;
	if(paFormatIsSupported == Pa_IsFormatSupported( &inputParameters, NULL, gRI.sampRateSelected))
	{
		EnableItem(gSizeMenu, 2);
		gRI.choiceSize = 2;
		gRI.formatSelected = paInt32;
		gRI.sampSizeSelected = 4;
	}

	GetDialogItem(gRecordDialog, REC_SIZE_MENU, &itemType, &itemHandle, &itemRect);
	SetControlValue((ControlHandle)itemHandle, gRI.choiceSize);

	
	GetDialogItem(gRecordDialog, REC_RATE_MENU, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 0);
	GetDialogItem(gRecordDialog, REC_SIZE_MENU, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 0);
	GetDialogItem(gRecordDialog, REC_CHAN_MENU, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 0);
	GetDialogItem(gRecordDialog, REC_RECORD_BUTTON, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 255);
	GetDialogItem(gRecordDialog, REC_STOP_BUTTON, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 255);
	GetDialogItem(gRecordDialog, REC_FILE_BUTTON, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 0);
	GetDialogItem(gRecordDialog, REC_CANCEL_BUTTON, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 0);
}
void
HandleRecordDialogEvent(short itemHit)
{
	unsigned long	finalTick;
	short	itemType;
	static Boolean	armed;
	long	theA5, endByte;
	short	sampleSizeBits;
	UnsignedFixed fixedSampleRate;
    PaStreamParameters inputParameters;
	
	Handle	itemHandle;
	OSErr	error;
	Rect	itemRect;
	SHStandardFileReply	reply;
	WindInfo	*myWI;
	PaError err;
	
	theA5 = SetCurrentA5();
	switch(itemHit)
	{
		case REC_INPUT_MENU:
			SelectRecordInput();
			break;
		case REC_RATE_MENU:
			GetDialogItem(gRecordDialog, REC_RATE_MENU, &itemType, &itemHandle, &itemRect);
			gRI.choiceRate = GetControlValue((ControlHandle)itemHandle);
			gRI.sampRateSelected = gRecordRates[gRI.choiceRate - 1];
			break;
		case REC_SIZE_MENU:
			GetDialogItem(gRecordDialog, REC_SIZE_MENU, &itemType, &itemHandle, &itemRect);
			gRI.choiceSize = GetControlValue((ControlHandle)itemHandle);
			if(gRI.choiceSize == 1)
			{
				gRI.sampSizeSelected = 2;
				gRI.formatSelected = paInt16;
			}
			else if(gRI.choiceSize == 2)
//			{
//				gRI.sampSizeSelected = 3;
//				gRI.formatSelected = paInt24;
//			}
//			else if(gRI.choiceSize == 3)
			{
				gRI.sampSizeSelected = 4;
				gRI.formatSelected = paInt32;
			}
			break;
		case REC_CHAN_MENU:
			GetDialogItem(gRecordDialog, REC_CHAN_MENU, &itemType, &itemHandle, &itemRect);
			gRI.channelsSelected = GetControlValue((ControlHandle)itemHandle);
			break;
		case REC_FILE_BUTTON:
			// new file arms recording, 
			SetDialogFont(systemFont);
			SHStandardPutFile("\pSave SoundFile as:","\pUntitled", &reply);
			if(reply.sfGood == true)
			{
				StringCopy(reply.sfFile.name, gRecSI.sfSpec.name);
				gRecSI.sfSpec.parID = reply.sfFile.parID;
				gRecSI.sfSpec.vRefNum = reply.sfFile.vRefNum;
				
				/* Try to open to see if file exists */
				error = FSpOpenDF(&(gRecSI.sfSpec), fsCurPerm, &gRecSI.dFileNum);
				if(error == noErr)
				{
					FSClose(gRecSI.dFileNum);
					FlushVol((StringPtr)NIL_POINTER,gRecSI.sfSpec.vRefNum);
					FSpDelete(&(gRecSI.sfSpec));
					FlushVol((StringPtr)NIL_POINTER,gRecSI.sfSpec.vRefNum);
				}
				error = FSpCreate(&(gRecSI.sfSpec), 'SDHK', 'AIFF', reply.sfScript);
				error = FSpOpenDF(&(gRecSI.sfSpec), fsRdWrPerm, &gRecSI.dFileNum);
				// change to setup AIFF header
				fixedSampleRate = (unsigned long)(gRI.sampRateSelected * 65536);
				sampleSizeBits = gRI.sampSizeSelected * 8;
				error = SetupAIFFHeader(gRecSI.dFileNum, gRI.channelsSelected, fixedSampleRate, sampleSizeBits, kSoundNotCompressed, 0L, 0L);
				GetDialogItem(gRecordDialog, REC_RECORD_BUTTON, &itemType, &itemHandle, &itemRect);
				HiliteControl((ControlHandle)itemHandle, 0);
				GetDialogItem(gRecordDialog, REC_FILE_BUTTON, &itemType, &itemHandle, &itemRect);
				HiliteControl((ControlHandle)itemHandle, 255);
				GetDialogItem(gRecordDialog, REC_RATE_MENU, &itemType, &itemHandle, &itemRect);
				HiliteControl((ControlHandle)itemHandle, 255);
				GetDialogItem(gRecordDialog, REC_SIZE_MENU, &itemType, &itemHandle, &itemRect);
				HiliteControl((ControlHandle)itemHandle, 255);
				GetDialogItem(gRecordDialog, REC_CHAN_MENU, &itemType, &itemHandle, &itemRect);
				HiliteControl((ControlHandle)itemHandle, 255);
				GetDialogItem(gRecordDialog, REC_INPUT_MENU, &itemType, &itemHandle, &itemRect);
				HiliteControl((ControlHandle)itemHandle, 255);
				inputParameters.channelCount = gRI.channelsSelected;
				inputParameters.device = gRI.deviceID;
				inputParameters.hostApiSpecificStreamInfo = NULL;
				inputParameters.sampleFormat = gRI.formatSelected;
				inputParameters.suggestedLatency = gRI.deviceInfo->defaultLowInputLatency ;
				err = Pa_OpenStream(&(gRI.stream), &inputParameters, NULL, gRI.sampRateSelected,
					gRI.framesPerBuffer, NULL, PARecordCallback, &theA5);
				gRI.recording = false;
				gRI.metering = true;
				Pa_StartStream(gRI.stream);
			}
			break;
		case REC_CANCEL_BUTTON:
			gProcessEnabled = NO_PROCESS;
			if(gRI.metering == true)
			{
				Pa_AbortStream(gRI.stream);
				Pa_CloseStream(gRI.stream);
				gRI.metering = false;
			}
#if TARGET_API_MAC_CARBON == 1
			myWI = (WindInfoPtr)GetWRefCon(GetDialogWindow(gRecordDialog));
#else
			myWI = (WindInfoPtr)GetWRefCon(gRecordDialog);
#endif
			RemovePtr((Ptr)myWI);
			DisposeDialog(gRecordDialog);
			gRecordDialog = nil;
			MenuUpdate();
			break;
		case REC_RECORD_BUTTON:
			GetEOF(gRecSI.dFileNum, &endByte);
			SetFPos(gRecSI.dFileNum,fsFromStart,endByte);			
			gRI.inSParmBlk = (ParmBlkPtr) NewPtrClear(sizeof(ParamBlockRec));
			gRI.inSParmBlk->ioParam.ioCompletion = NULL;
			gRI.inSParmBlk->ioParam.ioRefNum = gRecSI.dFileNum;
			gRI.inSParmBlk->ioParam.ioReqCount = (long)(gRI.framesPerBuffer * gRI.sampSizeSelected * gRI.channelsSelected);
			gRI.inSParmBlk->ioParam.ioPosMode = fsAtMark;
			gRI.inSParmBlk->ioParam.ioBuffer = NewPtrClear(gRI.inSParmBlk->ioParam.ioReqCount);
			gRI.inSParmBlk->ioParam.ioResult = 0;
			
			GetDialogItem(gRecordDialog, REC_RECORD_BUTTON, &itemType, &itemHandle, &itemRect);
			HiliteControl((ControlHandle)itemHandle, 255);
			GetDialogItem(gRecordDialog, REC_CANCEL_BUTTON, &itemType, &itemHandle, &itemRect);
			HiliteControl((ControlHandle)itemHandle, 255);
			GetDialogItem(gRecordDialog, REC_STOP_BUTTON, &itemType, &itemHandle, &itemRect);
			HiliteControl((ControlHandle)itemHandle, 0);
			gRI.bytesRecorded = 0;
			gRI.recording = true;
			break;
		case REC_STOP_BUTTON:
			gProcessEnabled = NO_PROCESS;
			gRI.recording = false;
			gRI.metering = false;
			Pa_AbortStream(gRI.stream);
			Pa_CloseStream(gRI.stream);
#if TARGET_API_MAC_CARBON == 1
			myWI = (WindInfoPtr)GetWRefCon(GetDialogWindow(gRecordDialog));
#else
			myWI = (WindInfoPtr)GetWRefCon(gRecordDialog);
#endif
			RemovePtr((Ptr)myWI);
			DisposeDialog(gRecordDialog);
			gRecordDialog = nil;
			DisposePtr(gRI.inSParmBlk->ioParam.ioBuffer);
			DisposePtr((Ptr)(gRI.inSParmBlk));
			// now fill in the number of bytes actually recorded Write the AIFF header again
			// close the input device
			// stop closes dialog box and opens file in regular window.
			Delay(20,&finalTick);
			SetFPos(gRecSI.dFileNum, fsFromStart, 0);			
			fixedSampleRate = (unsigned long)(gRI.sampRateSelected * 65536);
			sampleSizeBits = gRI.sampSizeSelected * 8;
			error = SetupAIFFHeader(gRecSI.dFileNum, gRI.channelsSelected, fixedSampleRate, sampleSizeBits, kSoundNotCompressed, gRI.bytesRecorded, 0);
			FSClose(gRecSI.dFileNum);
			FlushVol((StringPtr)NIL_POINTER,gRecSI.sfSpec.vRefNum);
			if(OpenSoundFile(gRecSI.sfSpec, false) == -1)
				gRecSI.sfSpec.vRefNum = -32768;
			SetDialogFont(systemFont);
			MenuUpdate();
			break;
	}
}

int PARecordCallback(void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
				void *userData )
{
	long	theA5;
	short * shortBlock;
	long * longBlock;
	char * charBlock;
	float sample;
	long i;

	
	theA5 = SetA5(*(long*)userData);
	if(gRI.inBuffer == true)
	{
		theA5 = SetA5(theA5);
		return(1);
	}
	gRI.inBuffer = true;
	switch(gRI.sampSizeSelected)
	{
		case 2:
			shortBlock = (short *)inputBuffer;
			for(i = 0; i < framesPerBuffer * gRI.channelsSelected; i++)
				*(shortBlock+i) = EndianS16_NtoB(*(shortBlock+i));
			break;
		case 3:
			charBlock = (char *)inputBuffer;
			break;
		case 4:
			longBlock = (long *)inputBuffer;
			for(i = 0; i < framesPerBuffer * gRI.channelsSelected; i++)
				*(longBlock+i) = EndianS32_NtoB(*(longBlock+i));
			break;
	}
	if(gRI.recording && gRI.inSParmBlk->ioParam.ioResult == 0)
	{
		BlockMoveData(inputBuffer, gRI.inSParmBlk->ioParam.ioBuffer, gRI.inSParmBlk->ioParam.ioReqCount);
		gRI.error = PBWrite(gRI.inSParmBlk, true);
		gRI.bytesRecorded += gRI.inSParmBlk->ioParam.ioReqCount;
	}
	if(gRI.metering)
	{
		switch(gRI.sampSizeSelected)
		{
			case 2:
				shortBlock = (short *)inputBuffer;
				for(i = 0; i < framesPerBuffer * gRI.channelsSelected; i++)
				{
					if(*(shortBlock+i) > 0)
						sample = *(shortBlock+i);
					else
						sample = -(*(shortBlock+i));
					if(sample > gRI.meterLevel)
						gRI.meterLevel = sample;
				}
				break;
			case 3:
				charBlock = (char *)inputBuffer;
				break;
			case 4:
				longBlock = (long *)inputBuffer;
				for(i = 0; i < framesPerBuffer * gRI.channelsSelected; i++)
				{
					if(*(longBlock+i) > 0)
						sample = *(longBlock+i);
					else
						sample = -(*(longBlock+i));
					if(sample > gRI.meterLevel)
						gRI.meterLevel = sample;
				}
				break;
		}
	}
	gRI.inBuffer = false;
	theA5 = SetA5(theA5);
    return(0);
}
