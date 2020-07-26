// WriteHeader.c		All of the header writing routines. Spectral and sound.
#if powerc
#include <fp.h>
#endif	/*powerc*/

#include <stdio.h>
#include <QuickTime/QuickTimeComponents.h>
//#include <SoundComponents.h>
#include "IEEE80.h"
#include "SoundFile.h"
#include "PhaseVocoder.h"
#include "SpectralHeader.h"
#include "WriteHeader.h"
#include "ReadHeader.h"
#include "SoundHack.h"
#include "OpenSoundFile.h"
#include "Macros.h"
#include "Misc.h"
#include "ByteSwap.h"
#include "Dialog.h"
#include "QuickTimeImport.h"
#include "CarbonGlue.h"

/* globals */

extern short		gAppFileNum;
extern SoundInfoPtr	frontSIPtr;
extern PvocInfo 	gPI;
extern struct
{
	Boolean	soundPlay;
	Boolean	soundRecord;
	Boolean	soundCompress;
	Boolean	movie;
}	gGestalt;

Boolean
QueryWritable(FSSpec aFSSpec)
{
	HParamBlockRec	pb;
 	char   vol_Name[256];

	vol_Name[0] = '\0';
	// PBGetVInfo to get the volattrib struct
	pb.volumeParam.ioCompletion = nil;
	pb.volumeParam.ioVolIndex = 0;
	pb.volumeParam.ioNamePtr = (StringPtr)vol_Name;
	pb.volumeParam.ioVRefNum = aFSSpec.vRefNum;
	PBHGetVInfo(&pb,FALSE);
	// hardware lock check
	if(pb.volumeParam.ioVAtrb & 0x0080)
		return(FALSE);
	// software lock check
	if(pb.volumeParam.ioVAtrb & 0x8000)
		return(FALSE);
	// now check to see if the file is locked
/*	pb.fileParam.ioCompletion = nil;
	pb.fileParam.ioVRefNum = aFSSpec.vRefNum;
	pb.fileParam.ioFVersNum = 0;
	pb.fileParam.ioFDirIndex = 0;
	pb.fileParam.ioNamePtr = aFSSpec.name;
	PBHGetFInfo(&pb,FALSE);
	if(pb.fileParam.ioFlAttrib & 0x0001)
		return(FALSE);*/
	// unlock the file if its locked
	FSpRstFLock(&aFSSpec);
	return(TRUE);
}


short
WriteHeader(SoundInfo *mySI)
{
	short	returnValue;
		
	if(QueryWritable(mySI->sfSpec) == FALSE)
		return(TRUE);
		
	switch(mySI->sfType)
	{
		case AIFF:
		case QKTMA:
			returnValue = WriteAIFFHeader(mySI);
			break;
		case AIFC:
			returnValue = WriteAIFCHeader(mySI);
			break;
		case WAVE:
			returnValue = WriteWAVEHeader(mySI);
			break;
		case BICSF:
			returnValue = WriteBICSFHeader(mySI);
			break;
		case SDII:
		case MMIX:
		case AUDMED:
		case DSPD:
			returnValue = WriteSDIIHeader(mySI);
			break;
//			returnValue = WriteDSPDHeader(mySI);
//			break;
		case NEXT:
		case SUNAU:
			returnValue = WriteNEXTHeader(mySI);
			break;
		case CS_PVOC:
			returnValue = WritePVAHeader(mySI);
			break;
		case PICT:
		case TEXT:
		case RAW:
		case CS_HTRO:
		case LEMUR:
		case QKTMV:
		case MPEG:
		case SDIFF:
			returnValue = 1;
			break;
		default:
			DrawErrorMessage("\pUnknown Header Type");
			returnValue = -1;
			break;
	}
	FlushVol((StringPtr)NIL_POINTER,mySI->sfSpec.vRefNum);
	return(returnValue);
}

short
WriteSDIIHeader(SoundInfo *mySI)
{
	long		frameSize;
	Handle		frameSizeHandle, sRateHandle, nChansHandle;
	extern short	gAppFileNum;
	Str255	tmpStr;
	SInt8	state;
	
	/* SetVol is a change folder or volume command */
	mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec), fsCurPerm);
	UseResFile(mySI->rFileNum);
	frameSizeHandle = GetNamedResource('STR ',"\psample-size");
	if(frameSizeHandle==0)
		return(-1);
	nChansHandle = GetNamedResource('STR ',"\pchannels");
	if(nChansHandle==0)
		return(-1);
	sRateHandle = GetNamedResource('STR ',"\psample-rate");
	if(sRateHandle==0)
		return(-1);

	state = HGetState(nChansHandle);
	HNoPurge(nChansHandle);
	MoveHHi(nChansHandle);
	SetHandleSize(nChansHandle,255);
	HLock(nChansHandle);
	NumToString(mySI->nChans, tmpStr);
	StringCopy(tmpStr, (StringPtr)*nChansHandle);
	HUnlock(nChansHandle);
	HSetState(nChansHandle, state);
	ChangedResource(nChansHandle);
	
	state = HGetState(sRateHandle);
	HNoPurge(sRateHandle);
	MoveHHi(sRateHandle);
	SetHandleSize(sRateHandle,255);
	HLock(sRateHandle);
	FixToString(mySI->sRate, tmpStr);
	StringCopy(tmpStr, (StringPtr)*sRateHandle);
	HUnlock(sRateHandle);
	HSetState(sRateHandle, state);
	ChangedResource(sRateHandle);
	
	state = HGetState(frameSizeHandle);
	HNoPurge(frameSizeHandle);
	MoveHHi(frameSizeHandle);
	HLock(frameSizeHandle);
	frameSize = (long)mySI->frameSize;
	NumToString(frameSize, tmpStr);
	StringCopy(tmpStr, (StringPtr)*frameSizeHandle);
	HUnlock(frameSizeHandle);
	HSetState(frameSizeHandle, state);
	ChangedResource(frameSizeHandle);
	
	CloseResFile(mySI->rFileNum);
	ReleaseResource(nChansHandle);
	ReleaseResource(sRateHandle);
	ReleaseResource(frameSizeHandle);
	UseResFile(gAppFileNum);
	return(TRUE);
}

short
WriteDSPDHeader(SoundInfo *mySI)
{
	long		frameSize;
	Handle		frameSizeHandle, sRateHandle, nChansHandle, dspSignalHandle;
	extern short	gAppFileNum;
	DSPsResource	myDSPs;
	
	mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec), fsCurPerm);
	UseResFile(mySI->rFileNum);
	
	frameSize = (long)mySI->frameSize;

	frameSizeHandle = GetNamedResource('STR ',"\psample-size");
	if(frameSizeHandle==0)
		return(-1);
	nChansHandle = GetNamedResource('STR ',"\pchannels");
	if(nChansHandle==0)
		return(-1);
	sRateHandle = GetNamedResource('STR ',"\psample-rate");
	if(sRateHandle==0)
		return(-1);
	dspSignalHandle = GetNamedResource('DSPs',"\pSignal");
	if(dspSignalHandle==0)
		return(-1);
	/* Set Up DSP Resource */

	myDSPs.version = 1;
	myDSPs.frameSize = frameSize;
	myDSPs.streamOrBlock = 0;
	myDSPs.complexType = 0;
	myDSPs.domain = 0;
	myDSPs.objectType = 0;
	myDSPs.dBOrLinear = 0;
	myDSPs.radianOrDegree = 0;
	myDSPs.unused1 = 0;
	myDSPs.unused2 = 0;
	myDSPs.unused3 = 0;
	myDSPs.unused4 = 0;
	FixToString12(mySI->sRate, (StringPtr)myDSPs.sRateCstr);
	p2cstr((StringPtr)myDSPs.sRateCstr);
	myDSPs.initialTimeCstr[0] = '0';
	myDSPs.initialTimeCstr[1] = 0;
	myDSPs.fracScaleCstr[0] = '1';
	myDSPs.fracScaleCstr[1] = 0;
	
	MoveHHi(nChansHandle);
	MoveHHi(sRateHandle);
	MoveHHi(frameSizeHandle);
	MoveHHi(dspSignalHandle);
	HLock(nChansHandle);
	HLock(sRateHandle);
	HLock(frameSizeHandle);
	HLock(dspSignalHandle);
	NumToString(frameSize, (StringPtr)*frameSizeHandle);
	SetHandleSize(sRateHandle,16);
	FixToString(mySI->sRate, (StringPtr)*sRateHandle);
	NumToString(mySI->nChans, (StringPtr)*nChansHandle);
	BlockMoveData(&myDSPs,*dspSignalHandle,28);
	HUnlock(frameSizeHandle);
	HUnlock(sRateHandle);
	HUnlock(nChansHandle);
	HUnlock(dspSignalHandle);
	ChangedResource(sRateHandle);
	ChangedResource(frameSizeHandle);
	ChangedResource(nChansHandle);
	ChangedResource(dspSignalHandle);
	CloseResFile(mySI->rFileNum);
	ReleaseResource(nChansHandle);
	ReleaseResource(sRateHandle);
	ReleaseResource(frameSizeHandle);
	ReleaseResource(dspSignalHandle);
	UseResFile(gAppFileNum);
	return(TRUE);
}

short
WriteAIFFHeader(SoundInfo *mySI)
{
	Byte			zero;
	short				soundFound = FALSE, commonFound = FALSE, peakFound = FALSE;
	long			writeSize, readSize, fileSize, position;
	int				i;
	OSErr			error;
	unsigned long	*swapLong;
	FormChunk		myFormChunk; 
	AIFFCommonChunk		myCommonChunk;
	PeakChunkHeader		myPeakChunkHeader;
	PositionPeak	aPositionPeak;
	AIFFSoundDataChunk	mySoundDataChunk;
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myFormChunk);
	FSRead(mySI->dFileNum, &readSize, &myFormChunk);
#ifdef SHOSXUB
	myFormChunk.ckID = EndianU32_BtoN(myFormChunk.ckID);
	myFormChunk.formType = EndianU32_BtoN(myFormChunk.formType);
#endif
	if(myFormChunk.ckID != FORMID)
		return(-1);
	else if(myFormChunk.formType != AIFFTYPE)
		return(-1);
	
	/* Update FORM ckSize to reflect size of file */
	GetEOF(mySI->dFileNum, &fileSize);
	myFormChunk.ckSize = fileSize - sizeof(chunkHeader);
#ifdef SHOSXUB
	myFormChunk.ckID = EndianU32_NtoB(myFormChunk.ckID);
	myFormChunk.ckSize = EndianS32_NtoB(myFormChunk.ckSize);
	myFormChunk.formType = EndianU32_NtoB(myFormChunk.formType);
#endif
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	writeSize = sizeof(myFormChunk);
	FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
			
	/* Loop through the Chunks until finding the Common Chunk */
	/* I think I can get away with not updating the Sound Offset, but do that in
	 * CreateAIFFFile instead */
	while(commonFound == FALSE || soundFound == FALSE || peakFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		error = FSRead(mySI->dFileNum, &readSize, &chunkHeader);
#ifdef SHOSXUB
		chunkHeader.ckID = EndianU32_BtoN(chunkHeader.ckID);
		chunkHeader.ckSize = EndianU32_BtoN(chunkHeader.ckSize);
#endif
		if((error == eofErr) && commonFound && soundFound && (peakFound == FALSE))
		{
			GetEOF(mySI->dFileNum, &position);
			SetFPos(mySI->dFileNum, fsFromStart, position);
			myPeakChunkHeader.ckID = PEAKID;
			myPeakChunkHeader.ckSize = sizeof(myPeakChunkHeader) 
				+ (mySI->nChans * sizeof(aPositionPeak)) - sizeof(chunkHeader);
			myPeakChunkHeader.version = 1;
			GetDateTime(&myPeakChunkHeader.timeStamp);
			myPeakChunkHeader.timeStamp -= 2082844800UL;
#ifdef SHOSXUB
			myPeakChunkHeader.ckID = EndianU32_NtoB(myPeakChunkHeader.ckID);
			myPeakChunkHeader.ckSize = EndianS32_NtoB(myPeakChunkHeader.ckSize);
			myPeakChunkHeader.version = EndianU32_NtoB(myPeakChunkHeader.version);
			myPeakChunkHeader.timeStamp = EndianU32_NtoB(myPeakChunkHeader.timeStamp);
#endif
			writeSize = sizeof(myPeakChunkHeader);
			FSWrite(mySI->dFileNum, &writeSize, &myPeakChunkHeader);
			for(i = 0; i < mySI->nChans; i++)
			{
				switch(i)
				{
					case 0:
						aPositionPeak.value = mySI->peakFL;
						break;
					case 1:
						aPositionPeak.value = mySI->peakFR;
						break;
					case 2:
						aPositionPeak.value = mySI->peakRL;
						break;
					case 3:
						aPositionPeak.value = mySI->peakRR;
						break;
				}
				aPositionPeak.position = 0;
#ifdef SHOSXUB
				aPositionPeak.position = EndianU32_NtoB(aPositionPeak.position);
				swapLong = &(aPositionPeak.value);
				*swapLong = EndianU32_NtoB(*swapLong);
#endif
				writeSize = sizeof(PositionPeak);
				FSWrite(mySI->dFileNum, &writeSize, &aPositionPeak);
			}
			return(TRUE);
		}
		else if ((error == eofErr) && commonFound && soundFound && peakFound)
			return(TRUE);
		else if((error == eofErr) && (commonFound == FALSE || soundFound == FALSE))
			return(-1);
		switch(chunkHeader.ckID)
		{
			case COMMONID:
				double_to_ieee_80(mySI->sRate, myCommonChunk.sampleRate.Bytes);
				myCommonChunk.ckID = chunkHeader.ckID;
				myCommonChunk.ckSize = chunkHeader.ckSize;
				myCommonChunk.numChannels = mySI->nChans;
				myCommonChunk.sampleSize = (long)(8 * mySI->frameSize);
				myCommonChunk.numSampleFrames = (long)(mySI->numBytes / (mySI->frameSize * mySI->nChans));
				
#ifdef SHOSXUB
				myCommonChunk.ckID = EndianU32_NtoB(myCommonChunk.ckID);
				myCommonChunk.ckSize = EndianS32_NtoB(myCommonChunk.ckSize);
				myCommonChunk.numChannels = EndianS16_NtoB(myCommonChunk.numChannels);
				myCommonChunk.sampleSize = EndianS16_NtoB(myCommonChunk.sampleSize);
				myCommonChunk.numSampleFrames = EndianU32_NtoB(myCommonChunk.numSampleFrames);
#endif
				writeSize = sizeof(myCommonChunk);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &myCommonChunk);
				commonFound = TRUE;
				break;
			case SOUNDID:
				/* We need to set the ckSize */
				chunkHeader.ckSize = mySI->numBytes +
					sizeof(mySoundDataChunk.blockSize) + sizeof(mySoundDataChunk.offset);
				writeSize = sizeof(chunkHeader);
#ifdef SHOSXUB
				chunkHeader.ckID = EndianU32_NtoB(chunkHeader.ckID);
				chunkHeader.ckSize = EndianU32_NtoB(chunkHeader.ckSize);
#endif
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &chunkHeader);
				SetFPos(mySI->dFileNum, fsFromMark, chunkHeader.ckSize);
				if(chunkHeader.ckSize & 1)
				{
					writeSize = 1;
					zero = 0;
					FSWrite(mySI->dFileNum, &writeSize, &zero);
					/* Need to update FORM ckSize again */
					GetEOF(mySI->dFileNum, &fileSize);
					myFormChunk.ckSize = fileSize - sizeof(chunkHeader);
#ifdef SHOSXUB
					myFormChunk.ckID = FORMID;
					myFormChunk.formType = AIFFTYPE;
					myFormChunk.ckID = EndianU32_NtoB(myFormChunk.ckID);
					myFormChunk.ckSize = EndianS32_NtoB(myFormChunk.ckSize);
					myFormChunk.formType = EndianU32_NtoB(myFormChunk.formType);
#endif
					GetFPos(mySI->dFileNum, &position);
					SetFPos(mySI->dFileNum, fsFromStart, 0L);
					writeSize = sizeof(myFormChunk);
					FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
					SetFPos(mySI->dFileNum, fsFromStart, position);
				}
				soundFound = TRUE;
				break;
			case PEAKID:
				GetFPos(mySI->dFileNum, &position);
				myPeakChunkHeader.ckID = chunkHeader.ckID;
				myPeakChunkHeader.ckSize = chunkHeader.ckSize;
				myPeakChunkHeader.version = 1;
				GetDateTime(&myPeakChunkHeader.timeStamp);
				myPeakChunkHeader.timeStamp -= 2082844800UL;
#ifdef SHOSXUB
				myPeakChunkHeader.ckID = EndianU32_NtoB(myPeakChunkHeader.ckID);
				myPeakChunkHeader.ckSize = EndianS32_NtoB(myPeakChunkHeader.ckSize);
				myPeakChunkHeader.version = EndianU32_NtoB(myPeakChunkHeader.version);
				myPeakChunkHeader.timeStamp = EndianU32_NtoB(myPeakChunkHeader.timeStamp);
#endif
				writeSize = sizeof(myPeakChunkHeader);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &myPeakChunkHeader);
				for(i = 0; i < mySI->nChans; i++)
				{
					switch(i)
					{
						case 0:
							aPositionPeak.value = mySI->peakFL;
							break;
						case 1:
							aPositionPeak.value = mySI->peakFR;
							break;
						case 2:
							aPositionPeak.value = mySI->peakRL;
							break;
						case 3:
							aPositionPeak.value = mySI->peakRR;
							break;
					}
					aPositionPeak.position = 0;
#ifdef SHOSXUB
					aPositionPeak.position = EndianU32_NtoB(aPositionPeak.position);
					swapLong = &(aPositionPeak.value);
					*swapLong = EndianU32_NtoB(*swapLong);
#endif
					writeSize = sizeof(PositionPeak);
					FSWrite(mySI->dFileNum, &writeSize, &aPositionPeak);
				}
				position += chunkHeader.ckSize;
				EVENUP(position);
				SetFPos(mySI->dFileNum, fsFromStart, position);
				peakFound = TRUE;
				break;
			case FORMVERID:
			case MARKERID:
			case INSTRUMENTID:
			case MIDIDATAID:
			case AUDIORECORDINGID:
			case APPLICATIONSPECIFICID:
			case COMMENTID:
			case NAMEID:
			case AUTHORID:
			case COPYRIGHTID:
			case ANNOTATIONID:
			case JUNKID:
				SetFPos(mySI->dFileNum, fsFromMark, chunkHeader.ckSize);
				break;
			default:
				SetFPos(mySI->dFileNum, fsFromMark, chunkHeader.ckSize);
				break;
		}
	}
	if(mySI->sfType == QKTMA && gGestalt.movie == TRUE)
		AIFF2QTInPlace(mySI);
	return(TRUE);
}

short
WriteAIFCHeader(SoundInfo *mySI)
{
	Byte				zero;
	short				soundFound = FALSE, commonFound = FALSE, peakFound = FALSE;
	Boolean				appSpecNeeded;
	long				writeSize, readSize, fileSize, i, position;
	unsigned long		*swapLong;
	float				tmpFloat;
	FormChunk			myFormChunk;
	AIFCCommonChunk		myCommonChunk;
	AIFFSoundDataChunk	mySoundDataChunk;
	PeakChunkHeader		myPeakChunkHeader;
	PositionPeak	aPositionPeak;
	OSType				myOSType;
	OSErr				error;
	
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myFormChunk);
	FSRead(mySI->dFileNum, &readSize, &myFormChunk);
#ifdef SHOSXUB
	myFormChunk.ckID = EndianU32_BtoN(myFormChunk.ckID);
	myFormChunk.formType = EndianU32_BtoN(myFormChunk.formType);
#endif
	if(myFormChunk.ckID != FORMID)
		return(-1);
	else if(myFormChunk.formType != AIFCTYPE)
		return(-1);
	
	/* Update FORM ckSize to reflect size of file */
	GetEOF(mySI->dFileNum, &fileSize);
	myFormChunk.ckSize = fileSize - sizeof(chunkHeader);
#ifdef SHOSXUB
	myFormChunk.ckID = EndianU32_NtoB(myFormChunk.ckID);
	myFormChunk.ckSize = EndianS32_NtoB(myFormChunk.ckSize);
	myFormChunk.formType = EndianU32_NtoB(myFormChunk.formType);
#endif
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	writeSize = sizeof(myFormChunk);
	FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
			
	/* Loop through the Chunks until finding the Common Chunk */
	/* I think I can get away with not updating the Sound Offset, but do that in
	 * CreateAIFFFile instead */
	if(mySI->packMode == SF_FORMAT_32_FLOAT)
		appSpecNeeded = TRUE;
	else
		appSpecNeeded = FALSE;
		
	while(commonFound == FALSE || soundFound == FALSE || appSpecNeeded == TRUE || peakFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		error = FSRead(mySI->dFileNum, &readSize, &chunkHeader);
#ifdef SHOSXUB
		chunkHeader.ckID = EndianU32_BtoN(chunkHeader.ckID);
		chunkHeader.ckSize = EndianU32_BtoN(chunkHeader.ckSize);
#endif
		if((error == eofErr) && commonFound && soundFound && (peakFound == FALSE))
		{
			GetEOF(mySI->dFileNum, &position);
			SetFPos(mySI->dFileNum, fsFromStart, position);
			myPeakChunkHeader.ckID = PEAKID;
			myPeakChunkHeader.ckSize = sizeof(myPeakChunkHeader) 
				+ (mySI->nChans * sizeof(aPositionPeak)) - sizeof(chunkHeader);
			myPeakChunkHeader.version = 1;
			GetDateTime(&myPeakChunkHeader.timeStamp);
			myPeakChunkHeader.timeStamp -= 2082844800UL;
#ifdef SHOSXUB
			myPeakChunkHeader.ckID = EndianU32_NtoB(myPeakChunkHeader.ckID);
			myPeakChunkHeader.ckSize = EndianS32_NtoB(myPeakChunkHeader.ckSize);
			myPeakChunkHeader.version = EndianU32_NtoB(myPeakChunkHeader.version);
			myPeakChunkHeader.timeStamp = EndianU32_NtoB(myPeakChunkHeader.timeStamp);
#endif
			writeSize = sizeof(myPeakChunkHeader);
			FSWrite(mySI->dFileNum, &writeSize, &myPeakChunkHeader);
			for(i = 0; i < mySI->nChans; i++)
			{
				switch(i)
				{
					case 0:
						aPositionPeak.value = mySI->peakFL;
						break;
					case 1:
						aPositionPeak.value = mySI->peakFR;
						break;
					case 2:
						aPositionPeak.value = mySI->peakRL;
						break;
					case 3:
						aPositionPeak.value = mySI->peakRR;
						break;
				}
				aPositionPeak.position = 0;
#ifdef SHOSXUB
				aPositionPeak.position = EndianU32_NtoB(aPositionPeak.position);
				swapLong = &(aPositionPeak.value);
				*swapLong = EndianU32_NtoB(*swapLong);
#endif
				writeSize = sizeof(PositionPeak);
				FSWrite(mySI->dFileNum, &writeSize, &aPositionPeak);
			}
			return(TRUE);
		}
		else if ((error == eofErr) && commonFound && soundFound && peakFound)
			return(TRUE);
		else if((error == eofErr) && (commonFound == FALSE || soundFound == FALSE))
			return(-1);
		switch(chunkHeader.ckID)
		{
			case COMMONID:
				double_to_ieee_80(mySI->sRate, myCommonChunk.sampleRate.Bytes);
				myCommonChunk.ckID = chunkHeader.ckID;
				myCommonChunk.ckSize = chunkHeader.ckSize;
				myCommonChunk.numChannels = mySI->nChans;
				/* 
				 * We can get the compression name size from the chunk size,
				 * that will allow us to maintain chunk size, this depends on CreateAIFC creating
				 * the right size in the first place.
				 */
				for(i=0;i<=mySI->compName[0];i++)
					myCommonChunk.compressionName[i]=mySI->compName[i];
				/*
				 * For now I am forcing the sampleSize to equal 16 for compressed
				 * data. It is supposed to reflect the size of the sample before
				 * compression.
				 */
				switch(mySI->packMode)
				{
					case SF_FORMAT_4_ADIMA:
						if(gGestalt.soundCompress == FALSE)
						{
							DrawErrorMessage("\pSoundHack requires Sound Manager 3.2 to write IMA4 files");
							return(-1);
						}
						else
						{
							myCommonChunk.compressionType = kIMACompression;
							myCommonChunk.sampleSize = 16L;
						}
						break;
					case SF_FORMAT_MACE3:
						if(gGestalt.soundCompress == FALSE)
						{
							DrawErrorMessage("\pSoundHack requires Sound Manager 3.2 to write MACE3 files");
							return(-1);
						}
						else
						{
							myCommonChunk.compressionType = kMACE3Compression;
							myCommonChunk.sampleSize = 8L;
						}
						break;
					case SF_FORMAT_MACE6:
						if(gGestalt.soundCompress == FALSE)
						{
							DrawErrorMessage("\pSoundHack requires Sound Manager 3.2 to write MACE6 files");
							return(-1);
						}
						else
						{
							myCommonChunk.compressionType = kMACE6Compression;
							myCommonChunk.sampleSize = 8L;
						}
						break;
					case SF_FORMAT_4_ADDVI:
						myCommonChunk.compressionType = AIFC_ID_ADDVI;
						myCommonChunk.sampleSize = 16L;
						break;
					case SF_FORMAT_8_MULAW:
						myCommonChunk.compressionType = kULawCompression;
						myCommonChunk.sampleSize = 16L;
						break;
					case SF_FORMAT_8_ALAW:
						myCommonChunk.compressionType = kALawCompression;
						myCommonChunk.sampleSize = 16L;
						break;
					case SF_FORMAT_16_SWAP:
						myCommonChunk.compressionType = k16BitLittleEndianFormat;
						myCommonChunk.sampleSize = 16L;
						break;
					case SF_FORMAT_24_COMP:
					case SF_FORMAT_24_LINEAR:
						myCommonChunk.compressionType = k24BitFormat;
						myCommonChunk.sampleSize = 16L;
						break;
					case SF_FORMAT_32_LINEAR:
					case SF_FORMAT_32_COMP:
						myCommonChunk.compressionType = k32BitFormat;
						myCommonChunk.sampleSize = 16L;
						break;
					case AIFC_ID_FLT32:
					case kFloat32Format:
					case SF_FORMAT_32_FLOAT:
						myCommonChunk.compressionType = kFloat32Format;
						myCommonChunk.sampleSize = 16L;
						break;
					default:
						myCommonChunk.compressionType = kSoundNotCompressed;
						myCommonChunk.sampleSize = (long)(8 * mySI->frameSize);
						break;
				}
				myCommonChunk.numSampleFrames = (long)(mySI->numBytes /
					(mySI->frameSize * mySI->nChans));
				writeSize = myCommonChunk.ckSize + sizeof(chunkHeader);
#ifdef SHOSXUB
				myCommonChunk.ckID = EndianU32_NtoB(myCommonChunk.ckID);
				myCommonChunk.ckSize = EndianS32_NtoB(myCommonChunk.ckSize);
				myCommonChunk.numChannels = EndianS16_NtoB(myCommonChunk.numChannels);
				myCommonChunk.sampleSize = EndianS16_NtoB(myCommonChunk.sampleSize);
				myCommonChunk.numSampleFrames = EndianU32_NtoB(myCommonChunk.numSampleFrames);
				myCommonChunk.compressionType = EndianU32_NtoB(myCommonChunk.compressionType);
#endif
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &myCommonChunk);
				commonFound = TRUE;
				break;
			case SOUNDID:
				/* We need to set the ckSize */
				chunkHeader.ckSize = mySI->numBytes +
					sizeof(mySoundDataChunk.blockSize) + sizeof(mySoundDataChunk.offset);
#ifdef SHOSXUB
				chunkHeader.ckID = EndianU32_NtoB(chunkHeader.ckID);
				chunkHeader.ckSize = EndianU32_NtoB(chunkHeader.ckSize);
#endif
				writeSize = sizeof(chunkHeader);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &chunkHeader);
				SetFPos(mySI->dFileNum, fsFromMark, chunkHeader.ckSize);
				if(chunkHeader.ckSize & 1)
				{
					writeSize = 1;
					zero = 0;
					FSWrite(mySI->dFileNum, &writeSize, &zero);
					/* Need to update FORM ckSize again */
					GetEOF(mySI->dFileNum, &fileSize);
					myFormChunk.ckSize = fileSize - sizeof(chunkHeader);
					GetFPos(mySI->dFileNum, &position);
					SetFPos(mySI->dFileNum, fsFromStart, 0L);
#ifdef SHOSXUB
					myFormChunk.ckID = FORMID;
					myFormChunk.formType = AIFCTYPE;
					myFormChunk.ckID = EndianU32_NtoB(myFormChunk.ckID);
					myFormChunk.ckSize = EndianS32_NtoB(myFormChunk.ckSize);
					myFormChunk.formType = EndianU32_NtoB(myFormChunk.formType);
#endif
					writeSize = sizeof(myFormChunk);
					FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
					SetFPos(mySI->dFileNum, fsFromStart, position);
				}
				soundFound = TRUE;
				break;
			case PEAKID:
				GetFPos(mySI->dFileNum, &position);
				myPeakChunkHeader.ckID = chunkHeader.ckID;
				myPeakChunkHeader.ckSize = chunkHeader.ckSize;
				myPeakChunkHeader.version = 1;
				GetDateTime(&myPeakChunkHeader.timeStamp);
				myPeakChunkHeader.timeStamp -= 2082844800UL;
#ifdef SHOSXUB
				myPeakChunkHeader.ckID = EndianU32_NtoB(myPeakChunkHeader.ckID);
				myPeakChunkHeader.ckSize = EndianS32_NtoB(myPeakChunkHeader.ckSize);
				myPeakChunkHeader.version = EndianU32_NtoB(myPeakChunkHeader.version);
				myPeakChunkHeader.timeStamp = EndianU32_NtoB(myPeakChunkHeader.timeStamp);
#endif
				writeSize = sizeof(myPeakChunkHeader);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &myPeakChunkHeader);
				for(i = 0; i < mySI->nChans; i++)
				{
					switch(i)
					{
						case 0:
							aPositionPeak.value = mySI->peakFL;
							break;
						case 1:
							aPositionPeak.value = mySI->peakFR;
							break;
						case 2:
							aPositionPeak.value = mySI->peakRL;
							break;
						case 3:
							aPositionPeak.value = mySI->peakRR;
							break;
					}
					aPositionPeak.position = 0;
#ifdef SHOSXUB
					aPositionPeak.position = EndianU32_NtoB(aPositionPeak.position);
					swapLong = &(aPositionPeak.value);
					*swapLong = EndianU32_NtoB(*swapLong);
#endif
					writeSize = sizeof(PositionPeak);
					FSWrite(mySI->dFileNum, &writeSize, &aPositionPeak);
				}
				position += chunkHeader.ckSize;
				EVENUP(position);
				SetFPos(mySI->dFileNum, fsFromStart, position);
				peakFound = TRUE;
				break;
			case APPLICATIONSPECIFICID:
				GetFPos(mySI->dFileNum, &position); // position after chunkheader
				readSize = sizeof(OSType);
				FSRead(mySI->dFileNum, &readSize, &myOSType);
#ifdef SHOSXUB
				myOSType = EndianU32_BtoN(myOSType);
#endif
				if(myOSType == 'pErF')
				{
					appSpecNeeded = FALSE;
					if(chunkHeader.ckSize >= 8)
					{
						tmpFloat = mySI->peak;
#ifdef SHOSXUB
						swapLong = &tmpFloat;
						*swapLong = EndianU32_NtoB(*swapLong);
#endif
						writeSize = sizeof(float);
						FSWrite(mySI->dFileNum, &writeSize, &tmpFloat);
					}
					if(chunkHeader.ckSize >= 12)
					{
						tmpFloat = mySI->peakFL;
#ifdef SHOSXUB
						swapLong = &tmpFloat;
						*swapLong = EndianU32_NtoB(*swapLong);
#endif
						writeSize = sizeof(float);
						FSWrite(mySI->dFileNum, &writeSize, &tmpFloat);
					}
					if(chunkHeader.ckSize >= 16)
					{
						tmpFloat = mySI->peakFR;
#ifdef SHOSXUB
						swapLong = &tmpFloat;
						*swapLong = EndianU32_NtoB(*swapLong);
#endif
						writeSize = sizeof(float);
						FSWrite(mySI->dFileNum, &writeSize, &tmpFloat);
					}
					if(chunkHeader.ckSize >= 20)
					{
						tmpFloat = mySI->peakRL;
#ifdef SHOSXUB
						swapLong = &tmpFloat;
						*swapLong = EndianU32_NtoB(*swapLong);
#endif
						writeSize = sizeof(float);
						FSWrite(mySI->dFileNum, &writeSize, &tmpFloat);
					}
					if(chunkHeader.ckSize >= 24)
					{
						tmpFloat = mySI->peakRR;
#ifdef SHOSXUB
						swapLong = &tmpFloat;
						*swapLong = EndianU32_NtoB(*swapLong);
#endif
						writeSize = sizeof(float);
						FSWrite(mySI->dFileNum, &writeSize, &tmpFloat);
					}
				}
				else
					appSpecNeeded = TRUE;
				SetFPos(mySI->dFileNum, fsFromStart, (position + chunkHeader.ckSize));
				break;
			case MARKERID:
			case FORMVERID:
			case INSTRUMENTID:
			case MIDIDATAID:
			case AUDIORECORDINGID:
			case COMMENTID:
			case NAMEID:
			case AUTHORID:
			case COPYRIGHTID:
			case ANNOTATIONID:
			case JUNKID:
				SetFPos(mySI->dFileNum, fsFromMark, chunkHeader.ckSize);
				break;
			default:
				SetFPos(mySI->dFileNum, fsFromMark, chunkHeader.ckSize);
				break;
		}
	}
	return(0);
}

short
WriteWAVEHeader(SoundInfo *mySI)
{
	Byte				zero;
	short					soundFound = FALSE, commonFound = FALSE;
	short				sampleSize;
	long				tmpLong, fileSize, writeSize, readSize, position;
	RIFFFormChunk		myFormChunk;
	WAVEFormatChunk		myFormatChunk;
	
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myFormChunk);
	FSRead(mySI->dFileNum, &readSize, &myFormChunk);
#ifdef SHOSXUB
	myFormChunk.ckID = EndianU32_BtoN(myFormChunk.ckID);
	myFormChunk.formType = EndianU32_BtoN(myFormChunk.formType);
#endif
	if(myFormChunk.ckID != WAV_ID_RIFF)
		return(-1);
	else if(myFormChunk.formType != WAV_ID_WAVE)
		return(-1);
	/* 	Update FORM ckSize to reflect size of file */
	GetEOF(mySI->dFileNum, &fileSize);
#ifdef SHOSXUB
	myFormChunk.ckSize = EndianS32_NtoL(fileSize - sizeof(chunkHeader));
	myFormChunk.ckID = EndianU32_NtoB(myFormChunk.ckID);
	myFormChunk.formType = EndianU32_NtoB(myFormChunk.formType);
#else
	myFormChunk.ckSize = ByteSwapLong(fileSize - sizeof(chunkHeader));
#endif
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	writeSize = sizeof(myFormChunk);
	FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
			
	/* Loop through the Chunks until finding the Format Chunk and Sound Chunk */
	while(commonFound == FALSE || soundFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		FSRead(mySI->dFileNum, &readSize, &chunkHeader);
#ifdef SHOSXUB
		chunkHeader.ckID = EndianU32_BtoN(chunkHeader.ckID);
		chunkHeader.ckSize = EndianS32_LtoN(chunkHeader.ckSize);
#else
		chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
		switch(chunkHeader.ckID)
		{
			case WAV_ID_FORMAT:
				GetFPos(mySI->dFileNum, &position);
				myFormatChunk.ckID = chunkHeader.ckID;
				myFormatChunk.ckSize = chunkHeader.ckSize;
				switch(mySI->packMode)
				{
					case SF_FORMAT_8_UNSIGNED:
					case SF_FORMAT_16_SWAP:
					case SF_FORMAT_24_SWAP:
					case SF_FORMAT_32_SWAP:
						myFormatChunk.wFormatTag = WAV_FORMAT_PCM;
						break;
					case SF_FORMAT_8_MULAW:
						myFormatChunk.wFormatTag = IBM_FORMAT_MULAW;
						break;
					case SF_FORMAT_8_ALAW:
						myFormatChunk.wFormatTag = IBM_FORMAT_ALAW;
						break;
					case SF_FORMAT_4_ADDVI:
						myFormatChunk.wFormatTag = IBM_FORMAT_ADDVI;
						break;
				}
				myFormatChunk.wChannels = mySI->nChans;
				myFormatChunk.dwSamplePerSec = (long)mySI->sRate;
				myFormatChunk.dwAvgBytesPerSec = (long)(mySI->sRate * mySI->frameSize * mySI->nChans);
				myFormatChunk.wBlockAlign = (short)(mySI->frameSize * mySI->nChans);
				myFormatChunk.sampleSize = (short)(mySI->frameSize * 8);
				
				if(myFormatChunk.wFormatTag == WAV_FORMAT_PCM)
					writeSize = sizeof(myFormatChunk);
				else
					writeSize = sizeof(myFormatChunk) - sizeof(myFormatChunk.sampleSize);


#ifdef SHOSXUB
				myFormatChunk.ckID = EndianU32_NtoB(myFormatChunk.ckID);
				myFormatChunk.ckSize = EndianS32_NtoL(myFormatChunk.ckSize);
				myFormatChunk.wFormatTag = EndianS16_NtoL(myFormatChunk.wFormatTag);
				myFormatChunk.wChannels = EndianS16_NtoL(myFormatChunk.wChannels);
				myFormatChunk.dwSamplePerSec = EndianS32_NtoL(myFormatChunk.dwSamplePerSec);
				myFormatChunk.dwAvgBytesPerSec = EndianS32_NtoL(myFormatChunk.dwAvgBytesPerSec);
				myFormatChunk.wBlockAlign = EndianS16_NtoL(myFormatChunk.wBlockAlign);/* frameSize?*/
				myFormatChunk.sampleSize = EndianS16_NtoL(myFormatChunk.sampleSize);
#else
				myFormatChunk.wFormatTag = ByteSwapShort(myFormatChunk.wFormatTag);
				myFormatChunk.wChannels = ByteSwapShort(myFormatChunk.wChannels);
				myFormatChunk.dwSamplePerSec = ByteSwapLong(myFormatChunk.dwSamplePerSec);
				myFormatChunk.dwAvgBytesPerSec = ByteSwapLong(myFormatChunk.dwAvgBytesPerSec);
				myFormatChunk.wBlockAlign = ByteSwapShort(myFormatChunk.wBlockAlign);/* frameSize?*/
				myFormatChunk.sampleSize = ByteSwapShort(myFormatChunk.sampleSize);
#endif
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &myFormatChunk);
				
/*				sampleSize = (short)(mySI->frameSize * 8);
#ifdef SHOSXUB
				sampleSize = EndianS16_NtoL(sampleSize);
#else
				sampleSize = ByteSwapShort(sampleSize);
#endif
				writeSize = 2;
				FSWrite(mySI->dFileNum, &writeSize, &sampleSize);*/
				commonFound = TRUE;
				SetFPos(mySI->dFileNum, fsFromStart, position+chunkHeader.ckSize);
				GetFPos(mySI->dFileNum, &position);
				break;
			case WAV_ID_DATA:
				/* We only want to get the offset for start of sound and the number of bytes */
				tmpLong = chunkHeader.ckSize = mySI->numBytes;
#ifdef SHOSXUB
				chunkHeader.ckID = EndianU32_NtoB(chunkHeader.ckID);
				chunkHeader.ckSize = EndianS32_NtoL(chunkHeader.ckSize);
#else
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				writeSize = sizeof(chunkHeader);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &chunkHeader);
				if(tmpLong & 1)
				{
					writeSize = 1;
					zero = 0;
					FSWrite(mySI->dFileNum, &writeSize, &zero);
					/* Need to update FORM ckSize again */
					GetEOF(mySI->dFileNum, &fileSize);
#ifdef SHOSXUB
					myFormChunk.ckID = WAV_ID_RIFF;
					myFormChunk.formType = WAV_ID_WAVE;
					myFormChunk.ckID = EndianS32_NtoB(myFormChunk.ckID);
					myFormChunk.formType = EndianS32_NtoB(myFormChunk.formType);
					myFormChunk.ckSize = EndianS32_NtoL(fileSize - sizeof(chunkHeader));
#else
					myFormChunk.ckSize = ByteSwapLong(fileSize - sizeof(chunkHeader));
#endif
					SetFPos(mySI->dFileNum, fsFromStart, 0L);
					writeSize = sizeof(myFormChunk);
					FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
				}
				soundFound = TRUE;
				break;
			default:
				position = chunkHeader.ckSize;
				EVENUP(position);
				SetFPos(mySI->dFileNum, fsFromMark, position);
				break;
		}
	}
	return(TRUE);
}

short	WriteNEXTHeader(SoundInfo *mySI)
{
	long			writeSize, theFilePos, tmpLocation;
	NeXTSoundInfo	myNeXTSI;
	char			*peakString;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	writeSize = sizeof(myNeXTSI);
	myNeXTSI.magic = NEXTMAGIC;
	myNeXTSI.dataLocation = mySI->dataStart;
	myNeXTSI.dataSize = mySI->numBytes;
	switch(mySI->packMode)
	{
		case SF_FORMAT_8_LINEAR:
			myNeXTSI.dataFormat = NEXT_FORMAT_LINEAR_8;
			break;
		case SF_FORMAT_16_LINEAR:
			myNeXTSI.dataFormat = NEXT_FORMAT_LINEAR_16;
			break;
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_COMP:
			myNeXTSI.dataFormat = NEXT_FORMAT_LINEAR_24;
			break;
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
			myNeXTSI.dataFormat = NEXT_FORMAT_LINEAR_32;
			break;
		case SF_FORMAT_8_MULAW:
			myNeXTSI.dataFormat = NEXT_FORMAT_MULAW_8;
			break;
		case SF_FORMAT_8_ALAW:
			myNeXTSI.dataFormat = NEXT_FORMAT_ALAW_8;
			break;
		case SF_FORMAT_32_FLOAT:
			myNeXTSI.dataFormat = NEXT_FORMAT_FLOAT;
			break;
	}
	myNeXTSI.samplingRate = (long)mySI->sRate;
	myNeXTSI.channelCount = mySI->nChans;
	
	tmpLocation = myNeXTSI.dataLocation;
#ifdef SHOSXUB
	myNeXTSI.magic = EndianS32_NtoB(myNeXTSI.magic);
	myNeXTSI.dataLocation = EndianS32_NtoB(myNeXTSI.dataLocation);
	myNeXTSI.dataSize = EndianS32_NtoB(myNeXTSI.dataSize);
	myNeXTSI.dataFormat = EndianS32_NtoB(myNeXTSI.dataFormat);
	myNeXTSI.samplingRate = EndianS32_NtoB(myNeXTSI.samplingRate);
	myNeXTSI.channelCount = EndianS32_NtoB(myNeXTSI.channelCount);
#endif
	FSWrite(mySI->dFileNum, &writeSize, &myNeXTSI);
	
	GetFPos(mySI->dFileNum, &theFilePos);
	writeSize = tmpLocation - theFilePos;
	if(writeSize >= 32)
	{
		peakString = (char *)NewPtr(writeSize);
		sprintf(peakString, "Peak %.12f", mySI->peak);
		FSWrite(mySI->dFileNum, &writeSize, peakString);
	}
	return(TRUE);
}

short
WriteBICSFHeader(SoundInfo *mySI)
{
	long			writeSize;
	BICSFSoundInfo	myBICSFSI;
	BICSFCodeHeader myCodeHeader;
	short			codePosition;
	unsigned long	tmpLong;
	float			tmpFloat;
	unsigned long	*swapLong;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	writeSize = sizeof(myBICSFSI);
	myBICSFSI.magic = BICSFMAGIC;
	myBICSFSI.srate = mySI->sRate;
	myBICSFSI.chans = mySI->nChans;
	myBICSFSI.packMode = (long)mySI->frameSize;
#ifdef SHOSXUB
	myBICSFSI.magic = EndianS32_NtoB(myBICSFSI.magic);
	myBICSFSI.chans = EndianS32_NtoB(myBICSFSI.chans);
	myBICSFSI.packMode = EndianS32_NtoB(myBICSFSI.packMode);
	swapLong = &(myBICSFSI.srate);
	*swapLong = EndianU32_NtoB(*swapLong);
#endif
	codePosition = 0;
	tmpLong = 0L;
	if(myBICSFSI.packMode == BICSF_FORMAT_FLOAT)
	{
		// put a maxamp code in the code area
		myCodeHeader.code = BICSF_CODE_MAXAMP;
		myCodeHeader.bsize = (sizeof(float) + sizeof(long)) * mySI->nChans + sizeof(long);
#ifdef SHOSXUB
		myCodeHeader.code = EndianS16_NtoB(myCodeHeader.code);
		myCodeHeader.bsize = EndianS16_NtoB(myCodeHeader.bsize);
#endif		
		BlockMoveData(&myCodeHeader, &myBICSFSI.codes[codePosition], sizeof(myCodeHeader));
		codePosition += sizeof(myCodeHeader);
		
		tmpFloat = mySI->peakFL;
#ifdef SHOSXUB
		swapLong = &(tmpFloat);
		*swapLong = EndianU32_NtoB(*swapLong);
#endif		
		BlockMoveData(&tmpFloat, &myBICSFSI.codes[codePosition], sizeof(float));
		codePosition += sizeof(float);
		if(mySI->nChans > 1)
		{
			tmpFloat = mySI->peakFR;
#ifdef SHOSXUB
			swapLong = &(tmpFloat);
			*swapLong = EndianU32_NtoB(*swapLong);
#endif		
			BlockMoveData(&tmpFloat, &myBICSFSI.codes[codePosition], sizeof(float));
			codePosition += sizeof(float);
		}
		if(mySI->nChans > 2)
		{
			tmpFloat = mySI->peakRL;
#ifdef SHOSXUB
			swapLong = &(tmpFloat);
			*swapLong = EndianU32_NtoB(*swapLong);
#endif		
			BlockMoveData(&tmpFloat, &myBICSFSI.codes[codePosition], sizeof(float));
			codePosition += sizeof(float);
		}
		if(mySI->nChans > 3)
		{
			tmpFloat = mySI->peakRR;
#ifdef SHOSXUB
			swapLong = &(tmpFloat);
			*swapLong = EndianU32_NtoB(*swapLong);
#endif		
			BlockMoveData(&tmpFloat, &myBICSFSI.codes[codePosition], sizeof(float));
			codePosition += sizeof(float);
		}

		BlockMoveData(&tmpLong, &myBICSFSI.codes[codePosition], sizeof(long));
		codePosition += sizeof(long);
		if(mySI->nChans > 1)
		{
			BlockMoveData(&tmpLong, &myBICSFSI.codes[codePosition], sizeof(long));
			codePosition += sizeof(long);
		}
		if(mySI->nChans > 2)
		{
			BlockMoveData(&tmpLong, &myBICSFSI.codes[codePosition], sizeof(long));
			codePosition += sizeof(long);
		}
		if(mySI->nChans > 3)
		{
			BlockMoveData(&tmpLong, &myBICSFSI.codes[codePosition], sizeof(long));
			codePosition += sizeof(long);
		}
		GetDateTime(&tmpLong);
		tmpLong -= 2082844800UL;
#ifdef SHOSXUB
		tmpLong = EndianS32_NtoB(tmpLong);
#endif
		BlockMoveData(&tmpLong, &myBICSFSI.codes[codePosition], sizeof(long));
		codePosition += sizeof(long);
		tmpLong = 0L;	// combined zero code and zero size
		BlockMoveData(&tmpLong, &myBICSFSI.codes[codePosition], sizeof(long));
	}
	else
	{
		tmpLong = 0L;	// combined zero code and zero size
		BlockMoveData(&tmpLong, &myBICSFSI.codes[codePosition], sizeof(long));
	}	
	FSWrite(mySI->dFileNum, &writeSize, &myBICSFSI);
	return(TRUE);
}

short	WriteTX16WHeader(SoundInfo *mySI)
{
	TX16WHeader		myTX16WHeader;
	int i;
	long numFrames, attackFrames, repeatFrames, writeSize;
	static char tx16WMagic1[4] = {0, 0x06, 0x10, 0xF6};
	static char tx16WMagic2[4] = {0, 0x52, 0x00, 0x52};
	
	myTX16WHeader.filetype[0] = 'L';
	myTX16WHeader.filetype[0] = 'M';
	myTX16WHeader.filetype[0] = '8';
	myTX16WHeader.filetype[0] = '9';
	myTX16WHeader.filetype[0] = '5';
	myTX16WHeader.filetype[0] = '3';
	for(i = 0; i < 10; i++)
		myTX16WHeader.nulls[i] = 0;
	for(i = 0; i < 2; i++)
		myTX16WHeader.dummy_aeg[i] = 0;
	for(i = 2; i < 6; i++)
		myTX16WHeader.dummy_aeg[i] = 0x7F;
	for(i = 0; i < 2; i++)
		myTX16WHeader.unused[i] = 0;
	// using looped format to shove more sound in there.
	myTX16WHeader.format = TX16W_NOLOOP;
	
	// we won't go for exact rates
	if(mySI->sRate < 24000.0)
		myTX16WHeader.sample_rate = 3;
	else if(mySI->sRate < 41000.0)
		myTX16WHeader.sample_rate = 1;
	else
		myTX16WHeader.sample_rate = 2;
		
	numFrames = mySI->numBytes/(mySI->frameSize*mySI->nChans);
    if (numFrames >= TX16W_MAXLENGTH)
		attackFrames = repeatFrames  = TX16W_MAXLENGTH/2;
    else if(numFrames >=TX16W_MAXLENGTH/2)
    {
		attackFrames = TX16W_MAXLENGTH/2;
		repeatFrames = numFrames - attackFrames;
        if (repeatFrames < 0x40)
        {
			repeatFrames += 0x40;
            attackFrames -= 0x40;
        }
    }    
    else if(numFrames >= 0x80)
	{
		attackFrames = numFrames - 0x40;
		repeatFrames = 0x40;
	}
    else
        attackFrames = repeatFrames = 0x40;

    myTX16WHeader.atc_length[0] = 0xFF & attackFrames;
    myTX16WHeader.atc_length[1] = 0xFF & (attackFrames >> 8);
    myTX16WHeader.atc_length[2] = (0x01 & (attackFrames >> 16)) + tx16WMagic1[myTX16WHeader.sample_rate];
    
    myTX16WHeader.rpt_length[0] = 0xFF & repeatFrames;
    myTX16WHeader.rpt_length[1] = 0xFF & (repeatFrames >> 8);
    myTX16WHeader.rpt_length[2] = (0x01 & (repeatFrames >> 16)) + tx16WMagic2[myTX16WHeader.sample_rate];
    
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
    writeSize = sizeof(myTX16WHeader);
	FSWrite(mySI->dFileNum, &writeSize, &myTX16WHeader);

	return(TRUE);
}



/* This function will update the values for one or two phase vocoder analysis files */

short
WritePVAHeader(SoundInfo *mySI)
{
	long writeSize, endoffile;
	unsigned long *swapLong;
	CsoundHeader	myCSHeader;
	
	myCSHeader.magic = CSA_MAGIC;
	if(mySI->packMode == SF_FORMAT_SPECT_AMPFRQ)
	{	
    	myCSHeader.frameFormat = PVA_PVOC;		/* (short) how words are org'd in frms */
		myCSHeader.channels = 1;
    }
    else if(mySI->packMode == SF_FORMAT_SPECT_AMPPHS)
	{	
    	myCSHeader.frameFormat = PVA_POLAR;		/* (short) how words are org'd in frms */
		myCSHeader.channels = mySI->nChans;
    }
	myCSHeader.headBsize = sizeof(myCSHeader);
	GetEOF(mySI->dFileNum, &endoffile);
	if(endoffile > myCSHeader.headBsize)
		myCSHeader.dataBsize = mySI->numBytes = endoffile - myCSHeader.headBsize;
	else
		myCSHeader.dataBsize = mySI->numBytes =  0;
	myCSHeader.dataFormat = PVA_FLOAT;
	myCSHeader.samplingRate = mySI->sRate;
	myCSHeader.frameSize = mySI->spectFrameSize;
	myCSHeader.frameIncr = mySI->spectFrameIncr;
    myCSHeader.frameBsize = sizeof(float) * (myCSHeader.frameSize>>1 + 1)<<1;
    myCSHeader.minFreq = 0.0;			/* freq in Hz of lowest bin (exists) */
    myCSHeader.maxFreq = mySI->sRate/2.0;	/* freq in Hz of highest (or next) */
    myCSHeader.freqFormat = PVA_LIN;		/* (short) flag for log/lin frq */
#ifdef SHOSXUB
	myCSHeader.magic = EndianS32_NtoB(myCSHeader.magic);
	myCSHeader.headBsize = EndianS32_NtoB(myCSHeader.headBsize);
	myCSHeader.dataBsize = EndianS32_NtoB(myCSHeader.dataBsize);
	myCSHeader.dataFormat = EndianS32_NtoB(myCSHeader.dataFormat);
	swapLong = &(myCSHeader.samplingRate);
	*swapLong = EndianU32_NtoB(*swapLong);
	myCSHeader.channels = EndianS32_NtoB(myCSHeader.channels);
	myCSHeader.frameSize = EndianS32_NtoB(myCSHeader.frameSize);
	myCSHeader.frameIncr = EndianS32_NtoB(myCSHeader.frameIncr);
	myCSHeader.frameBsize = EndianS32_NtoB(myCSHeader.frameBsize);
	myCSHeader.frameFormat = EndianS32_NtoB(myCSHeader.frameFormat);
	swapLong = &(myCSHeader.minFreq);
	*swapLong = EndianU32_NtoB(*swapLong);
	swapLong = &(myCSHeader.maxFreq);
	*swapLong = EndianU32_NtoB(*swapLong);
	myCSHeader.freqFormat = EndianS32_NtoB(myCSHeader.freqFormat);
#endif
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	writeSize = sizeof(myCSHeader);
	FSWrite(mySI->dFileNum, &writeSize, &myCSHeader);
	return(TRUE);
}
