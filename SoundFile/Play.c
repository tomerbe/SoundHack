// Play.c
// all play routines, probably a quicktime routing and a
// portaudio routine. 
#include <QuickTime/Movies.h>
#include "SoundFile.h"
#include "SoundHack.h"
#include "Menu.h"
#include "ByteSwap.h"
#include "Play.h"
#include "PAPlay.h"
#include "QT2Play.h"
#include "muLaw.h"
#include "ALaw.h"
#include "ADPCMDVI.h"
//#include <Math64.h>


extern ControlHandle	gSliderCntl;
extern MenuHandle		gFileMenu;
extern short			gProcessEnabled;

PlayInfo	gPlayInfo;
PAPlayInfo	gPAPlayInfo;
QTPlayInfo	gQTPlayInfo;

void InitializePlay()
{
	gPlayInfo.playing = false;
	gPlayInfo.looping = false;
	gPlayInfo.player = kNoPlayer;
	gPAPlayInfo.init = Pa_Initialize();
	gQTPlayInfo.theMovie = NULL;
	gQTPlayInfo.theMC = NULL;

}

void TerminatePlay()
{
	if(gPAPlayInfo.init == paNoError)
		Pa_Terminate();
}

Boolean	StartPlay(SoundInfo *theSI, double startTime, double endTime)
{
	Boolean returnValue;
	gPlayInfo.length = (theSI->numBytes/(theSI->frameSize*theSI->nChans*theSI->sRate));
	if(endTime == 0.0)
		endTime = gPlayInfo.length;
	gPlayInfo.file = theSI;
	gPlayInfo.player = GetPlayerType(theSI);
	switch(gPlayInfo.player)
	{
		case kQuickTimePlayer:
			returnValue = StartQTPlay(theSI, startTime, endTime);
			break;
		case kPortAudioPlayer:
			returnValue = StartPAPlay(theSI, startTime, endTime);
			break;
		case kNoPlayer:
			gPlayInfo.file = NULL;
			returnValue = false;
			break;
	}
	gPlayInfo.playing = returnValue;
	return(returnValue);
}

void	UpdatePlay()
{
	switch(gPlayInfo.player)
	{
		case kQuickTimePlayer:
			MoviesTask(gQTPlayInfo.theMovie, 0);
			if(IsMovieDone(gQTPlayInfo.theMovie))
				StopPlay(false);
			break;
		case kPortAudioPlayer:
			if(gPAPlayInfo.done == 2)
			{
				StopPlay(false);
				gPAPlayInfo.done = 0;
			}
			break;
		case kNoPlayer:
			break;
	}
}
void	StopPlay(Boolean wait)
{
	if(gPlayInfo.playing == false)
		return;
	switch(gPlayInfo.player)
	{
		case kQuickTimePlayer:
			StopQTPlay(wait);
			break;
		case kPortAudioPlayer:
			StopPAPlay(wait);
			break;
		case kNoPlayer:
			break;
	}
	gPlayInfo.playing = false;
}

long GetPlayerType(SoundInfo *theSI)
{
	switch(theSI->sfType)
	{
		case AIFF:
		case MPEG:
		case QKTMA:
			switch(theSI->packMode)
			{
				case SF_FORMAT_8_LINEAR:
				case SF_FORMAT_16_LINEAR:
				case SF_FORMAT_24_LINEAR:
				case SF_FORMAT_24_COMP:
				case SF_FORMAT_32_LINEAR:
				case SF_FORMAT_32_COMP:
					return(kPortAudioPlayer);
					break;
			}
			return(kQuickTimePlayer);
			break;
		case AIFC:
			switch(theSI->packMode)
			{
				case SF_FORMAT_8_MULAW:
				case SF_FORMAT_8_ALAW:
				case SF_FORMAT_4_ADIMA:
				case SF_FORMAT_MACE3:
				case SF_FORMAT_MACE6:
					return(kQuickTimePlayer);
					break;
				case SF_FORMAT_8_LINEAR:
				case SF_FORMAT_16_SWAP:
				case SF_FORMAT_16_LINEAR:
				case SF_FORMAT_4_ADDVI:
				case SF_FORMAT_24_COMP:
				case SF_FORMAT_24_LINEAR:
				case SF_FORMAT_32_FLOAT:
				case SF_FORMAT_32_LINEAR:
				case SF_FORMAT_32_COMP:
					return(kPortAudioPlayer);
				default:
					return(kQuickTimePlayer);
					break;
			}
			break;
		case AUDMED:
		case DSPD:
		case SDII:
			return(kPortAudioPlayer);
			break;
		case BICSF:
			return(kPortAudioPlayer);
			break;
		case MMIX:
			return(kPortAudioPlayer);
			break;
		case WAVE:
			switch(theSI->packMode)
			{
				case SF_FORMAT_16_SWAP:
				case SF_FORMAT_24_SWAP:
				case SF_FORMAT_32_SWAP:
					return(kPortAudioPlayer);
					break;
				default:
					return(kQuickTimePlayer);
					break;
			}
			break;
		case NEXT:
		case SUNAU:
			return(kPortAudioPlayer);
			break;
		case TEXT:
		case RAW:
			return(kNoPlayer);
			break;
		default:
			return(kNoPlayer);
	}
	return(kNoPlayer);
}
void SetPlayLooping(Boolean looping)
{
	gPlayInfo.looping = looping;
}

Boolean GetPlayLooping()
{
	return(gPlayInfo.looping);
}

float GetPlayTime()
{
	if(GetPlayState() == false)
		return(0.0f);
	
	if(gPlayInfo.player == kQuickTimePlayer)
	{
		return(GetQTPlayTime());
	}
	else if(gPlayInfo.player == kPortAudioPlayer)
	{
		return(GetPAPlayTime());
	}
	// if we get this far, something is wrong, just return zero
	return(0.0f);
}

Boolean	GetPlayState()
{
	return(gPlayInfo.playing);
}

Boolean	GetFilePlayState(SoundInfo *theSI)
{
	if(gPlayInfo.playing && (gPlayInfo.file == theSI))
		return(true);
	return(false);
}

void FinishPlay(void)
{
	if(gProcessEnabled == PLAY_PROCESS)
	{
		StopPlay(false);
		SetMenuItemText(gFileMenu, PLAY_ITEM, "\pPlay File (space bar)");
	}
}
	
void RestartPlay(void)
{
	float	startTime, frontLength;
	unsigned long	controlValue;
	SoundInfoPtr	tmpSIPtr;
	
	if(gProcessEnabled == PLAY_PROCESS)
	{
		tmpSIPtr = gPlayInfo.file;
		StopPlay(false);
		SetMenuItemText(gFileMenu, PLAY_ITEM, "\pPlay File (space bar)");
		controlValue = GetControlValue(gSliderCntl);
		frontLength = tmpSIPtr->numBytes/(tmpSIPtr->sRate * tmpSIPtr->nChans * tmpSIPtr->frameSize);
		startTime = (controlValue * frontLength)/420.0;
		SetMenuItemText(gFileMenu, PLAY_ITEM, "\pStop Play (space)");
		StartPlay(tmpSIPtr, startTime, 0.0);
		gProcessEnabled = PLAY_PROCESS;
	}
}

////
 // the quicktime routines
////

Boolean StartQTPlay(SoundInfo *theSI, double startTime, double endTime)
{
	OSErr	err;
	short	movieResFile;
	TimeScale myScale;
	TimeValue selectionTime, selectionDuration;
	Rect dummyRect;
	
	dummyRect.left = 0;
	dummyRect.right = 10;
	dummyRect.top = 0;
	dummyRect.bottom = 10;

	err = OpenMovieFile (&theSI->sfSpec, &movieResFile, fsRdPerm);
	if (err == noErr) 
	{
		short		movieResID = 0;		/* want first movie */
		Str255		movieName;
		Boolean		wasChanged;
		
		err = NewMovieFromFile(&gQTPlayInfo.theMovie, movieResFile, &movieResID, movieName, newMovieActive, &wasChanged);
		CloseMovieFile (movieResFile);
	}
	
	// set myScale to current scale of movie, for use in other methods
	// increase the scale for better resolution
	myScale = GetMovieTimeScale(gQTPlayInfo.theMovie);
	if(endTime != 0.0)
	{
		selectionTime = startTime * myScale;
		selectionDuration = (endTime - startTime) * myScale;
		SetMovieActiveSegment(gQTPlayInfo.theMovie, selectionTime, selectionDuration);
	}
	gQTPlayInfo.theMC = NewMovieController(gQTPlayInfo.theMovie, &dummyRect, mcNotVisible);
	if(gPlayInfo.looping)
		MCDoAction(gQTPlayInfo.theMC, mcActionSetLooping, (Ptr) true);
	else
		MCDoAction(gQTPlayInfo.theMC, mcActionSetLooping, (Ptr) false);
	MCDoAction(gQTPlayInfo.theMC, mcActionPlay, NULL);
	StartMovie(gQTPlayInfo.theMovie);
	return(true);
}

float GetQTPlayTime()
{
	TimeValue theTime;
	TimeScale theScale;
	float time = 0.0;
	
	if(gQTPlayInfo.theMovie != NULL)
	{
		theTime = GetMovieTime (gQTPlayInfo.theMovie, NULL);
		theScale = GetMovieTimeScale(gQTPlayInfo.theMovie);
		time = (float)theTime/theScale;
	}
	return(time);
}

void	StopQTPlay(Boolean wait)
{
	StopMovie(gQTPlayInfo.theMovie);
	if(gQTPlayInfo.theMC != NULL)
	{
		DisposeMovieController(gQTPlayInfo.theMC);
		gQTPlayInfo.theMC = NULL;
	}
	if(gQTPlayInfo.theMovie != NULL)
	{
		DisposeMovie(gQTPlayInfo.theMovie);
		gQTPlayInfo.theMovie = NULL;
	}
}

////
 // the port audio routines
////

Boolean StartPAPlay(SoundInfo *theSI, double startTime, double endTime)
{
	PaError err;
	long	theA5;
	
	gPAPlayInfo.framesPerRead = 1024;
	gPAPlayInfo.done = 0;

	gPAPlayInfo.floatBlock = NULL;
	gPAPlayInfo.longBlock = NULL;
	gPAPlayInfo.shortBlock = NULL;
	gPAPlayInfo.fileBlock = NULL;
	gPAPlayInfo.inSParmBlk = NULL;
	gPAPlayInfo.numChannels = theSI->nChans;
	gPAPlayInfo.timeFactor = 1.0/(theSI->frameSize*theSI->nChans*theSI->sRate);
	gPAPlayInfo.startTime = startTime;
	gPAPlayInfo.endTime = endTime;
	
	if(gPAPlayInfo.init != paNoError)
		return(false);
	switch(theSI->packMode)
	{
		case SF_FORMAT_8_MULAW:
		case SF_FORMAT_8_ALAW:
		case SF_FORMAT_8_LINEAR:
		case SF_FORMAT_4_ADIMA:
		case SF_FORMAT_4_ADDVI:
			gPAPlayInfo.sampleFormat = paInt16;
			gPAPlayInfo.shortBlock = (short *)NewPtrClear((long)(gPAPlayInfo.framesPerRead * sizeof(short) * theSI->nChans));
			break;
		case SF_FORMAT_16_LINEAR:
		case SF_FORMAT_16_SWAP:
		case SF_FORMAT_24_COMP:
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_SWAP:
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
		case SF_FORMAT_32_SWAP:
			gPAPlayInfo.gain = 1.0/2147483648.0;
			gPAPlayInfo.sampleFormat = paFloat32;
			gPAPlayInfo.floatBlock = (float *)NewPtrClear((long)(gPAPlayInfo.framesPerRead * sizeof(float) * theSI->nChans));
			break;
		case SF_FORMAT_32_FLOAT:
			gPAPlayInfo.gain = 1.0/theSI->peak;
			gPAPlayInfo.sampleFormat = paFloat32;
			gPAPlayInfo.floatBlock = (float *)NewPtrClear((long)(gPAPlayInfo.framesPerRead * sizeof(float) * theSI->nChans));
			break;
	}
	theA5 = SetCurrentA5();
	err = Pa_OpenDefaultStream( &(gPAPlayInfo.stream), 0, theSI->nChans, gPAPlayInfo.sampleFormat,      
    	theSI->sRate, gPAPlayInfo.framesPerRead,  PAPlayCallback, &theA5);
    if(err != paNoError)
    {
    	StopPAPlay(true);
    	return(false);
    }
    gPAPlayInfo.bytesPerRead = (long)(gPAPlayInfo.framesPerRead * gPlayInfo.file->frameSize * gPlayInfo.file->nChans);
	gPAPlayInfo.startPosition = (long)(startTime * theSI->frameSize * theSI->sRate * theSI->nChans);
	gPAPlayInfo.endPosition = (long)(endTime * theSI->frameSize * theSI->sRate * theSI->nChans);
	// the grossest boundary would be 32-bits by two channels (8-bytes) unless IMA, then on a 34 byte boundary
	if(theSI->packMode == SF_FORMAT_4_ADIMA)
	{
		gPAPlayInfo.startPosition = theSI->dataStart + ((gPAPlayInfo.startPosition/34) * 34);
		gPAPlayInfo.endPosition = theSI->dataStart + ((gPAPlayInfo.endPosition/34) * 34);
	}
	else
	{
		gPAPlayInfo.startPosition = theSI->dataStart + ((gPAPlayInfo.startPosition/24) * 24);
		gPAPlayInfo.endPosition = theSI->dataStart + ((gPAPlayInfo.endPosition/24) * 24);
	}

	gPAPlayInfo.bufferPos = 0;
	gPAPlayInfo.fileBlock = (char *)NewPtr((long)(gPAPlayInfo.framesPerRead * theSI->frameSize * theSI->nChans));
	gPAPlayInfo.inSParmBlk = (ParmBlockA5Ptr) NewPtrClear(sizeof(ParmBlockA5));;
	
	gPAPlayInfo.inSParmBlk->io.ioParam.ioCompletion = NewIOCompletionUPP(DecompressBlock);
	gPAPlayInfo.inSParmBlk->io.ioParam.ioRefNum = theSI->dFileNum;
	gPAPlayInfo.inSParmBlk->io.ioParam.ioReqCount = (long)(gPAPlayInfo.framesPerRead * theSI->frameSize * theSI->nChans);
	gPAPlayInfo.inSParmBlk->io.ioParam.ioPosMode = fsFromStart;
	gPAPlayInfo.inSParmBlk->io.ioParam.ioPosOffset = gPAPlayInfo.startPosition;
	gPAPlayInfo.inSParmBlk->io.ioParam.ioBuffer = gPAPlayInfo.fileBlock;
	gPAPlayInfo.inSParmBlk->theA5 = theA5;
	gPAPlayInfo.fileRead = false;
	if((gPAPlayInfo.inSParmBlk->io.ioParam.ioPosOffset + gPAPlayInfo.bytesPerRead) > gPAPlayInfo.endPosition)
		gPAPlayInfo.inSParmBlk->io.ioParam.ioReqCount = gPAPlayInfo.endPosition - gPAPlayInfo.inSParmBlk->io.ioParam.ioPosOffset;
	PBRead((ParmBlkPtr)(gPAPlayInfo.inSParmBlk), true);
	gPAPlayInfo.tickStart = TickCount() - (long)(startTime * 60) + (long)(((float)gPAPlayInfo.framesPerRead/44100.0f) * 60.0f);
	gPAPlayInfo.looped = false;
	Pa_StartStream(gPAPlayInfo.stream);
	return(true);
}

float	GetPAPlayTime()
{
	float seconds, playTime;
	long ticks;
	
	playTime = gPAPlayInfo.endTime - gPAPlayInfo.startTime;
	ticks = TickCount();
	seconds = (ticks - gPAPlayInfo.tickStart) * 0.01666666666f;
	if(seconds > gPAPlayInfo.endTime)
		seconds -= playTime;
	if(seconds < gPAPlayInfo.startTime)
	{
		if(gPAPlayInfo.looped)
			seconds += playTime;
		else
			seconds = gPAPlayInfo.startTime;
	}
	return(seconds);
}

void	StopPAPlay(Boolean wait)
{
	Pa_AbortStream(gPAPlayInfo.stream);
	Pa_CloseStream(gPAPlayInfo.stream);
	if(gPAPlayInfo.shortBlock != NULL)
		DisposePtr((Ptr)(gPAPlayInfo.shortBlock));
	if(gPAPlayInfo.longBlock != NULL)
		DisposePtr((Ptr)(gPAPlayInfo.longBlock));
	if(gPAPlayInfo.floatBlock != NULL)
		DisposePtr((Ptr)(gPAPlayInfo.floatBlock));
	if(gPAPlayInfo.fileBlock != NULL)
		DisposePtr(gPAPlayInfo.fileBlock);
	if(gPAPlayInfo.inSParmBlk != NULL)
		DisposePtr((Ptr)(gPAPlayInfo.inSParmBlk));
	gPAPlayInfo.floatBlock = NULL;
	gPAPlayInfo.longBlock = NULL;
	gPAPlayInfo.shortBlock = NULL;
	gPAPlayInfo.fileBlock = NULL;
	gPAPlayInfo.inSParmBlk = NULL;

}

int PAPlayCallback(void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
				void *userData )
{
	long	theA5;
	long i;
	
	theA5 = SetA5(*(long*)userData);
	// normally just return 0 to play the entire buffer.
	if(gPAPlayInfo.done == 1)   
	{
		gPAPlayInfo.done = 2;
		theA5 = SetA5(theA5);
		return(1);
	}   
	if(gPAPlayInfo.fileRead == false)
	{
		gPAPlayInfo.tickStart += (long)(((float)framesPerBuffer/44100.0f) * 60.0f);
		theA5 = SetA5(theA5);
		return(0);
	}
	// first copy the data into the port audio output buffer
	switch(gPAPlayInfo.sampleFormat)
	{
		case paInt32:
			BlockMoveData (gPAPlayInfo.longBlock,outputBuffer,(gPAPlayInfo.numChannels * framesPerBuffer * sizeof(long)));
			break;
		case paFloat32:
			BlockMoveData (gPAPlayInfo.floatBlock,outputBuffer,(gPAPlayInfo.numChannels * framesPerBuffer * sizeof(float)));
			break;
		case paInt16:
			BlockMoveData (gPAPlayInfo.shortBlock,outputBuffer,(gPAPlayInfo.numChannels * framesPerBuffer * sizeof(short)));
			break;
	}
	// now prepare for the next read
	
	if(gPAPlayInfo.inSParmBlk->io.ioParam.ioPosOffset >= gPAPlayInfo.endPosition & gPlayInfo.looping == false)
	{
		gPAPlayInfo.done = 1;
	}
	else
	{
	// zero out the block
		for(i = 0; i < gPAPlayInfo.bytesPerRead; i++)
			gPAPlayInfo.fileBlock[i] = 0;
		gPAPlayInfo.fileRead = false;
		if((gPAPlayInfo.inSParmBlk->io.ioParam.ioPosOffset + gPAPlayInfo.bytesPerRead) > gPAPlayInfo.endPosition)
			gPAPlayInfo.inSParmBlk->io.ioParam.ioReqCount = gPAPlayInfo.endPosition - gPAPlayInfo.inSParmBlk->io.ioParam.ioPosOffset;
		PBRead((ParmBlkPtr)(gPAPlayInfo.inSParmBlk), true);
	}
	theA5 = SetA5(theA5);
    return(0);
}



pascal void DecompressBlock (ParmBlkPtr paramBlock)
{
	
	ParmBlockA5Ptr inSParmBlk = (ParmBlockA5*)paramBlock;
	long	theA5;
	int		i,j;
	float * floatBlock;
	long * longBlock;
	short * shortBlock;
	long tmpLong, num24samps;
	long bytesRead;
	short tmpShort;
	unsigned long *longSwap;
	
	theA5 = SetA5(inSParmBlk->theA5);
	if((gPAPlayInfo.inSParmBlk->io.ioParam.ioPosOffset >= gPAPlayInfo.endPosition) 
		&& gPlayInfo.looping)
	{
		gPAPlayInfo.bufferPos += gPAPlayInfo.inSParmBlk->io.ioParam.ioActCount;
		// do a read to fill out the rest of the buffer.
		gPAPlayInfo.inSParmBlk->io.ioParam.ioPosOffset = gPAPlayInfo.startPosition;
		gPAPlayInfo.inSParmBlk->io.ioParam.ioReqCount = gPAPlayInfo.bytesPerRead - gPAPlayInfo.bufferPos;
		gPAPlayInfo.inSParmBlk->io.ioParam.ioBuffer += gPAPlayInfo.inSParmBlk->io.ioParam.ioActCount;
		PBRead(paramBlock, true);
		gPAPlayInfo.tickStart += (long)((gPAPlayInfo.endTime - gPAPlayInfo.startTime) * 60);
		gPAPlayInfo.looped = true;
		theA5 = SetA5(theA5);
		return;
	}

	gPAPlayInfo.bufferPos = 0;
	if(gPlayInfo.looping)
		bytesRead = gPAPlayInfo.bytesPerRead;
	else 
		bytesRead = gPAPlayInfo.inSParmBlk->io.ioParam.ioActCount;
		
	floatBlock = (float *)gPAPlayInfo.fileBlock;
	longBlock = (long *)gPAPlayInfo.fileBlock;
	shortBlock = (short *)gPAPlayInfo.fileBlock;
	// first, decompress whatever we have
	switch(gPlayInfo.file->packMode)
	{
		case SF_FORMAT_8_MULAW:
			Ulaw2ShortBlock((unsigned char *)(gPAPlayInfo.fileBlock), gPAPlayInfo.shortBlock, bytesRead);
			break;
		case SF_FORMAT_8_ALAW:
			Alaw2ShortBlock((unsigned char *)(gPAPlayInfo.fileBlock), gPAPlayInfo.shortBlock, bytesRead);
			break;
		case SF_FORMAT_8_LINEAR:
			for(i = 0; i < bytesRead; i++)
			{
				tmpShort = *(gPAPlayInfo.fileBlock + i);
				*(gPAPlayInfo.shortBlock + i) = tmpShort<<8;
			}
			break;
		case SF_FORMAT_16_LINEAR:
			for(i = 0; i < bytesRead >> 1; i++)
				*(gPAPlayInfo.floatBlock + i) = EndianS16_BtoN(*(shortBlock + i)) * gPAPlayInfo.gain * 65536.0f;
			break;
		case SF_FORMAT_16_SWAP:
			for(i = 0; i < bytesRead >> 1; i++)
				*(gPAPlayInfo.floatBlock + i) = EndianS16_LtoN(*(shortBlock + i)) * gPAPlayInfo.gain * 65536.0f;
			break;
		case SF_FORMAT_4_ADDVI:
			BlockADDVIDecode((unsigned char *)(gPAPlayInfo.fileBlock), gPAPlayInfo.shortBlock, 
				((bytesRead/gPlayInfo.file->nChans) << 1), gPlayInfo.file->nChans, FALSE);
			break;
		case SF_FORMAT_32_FLOAT:
			for(i = 0; i < bytesRead >> 2; i++)
			{
				longSwap = floatBlock+i;
				*longSwap = EndianU32_BtoN(*longSwap);
				*(gPAPlayInfo.floatBlock + i) = *(floatBlock + i) * gPAPlayInfo.gain;
			}
			break;
		case SF_FORMAT_24_COMP:
		case SF_FORMAT_24_LINEAR:
			num24samps = bytesRead * 0.33333333333333;
#if defined (__ppc__) || defined(__ppc64__)
			for(i = j = 0; i < num24samps; i += 4, j += 3)
			{
				tmpLong = *(longBlock+j) & 0xffffff00L;
				*(gPAPlayInfo.floatBlock + i) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong = ((*(longBlock+j) & 0x000000ffL) << 24) + ((*(longBlock+j+1) & 0xffff0000L) >> 8);
				*(gPAPlayInfo.floatBlock + i + 1) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong = ((*(longBlock+j+1) & 0x0000ffffL) << 16) + ((*(longBlock+j+2) & 0xff000000L) >> 16);
				*(gPAPlayInfo.floatBlock + i + 2) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong = ((*(longBlock+j+2) & 0x00ffffffL) << 8);
				*(gPAPlayInfo.floatBlock + i + 3) = tmpLong * gPAPlayInfo.gain;
			}
#elif defined (__i386__)
			for(i = j = 0; i < num24samps; i += 4, j += 3)
			{
				tmpLong = (EndianU32_BtoN(*(longBlock+j) & 0x00ffffffL));
				*(gPAPlayInfo.floatBlock + i) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong =  ((EndianU32_BtoN(*(longBlock+j) & 0xff000000L)) << 24) 
						+ ((EndianU32_BtoN(*(longBlock+j+1) & 0x0000ffffL)) >> 8);
				*(gPAPlayInfo.floatBlock + i + 1) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong =  ((EndianU32_BtoN(*(longBlock+j+1) & 0xffff0000L)) << 16) 
						+ ((EndianU32_BtoN(*(longBlock+j+2) & 0x000000ffL)) >> 16);
				*(gPAPlayInfo.floatBlock + i + 2) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong =  (EndianU32_BtoN(*(longBlock+j+2) & 0xffffff00L)) << 8;
				*(gPAPlayInfo.floatBlock + i + 3) = tmpLong * gPAPlayInfo.gain;
			}
#endif
			break;
		case SF_FORMAT_24_SWAP:
			num24samps = bytesRead * 0.33333333333333;
#if defined (__ppc__) || defined(__ppc64__)
			for(i = j = 0; i < num24samps; i += 4, j += 3)
			{
				tmpLong = (ByteSwapLong(*(longBlock+j)) & 0x00ffffffL) << 8;
				*(gPAPlayInfo.floatBlock + i) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong =  ((ByteSwapLong(*(longBlock+j)) & 0xff000000L) >> 16) 
						+ ((ByteSwapLong(*(longBlock+j+1)) & 0x0000ffffL) << 16);
				*(gPAPlayInfo.floatBlock + i + 1) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong =  ((ByteSwapLong(*(longBlock+j+1)) & 0xffff0000L) >> 8) 
						+ ((ByteSwapLong(*(longBlock+j+2)) & 0x000000ffL) << 24);
				*(gPAPlayInfo.floatBlock + i + 2) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong =  ((ByteSwapLong(*(longBlock+j+2)) & 0xffffff00L));
				*(gPAPlayInfo.floatBlock + i + 3) = tmpLong * gPAPlayInfo.gain;
			}
#elif defined (__i386__)
			for(i = j = 0; i < num24samps; i += 4, j += 3)
			{
				tmpLong = (*(longBlock+j) & 0x00ffffffL) << 8;
				*(gPAPlayInfo.floatBlock + i) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong = ((*(longBlock+j) & 0xff000000L) >> 16) + ((*(longBlock+j+1) & 0x0000ffffL) << 16);
				*(gPAPlayInfo.floatBlock + i + 1) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong = ((*(longBlock+j+1) & 0xffff0000L) >> 8) + ((*(longBlock+j+2) & 0x000000ffL) << 24);
				*(gPAPlayInfo.floatBlock + i + 2) = tmpLong * gPAPlayInfo.gain;
				if(i == num24samps)
					break;
				tmpLong = (*(longBlock+j+2) & 0xffffff00L);
				*(gPAPlayInfo.floatBlock + i + 3) = tmpLong * gPAPlayInfo.gain;
			}
#endif
			break;
		case SF_FORMAT_32_COMP:
		case SF_FORMAT_32_LINEAR:
			for(i = 0; i < bytesRead >> 2; i++)
				*(gPAPlayInfo.floatBlock + i) = EndianS32_BtoN(*(longBlock + i)) * gPAPlayInfo.gain;
			break;
		case SF_FORMAT_32_SWAP:
			for(i = 0; i < bytesRead >> 2; i++)
				*(gPAPlayInfo.floatBlock + i) = EndianS32_LtoN(*(longBlock + i)) * gPAPlayInfo.gain;
			break;
	}
	// reset the request count for the next buffer
	gPAPlayInfo.inSParmBlk->io.ioParam.ioBuffer = gPAPlayInfo.fileBlock;
	gPAPlayInfo.inSParmBlk->io.ioParam.ioReqCount = gPAPlayInfo.bytesPerRead;
	gPAPlayInfo.fileRead = true;
	theA5 = SetA5(theA5);
}

