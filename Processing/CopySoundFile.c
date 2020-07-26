//#include <AppleEvents.h>
//#include <SoundComponents.h>
#include <QuickTime/Movies.h>
#include "SoundFile.h"
#include "AppleCompression.h"
#include "Dialog.h"
#include "Menu.h"
#include "SoundHack.h"
#include "CreateSoundFile.h"
#include "WriteHeader.h"
#include "CopySoundFile.h"
#include "Misc.h"
#include "Markers.h"

extern MenuHandle	gAppleMenu, gFileMenu, gEditMenu, gProcessMenu, gControlMenu;
extern SoundInfoPtr	firstSIPtr, frontSIPtr, inSIPtr, outSIPtr, lastSIPtr, lastInSIPtr;
extern short			gProcessEnabled, gProcessDisabled, gStopProcess;
float		*copyBlock1, *copyBlock2, *copyBlock3, *copyBlock4;
extern float		gScale, gScaleL, gScaleR, gScaleDivisor;
extern struct
{
	Str255	editorName;
	long	editorID;
	short	openPlay;
	short	procPlay;
	short	defaultType;
	short	defaultFormat;
}	gPreferences;
long				gNumberBlocks, gBlockSize;
SoundInfoPtr		*multSIPtr;
char		*splitIn, *splitOut1, *splitOut2, *splitOut3, *splitOut4;

short
InitCopySoundFile(void)
{		
	gNumberBlocks = 0;
	outSIPtr = nil;
	outSIPtr = (SoundInfo *)NewPtr(sizeof(SoundInfo));
	if(gPreferences.defaultType == 0)
	{
		if(inSIPtr->sfType == CS_PVOC || inSIPtr->sfType == PICT || inSIPtr->packMode == SF_FORMAT_3DO_CONTENT)
		{
			outSIPtr->sfType = AIFF;
			outSIPtr->packMode = SF_FORMAT_16_LINEAR;
		}
		else
		{
			outSIPtr->sfType = inSIPtr->sfType;
			outSIPtr->packMode = inSIPtr->packMode;
		}
	}
	else
	{
		outSIPtr->sfType = gPreferences.defaultType;
		outSIPtr->packMode = gPreferences.defaultFormat;
	}
	NameFile(inSIPtr->sfSpec.name, "\pCPY", outSIPtr->sfSpec.name);
	
	outSIPtr->sRate = inSIPtr->sRate;
	outSIPtr->nChans = inSIPtr->nChans;
	outSIPtr->numBytes = 0;
	if(CreateSoundFile(&outSIPtr, SOUND_CUST_DIALOG) == -1)
	{
		gProcessDisabled = gProcessEnabled = NO_PROCESS;
		MenuUpdate();
		RemovePtr((Ptr)outSIPtr);
		outSIPtr = nil;
		inSIPtr = nil;
		return(-1);
	}
	outSIPtr->peak = inSIPtr->peak;
	outSIPtr->peakFL = inSIPtr->peakFL;
	outSIPtr->peakFR = inSIPtr->peakFR;
	outSIPtr->peakRL = inSIPtr->peakRL;
	outSIPtr->peakRR = inSIPtr->peakRR;
	WriteHeader(outSIPtr);
	UpdateInfoWindow(outSIPtr);
		
	gBlockSize = 8192L;
	copyBlock1 = copyBlock2 = copyBlock3 = copyBlock4 = nil;

	copyBlock1 = (float *)NewPtr(gBlockSize * 2 * sizeof(float));
	copyBlock2 = (float *)NewPtr(gBlockSize * 2 * sizeof(float));
	copyBlock3 = (float *)NewPtr(gBlockSize * 2 * sizeof(float));
	copyBlock4 = (float *)NewPtr(gBlockSize * 2 * sizeof(float));
	if(inSIPtr->packMode == SF_FORMAT_32_FLOAT)
		gScaleDivisor = 1.0/inSIPtr->peak;
	else
		gScaleDivisor = 1.0;

	SetOutputScale(outSIPtr->packMode);
	
	SetFPos(inSIPtr->dFileNum, fsFromStart, inSIPtr->dataStart);
	SetFPos(outSIPtr->dFileNum, fsFromStart, outSIPtr->dataStart);
		
	UpdateProcessWindow("\p", "\p","\pcopying file", 0.0);
	
	gProcessEnabled = COPY_PROCESS;
	MenuUpdate();
	return(TRUE);
}

short	CopyBlock(void)
{
	long	readSize, writeSize, dataLeft;
	Str255	errStr;
	float	length;
	OSErr	error;
	
	if(gStopProcess == TRUE)
	{
		FinishCopySoundFile();
		return(-1);
	}
	if(inSIPtr->nChans == MONO)
	{
		if(inSIPtr->packMode != SF_FORMAT_TEXT)
			SetFPos(inSIPtr->dFileNum, fsFromStart, (inSIPtr->dataStart 
				+ (long)(gBlockSize * inSIPtr->frameSize * gNumberBlocks)));
		if(outSIPtr->packMode != SF_FORMAT_TEXT)
			SetFPos(outSIPtr->dFileNum, fsFromStart, (outSIPtr->dataStart 
				+ (long)(gBlockSize * outSIPtr->frameSize * gNumberBlocks)));
		readSize = ReadMonoBlock(inSIPtr, gBlockSize, copyBlock1);
		length = (readSize + (gNumberBlocks * gBlockSize))/inSIPtr->sRate;
		HMSTimeString(length, errStr);
		length = length / (inSIPtr->numBytes/(inSIPtr->sRate * inSIPtr->nChans * inSIPtr->frameSize));
		UpdateProcessWindow(errStr,"\p","\p", length);
		writeSize = WriteMonoBlock(outSIPtr, readSize, copyBlock1);
	} 
	else
	{
		if(inSIPtr->packMode != SF_FORMAT_TEXT)
			SetFPos(inSIPtr->dFileNum, fsFromStart, (inSIPtr->dataStart 
				+ (long)(gBlockSize * inSIPtr->frameSize * gNumberBlocks * inSIPtr->nChans)));
		if(outSIPtr->packMode != SF_FORMAT_TEXT)
			SetFPos(outSIPtr->dFileNum, fsFromStart, (outSIPtr->dataStart 
				+ (long)(gBlockSize * outSIPtr->frameSize * gNumberBlocks * outSIPtr->nChans)));
		readSize = ReadStereoBlock(inSIPtr, gBlockSize, copyBlock1, copyBlock2);
		length = (readSize + (gNumberBlocks * gBlockSize))/inSIPtr->sRate;
		HMSTimeString(length, errStr);
		length = length / (inSIPtr->numBytes/(inSIPtr->sRate * inSIPtr->nChans * inSIPtr->frameSize));
		UpdateProcessWindow(errStr,"\p","\p",length);
		writeSize = WriteStereoBlock(outSIPtr, readSize, copyBlock1, copyBlock2);
	}
	length = (float)(outSIPtr->numBytes/(outSIPtr->frameSize*outSIPtr->nChans*outSIPtr->sRate));
	HMSTimeString(length, errStr);
	UpdateProcessWindow("\p", errStr, "\p", -1.0);
	gNumberBlocks++;
	if(outSIPtr->packMode == SF_FORMAT_MACE3 || inSIPtr->packMode == SF_FORMAT_MACE3)
	{
		if(writeSize+3 < gBlockSize)
			FinishCopySoundFile();
	}
	else if(outSIPtr->packMode == SF_FORMAT_4_ADIMA || inSIPtr->packMode == SF_FORMAT_4_ADIMA)
	{
		if(writeSize+4 < gBlockSize)
			FinishCopySoundFile();
	}
	else if(outSIPtr->packMode == SF_FORMAT_MACE6 || inSIPtr->packMode == SF_FORMAT_MACE6)
	{
		if(writeSize+6 < gBlockSize)
			FinishCopySoundFile();
	}
	else if(outSIPtr->packMode == SF_FORMAT_MPEG_III || inSIPtr->packMode == SF_FORMAT_MPEG_III ||
			outSIPtr->packMode == SF_FORMAT_MPEG_II || inSIPtr->packMode == SF_FORMAT_MPEG_II ||
			outSIPtr->packMode == SF_FORMAT_MPEG_I || inSIPtr->packMode == SF_FORMAT_MPEG_I)
	{
		if(writeSize+30 < gBlockSize)
			FinishCopySoundFile();
	}
	else
	{
		if(writeSize != gBlockSize)
			FinishCopySoundFile();
	}

	return(TRUE);
}

void	FinishCopySoundFile(void)
{
		if(outSIPtr->packMode == SF_FORMAT_4_ADIMA || outSIPtr->packMode == SF_FORMAT_MACE3 || outSIPtr->packMode == SF_FORMAT_MACE6 || 
			outSIPtr->packMode == SF_FORMAT_MPEG_III || outSIPtr->packMode == SF_FORMAT_MPEG_II || outSIPtr->packMode == SF_FORMAT_MPEG_I ) 
			Float2AppleCompressed(outSIPtr, 0, nil, nil);
		WriteHeader(outSIPtr);
		CopyMarkers();
		gNumberBlocks = 0;
		FinishProcess();
		DeAllocCopyMemory();
}

void	DeAllocCopyMemory(void)
{
	RemovePtr((Ptr)copyBlock1);
	RemovePtr((Ptr)copyBlock2);
	RemovePtr((Ptr)copyBlock3);
	RemovePtr((Ptr)copyBlock4);
	copyBlock1 = copyBlock2 = copyBlock3 = copyBlock4 = nil;
}

short
InitSplitSoundFile(void)
{	
	long 	i, allocSize;
	Str255	tmpAStr, tmpBStr;
	
	switch(inSIPtr->nChans)
	{
		case 1:
			DrawErrorMessage("\pYou cannot split one channel into mono files!");
			return(-1);
			break;
		case 2:
		case 4:
			break;
		case 3:
			DrawErrorMessage("\pDo you really have a three channel soundfile? Send me email (tre@music.ucsd.edu)");
			return(-1);
			break;
		default:
			DrawErrorMessage("\pCannot split more than 4 channels at present");
			return(-1);
			break;
	}
	
	if(inSIPtr->frameSize == 3.0)
	{
		DrawErrorMessage("\pCannot presently split 24-bit soundfiles. Save as 32-bit first");
		return(-1);
	}
	multSIPtr = nil;
	multSIPtr = (SoundInfoPtr *)NewPtr(sizeof(SoundInfoPtr) * inSIPtr->nChans);
	
	gNumberBlocks = 0;
	for(i=0; i<inSIPtr->nChans; i++)
	{
		(*(multSIPtr + i)) = (SoundInfo *)NewPtr(sizeof(SoundInfo));
		if(gPreferences.defaultType == 0)
		{
			(*(multSIPtr + i))->sfType = inSIPtr->sfType;
			(*(multSIPtr + i))->packMode = inSIPtr->packMode;
		}
		else
		{
			(*(multSIPtr + i))->sfType = gPreferences.defaultType;
			(*(multSIPtr + i))->packMode = gPreferences.defaultFormat;
		}
		(*(multSIPtr + i))->frameSize = inSIPtr->frameSize;
		NumToString((i+1),tmpBStr);
		StringAppend(inSIPtr->sfSpec.name, "\pCH", tmpAStr);
		StringAppend(tmpAStr, tmpBStr, (*(multSIPtr + i))->sfSpec.name);
		(*(multSIPtr + i))->sRate = inSIPtr->sRate;
		(*(multSIPtr + i))->nChans = 1;
		(*(multSIPtr + i))->numBytes = 0;
		if(CreateSoundFile((multSIPtr + i), SOUND_CUST_DIALOG) == -1)
		{
			gProcessDisabled = gProcessEnabled = NO_PROCESS;
			MenuUpdate();
			RemovePtr((Ptr)multSIPtr);
			multSIPtr = nil;
			inSIPtr = nil;
			return(-1);
		}
		WriteHeader((*(multSIPtr + i)));
		UpdateInfoWindow((*(multSIPtr + i)));
	}
	
	gBlockSize = 8192L;
	copyBlock1 = copyBlock2 = copyBlock3 = copyBlock4 = nil;

	copyBlock1 = (float *)NewPtr(gBlockSize * sizeof(float));
	copyBlock2 = (float *)NewPtr(gBlockSize * sizeof(float));
	copyBlock3 = (float *)NewPtr(gBlockSize * sizeof(float));
	copyBlock4 = (float *)NewPtr(gBlockSize * sizeof(float));
	if(inSIPtr->packMode == SF_FORMAT_32_FLOAT)
		gScaleDivisor = 1.0/inSIPtr->peak;
	else
		gScaleDivisor = 1.0;

	
	allocSize = (long)(gBlockSize * inSIPtr->frameSize);
	splitIn = (char *)NewPtr(allocSize * inSIPtr->nChans);
	splitOut1 = (char *)NewPtr(allocSize);
	splitOut2 = (char *)NewPtr(allocSize);
	if(inSIPtr->nChans == 4)
	{
		splitOut3 = (char *)NewPtr(allocSize);
		splitOut4 = (char *)NewPtr(allocSize);
	}
	gScaleDivisor = 1.0;
	SetOutputScale((*(multSIPtr + 0))->packMode);

	SetFPos(inSIPtr->dFileNum, fsFromStart, inSIPtr->dataStart);
	SetFPos((*(multSIPtr + 0))->dFileNum, fsFromStart, (*(multSIPtr + 0))->dataStart);
	SetFPos((*(multSIPtr + 1))->dFileNum, fsFromStart, (*(multSIPtr + 1))->dataStart);
	if(inSIPtr->nChans == 4)
	{
		SetFPos((*(multSIPtr + 2))->dFileNum, fsFromStart, (*(multSIPtr + 2))->dataStart);
		SetFPos((*(multSIPtr + 3))->dFileNum, fsFromStart, (*(multSIPtr + 3))->dataStart);
	}
	UpdateProcessWindow("\p", "\p", "\psplitting into mono files", 0.0);
	
	gProcessDisabled = NO_PROCESS;
	gProcessEnabled = SPLIT_PROCESS;
	MenuUpdate();
	return(TRUE);
}

short	SplitBlock(void)
{
	long	readSize, writeSize, dataLeft;
	Str255	errStr;
	float	length;
	OSErr	error;
	
	if(gStopProcess == TRUE)
	{
		WriteHeader((*(multSIPtr + 0)));
		UpdateInfoWindow((*(multSIPtr + 0)));
		WriteHeader((*(multSIPtr + 1)));
		UpdateInfoWindow((*(multSIPtr + 1)));
		if(inSIPtr->nChans == 4)
		{
			WriteHeader((*(multSIPtr + 2)));
			UpdateInfoWindow((*(multSIPtr + 2)));
			WriteHeader((*(multSIPtr + 3)));
			UpdateInfoWindow((*(multSIPtr + 3)));
		}
		DeAllocSplitMemory();
		FinishProcess();
		gNumberBlocks = 0;
		return(-1);
	}

	if(inSIPtr->nChans == STEREO)
	{
		if(inSIPtr->packMode != SF_FORMAT_TEXT)
			SetFPos(inSIPtr->dFileNum, fsFromStart, (inSIPtr->dataStart 
				+ (long)(gBlockSize * inSIPtr->frameSize * gNumberBlocks * inSIPtr->nChans)));
		readSize = ReadStereoBlock(inSIPtr, gBlockSize, copyBlock1, copyBlock2);
		length = (readSize + (gNumberBlocks * gBlockSize))/inSIPtr->sRate;
		HMSTimeString(length, errStr);
		length = length / (inSIPtr->numBytes/(inSIPtr->sRate * inSIPtr->nChans * inSIPtr->frameSize));
		UpdateProcessWindow(errStr,"\p","\p",length);
		if((*(multSIPtr + 0))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 0))->dFileNum, fsFromStart, ((*(multSIPtr + 0))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 0))->frameSize * gNumberBlocks)));
		if((*(multSIPtr + 1))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 1))->dFileNum, fsFromStart, ((*(multSIPtr + 1))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 1))->frameSize * gNumberBlocks)));
		writeSize = WriteMonoBlock(*(multSIPtr + 0), readSize, copyBlock1);
		writeSize = WriteMonoBlock(*(multSIPtr + 1), readSize, copyBlock2);
	}
	else if(inSIPtr->nChans == QUAD)
	{
		if(inSIPtr->packMode != SF_FORMAT_TEXT)
			SetFPos(inSIPtr->dFileNum, fsFromStart, (inSIPtr->dataStart 
				+ (long)(gBlockSize * inSIPtr->frameSize * gNumberBlocks * inSIPtr->nChans)));
		readSize = ReadQuadBlock(inSIPtr, gBlockSize, copyBlock1, copyBlock2, copyBlock3, copyBlock4);
		length = (readSize + (gNumberBlocks * gBlockSize))/inSIPtr->sRate;
		HMSTimeString(length, errStr);
		length = length / (inSIPtr->numBytes/(inSIPtr->sRate * inSIPtr->nChans * inSIPtr->frameSize));
		UpdateProcessWindow(errStr,"\p","\p",length);
		if((*(multSIPtr + 0))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 0))->dFileNum, fsFromStart, ((*(multSIPtr + 0))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 0))->frameSize * gNumberBlocks)));
		if((*(multSIPtr + 1))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 1))->dFileNum, fsFromStart, ((*(multSIPtr + 1))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 1))->frameSize * gNumberBlocks)));
		if((*(multSIPtr + 2))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 2))->dFileNum, fsFromStart, ((*(multSIPtr + 2))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 2))->frameSize * gNumberBlocks)));
		if((*(multSIPtr + 3))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 3))->dFileNum, fsFromStart, ((*(multSIPtr + 3))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 3))->frameSize * gNumberBlocks)));
		writeSize = WriteMonoBlock(*(multSIPtr + 0), readSize, copyBlock1);
		writeSize = WriteMonoBlock(*(multSIPtr + 1), readSize, copyBlock2);
		writeSize = WriteMonoBlock(*(multSIPtr + 2), readSize, copyBlock3);
		writeSize = WriteMonoBlock(*(multSIPtr + 3), readSize, copyBlock4);
	}
	length = (float)( (*(multSIPtr + 0))->numBytes/((*(multSIPtr + 0))->frameSize* (*(multSIPtr + 0))->nChans * (*(multSIPtr + 0))->sRate));
	HMSTimeString(length, errStr);
	UpdateProcessWindow("\p", errStr, "\p", -1.0);
	gNumberBlocks++;
	
	// finish routine
	if((*(multSIPtr + 0))->packMode == SF_FORMAT_MACE3 || inSIPtr->packMode == SF_FORMAT_MACE3)
	{
		if(writeSize+3 < gBlockSize)
			FinishSplitSoundFile();
	}
	else if((*(multSIPtr + 0))->packMode == SF_FORMAT_4_ADIMA || inSIPtr->packMode == SF_FORMAT_4_ADIMA)
	{
		if(writeSize+4 < gBlockSize)
			FinishSplitSoundFile();
	}
	else if((*(multSIPtr + 0))->packMode == SF_FORMAT_MACE6 || inSIPtr->packMode == SF_FORMAT_MACE6)
	{
		if(writeSize+6 < gBlockSize)
			FinishSplitSoundFile();
	}
	else if((*(multSIPtr + 0))->packMode == SF_FORMAT_MPEG_III || inSIPtr->packMode == SF_FORMAT_MPEG_III ||
			(*(multSIPtr + 0))->packMode == SF_FORMAT_MPEG_II || inSIPtr->packMode == SF_FORMAT_MPEG_II ||
			(*(multSIPtr + 0))->packMode == SF_FORMAT_MPEG_I || inSIPtr->packMode == SF_FORMAT_MPEG_I)
	{
		if(writeSize+30 < gBlockSize)
			FinishSplitSoundFile();
	}
	else
	{
		if(writeSize != gBlockSize)
			FinishSplitSoundFile();
	}

	return(TRUE);
}

/*short
SplitBlock(void)
{
	long	readSize, writeSize, dataLeft, i, j;
	Str255	errStr;
	OSErr	error;
	float	length;
	
	if(gStopProcess == TRUE)
	{
		WriteHeader((*(multSIPtr + 0)));
		UpdateInfoWindow((*(multSIPtr + 0)));
		WriteHeader((*(multSIPtr + 1)));
		UpdateInfoWindow((*(multSIPtr + 1)));
		if(inSIPtr->nChans == 4)
		{
			WriteHeader((*(multSIPtr + 2)));
			UpdateInfoWindow((*(multSIPtr + 2)));
			WriteHeader((*(multSIPtr + 3)));
			UpdateInfoWindow((*(multSIPtr + 3)));
		}
		DeAllocSplitMemory();
		FinishProcess();
		gNumberBlocks = 0;
		return(-1);
	}
	if(inSIPtr->packMode != SF_FORMAT_TEXT)
		SetFPos(inSIPtr->dFileNum, fsFromStart, (inSIPtr->dataStart 
			+ (long)(gBlockSize * inSIPtr->frameSize * gNumberBlocks * inSIPtr->nChans)));
	dataLeft = inSIPtr->numBytes - (GetPtrSize((Ptr)splitIn) * gNumberBlocks);
	readSize = GetPtrSize((Ptr)splitIn);
	if(readSize > dataLeft)
		readSize = dataLeft;
	//U.B. - none of this should work....
	FSRead(inSIPtr->dFileNum, &readSize, splitIn);
		
	length = readSize/(inSIPtr->sRate*inSIPtr->nChans*inSIPtr->frameSize) + (gNumberBlocks * gBlockSize)/(inSIPtr->sRate);
	HMSTimeString(length, errStr);
	length = length / (inSIPtr->numBytes/(inSIPtr->sRate * inSIPtr->nChans * inSIPtr->frameSize));
	UpdateProcessWindow(errStr, "\p", "\p", length);
	
	// stereo and quad routines
	if(inSIPtr->nChans == 2)
	{
		if((*(multSIPtr + 0))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 0))->dFileNum, fsFromStart, ((*(multSIPtr + 0))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 0))->frameSize * gNumberBlocks)));
		if((*(multSIPtr + 1))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 1))->dFileNum, fsFromStart, ((*(multSIPtr + 1))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 1))->frameSize * gNumberBlocks)));
		
		for(i=0, j=0; i < gBlockSize; i++, j+=2)
		{
			if(inSIPtr->frameSize == 0.5)
			{
				*(char *)(splitOut1+i) = (*(char *)(splitIn+j) & 0x0f) + ((*(char *)(splitIn+j+1) & 0x0f) << 1);
				*(char *)(splitOut2+i) = ((*(char *)(splitIn+j) & 0xf0) >> 1) + (*(char *)(splitIn+j+1) & 0xf0);
			}
			else if(inSIPtr->frameSize == 1.0)
			{
				*(char *)(splitOut1+i) = *(char *)(splitIn+j);
				*(char *)(splitOut2+i) = *(char *)(splitIn+j+1);
			}
			else if(inSIPtr->frameSize == 2.0)
			{
				*(short *)(splitOut1+(i<<1)) = *(short *)(splitIn+(j<<1));
				*(short *)(splitOut2+(i<<1)) = *(short *)(splitIn+((j+1)<<1));
			}
			else if(inSIPtr->frameSize == 4.0)
			{
				*(long *)(splitOut1+(i<<2)) = *(long *)(splitIn + (j << 2));
				*(long *)(splitOut2+(i<<2)) = *(long *)(splitIn + ((j+1) << 2));
			}
		}
		
		writeSize = readSize>>1;
		
		error = FSWrite((*(multSIPtr + 0))->dFileNum, &writeSize, splitOut1);
		
		if(error == dskFulErr)
			DrawErrorMessage("\pDisk Full!");
		else if(error != 0)
			DrawErrorMessage("\pWrite Error");
		(*(multSIPtr + 0))->numBytes += writeSize;
		
		error = FSWrite((*(multSIPtr + 1))->dFileNum, &writeSize, splitOut2);
		
		if(error == dskFulErr)
			DrawErrorMessage("\pDisk Full!");
		else if(error != 0)
			DrawErrorMessage("\pWrite Error");
		(*(multSIPtr + 1))->numBytes += writeSize;
	}
	else if(inSIPtr->nChans == 4)
	{
		if((*(multSIPtr + 0))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 0))->dFileNum, fsFromStart, ((*(multSIPtr + 0))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 0))->frameSize * gNumberBlocks)));
		if((*(multSIPtr + 1))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 1))->dFileNum, fsFromStart, ((*(multSIPtr + 1))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 1))->frameSize * gNumberBlocks)));
		if((*(multSIPtr + 2))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 2))->dFileNum, fsFromStart, ((*(multSIPtr + 2))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 2))->frameSize * gNumberBlocks)));
		if((*(multSIPtr + 3))->packMode != SF_FORMAT_TEXT)
			SetFPos((*(multSIPtr + 3))->dFileNum, fsFromStart, ((*(multSIPtr + 3))->dataStart 
				+ (long)(gBlockSize * (*(multSIPtr + 3))->frameSize * gNumberBlocks)));
		
		for(i=0, j=0; i < gBlockSize; i++, j+=4)
		{
			if(inSIPtr->frameSize == 0.5)
			{
				*(char *)(splitOut1+i) = (*(char *)(splitIn+j) & 0x0f) + ((*(char *)(splitIn+j+1) & 0x0f) << 1);
				*(char *)(splitOut2+i) = ((*(char *)(splitIn+j) & 0xf0) >> 1) + (*(char *)(splitIn+j+1) & 0xf0);
				*(char *)(splitOut3+i) = (*(char *)(splitIn+j+2) & 0x0f) + ((*(char *)(splitIn+j+3) & 0x0f) << 1);
				*(char *)(splitOut4+i) = ((*(char *)(splitIn+j+2) & 0xf0) >> 1) + (*(char *)(splitIn+j+3) & 0xf0);
			}
			else if(inSIPtr->frameSize == 1.0)
			{
				*(char *)(splitOut1+i) = *(char *)(splitIn+j);
				*(char *)(splitOut2+i) = *(char *)(splitIn+j+1);
				*(char *)(splitOut3+i) = *(char *)(splitIn+j+2);
				*(char *)(splitOut4+i) = *(char *)(splitIn+j+3);
			}
			else if(inSIPtr->frameSize == 2.0)
			{
				*(short *)(splitOut1+(i<<1)) = *(short *)(splitIn+(j<<1));
				*(short *)(splitOut2+(i<<1)) = *(short *)(splitIn+((j+1)<<1));
				*(short *)(splitOut3+(i<<1)) = *(short *)(splitIn+((j+2)<<1));
				*(short *)(splitOut4+(i<<1)) = *(short *)(splitIn+((j+3)<<1));
			}
			else if(inSIPtr->frameSize == 4.0)
			{
				*(long *)(splitOut1+(i<<2)) = *(long *)(splitIn + (j<<2));
				*(long *)(splitOut2+(i<<2)) = *(long *)(splitIn + ((j+1)<<2));
				*(long *)(splitOut3+(i<<2)) = *(long *)(splitIn + ((j+2)<<2));
				*(long *)(splitOut4+(i<<2)) = *(long *)(splitIn + ((j+3)<<2));
			}
		}
				
		writeSize = readSize>>2;
		
		error = FSWrite((*(multSIPtr + 0))->dFileNum, &writeSize, splitOut1);
		
		if(error == dskFulErr)
			DrawErrorMessage("\pDisk Full!");
		else if(error != 0)
			DrawErrorMessage("\pWrite Error");
		(*(multSIPtr + 0))->numBytes += writeSize;
		
		writeSize = readSize>>2;

		error = FSWrite((*(multSIPtr + 1))->dFileNum, &writeSize, splitOut2);
		
		if(error == dskFulErr)
			DrawErrorMessage("\pDisk Full!");
		else if(error != 0)
			DrawErrorMessage("\pWrite Error");
		(*(multSIPtr + 1))->numBytes += writeSize;
		
		writeSize = readSize>>2;

		error = FSWrite((*(multSIPtr + 2))->dFileNum, &writeSize, splitOut3);
		
		if(error == dskFulErr)
			DrawErrorMessage("\pDisk Full!");
		else if(error != 0)
			DrawErrorMessage("\pWrite Error");
		(*(multSIPtr + 2))->numBytes += writeSize;
		
		writeSize = readSize>>2;

		error = FSWrite((*(multSIPtr + 3))->dFileNum, &writeSize, splitOut4);
		
		if(error == dskFulErr)
			DrawErrorMessage("\pDisk Full!");
		else if(error != 0)
			DrawErrorMessage("\pWrite Error");
		(*(multSIPtr + 3))->numBytes += writeSize;
	}
	length = (float)((*(multSIPtr + 0))->numBytes/((*(multSIPtr + 0))->frameSize * 
		(*(multSIPtr + 0))->sRate));
	HMSTimeString(length, errStr);
	UpdateProcessWindow("\p", errStr, "\p", -1.0);
	gNumberBlocks++;
	if(writeSize != gBlockSize*inSIPtr->frameSize)
	{
		WriteHeader((*(multSIPtr + 0)));
		UpdateInfoWindow((*(multSIPtr + 0)));
		WriteHeader((*(multSIPtr + 1)));
		UpdateInfoWindow((*(multSIPtr + 1)));
		if(inSIPtr->nChans == 4)
		{
			WriteHeader((*(multSIPtr + 2)));
			UpdateInfoWindow((*(multSIPtr + 2)));
			WriteHeader((*(multSIPtr + 3)));
			UpdateInfoWindow((*(multSIPtr + 3)));
		}
		DeAllocSplitMemory();
		FinishProcess();
		gNumberBlocks = 0;
	}
	return(TRUE);
}*/

void	FinishSplitSoundFile(void)
{
		WriteHeader((*(multSIPtr + 0)));
		UpdateInfoWindow((*(multSIPtr + 0)));
		WriteHeader((*(multSIPtr + 1)));
		UpdateInfoWindow((*(multSIPtr + 1)));
		if(inSIPtr->nChans == 4)
		{
			WriteHeader((*(multSIPtr + 2)));
			UpdateInfoWindow((*(multSIPtr + 2)));
			WriteHeader((*(multSIPtr + 3)));
			UpdateInfoWindow((*(multSIPtr + 3)));
		}
		gNumberBlocks = 0;
		DeAllocSplitMemory();
		FinishProcess();
}
void
DeAllocSplitMemory(void)
{
	RemovePtr((Ptr)copyBlock1);
	RemovePtr((Ptr)copyBlock2);
	RemovePtr((Ptr)copyBlock3);
	RemovePtr((Ptr)copyBlock4);
	copyBlock1 = copyBlock2 = copyBlock3 = copyBlock4 = nil;

	RemovePtr((Ptr)splitIn);
	RemovePtr((Ptr)splitOut1);
	RemovePtr((Ptr)splitOut2);
	RemovePtr((Ptr)multSIPtr);
	if(inSIPtr->nChans == 4)
	{
		RemovePtr((Ptr)splitOut3);
		RemovePtr((Ptr)splitOut4);
	}
	multSIPtr = nil;
	splitIn = splitOut1 = splitOut2 = splitOut3 = splitOut4 = nil;
}