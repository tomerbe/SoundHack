/*	
 *	In this file I will put the functions to ReadHeader, WriteHeader and CreateSoundFile
 *	for Audio IFF, Audio IFC, Sound Designer II, NeXT or Sun and BICSF format
 *	soundfiles.
 *	For most formats, direct reads and writes to the data fork are all that is needed.
 *	AIFF and AIFC need a call to WriteHeader before closing the file to update the
 *	SoundInfo.numBytes information in the FORM and SOUND chunks.
 *	The AIFF and AIFC routines in here will deal with only those files with a single
 *	FORM chunk. Dealing with multiple FORM chunks would require considerable
 *	modification.
 */
#if powerc
#include <fp.h>
#endif	/*powerc*/

#include <stdio.h>
#include <stdlib.h>
#include <QuickTime/Movies.h>
#include "IEEE80.h"
#include "SoundFile.h"
#include "WriteHeader.h"
#include "CreateSoundFile.h"
#include "OpenSoundFile.h"
#include "CloseSoundFile.h"
#include "SoundHack.h"
#include "Macros.h"
#include "Misc.h"
#include "ByteSwap.h"
#include "Dialog.h"
#include "Analysis.h"
#include "PhaseVocoder.h"
#include "SpectralHeader.h"
#include "CarbonGlue.h"
#include "sdif.h"

/* globals */
SHStandardFileReply	gCreateReply;

extern short		gAppFileNum;
extern long	gNumFilesOpen;
extern SoundInfoPtr	frontSIPtr, firstSIPtr, inSIPtr;
extern PvocInfo 	gPI;
extern long gNumberBlocks;
/* All of the CreateSoundFile calls will be followed by one or more WriteHeader
 * calls, so one only needs to set up a header framework if needed by WriteHeader
 * (as with AIFF and SDII files).  Otherwise just create the files with the
 * proper fileType.
 */
 
short	CreateSoundFile(SoundInfoPtr *mySI, short dialog)
{
	short		returnValue;
	OSErr	error;
	long	i;
	SoundInfoPtr	previousSIPtr, currentSIPtr;
	
	SetDialogFont(systemFont);
	
	switch(dialog)
	{
		case NO_DIALOG:
			StringCopy((*mySI)->sfSpec.name, gCreateReply.sfFile.name);
			gCreateReply.sfFile.vRefNum = (*mySI)->sfSpec.vRefNum;
			gCreateReply.sfFile.parID = (*mySI)->sfSpec.parID;
			break;
		case SPECT_DIALOG:
			SHStandardPutFile("\pSave Spectra as:", (*mySI)->sfSpec.name, &gCreateReply);
			break;
		case SOUND_CUST_DIALOG:
			CreateSoundPutFile("\pSave SoundFile as:", (*mySI)->sfSpec.name, &gCreateReply, mySI);
			break;
		case SOUND_SIMP_DIALOG:
			SHStandardPutFile("\pSave SoundFile as:", (*mySI)->sfSpec.name, &gCreateReply);
			break;
	}
	if(gCreateReply.sfGood == FALSE)
	{
		*mySI = nil;
		return(-1);
	}
	switch((*mySI)->packMode)
	{
		case SF_FORMAT_MACE6:
			(*mySI)->frameSize = 1.0/6.0;
			StringCopy("\p6:1 MACE",(*mySI)->compName);
			break;
		case SF_FORMAT_MACE3:
			(*mySI)->frameSize = 1.0/3.0;
			StringCopy("\p3:1 MACE",(*mySI)->compName);
			break;
		case SF_FORMAT_4_ADIMA:
			(*mySI)->frameSize = 0.53125;
			StringCopy("\p4:1 IMA4 ADPCM",(*mySI)->compName);
			break;
		case SF_FORMAT_4_ADDVI:
			(*mySI)->frameSize = 0.5;
			StringCopy("\p4:1 Intel/DVI ADPCM",(*mySI)->compName);
			break;
		case SF_FORMAT_8_LINEAR:
		case SF_FORMAT_8_UNSIGNED:
			(*mySI)->frameSize = 1;
			StringCopy("\pnot compressed",(*mySI)->compName);
			break;
		case SF_FORMAT_8_MULAW:
			(*mySI)->frameSize = 1;
			StringCopy("\pCCITT G711 µLaw",(*mySI)->compName);
			break;
		case SF_FORMAT_8_ALAW:
			(*mySI)->frameSize = 1;
			StringCopy("\pCCITT G711 aLaw",(*mySI)->compName);
			break;
		case SF_FORMAT_16_SWAP:
		case SF_FORMAT_16_LINEAR:
		case SF_FORMAT_3DO_CONTENT:
			(*mySI)->frameSize = 2;
			StringCopy("\pnot compressed",(*mySI)->compName);
			break;
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_SWAP:
		case SF_FORMAT_24_COMP:
			(*mySI)->frameSize = 3;
			StringCopy("\pnot compressed",(*mySI)->compName);
			break;
		case SF_FORMAT_32_COMP:
		case SF_FORMAT_32_SWAP:
		case SF_FORMAT_32_LINEAR:
			(*mySI)->frameSize = 4;
			StringCopy("\pnot compressed",(*mySI)->compName);
			break;
		case SF_FORMAT_32_FLOAT:
			(*mySI)->frameSize = 4;
			StringCopy("\pFloat 32",(*mySI)->compName);
			break;
	}
	StringCopy(gCreateReply.sfFile.name, (*mySI)->sfSpec.name);
	(*mySI)->sfSpec.vRefNum = gCreateReply.sfFile.vRefNum;
	(*mySI)->sfSpec.parID = gCreateReply.sfFile.parID;
	
	/* Try to open to see if file exists */
	error = FSpOpenDF(&((*mySI)->sfSpec), fsCurPerm, &((*mySI)->dFileNum));
	if(error == noErr)
	{
		FSClose((*mySI)->dFileNum);
		FlushVol((StringPtr)NIL_POINTER,(*mySI)->sfSpec.vRefNum);
		FSpDelete(&(*mySI)->sfSpec);
		FlushVol((StringPtr)NIL_POINTER,(*mySI)->sfSpec.vRefNum);
	}
	else if(error == opWrErr)
	{
		// check to see if this file is already open
		previousSIPtr = currentSIPtr = firstSIPtr;
		for(i = 0; i < gNumFilesOpen; i++)
		{
			if(currentSIPtr->sfSpec.vRefNum == (*mySI)->sfSpec.vRefNum 
				&& currentSIPtr->sfSpec.parID == (*mySI)->sfSpec.parID
				&& EqualString(currentSIPtr->sfSpec.name, (*mySI)->sfSpec.name, FALSE, FALSE))
			{
				CloseSoundFile(currentSIPtr);
				FSpDelete(&(*mySI)->sfSpec);
				FlushVol((StringPtr)NIL_POINTER,(*mySI)->sfSpec.vRefNum);
				break;
			}
			else
			{
				previousSIPtr = currentSIPtr;
				currentSIPtr = (SoundInfoPtr)previousSIPtr->nextSIPtr;
			}
		}
	}
	(*mySI)->peak = (*mySI)->peakFL = (*mySI)->peakFR = (*mySI)->peakRL = (*mySI)->peakRR = 0.0; 
	switch((*mySI)->sfType)
	{
		case AIFF:
		case QKTMA:
			returnValue = CreateAIFF((*mySI));
			break;
		case AIFC:
			returnValue = CreateAIFC((*mySI));
			break;
		case WAVE:
			returnValue = CreateWAVE((*mySI));
			break;
		case BICSF:
			returnValue = CreateBICSF((*mySI));
			break;
		case SDII:
		case MMIX:
		case AUDMED:
			returnValue = CreateSDII((*mySI));
			break;
		case DSPD:
			returnValue = CreateDSPD((*mySI));
			break;
		case NEXT:
		case SUNAU:
			returnValue = CreateNEXT((*mySI));
			break;
		case TEXT:
			returnValue = CreateTEXT((*mySI));
			break;
		case RAW:
			returnValue = CreateRAW((*mySI));
			break;
		case CS_PVOC:
			returnValue = CreatePVAFile((*mySI));
			break;
		case SDIFF:
			returnValue = CreateSDIFFile((*mySI));
			break;
		case PICT:
			returnValue = CreatePICT((*mySI));
			break;
		case QKTMV:
			returnValue = CreateMovie((*mySI));
			break;
		default:
			DrawErrorMessage("\pUnknown SoundFile Type");
			returnValue = -1;
			break;
	}
	if(returnValue != -1)
		WriteHeader(*mySI);
	FSClose((*mySI)->dFileNum);
	FlushVol((StringPtr)NIL_POINTER,(*mySI)->sfSpec.vRefNum);
	if(returnValue != -1)
	{
		// this is a bit awkward.
		// i am opening the file, simply to take advantage of opensoundfile's
		// list manipulation, etc.
		OpenSoundFile((*mySI)->sfSpec, FALSE);
		// Copy over items from old SI to new SI
		frontSIPtr->sRate = (*mySI)->sRate;
		frontSIPtr->dataStart = (*mySI)->dataStart;
		frontSIPtr->numBytes = 0;
		frontSIPtr->packMode = (*mySI)->packMode;
		StringCopy((*mySI)->compName, frontSIPtr->compName);
		frontSIPtr->nChans = (*mySI)->nChans;
		frontSIPtr->frameSize = (*mySI)->frameSize;
		frontSIPtr->sfType = (*mySI)->sfType;
		frontSIPtr->spectFrameSize = (*mySI)->spectFrameSize;
		frontSIPtr->spectFrameIncr = (*mySI)->spectFrameIncr;
		frontSIPtr->peak = (*mySI)->peak;
		frontSIPtr->peakFL = (*mySI)->peakFL;
		frontSIPtr->peakFR = (*mySI)->peakFR;
		frontSIPtr->peakRL = (*mySI)->peakRL;
		frontSIPtr->peakRR = (*mySI)->peakRR; 
		RemovePtr((Ptr)(*mySI));
		(*mySI) = frontSIPtr;
	}
	else
	{
		RemovePtr((Ptr)(*mySI));
		(*mySI) = nil;
	}
	return(returnValue);
}


/* CreateAIFF must create the FORM, COMMON and SOUND chunks with empty values, I
 * will put the SOUND chunk at the end so that it is easy to add sound later
 */

short
CreateAIFF(SoundInfo *mySI)
{	
	long			writeSize, filePos;
	FormChunk		myFormChunk;
	AIFFCommonChunk		myCommonChunk;
	AIFFSoundDataChunk	mySoundDataChunk;
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;

	/*Create File*/
	if(mySI->sfType == QKTMA)
		FSpCreate(&(mySI->sfSpec), 'TVOD', 'AIFF', gCreateReply.sfScript);
	else
		FSpCreate(&(mySI->sfSpec), 'SDHK', 'AIFF', gCreateReply.sfScript);
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);
	/* Initialize chunks */
	myFormChunk.ckID = FORMID;
	myFormChunk.ckSize = sizeof(myFormChunk) - sizeof(chunkHeader);
	myFormChunk.formType = AIFFTYPE;
	
	myCommonChunk.ckID = COMMONID;
	myCommonChunk.ckSize = sizeof(myCommonChunk) - sizeof(chunkHeader);
	myCommonChunk.sampleSize = (long)(8 * mySI->frameSize);
	/* The rest of the common chunk will be updated in WriteHeader */
	
	mySoundDataChunk.ckID = SOUNDID;
	mySoundDataChunk.ckSize = sizeof(mySoundDataChunk) - sizeof(chunkHeader);
	mySoundDataChunk.offset = 0L;
	mySoundDataChunk.blockSize = 0L;
	
#ifdef SHOSXUB
	myFormChunk.ckID = EndianU32_NtoB(myFormChunk.ckID);
	myFormChunk.ckSize = EndianS32_NtoB(myFormChunk.ckSize);
	myFormChunk.formType = EndianU32_NtoB(myFormChunk.formType);
#endif
	writeSize = sizeof(myFormChunk);
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
	
	
#ifdef SHOSXUB
	myCommonChunk.ckID = EndianU32_NtoB(myCommonChunk.ckID);
	myCommonChunk.ckSize = EndianS32_NtoB(myCommonChunk.ckSize);
	myCommonChunk.sampleSize = EndianS16_NtoB(myCommonChunk.sampleSize);
#endif
	writeSize = sizeof(myCommonChunk);
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myCommonChunk);
	
	GetFPos(mySI->dFileNum, &filePos);
	mySI->dataStart = filePos + sizeof(chunkHeader) 
		+ sizeof(mySoundDataChunk.blockSize) + sizeof(mySoundDataChunk.offset)
		+ mySoundDataChunk.offset;
	
#ifdef SHOSXUB
	mySoundDataChunk.ckID = EndianU32_NtoB(mySoundDataChunk.ckID);
	mySoundDataChunk.ckSize = EndianS32_NtoB(mySoundDataChunk.ckSize);
	mySoundDataChunk.offset = EndianU32_NtoB(mySoundDataChunk.offset);
	mySoundDataChunk.blockSize = EndianU32_NtoB(mySoundDataChunk.blockSize);
#endif
	writeSize = sizeof(mySoundDataChunk);
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &mySoundDataChunk);
	return(TRUE);
}

/*
 * CreateAIFC must create the FORM, FORMVER, COMMON and SOUND chunks with empty values, I
 * will put the SOUND chunk at the end so that it is easy to add sound later
 */
short 
CreateAIFC(SoundInfo *mySI)
{	
	long				writeSize, filePos;
	Ptr					dummyBlock;
	FormChunk			myFormChunk;
	AIFCFormatVersionChunk	myFormatVersionChunk;
	AIFCCommonChunk		myCommonChunk;
	AIFFSoundDataChunk		mySoundDataChunk;
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	
	struct
	{
		long 	ckID;
		long	ckSize;
		OSType	myOSType;
	}	appSpecHeader;
	
	/*Create File*/
	FSpCreate(&(mySI->sfSpec), 'SDHK', 'AIFC', gCreateReply.sfScript);
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);

	/* Initialize chunks */
	myFormChunk.ckID = FORMID;
	myFormChunk.ckSize = sizeof(myFormChunk) - sizeof(chunkHeader);
	myFormChunk.formType = AIFCTYPE;

	myFormatVersionChunk.ckID = FORMVERID;
	myFormatVersionChunk.ckSize = sizeof(myFormatVersionChunk) - sizeof(chunkHeader);
	myFormatVersionChunk.timestamp = AIFCVERSION1;

	myCommonChunk.ckID = COMMONID;
	myCommonChunk.sampleSize = (long)(8 * mySI->frameSize);
	myCommonChunk.ckSize = 23L + mySI->compName[0];
	EVENUP(myCommonChunk.ckSize);
	/* The rest of the common chunk will be updated in WriteHeader */
	
	appSpecHeader.ckID = APPLICATIONSPECIFICID;
	appSpecHeader.ckSize = sizeof(OSType) + (sizeof(float) * (mySI->nChans + 1));
	appSpecHeader.myOSType = 'pErF';
	
	mySoundDataChunk.ckID = SOUNDID;
	mySoundDataChunk.ckSize = sizeof(mySoundDataChunk) - sizeof(chunkHeader);
	mySoundDataChunk.offset = 0L;
	mySoundDataChunk.blockSize = 0L;
	
#ifdef SHOSXUB
	myFormChunk.ckID = EndianU32_NtoB(myFormChunk.ckID);
	myFormChunk.ckSize = EndianS32_NtoB(myFormChunk.ckSize);
	myFormChunk.formType = EndianU32_NtoB(myFormChunk.formType);
#endif
	writeSize = sizeof(myFormChunk);
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
	
#ifdef SHOSXUB
	myFormatVersionChunk.ckID = EndianU32_NtoB(myFormatVersionChunk.ckID);
	myFormatVersionChunk.ckSize = EndianS32_NtoB(myFormatVersionChunk.ckSize);
	myFormatVersionChunk.timestamp = EndianU32_NtoB(myFormatVersionChunk.timestamp);
#endif
	writeSize = sizeof(myFormatVersionChunk);
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myFormatVersionChunk);
	
	writeSize = sizeof(chunkHeader) + myCommonChunk.ckSize;
#ifdef SHOSXUB
	myCommonChunk.ckID = EndianU32_NtoB(myCommonChunk.ckID);
	myCommonChunk.ckSize = EndianS32_NtoB(myCommonChunk.ckSize);
	myCommonChunk.sampleSize = EndianS16_NtoB(myCommonChunk.sampleSize);
#endif
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myCommonChunk);
	
#ifdef SHOSXUB
	appSpecHeader.ckID = EndianU32_NtoB(appSpecHeader.ckID);
	appSpecHeader.ckSize = EndianS32_NtoB(appSpecHeader.ckSize);
	appSpecHeader.myOSType = EndianU32_NtoB(appSpecHeader.myOSType);
#endif
	writeSize = sizeof(appSpecHeader);
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &appSpecHeader);
	writeSize = (sizeof(float) * (mySI->nChans + 1));
	dummyBlock = NewPtr(writeSize);
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &dummyBlock);
	DisposePtr(dummyBlock);
	
	GetFPos(mySI->dFileNum, &filePos);
	mySI->dataStart = filePos + sizeof(chunkHeader) 
		+ sizeof(mySoundDataChunk.blockSize) + sizeof(mySoundDataChunk.offset)
		+ mySoundDataChunk.offset;
	
#ifdef SHOSXUB
	mySoundDataChunk.ckID = EndianU32_NtoB(mySoundDataChunk.ckID);
	mySoundDataChunk.ckSize = EndianS32_NtoB(mySoundDataChunk.ckSize);
	mySoundDataChunk.blockSize = EndianU32_NtoB(mySoundDataChunk.blockSize);
	mySoundDataChunk.offset = EndianU32_NtoB(mySoundDataChunk.offset);
#endif
	writeSize = sizeof(mySoundDataChunk);
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &mySoundDataChunk);
	return(TRUE);
}

short
CreateWAVE(SoundInfo *mySI)
{	
	long				writeSize, filePos;
	RIFFFormChunk		myFormChunk;
	WAVEFormatChunk		myFormatChunk;
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;

	/*Create File*/
	FSpCreate(&(mySI->sfSpec), 'SDHK', 'WAVE', gCreateReply.sfScript);
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);

	/* Initialize chunks */
	myFormChunk.ckID = WAV_ID_RIFF;
	myFormChunk.ckSize = sizeof(myFormChunk) + sizeof(myFormatChunk);
	myFormChunk.formType = WAV_ID_WAVE;

	myFormatChunk.ckID = WAV_ID_FORMAT;
	myFormatChunk.ckSize = sizeof(myFormatChunk) - sizeof(chunkHeader);
	myFormatChunk.wFormatTag = WAV_FORMAT_PCM;
	myFormatChunk.wBlockAlign = (short)mySI->frameSize;
	/* The rest of the common chunk will be updated in WriteHeader */
	
	chunkHeader.ckID = WAV_ID_DATA;
	chunkHeader.ckSize = 0L;
	
#ifdef SHOSXUB
	myFormChunk.ckID = EndianU32_NtoB(myFormChunk.ckID);
	myFormChunk.ckSize = EndianS32_NtoL(myFormChunk.ckSize);
	myFormChunk.formType = EndianU32_NtoB(myFormChunk.formType);
#else
	myFormChunk.ckSize = ByteSwapLong(myFormChunk.ckSize);
#endif
	writeSize = sizeof(myFormChunk);
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myFormChunk);
		
#ifdef SHOSXUB
	myFormatChunk.ckID = EndianU32_NtoB(myFormatChunk.ckID);
	myFormatChunk.ckSize = EndianS32_NtoL(myFormatChunk.ckSize);
	myFormatChunk.wFormatTag = EndianS16_NtoL(myFormatChunk.wFormatTag);
	myFormatChunk.wBlockAlign = EndianS16_NtoL(myFormatChunk.wBlockAlign);/* frameSize?*/
#else
	myFormatChunk.ckSize = ByteSwapLong(myFormatChunk.ckSize);
	myFormatChunk.wFormatTag = ByteSwapShort(myFormatChunk.wFormatTag);
	myFormatChunk.wBlockAlign = ByteSwapShort(myFormatChunk.wBlockAlign);/* frameSize?*/
#endif
	writeSize = sizeof(myFormatChunk);
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myFormatChunk);
	
	GetFPos(mySI->dFileNum, &filePos);
	mySI->dataStart = filePos + sizeof(chunkHeader);
	
#ifdef SHOSXUB
	chunkHeader.ckID = EndianU32_NtoB(chunkHeader.ckID);
	chunkHeader.ckSize = EndianS32_NtoL(chunkHeader.ckSize);
#endif
	writeSize = sizeof(chunkHeader);
	SetFPos(mySI->dFileNum, fsAtMark, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &chunkHeader);
	return(TRUE);
}

short
CreateBICSF(SoundInfo *mySI)
{
	BICSFSoundInfo	myBICSF;
	long writeSize;
	
	/*Create File*/
	FSpCreate(&(mySI->sfSpec), 'SDHK', 'IRCM', gCreateReply.sfScript);
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);
	myBICSF.magic = BICSFMAGIC;
	myBICSF.packMode = BICSF_FORMAT_LINEAR_16;
#ifdef SHOSXUB
	myBICSF.magic = EndianS32_NtoB(myBICSF.magic);
	myBICSF.packMode = EndianS32_NtoB(myBICSF.packMode);
#endif
	writeSize = sizeof(myBICSF);
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myBICSF);
	mySI->dataStart = sizeof(BICSFSoundInfo);
	return(TRUE);
}

/* Need to create resources */
short
CreateSDII(SoundInfo *mySI)
{
	Handle		frameSizeHandle, sRateHandle, nChansHandle;

	/*Create File*/
	if(mySI->sfType == SDII)
	{
		FSpCreate(&(mySI->sfSpec), 'Sd2a', 'Sd2f', gCreateReply.sfScript);
		FSpCreateResFile(&(mySI->sfSpec), 'Sd2a', 'Sd2f', gCreateReply.sfScript);
	}
	else if(mySI->sfType == AUDMED)
	{
		FSpCreate(&(mySI->sfSpec), 'Sd2b', 'Sd2f', gCreateReply.sfScript);
		FSpCreateResFile(&(mySI->sfSpec), 'Sd2b', 'Sd2f', gCreateReply.sfScript);
	}
	else if(mySI->sfType == MMIX)
	{
		FSpCreate(&(mySI->sfSpec), 'MMIX', 'MSND', gCreateReply.sfScript);
		FSpCreateResFile(&(mySI->sfSpec), 'MMIX', 'MSND', gCreateReply.sfScript);
	}
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);

	/* Make New Resources */
	frameSizeHandle = NewHandle(2);
	sRateHandle = NewHandle(13);
	nChansHandle = NewHandle(2);
	mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec),fsCurPerm);
	UseResFile(mySI->rFileNum);
	AddResource(frameSizeHandle,'STR ',1000,"\psample-size");
	AddResource(sRateHandle,'STR ',1001,"\psample-rate");
	AddResource(nChansHandle,'STR ',1002,"\pchannels");
	CloseResFile(mySI->rFileNum);
	ReleaseResource(nChansHandle);
	ReleaseResource(sRateHandle);
	ReleaseResource(frameSizeHandle);
	UseResFile(gAppFileNum);
	mySI->dataStart = 0;
	return(TRUE);
}

/* Need to create resources */
short
CreateDSPD(SoundInfo *mySI)
{
	Handle		frameSizeHandle, sRateHandle, nChansHandle, dspSignalHandle;

	/*Create File*/
	{
		FSpCreate(&(mySI->sfSpec), 'DSP!', 'DSPs', gCreateReply.sfScript);
		FSpCreateResFile(&(mySI->sfSpec), 'DSP!', 'DSPs', gCreateReply.sfScript);
	}
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);

	/* Make New Resources */
	frameSizeHandle = NewHandle(2);
	sRateHandle = NewHandle(13);
	nChansHandle = NewHandle(2);
	dspSignalHandle = NewHandle(28);
	mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec), fsCurPerm);
	UseResFile(mySI->rFileNum);
	AddResource(frameSizeHandle,'STR ',1000,"\psample-size");
	AddResource(sRateHandle,'STR ',1001,"\psample-rate");
	AddResource(nChansHandle,'STR ',1002,"\pchannels");
	AddResource(dspSignalHandle,'DSPs',128,"\pSignal");
	CloseResFile(mySI->rFileNum);
	ReleaseResource(nChansHandle);
	ReleaseResource(sRateHandle);
	ReleaseResource(frameSizeHandle);
	ReleaseResource(dspSignalHandle);
	UseResFile(gAppFileNum);
	return(TRUE);
}

/* No Header Initialization needed */
short	CreateNEXT(SoundInfo *mySI)
{
	NeXTSoundInfo	myNI;
	long	writeSize;
	FInfo	fileInfo;
	
	/*Create File*/
	FSpCreate(&(mySI->sfSpec), 'SDHK', 'NxTS', gCreateReply.sfScript);
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);
	myNI.magic = NEXTMAGIC;
	myNI.dataSize = 0;
	switch(mySI->packMode)
	{
		case SF_FORMAT_8_LINEAR:
			myNI.dataFormat = NEXT_FORMAT_LINEAR_8;
			break;
		case SF_FORMAT_16_LINEAR:
			myNI.dataFormat = NEXT_FORMAT_LINEAR_16;
			break;
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_SWAP:
		case SF_FORMAT_24_COMP:
			myNI.dataFormat = NEXT_FORMAT_LINEAR_24;
			break;
		case SF_FORMAT_32_COMP:
		case SF_FORMAT_32_LINEAR:
			myNI.dataFormat = NEXT_FORMAT_LINEAR_32;
			break;
		case SF_FORMAT_8_MULAW:
			FSpGetFInfo(&(mySI->sfSpec),&fileInfo);
			fileInfo.fdType = 'ULAW';
			FSpSetFInfo(&(mySI->sfSpec),&fileInfo);
			myNI.dataFormat = NEXT_FORMAT_MULAW_8;
			break;
		case SF_FORMAT_8_ALAW:
			FSpGetFInfo(&(mySI->sfSpec),&fileInfo);
			fileInfo.fdType = 'ULAW';
			FSpSetFInfo(&(mySI->sfSpec),&fileInfo);
			myNI.dataFormat = NEXT_FORMAT_ALAW_8;
			break;
		case SF_FORMAT_32_FLOAT:
			myNI.dataFormat = NEXT_FORMAT_FLOAT;
			break;
	}
	// allocate 32 extra bytes for string
	myNI.dataLocation = mySI->dataStart = sizeof(NeXTSoundInfo) + 32;
#ifdef SHOSXUB
	myNI.magic = EndianS32_NtoB(myNI.magic);
	myNI.dataSize = EndianS32_NtoB(myNI.dataSize);
	myNI.dataFormat = EndianS32_NtoB(myNI.dataFormat);
	myNI.dataLocation = EndianS32_NtoB(myNI.dataLocation);
#endif
	writeSize = sizeof(NeXTSoundInfo) + 32;
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myNI);
	return(TRUE);
}

/* No Header */
short
CreateTEXT(SoundInfo *mySI)
{
	/*Create File*/
	FSpCreate(&(mySI->sfSpec), 'TEXT', 'TEXT', gCreateReply.sfScript);
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);
	mySI->dataStart = 0;
	return(TRUE);
}

/* No Header */
short
CreateRAW(SoundInfo *mySI)
{
	/*Create File*/
	FSpCreate(&(mySI->sfSpec), 'SDHK', 'DATA', gCreateReply.sfScript);
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);
	mySI->dataStart = 0;
	return(TRUE);
}

//	This function will create one or two, as neccesary phase vocoder analysis files.
//	The initial names it chooses are pvoc.1 and pvoc.2 as csound likes

short
CreatePVAFile(SoundInfo *mySI)
{
	CsoundHeader	myCSHeader;
	long writeSize;
	unsigned long *swapLong;
	
	FSpCreate(&(mySI->sfSpec), 'SDHK', 'DATA', gCreateReply.sfScript);
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);

	/* Set up the struct for pvoc analysis file, we can use the same struct for
		stereo analysis, since we will create two mono files */
	if(gPI.analysisType == CSOUND_ANALYSIS)
	{	
    	myCSHeader.frameFormat = PVA_PVOC;		/* (short) how words are org'd in frms */
		myCSHeader.channels = 1;
    }
    else if(gPI.analysisType == SOUNDHACK_ANALYSIS)
	{	
    	myCSHeader.frameFormat = PVA_POLAR;		/* (short) how words are org'd in frms */
		myCSHeader.channels = mySI->nChans;
    }
	myCSHeader.magic = CSA_MAGIC;
	myCSHeader.headBsize = sizeof(myCSHeader);
	myCSHeader.dataBsize = 0;
	myCSHeader.dataFormat = PVA_FLOAT;
	myCSHeader.samplingRate = mySI->sRate;
	myCSHeader.frameSize = mySI->spectFrameSize; // = gPI.points;
	myCSHeader.frameIncr = mySI->spectFrameIncr; // = gPI.decimation;
    myCSHeader.frameBsize = sizeof(float) * (myCSHeader.frameSize>>1 + 1)<<1;
    myCSHeader.minFreq = 0.0;			/* freq in Hz of lowest bin (exists) */
    myCSHeader.maxFreq = mySI->sRate/2.0;	/* freq in Hz of highest (or next) */
    myCSHeader.freqFormat = PVA_LIN;		/* (short) flag for log/lin frq */
	mySI->frameSize = (myCSHeader.frameSize * sizeof(float))/myCSHeader.frameIncr;
	mySI->dataStart = sizeof(myCSHeader);
	writeSize = sizeof(myCSHeader);
#ifdef SHOSXUB
	myCSHeader.magic = EndianS32_NtoB(myCSHeader.magic);
	myCSHeader.headBsize = EndianS32_NtoB(myCSHeader.headBsize);
	myCSHeader.dataBsize = EndianS32_NtoB(myCSHeader.dataBsize);
	myCSHeader.dataFormat = EndianS32_NtoB(myCSHeader.dataFormat);
	swapLong = &(myCSHeader.samplingRate);
	*swapLong = EndianU32_NtoB(*swapLong);
	myCSHeader.frameSize = EndianS32_NtoB(myCSHeader.frameSize);
	myCSHeader.frameIncr = EndianS32_NtoB(myCSHeader.frameIncr);
	myCSHeader.frameBsize = EndianS32_NtoB(myCSHeader.frameBsize);
	swapLong = &(myCSHeader.minFreq);
	*swapLong = EndianU32_NtoB(*swapLong);
	swapLong = &(myCSHeader.maxFreq);
	*swapLong = EndianU32_NtoB(*swapLong);
	myCSHeader.freqFormat = EndianS32_NtoB(myCSHeader.freqFormat);
#endif
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	FSWrite(mySI->dFileNum, &writeSize, &myCSHeader);
    return(TRUE);
}

short	CreateSDIFFile(SoundInfo *mySI)
{
	FILE	*file;
	
	float *spectrum, *polarSpectrum;
	double offset;
	char path[255];
	
	
	FSpCreate(&(mySI->sfSpec), 'SDHK', 'SDIF', gCreateReply.sfScript);
	FSS2Path(path, &(mySI->sfSpec));
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);
	file = fopen(path, "wb");
	
	if(SDIF_BeginWrite(file) != ESDIF_SUCCESS)
	{
		fclose(file);
		return(-1);
	}
	else
	{
		// write some dummy blocks to start with
		fflush(file);
		GetEOF(mySI->dFileNum, &(mySI->dataStart));
		mySI->frameSize = 32.5;
		spectrum = (float *)NewPtrClear(mySI->spectFrameSize*sizeof(float));
		polarSpectrum = (float *)NewPtrClear((mySI->spectFrameSize+2)*sizeof(float));
		WriteSDIFData(mySI, spectrum, polarSpectrum, 0.0, LEFT);
		gNumberBlocks = 1;
		if(mySI->nChans == STEREO)
			WriteSDIFData(mySI, spectrum, polarSpectrum, 0.0, RIGHT);
		offset = mySI->spectFrameIncr/mySI->sRate;
		WriteSDIFData(mySI, spectrum, polarSpectrum, offset, LEFT);
		if(mySI->nChans == STEREO)
			WriteSDIFData(mySI, spectrum, polarSpectrum, offset, RIGHT);
		gNumberBlocks = 0;
		fclose(file);
		DisposePtr((char *)spectrum);
		DisposePtr((char *)polarSpectrum);
		return(TRUE);
	}
}

//	this creates a PICT resource file for spectral data...

short	CreatePICT(SoundInfo *mySI)
{
	
	FSpCreateResFile(&(mySI->sfSpec), 'SDHK', 'SCRN', gCreateReply.sfScript);
	
    return(TRUE);
}

//	this creates a Quicktime Movie for spectral data...
// it is dependant on the inSIPtr, probably a bad idea.

short	CreateMovie(SoundInfo *mySI)
{
	Movie 			theMovie = nil;
	short 			resId = 0;
	OSErr 			err = noErr;	
	
	err = CreateMovieFile(&(mySI->sfSpec), 'TVOD', gCreateReply.sfScript, 
				createMovieFileDeleteCurFile, &(mySI->rFileNum), &theMovie );
	if(err != 0) return(err);

	err = AddMovieResource(theMovie, mySI->rFileNum, &resId, mySI->sfSpec.name);
	
	if(err != 0)
	{
			CloseMovieFile(mySI->rFileNum);
			return(err);
	}
	
	if(mySI->rFileNum) 
		CloseMovieFile(mySI->rFileNum);
	DisposeMovie(theMovie);

    return(TRUE);
}

