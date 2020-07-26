/*
 *	SoundHackª
 *	Copyright ©1993 Tom Erbe - Mills College
 *
 *	Markers.c - a way of dealing with markers, loops, high and low notes and all the
 *	other sampler related info. This will include functions to read and write this 
 *	info to and from all soundfile formats which deal with this sort of information.
 *
 *	Functions:
 *	HandleMarkerDialog()	Handle dialog for marker modification and inspection
 *							on the input soundfile.
 *	InitMarkerStruct()		Put default values into internal marker structure.
 *	ReadMarkers()			Read marker information from appropriate area in
 *							soundfile to internal structure. This is really just a
 *							branch to routines for specific soundfile types.
 *	WriteMarkers()			Write marker information from internal structure to the
 *							appropriate area in the soundfile. Again, this is just a
 *							branch to routines for specific soundfile types.
 */

#include <math.h>
#include "SoundFile.h"
#include "Dialog.h"
#include "SoundHack.h"
#include "Menu.h"
#include "Macros.h"
#include "Markers.h"
#include "Misc.h"
#include "ByteSwap.h"

DialogPtr	gMarkerDialog; 
extern MenuHandle	gTypeMenu, gFormatMenu, gProcessMenu, gFileMenu;
extern MarkerInfo	inMI, outMI;
extern SoundInfoPtr	inSIPtr, outSIPtr;
extern short	gAppFileNum, gProcessDisabled, gProcessEnabled;
extern struct
{
	Str255	editorName;
	long	editorID;
	short	openPlay;
	short	procPlay;
	short	defaultType;
	short	defaultFormat;
}	gPreferences;


void
HandleMarkerDialog(void)
{
	Str255	tmpString;
	short	itemType;
	Rect	itemRect;
	Handle	itemHandle;
	WindInfo	*theWindInfo;

	gMarkerDialog = GetNewDialog(MARKER_DLOG,NIL_POINTER,(WindowPtr)MOVE_TO_FRONT);
	
	theWindInfo = (WindInfo *)NewPtr(sizeof(WindInfo));
	theWindInfo->windType = PROCWIND;
	theWindInfo->structPtr = (Ptr)MARKER_DLOG;

#if TARGET_API_MAC_CARBON == 1
	SetWRefCon(GetDialogWindow(gMarkerDialog), (long)theWindInfo);
#else
	SetWRefCon(gMarkerDialog, (long)theWindInfo);
#endif

	gProcessDisabled = UTIL_PROCESS;
	MenuUpdate();
	InitMarkerStruct(&inMI);
	ReadMarkers(inSIPtr,&inMI);	

	inMI.currentMarker = inMI.currentLoop = 1L;
	/* Set Up initial window */
	if(inMI.numMarkers == 0)
	{
		GetDialogItem(gMarkerDialog, MK_MARKER_NUMBER_FIELD, &itemType, &itemHandle, &itemRect);
		SetDialogItemText(itemHandle, "\pNo Markers");
		HideDialogItem(gMarkerDialog, MK_MARKER_TIME_FIELD);
	}
	else
	{
		ShowDialogItem(gMarkerDialog, MK_MARKER_TIME_FIELD);
		NumToString(inMI.currentMarker, tmpString);
		GetDialogItem(gMarkerDialog, MK_MARKER_NUMBER_FIELD, &itemType, &itemHandle, &itemRect);
		SetDialogItemText(itemHandle, tmpString);
		NumToString(inMI.markPos[0], tmpString);
		GetDialogItem(gMarkerDialog, MK_MARKER_TIME_FIELD, &itemType, &itemHandle, &itemRect);
		SetDialogItemText(itemHandle, tmpString);
	}
	
	if(inMI.numLoops == 0)
	{
		GetDialogItem(gMarkerDialog, MK_LOOP_NUMBER_FIELD, &itemType, &itemHandle, &itemRect);
		SetDialogItemText(itemHandle, "\pNo Loops");
		HideDialogItem(gMarkerDialog, MK_LOOP_START_TIME_FIELD);
		HideDialogItem(gMarkerDialog, MK_LOOP_END_TIME_FIELD);
	}
	else
	{
		ShowDialogItem(gMarkerDialog, MK_LOOP_START_TIME_FIELD);
		ShowDialogItem(gMarkerDialog, MK_LOOP_END_TIME_FIELD);
		NumToString(inMI.currentLoop, tmpString);
		GetDialogItem(gMarkerDialog, MK_LOOP_NUMBER_FIELD, &itemType, &itemHandle, &itemRect);
		SetDialogItemText(itemHandle, tmpString);
		NumToString(inMI.loopStart[0], tmpString);
		GetDialogItem(gMarkerDialog, MK_LOOP_START_TIME_FIELD, &itemType, &itemHandle, &itemRect);
		SetDialogItemText(itemHandle, tmpString);
		NumToString(inMI.loopEnd[0], tmpString);
		GetDialogItem(gMarkerDialog, MK_LOOP_END_TIME_FIELD, &itemType, &itemHandle, &itemRect);
		SetDialogItemText(itemHandle, tmpString);
	}

	NumToString(inMI.baseNote, tmpString);
	GetDialogItem(gMarkerDialog, MK_BASE_NOTE_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpString);
	NumToString(inMI.lowNote, tmpString);
	GetDialogItem(gMarkerDialog, MK_LOW_NOTE_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpString);
	NumToString(inMI.highNote, tmpString);
	GetDialogItem(gMarkerDialog, MK_HIGH_NOTE_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpString);
	
	NumToString((long)inMI.lowVelocity, tmpString);
	GetDialogItem(gMarkerDialog, MK_LOW_VEL_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpString);
	NumToString((long)inMI.highVelocity, tmpString);
	GetDialogItem(gMarkerDialog, MK_HIGH_VEL_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpString);
	
	NumToString((long)inMI.detune, tmpString);
	GetDialogItem(gMarkerDialog, MK_DETUNE_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpString);
	NumToString((long)inMI.gain, tmpString);
	GetDialogItem(gMarkerDialog, MK_GAIN_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpString);

#if TARGET_API_MAC_CARBON == 1
	ShowWindow(GetDialogWindow(gMarkerDialog));
	SelectWindow(GetDialogWindow(gMarkerDialog));
	SetPort(GetDialogPort(gMarkerDialog));
#else
	ShowWindow(gMarkerDialog);
	SelectWindow(gMarkerDialog);
	SetPort(gMarkerDialog);
#endif
}

void
HandleMarkerDialogEvent(short itemHit)
{
	long	tmpLong;
	Str255	tmpString;
	short	itemType;
	Rect	itemRect;
	Handle	itemHandle;
	WindInfo	*myWI;
		
		switch(itemHit)
		{
			case MK_MARKER_UP_BUTTON:
				if(inMI.currentMarker < inMI.numMarkers)
				{
					inMI.currentMarker++;
					NumToString(inMI.currentMarker, tmpString);
					GetDialogItem(gMarkerDialog, MK_MARKER_NUMBER_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
					NumToString(inMI.markPos[inMI.currentMarker-1], tmpString);
					GetDialogItem(gMarkerDialog, MK_MARKER_TIME_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
				}
				break;
			case MK_MARKER_DOWN_BUTTON:
				if(inMI.currentMarker > 1)
				{
					inMI.currentMarker--;
					NumToString(inMI.currentMarker, tmpString);
					GetDialogItem(gMarkerDialog, MK_MARKER_NUMBER_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
					NumToString(inMI.markPos[inMI.currentMarker-1], tmpString);
					GetDialogItem(gMarkerDialog, MK_MARKER_TIME_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
				}
				break;
			case MK_LOOP_UP_BUTTON:
				if(inMI.currentLoop < inMI.numLoops)
				{
					inMI.currentLoop++;
					NumToString(inMI.currentLoop, tmpString);
					GetDialogItem(gMarkerDialog, MK_LOOP_NUMBER_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
					NumToString(inMI.loopStart[inMI.currentLoop-1], tmpString);
					GetDialogItem(gMarkerDialog, MK_LOOP_START_TIME_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
					NumToString(inMI.loopEnd[inMI.currentLoop-1], tmpString);
					GetDialogItem(gMarkerDialog, MK_LOOP_END_TIME_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
				}
				break;
			case MK_LOOP_DOWN_BUTTON:
				if(inMI.currentLoop > 1)
				{
					inMI.currentLoop--;
					NumToString(inMI.currentLoop, tmpString);
					GetDialogItem(gMarkerDialog, MK_LOOP_NUMBER_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
					NumToString(inMI.loopStart[inMI.currentLoop-1], tmpString);
					GetDialogItem(gMarkerDialog, MK_LOOP_START_TIME_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
					NumToString(inMI.loopEnd[inMI.currentLoop-1], tmpString);
					GetDialogItem(gMarkerDialog, MK_LOOP_END_TIME_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpString);
				}
				break;
			case MK_MARKER_TIME_FIELD:
				GetDialogItem(gMarkerDialog, MK_MARKER_TIME_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &(inMI.markPos[inMI.currentMarker - 1]));
				break;
			case MK_LOOP_START_TIME_FIELD:
				GetDialogItem(gMarkerDialog, MK_LOOP_START_TIME_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &(inMI.loopStart[inMI.currentLoop-1]));
				break;
			case MK_LOOP_END_TIME_FIELD:
				GetDialogItem(gMarkerDialog, MK_LOOP_END_TIME_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &(inMI.loopEnd[inMI.currentLoop-1]));
				break;
			case MK_DETUNE_FIELD:
				GetDialogItem(gMarkerDialog, MK_DETUNE_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &tmpLong);
				inMI.detune = tmpLong;
				break;
			case MK_GAIN_FIELD:
				GetDialogItem(gMarkerDialog, MK_GAIN_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &tmpLong);
				inMI.gain = tmpLong;
				break;
			case MK_HIGH_VEL_FIELD:
				GetDialogItem(gMarkerDialog, MK_HIGH_VEL_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &tmpLong);
				inMI.highVelocity = tmpLong;
				break;
			case MK_LOW_VEL_FIELD:
				GetDialogItem(gMarkerDialog, MK_LOW_VEL_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &tmpLong);
				inMI.lowVelocity = tmpLong;
				break;
			case MK_LOW_NOTE_FIELD:
				GetDialogItem(gMarkerDialog, MK_LOW_NOTE_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &tmpLong);
				inMI.lowNote = tmpLong;
				break;
			case MK_BASE_NOTE_FIELD:
				GetDialogItem(gMarkerDialog, MK_BASE_NOTE_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &tmpLong);
				inMI.baseNote = tmpLong;
				break;
			case MK_HIGH_NOTE_FIELD:
				GetDialogItem(gMarkerDialog, MK_HIGH_NOTE_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpString);
				StringToNum(tmpString, &tmpLong);
				inMI.highNote = tmpLong;
				break;
			case MK_CANCEL_BUTTON:
#if TARGET_API_MAC_CARBON == 1
				myWI = (WindInfoPtr)GetWRefCon(GetDialogWindow(gMarkerDialog));
#else
				myWI = (WindInfoPtr)GetWRefCon(gMarkerDialog);
#endif
				RemovePtr((Ptr)myWI);
				DisposeDialog(gMarkerDialog);
				gMarkerDialog = nil;
				gProcessDisabled = gProcessEnabled = NO_PROCESS;
				MenuUpdate();
				inSIPtr = nil;
				break;
			case MK_OK_BUTTON:
				WriteMarkers(inSIPtr,&inMI);
#if TARGET_API_MAC_CARBON == 1
				myWI = (WindInfoPtr)GetWRefCon(GetDialogWindow(gMarkerDialog));
#else
				myWI = (WindInfoPtr)GetWRefCon(gMarkerDialog);
#endif
				RemovePtr((Ptr)myWI);
				DisposeDialog(gMarkerDialog);
				gMarkerDialog = nil;
				gProcessDisabled = gProcessEnabled = NO_PROCESS;
				MenuUpdate();
				inSIPtr = nil;
				break;
			default:
				break;
		}
}

void
InitMarkerStruct(MarkerInfo *myMarkerInfo)
{
	myMarkerInfo->baseNote = 60;
	myMarkerInfo->lowNote = 0;
	myMarkerInfo->highNote = 127;
	myMarkerInfo->lowVelocity = 1;
	myMarkerInfo->highVelocity = 127;
	myMarkerInfo->detune = 0;
	myMarkerInfo->gain = 0;
	myMarkerInfo->numLoops = 0;
	myMarkerInfo->numMarkers = 0;
}

short
ReadMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	short	returnValue;
		
	switch(mySI->sfType)
	{
		case AIFF:
		case QKTMA:
		case AIFC:
			returnValue = ReadAIFFMarkers(mySI,myMarkerInfo);
			break;
		case SDII:
		case MMIX:
		case AUDMED:
			returnValue = ReadSDIIMarkers(mySI,myMarkerInfo);
			break;
		case WAVE:
			returnValue = ReadWAVEMarkers(mySI,myMarkerInfo);
			break;
		case BICSF:
		case MPEG:
		case DSPD:
		case NEXT:
		case SUNAU:
		case TEXT:
		case RAW:
			returnValue = TRUE;
			break;
		default:
			DrawErrorMessage("\pUnknown Sound File Type");
			returnValue = -1;
			break;
	}
	return(returnValue);
}

short
ReadAIFFMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	Str255				errStr, tmpStr;
	short					markerFound = FALSE, instrumentFound = FALSE;
	long				readSize, filePos, fileEndPos, markerBlockSize;
	long				i, j, k, l;
	Byte				textSize;
	unsigned short		sustainLoopStartID, sustainLoopEndID,
						releaseLoopStartID, releaseLoopEndID;
	FormChunk			myFormChunk;
	AIFFInstrumentChunk		myInstrumentChunk;
	MarkerChunkHeader	myMarkerChunkHeader;
	AIFFShortMarker		*myMarkerBlock;
	
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	
	
	/* Move past form chunk */
	SetFPos(mySI->dFileNum, fsFromStart, sizeof(myFormChunk));
	/* Initialize so we can get out gracefully later, if no Inst or Marker Chunks */
	myMarkerChunkHeader.numMarkers = 0;
	/* Loop through the Chunks until finding the Inst Chunk and Mark Chunk */
	while(instrumentFound == FALSE || markerFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		FSRead(mySI->dFileNum, &readSize, &chunkHeader);
#ifdef SHOSXUB
		chunkHeader.ckID = EndianU32_BtoN(chunkHeader.ckID);
		chunkHeader.ckSize = EndianU32_BtoN(chunkHeader.ckSize);
#endif
		switch(chunkHeader.ckID)
		{
			case INSTRUMENTID:
				readSize = sizeof(myInstrumentChunk);
				GetFPos(mySI->dFileNum, &filePos);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSRead(mySI->dFileNum, &readSize, &myInstrumentChunk);
#ifdef SHOSXUB
				myInstrumentChunk.gain = EndianS16_BtoN(myInstrumentChunk.gain);
				myInstrumentChunk.sustainLoop.playMode = EndianS16_BtoN(myInstrumentChunk.sustainLoop.playMode);
				myInstrumentChunk.sustainLoop.beginLoop = EndianU16_BtoN(myInstrumentChunk.sustainLoop.beginLoop);
				myInstrumentChunk.sustainLoop.endLoop = EndianU16_BtoN(myInstrumentChunk.sustainLoop.endLoop);
				myInstrumentChunk.releaseLoop.playMode = EndianS16_BtoN(myInstrumentChunk.releaseLoop.playMode);
				myInstrumentChunk.releaseLoop.beginLoop = EndianU16_BtoN(myInstrumentChunk.releaseLoop.beginLoop);
				myInstrumentChunk.releaseLoop.endLoop = EndianU16_BtoN(myInstrumentChunk.releaseLoop.endLoop);
#endif
				myMarkerInfo->baseNote = myInstrumentChunk.baseFrequency;
				myMarkerInfo->lowNote = myInstrumentChunk.lowFrequency;
				myMarkerInfo->highNote = myInstrumentChunk.highFrequency;
				myMarkerInfo->lowVelocity = myInstrumentChunk.lowVelocity;
				myMarkerInfo->highVelocity = myInstrumentChunk.highVelocity;
				myMarkerInfo->detune = myInstrumentChunk.detune;
				myMarkerInfo->gain = myInstrumentChunk.gain;
				if(myInstrumentChunk.sustainLoop.playMode != 0)
					myMarkerInfo->numLoops = 1;
				else
					myMarkerInfo->numLoops = 0;
				sustainLoopStartID = myInstrumentChunk.sustainLoop.beginLoop;
				sustainLoopEndID = myInstrumentChunk.sustainLoop.endLoop;
				if(myInstrumentChunk.releaseLoop.playMode != 0)
					myMarkerInfo->numLoops++;
				releaseLoopStartID = myInstrumentChunk.releaseLoop.beginLoop;
				releaseLoopEndID = myInstrumentChunk.releaseLoop.endLoop;
				instrumentFound = TRUE;
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				break;
			case MARKERID:
				readSize = sizeof(myMarkerChunkHeader);
				GetFPos(mySI->dFileNum, &filePos);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSRead(mySI->dFileNum, &readSize, &myMarkerChunkHeader);
#ifdef SHOSXUB
				myMarkerChunkHeader.numMarkers = EndianU16_BtoN(myMarkerChunkHeader.numMarkers);
#endif
				markerBlockSize = myMarkerChunkHeader.numMarkers * sizeof(AIFFShortMarker);
				myMarkerBlock = (AIFFShortMarker *)NewPtr(markerBlockSize);
				/* Read the Markers, skipping over the text */
				for(i = 0; i < myMarkerChunkHeader.numMarkers; i++)
				{
					readSize = sizeof(AIFFShortMarker);
					FSRead(mySI->dFileNum, &readSize, (myMarkerBlock + i));
#ifdef SHOSXUB
					(myMarkerBlock + i)->MarkerID = EndianU16_BtoN((myMarkerBlock + i)->MarkerID);
					(myMarkerBlock + i)->position = EndianU32_BtoN((myMarkerBlock + i)->position);
#endif
					readSize = 1L;
					FSRead(mySI->dFileNum, &readSize, &textSize);
					SetFPos(mySI->dFileNum, fsFromMark, textSize+1);
				}
				markerFound = TRUE;
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				break;
			case FORMVERID:
			case SOUNDID:
			case COMMONID:
			case MIDIDATAID:
			case AUDIORECORDINGID:
			case APPLICATIONSPECIFICID:
			case COMMENTID:
			case NAMEID:
			case JUNKID:
			case AUTHORID:
			case COPYRIGHTID:
			case ANNOTATIONID:
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
			default:
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
		}
		GetFPos(mySI->dFileNum, &filePos);
		GetEOF(mySI->dFileNum, &fileEndPos);
		if(filePos >= fileEndPos)
		{
			if(instrumentFound == FALSE || markerFound == FALSE)
			{
				instrumentFound = markerFound = TRUE; /* Set these to jump out of loop */
				myMarkerInfo->numLoops = 0;
				myMarkerInfo->numMarkers = 0;
			}
		}
	}
	if(myMarkerChunkHeader.numMarkers == 0)
	{
		myMarkerInfo->numMarkers = 0;
		myMarkerInfo->numLoops = 0;
	}
	else
/*
 *	Go through the markers and see which ones correspond to loops. Set the loopStart
 *	and loopEnd. Set the markPos for markers that don't correspond to loops.
 *	Deallocate the AIFFMarker memory when done. This is so much fun!
 */
	{
		myMarkerInfo->numMarkers = myMarkerChunkHeader.numMarkers - (myMarkerInfo->numLoops * 2);
		if(myMarkerInfo->loopStart != nil)
			DisposePtr((Ptr)myMarkerInfo->loopStart);
		myMarkerInfo->loopStart = (long *)NewPtr(myMarkerInfo->numLoops * sizeof(long));
		if(myMarkerInfo->loopEnd != nil)
			DisposePtr((Ptr)myMarkerInfo->loopEnd);
		myMarkerInfo->loopEnd = (long *)NewPtr(myMarkerInfo->numLoops * sizeof(long));
		if(myMarkerInfo->markPos != nil)
			DisposePtr((Ptr)myMarkerInfo->markPos);
		myMarkerInfo->markPos = (long *)NewPtr(myMarkerInfo->numMarkers * sizeof(long));

		for(i = j = k = l = 0; (myMarkerBlock + i)<(myMarkerBlock + myMarkerChunkHeader.numMarkers); i++)
		{
			if(((myMarkerBlock + i)->MarkerID) == sustainLoopStartID)
			{
				myMarkerInfo->loopStart[j] = (myMarkerBlock + i)->position;
				j++;
			}
			else if(((myMarkerBlock + i)->MarkerID) == sustainLoopEndID)
			{
				myMarkerInfo->loopEnd[k] = (myMarkerBlock + i)->position;
				k++;
			}
			else if(((myMarkerBlock + i)->MarkerID) == releaseLoopStartID)
			{
				myMarkerInfo->loopStart[j] = (myMarkerBlock + i)->position;
				j++;
			}
			else if(((myMarkerBlock + i)->MarkerID) == releaseLoopEndID)
			{
				myMarkerInfo->loopEnd[k] = (myMarkerBlock + i)->position;
				k++;
			}
			else
			{
				myMarkerInfo->markPos[l] = (myMarkerBlock + i)->position;
				l++;
			}
		}
		RemovePtr((Ptr)myMarkerBlock);
	}
	return(TRUE);
}

short	ReadWAVEMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	short			smplFound = FALSE, instFound = FALSE, cueFound = FALSE;
	long			readSize, filePos, fileEndPos, i;
	WAVEInstChunk	aInstChunk;
	WAVESampleHeader	aSampHeader;
	WAVESampleLoop	aLoop;
	WAVECueHeader	aCueHeader;
	WAVECuePoint	aCuePoint;
	
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	
	SetFPos(mySI->dFileNum, fsFromStart, sizeof(RIFFFormChunk));
	/* Loop through the Chunks until finding the Instrument Chunk and Sample Chunk */
	while(smplFound == FALSE || instFound == FALSE || cueFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		FSRead(mySI->dFileNum, &readSize, &chunkHeader);
		switch(chunkHeader.ckID)
		{
			case WAV_ID_SAMPLE:
				GetFPos(mySI->dFileNum, &filePos);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				readSize = sizeof(aSampHeader);
				FSRead(mySI->dFileNum, &readSize, &aSampHeader);
#ifdef SHOSXUB
				chunkHeader.ckSize = EndianS32_LtoN(chunkHeader.ckSize);
				myMarkerInfo->baseNote = EndianS32_LtoN(aSampHeader.dwMIDIUnityNote);
				myMarkerInfo->detune = EndianS32_LtoN(aSampHeader.dwMIDIPitchFraction)/42949672L;
				myMarkerInfo->numLoops = EndianS32_LtoN(aSampHeader.cSampleLoops);
#else
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
				myMarkerInfo->baseNote = ByteSwapLong(aSampHeader.dwMIDIUnityNote);
				myMarkerInfo->detune = ByteSwapLong(aSampHeader.dwMIDIPitchFraction)/42949672L;
				myMarkerInfo->numLoops = ByteSwapLong(aSampHeader.cSampleLoops);
#endif

				if(myMarkerInfo->loopStart != nil)
					DisposePtr((Ptr)myMarkerInfo->loopStart);
				myMarkerInfo->loopStart = (long *)NewPtr(myMarkerInfo->numLoops * sizeof(long));
				if(myMarkerInfo->loopEnd != nil)
					DisposePtr((Ptr)myMarkerInfo->loopEnd);
				myMarkerInfo->loopEnd = (long *)NewPtr(myMarkerInfo->numLoops * sizeof(long));
				for(i = 0; i < myMarkerInfo->numLoops; i++)
				{
					readSize = sizeof(aLoop);
					FSRead(mySI->dFileNum, &readSize, &aLoop);
#ifdef SHOSXUB
					myMarkerInfo->loopStart[i] = EndianS32_LtoN(aLoop.dwStart);
					myMarkerInfo->loopEnd[i] = EndianS32_LtoN(aLoop.dwEnd);
#else
					myMarkerInfo->loopStart[i] = ByteSwapLong(aLoop.dwStart);
					myMarkerInfo->loopEnd[i] = ByteSwapLong(aLoop.dwEnd);
#endif
				}
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				GetFPos(mySI->dFileNum, &filePos);
				smplFound = TRUE;
				break;
			case WAV_ID_CUE:
				GetFPos(mySI->dFileNum, &filePos);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				readSize = sizeof(aCueHeader);
				FSRead(mySI->dFileNum, &readSize, &aCueHeader);
#ifdef SHOSXUB
				chunkHeader.ckSize = EndianS32_LtoN(chunkHeader.ckSize);
				myMarkerInfo->numMarkers = EndianS32_LtoN(aCueHeader.dwCuePoints);
#else
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
				myMarkerInfo->numMarkers = ByteSwapLong(aCueHeader.dwCuePoints);
#endif
				if(myMarkerInfo->markPos != nil)
					DisposePtr((Ptr)myMarkerInfo->markPos);
				myMarkerInfo->markPos = (long *)NewPtr(myMarkerInfo->numMarkers * sizeof(long));
				for(i = 0; i < myMarkerInfo->numMarkers; i++)
				{
					readSize = sizeof(aCuePoint);
					FSRead(mySI->dFileNum, &readSize, &aCuePoint);
#ifdef SHOSXUB
					myMarkerInfo->markPos[i] = EndianS32_LtoN(aCuePoint.dwSampleOffset);
#else
					myMarkerInfo->markPos[i] = ByteSwapLong(aCuePoint.dwSampleOffset);
#endif
				}
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				GetFPos(mySI->dFileNum, &filePos);
				cueFound = TRUE;
				break;
			case WAV_ID_INST:
				GetFPos(mySI->dFileNum, &filePos);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				readSize = sizeof(aInstChunk);
				FSRead(mySI->dFileNum, &readSize, &aInstChunk);
#ifdef SHOSXUB
				chunkHeader.ckSize = EndianS32_LtoN(chunkHeader.ckSize);
#else
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				myMarkerInfo->baseNote = aInstChunk.bUnshiftedNote;
				myMarkerInfo->detune = aInstChunk.chFineTune;
				myMarkerInfo->gain = aInstChunk.chGain;
				myMarkerInfo->lowNote = aInstChunk.bLowNote;
				myMarkerInfo->highNote = aInstChunk.bHighNote;
				myMarkerInfo->lowVelocity = aInstChunk.bLowVelocity;
				myMarkerInfo->highVelocity = aInstChunk.bHighVelocity;
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				GetFPos(mySI->dFileNum, &filePos);
				instFound = TRUE;
				break;
			default:
#ifdef SHOSXUB
				chunkHeader.ckSize = EndianS32_LtoN(chunkHeader.ckSize);
#else
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
		}
		GetFPos(mySI->dFileNum, &filePos);
		GetEOF(mySI->dFileNum, &fileEndPos);
		if(filePos >= fileEndPos)
			cueFound = smplFound = instFound = TRUE; /* Set these to jump out of loop */
	}
	return(TRUE);
}

short	ReadSDIIMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	Handle		markerHandle, loopHandle;
	extern short	gAppFileNum;
	
	long		i, size, offset, blockSize;
	SDMarkerHeader	mySDMarkerHeader;
	SDLoopHeader	mySDLoopHeader;
	SDShortMarker	*myMarkerBlock;
	SDLoop			*myLoopBlock;
	
	
	mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec), fsCurPerm);
	if(mySI->rFileNum == -1)
		return(-1);
	UseResFile(mySI->rFileNum);
	markerHandle = GetResource('sdML', 1000);
	loopHandle = GetResource('sdLL', 1000);
	
	myMarkerInfo->numLoops = 0;
	mySDMarkerHeader.numMarkers = myMarkerInfo->numMarkers = 0;	/* Initialize */
	if(markerHandle!=0)
	{
		MoveHHi(markerHandle);
		HLock(markerHandle);
		size = GetResourceSizeOnDisk(markerHandle);
		BlockMoveData(*markerHandle, &mySDMarkerHeader, size);
		blockSize = mySDMarkerHeader.numMarkers * sizeof(SDShortMarker);
		myMarkerBlock = (SDShortMarker *)NewPtr(blockSize);
		myMarkerInfo->numMarkers = mySDMarkerHeader.numMarkers;
		if(myMarkerInfo->markPos != nil)
			DisposePtr((Ptr)myMarkerInfo->markPos);
		myMarkerInfo->markPos = (long *)NewPtr(myMarkerInfo->numMarkers * sizeof(long));
		offset = sizeof(mySDMarkerHeader);
		for(i = 0; i < mySDMarkerHeader.numMarkers; i++)
		{
			BlockMoveData((*markerHandle+offset), (myMarkerBlock + i), sizeof(SDShortMarker));
			offset = offset + sizeof(SDShortMarker) + (myMarkerBlock + i)->textLength;
			myMarkerInfo->markPos[i] = (myMarkerBlock + i)->position;
		}
		HUnlock(markerHandle);
		RemovePtr((Ptr)myMarkerBlock);
	}
	if(loopHandle!=0)
	{
		MoveHHi(loopHandle);
		HLock(loopHandle);
		size = GetResourceSizeOnDisk(loopHandle);
		BlockMoveData(*loopHandle, &mySDLoopHeader, size);
		blockSize = mySDLoopHeader.numLoops * sizeof(SDLoop);
		myLoopBlock = (SDLoop *)NewPtr(blockSize);
		myMarkerInfo->numLoops = mySDLoopHeader.numLoops;
		if(myMarkerInfo->loopStart != nil)
			DisposePtr((Ptr)myMarkerInfo->loopStart);
		myMarkerInfo->loopStart = (long *)NewPtr(myMarkerInfo->numLoops * sizeof(long));
		if(myMarkerInfo->loopEnd != nil)
			DisposePtr((Ptr)myMarkerInfo->loopEnd);
		myMarkerInfo->loopEnd = (long *)NewPtr(myMarkerInfo->numLoops * sizeof(long));
		offset = sizeof(mySDLoopHeader);
		for(i = 0; i < mySDLoopHeader.numLoops; i++)
		{
			BlockMoveData((*loopHandle+offset), (myLoopBlock + i), sizeof(SDLoop));
			offset = offset + sizeof(SDLoop);
			myMarkerInfo->loopStart[i] = (myLoopBlock + i)->loopStart;
			myMarkerInfo->loopEnd[i] = (myLoopBlock + i)->loopEnd;
		}
		HUnlock(loopHandle);
		RemovePtr((Ptr)myLoopBlock);
	}
	CloseResFile(mySI->rFileNum);
	ReleaseResource(markerHandle);
	ReleaseResource(loopHandle);
	UseResFile(gAppFileNum);
	return(TRUE);
}

short
WriteMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	short	returnValue;
		
	switch(mySI->sfType)
	{
		case AIFF:
		case QKTMA:
		case AIFC:
			returnValue = WriteAIFFMarkers(mySI,myMarkerInfo);
			break;
		case WAVE:
			returnValue = WriteWAVEMarkers(mySI,myMarkerInfo);
			break;
		case SDII:
		case MMIX:
		case AUDMED:
			returnValue = WriteSDIIMarkers(mySI,myMarkerInfo);
			break;
		case BICSF:
		case MPEG:
		case DSPD:
		case NEXT:
		case SUNAU:
		case TEXT:
		case RAW:
			returnValue = TRUE;
			break;
		default:
			DrawErrorMessage("\pUnknown Sound File Type");
			returnValue = -1;
			break;
	}
	return(returnValue);
}


short WriteAIFFMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	Str255				errStr, tmpStr;
	short					markerFound = FALSE, instrumentFound = FALSE;
	long				readSize, writeSize, filePos, fileEndPos;
	long				i, j, k;
	Byte				textSize;
	unsigned short		sustainLoopStartID, sustainLoopEndID,
						releaseLoopStartID, releaseLoopEndID;
	FormChunk			myFormChunk;
	AIFFInstrumentChunk		myInstrumentChunk;
	MarkerChunkHeader	myMarkerChunkHeader;
	AIFFShortMarker		myAIFFMarker;
	
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	
	sustainLoopStartID = 1;	/* Using these marker ID numbers shouldn't hurt anything */
	sustainLoopEndID = 2;
	releaseLoopStartID = 3;
	releaseLoopEndID = 4;
	/* Move past form chunk */
	SetFPos(mySI->dFileNum, fsFromStart, sizeof(myFormChunk));
	/* Loop through the Chunks until finding the Instrument Chunk and Marker Chunk */
	while(instrumentFound == FALSE || markerFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		FSRead(mySI->dFileNum, &readSize, &chunkHeader);
#ifdef SHOSXUB
		chunkHeader.ckID = EndianU32_BtoN(chunkHeader.ckID);
		chunkHeader.ckSize = EndianU32_BtoN(chunkHeader.ckSize);
#endif
		switch(chunkHeader.ckID)
		{
			case INSTRUMENTID:
				writeSize = sizeof(myInstrumentChunk);
				GetFPos(mySI->dFileNum, &filePos);
				myInstrumentChunk.ckID = chunkHeader.ckID;
				myInstrumentChunk.ckSize = chunkHeader.ckSize;
				myInstrumentChunk.baseFrequency = myMarkerInfo->baseNote;
				myInstrumentChunk.lowFrequency = myMarkerInfo->lowNote;
				myInstrumentChunk.highFrequency = myMarkerInfo->highNote;
				myInstrumentChunk.lowVelocity = myMarkerInfo->lowVelocity;
				myInstrumentChunk.highVelocity = myMarkerInfo->highVelocity;
				myInstrumentChunk.detune = myMarkerInfo->detune;
				myInstrumentChunk.gain = myMarkerInfo->gain;
				
				myInstrumentChunk.releaseLoop.playMode = 0; /* Initialize */
				myInstrumentChunk.sustainLoop.playMode = 0;
				switch(myMarkerInfo->numLoops)
				{
					default:
					case 2:
						myInstrumentChunk.sustainLoop.playMode = 1;
						myInstrumentChunk.sustainLoop.beginLoop = sustainLoopStartID;
						myInstrumentChunk.sustainLoop.endLoop = sustainLoopEndID;
						myInstrumentChunk.releaseLoop.playMode = 1;
						myInstrumentChunk.releaseLoop.beginLoop = releaseLoopStartID;
						myInstrumentChunk.releaseLoop.endLoop = releaseLoopEndID;
						break;
					case 1:
						myInstrumentChunk.sustainLoop.playMode = 1;
						myInstrumentChunk.sustainLoop.beginLoop = sustainLoopStartID;
						myInstrumentChunk.sustainLoop.endLoop = sustainLoopEndID;
						myInstrumentChunk.releaseLoop.playMode = 0;
						myInstrumentChunk.releaseLoop.beginLoop = 0;
						myInstrumentChunk.releaseLoop.endLoop = 0;
						break;
					case 0:
						myInstrumentChunk.sustainLoop.playMode = 0;
						myInstrumentChunk.sustainLoop.beginLoop = 0;
						myInstrumentChunk.sustainLoop.endLoop = 0;
						myInstrumentChunk.releaseLoop.playMode = 0;
						myInstrumentChunk.releaseLoop.beginLoop = 0;
						myInstrumentChunk.releaseLoop.endLoop = 0;
						break;
				}
#ifdef SHOSXUB
				myInstrumentChunk.gain = EndianS16_NtoB(myInstrumentChunk.gain);
				myInstrumentChunk.sustainLoop.playMode = EndianS16_NtoB(myInstrumentChunk.sustainLoop.playMode);
				myInstrumentChunk.sustainLoop.beginLoop = EndianU16_NtoB(myInstrumentChunk.sustainLoop.beginLoop);
				myInstrumentChunk.sustainLoop.endLoop = EndianU16_NtoB(myInstrumentChunk.sustainLoop.endLoop);
				myInstrumentChunk.releaseLoop.playMode = EndianS16_NtoB(myInstrumentChunk.releaseLoop.playMode);
				myInstrumentChunk.releaseLoop.beginLoop = EndianU16_NtoB(myInstrumentChunk.releaseLoop.beginLoop);
				myInstrumentChunk.releaseLoop.endLoop = EndianU16_NtoB(myInstrumentChunk.releaseLoop.endLoop);
#endif
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &myInstrumentChunk);
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				instrumentFound = TRUE;
				break;
			case MARKERID:
				GetFPos(mySI->dFileNum, &filePos);
				writeSize = sizeof(myMarkerChunkHeader);
				myMarkerChunkHeader.ckID = chunkHeader.ckID;
				myMarkerChunkHeader.ckSize = chunkHeader.ckSize;
				myMarkerChunkHeader.numMarkers = myMarkerInfo->numMarkers + (myMarkerInfo->numLoops * 2);
#ifdef SHOSXUB
				myMarkerChunkHeader.numMarkers = EndianU16_NtoB(myMarkerChunkHeader.numMarkers);
#endif
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSWrite(mySI->dFileNum, &writeSize, &myMarkerChunkHeader);
				
				/* Write the Markers, skipping over the text */
				for(i = 0, j = 1; i < myMarkerInfo->numLoops; i++, j++)
				{
					writeSize = sizeof(AIFFShortMarker);
					readSize = 1L;
					myAIFFMarker.MarkerID = j;
					myAIFFMarker.position = myMarkerInfo->loopStart[i];
#ifdef SHOSXUB
					myAIFFMarker.MarkerID = EndianU16_NtoB(myAIFFMarker.MarkerID);
					myAIFFMarker.position = EndianU32_NtoB(myAIFFMarker.position);
#endif
					FSWrite(mySI->dFileNum, &writeSize, &myAIFFMarker);
					FSRead(mySI->dFileNum, &readSize, &textSize);
					SetFPos(mySI->dFileNum, fsFromMark, textSize+1);
					j++;
					myAIFFMarker.MarkerID = j;
					myAIFFMarker.position = myMarkerInfo->loopEnd[i];
#ifdef SHOSXUB
					myAIFFMarker.MarkerID = EndianU16_NtoB(myAIFFMarker.MarkerID);
					myAIFFMarker.position = EndianU32_NtoB(myAIFFMarker.position);
#endif
					FSWrite(mySI->dFileNum, &writeSize, &myAIFFMarker);
					FSRead(mySI->dFileNum, &readSize, &textSize);
					SetFPos(mySI->dFileNum, fsFromMark, textSize+1);
				}
				i += myMarkerInfo->numLoops; /* Have 'i' reflect the number of markers */
				for(k = 0; i < myMarkerChunkHeader.numMarkers; i++, j++, k++)
				{
					writeSize = sizeof(AIFFShortMarker);
					readSize = 1L;
					myAIFFMarker.MarkerID = j;
					myAIFFMarker.position = myMarkerInfo->markPos[k];
#ifdef SHOSXUB
					myAIFFMarker.MarkerID = EndianU16_NtoB(myAIFFMarker.MarkerID);
					myAIFFMarker.position = EndianU32_NtoB(myAIFFMarker.position);
#endif
					FSWrite(mySI->dFileNum, &writeSize, &myAIFFMarker);
					FSRead(mySI->dFileNum, &readSize, &textSize);
					SetFPos(mySI->dFileNum, fsFromMark, textSize+1);
				}
				markerFound = TRUE;
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				break;
			case FORMVERID:
			case SOUNDID:
			case COMMONID:
			case MIDIDATAID:
			case AUDIORECORDINGID:
			case APPLICATIONSPECIFICID:
			case COMMENTID:
			case NAMEID:
			case AUTHORID:
			case COPYRIGHTID:
			case ANNOTATIONID:
			case JUNKID:
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
			default:
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
		}
		GetFPos(mySI->dFileNum, &filePos);
		GetEOF(mySI->dFileNum, &fileEndPos);
		if(filePos >= fileEndPos)
			instrumentFound = markerFound = TRUE; /* Set these to jump out of loop */
	}
	return(TRUE);
}

short	WriteWAVEMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	short			smplFound = FALSE, instFound = FALSE, cueFound = FALSE;
	long			readSize, writeSize, filePos, fileEndPos, toBeSwapped, i;
	WAVEInstChunk	aInstChunk;
	WAVESampleHeader	aSampHeader;
	WAVESampleLoop	aLoop;
	WAVECueHeader	aCueHeader;
	WAVECuePoint	aCuePoint;
	
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	
	SetFPos(mySI->dFileNum, fsFromStart, sizeof(RIFFFormChunk));
	/* Loop through the Chunks until finding the Instrument Chunk and Sample Chunk */
	while(smplFound == FALSE || instFound == FALSE || cueFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		FSRead(mySI->dFileNum, &readSize, &chunkHeader);
		switch(chunkHeader.ckID)
		{
			case WAV_ID_SAMPLE:
				GetFPos(mySI->dFileNum, &filePos);
				aSampHeader.ckID = WAV_ID_SAMPLE;
				aSampHeader.ckSize = chunkHeader.ckSize;
#ifdef SHOSXUB
				chunkHeader.ckSize = EndianS32_NtoL(chunkHeader.ckSize);
#else
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				aSampHeader.dwManufacturer = 0UL;
				aSampHeader.dwProduct = 0UL;
				toBeSwapped = (long)(1000000000.0/mySI->sRate);
#ifdef SHOSXUB
				aSampHeader.dwSamplePeriod = EndianS32_NtoL(toBeSwapped);
#else				
				aSampHeader.dwSamplePeriod = ByteSwapLong(toBeSwapped);
#endif

				toBeSwapped = myMarkerInfo->baseNote;
#ifdef SHOSXUB
				aSampHeader.dwMIDIUnityNote = EndianS32_NtoL(toBeSwapped);
#else				
				aSampHeader.dwMIDIUnityNote = ByteSwapLong(toBeSwapped);
#endif
				
				toBeSwapped = myMarkerInfo->detune * 42949672L;
#ifdef SHOSXUB
				aSampHeader.dwMIDIPitchFraction = EndianS32_NtoL(toBeSwapped);
#else				
				aSampHeader.dwMIDIPitchFraction = ByteSwapLong(toBeSwapped);
#endif
				aSampHeader.dwSMPTEFormat = 0UL;
				aSampHeader.dwSMPTEOffset = 0UL;
				toBeSwapped = myMarkerInfo->numLoops;
#ifdef SHOSXUB
				aSampHeader.cSampleLoops = EndianS32_NtoL(toBeSwapped);
#else				
				aSampHeader.cSampleLoops = ByteSwapLong(toBeSwapped);
#endif
				aSampHeader.cbSamplerData = 0UL;
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				writeSize = sizeof(aSampHeader);
				FSWrite(mySI->dFileNum, &writeSize, &aSampHeader);
				for(i = 0; i < myMarkerInfo->numLoops; i++)
				{
#ifdef SHOSXUB
					aLoop.dwIdentifier = EndianS32_NtoL(i);
					aLoop.dwStart = EndianS32_NtoL(myMarkerInfo->loopStart[i]);
					aLoop.dwEnd = EndianS32_NtoL(myMarkerInfo->loopEnd[i]);
#else				
					aLoop.dwIdentifier = ByteSwapLong(i);
					aLoop.dwStart = ByteSwapLong(myMarkerInfo->loopStart[i]);
					aLoop.dwEnd = ByteSwapLong(myMarkerInfo->loopEnd[i]);
#endif
					aLoop.dwType = 0;
					aLoop.dwFraction = 0;
					aLoop.dwPlayCount = 0;
					writeSize = sizeof(aLoop);
					FSWrite(mySI->dFileNum, &writeSize, &aLoop);
				}
				smplFound = TRUE;
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				GetFPos(mySI->dFileNum, &filePos);
				break;
			case WAV_ID_CUE:
				GetFPos(mySI->dFileNum, &filePos);
				aCueHeader.ckID = WAV_ID_CUE;
				aCueHeader.ckSize = chunkHeader.ckSize;
				
				toBeSwapped = myMarkerInfo->numLoops;
#ifdef SHOSXUB
				chunkHeader.ckSize = EndianS32_NtoL(chunkHeader.ckSize);
#else				
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				toBeSwapped = myMarkerInfo->numMarkers;
#ifdef SHOSXUB
				aCueHeader.dwCuePoints = EndianS32_NtoL(toBeSwapped);
#else				
				aCueHeader.dwCuePoints = ByteSwapLong(toBeSwapped);
#endif
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				writeSize = sizeof(aCueHeader);
				FSWrite(mySI->dFileNum, &writeSize, &aCueHeader);
				for(i = 0; i < myMarkerInfo->numMarkers; i++)
				{
#ifdef SHOSXUB
					aCuePoint.dwName = EndianS32_NtoL(i);
					aCuePoint.dwPosition = EndianS32_NtoL(i);
					aCuePoint.dwSampleOffset = EndianS32_NtoL(myMarkerInfo->markPos[i]);
#else				
					aCuePoint.dwName = ByteSwapLong(i);
					aCuePoint.dwPosition = ByteSwapLong(i);
					aCuePoint.dwSampleOffset = ByteSwapLong(myMarkerInfo->markPos[i]);
#endif
					aCuePoint.fccChunk = WAV_ID_DATA;
					aCuePoint.dwChunkStart = 0L;
					aCuePoint.dwBlockStart = 0L;	
					writeSize = sizeof(aCuePoint);
					FSWrite(mySI->dFileNum, &writeSize, &aCuePoint);
				}
				cueFound = TRUE;
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				GetFPos(mySI->dFileNum, &filePos);
				break;
			case WAV_ID_INST:
				GetFPos(mySI->dFileNum, &filePos);
				aInstChunk.ckID = chunkHeader.ckID;
				aInstChunk.ckSize = chunkHeader.ckSize;
#ifdef SHOSXUB
				chunkHeader.ckSize = EndianS32_NtoL(chunkHeader.ckSize);
#else				
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				aInstChunk.bUnshiftedNote = myMarkerInfo->baseNote;
				aInstChunk.chFineTune = myMarkerInfo->detune;
				aInstChunk.chGain = myMarkerInfo->gain;
				aInstChunk.bLowNote = myMarkerInfo->lowNote;
				aInstChunk.bHighNote = myMarkerInfo->highNote;
				aInstChunk.bLowVelocity = myMarkerInfo->lowVelocity;
				aInstChunk.bHighVelocity = myMarkerInfo->highVelocity;
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				writeSize = sizeof(aInstChunk);
				FSWrite(mySI->dFileNum, &writeSize, &aInstChunk);
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				GetFPos(mySI->dFileNum, &filePos);
				instFound = TRUE;
				break;
			default:
#ifdef SHOSXUB
				chunkHeader.ckSize = EndianS32_NtoL(chunkHeader.ckSize);
#else				
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
		}
		GetFPos(mySI->dFileNum, &filePos);
		GetEOF(mySI->dFileNum, &fileEndPos);
		if(filePos >= fileEndPos)
			cueFound = smplFound = instFound = TRUE; /* Set these to jump out of loop */
	}
	return(TRUE);
}

short
WriteSDIIMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	Handle		markerHandle, loopHandle;
	
	long		i, size, offset, blockSize;
	SDMarkerHeader	mySDMarkerHeader;
	SDLoopHeader	mySDLoopHeader;
	SDShortMarker	*myMarkerBlock;
	SDLoop			*myLoopBlock;
	
	mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec), fsCurPerm);
	if(mySI->rFileNum == -1)
		return(-1);
	UseResFile(mySI->rFileNum);
	markerHandle = GetResource('sdML', 1000);
	loopHandle = GetResource('sdLL', 1000);
	
	if(markerHandle!=0)
	{
		size = sizeof(mySDMarkerHeader) + (myMarkerInfo->numMarkers * sizeof(SDShortMarker));
		SetHandleSize(markerHandle,size);
		MoveHHi(markerHandle);
		HLock(markerHandle);
		mySDMarkerHeader.version = 1;
		mySDMarkerHeader.markerOffset = 0L;
		mySDMarkerHeader.numMarkers = myMarkerInfo->numMarkers;
		BlockMoveData(&mySDMarkerHeader, *markerHandle, sizeof(mySDMarkerHeader));

		/* Create some memory for the marker structures */
		blockSize = mySDMarkerHeader.numMarkers * sizeof(SDShortMarker);
		myMarkerBlock = (SDShortMarker *)NewPtr(blockSize);
		offset = sizeof(mySDMarkerHeader);

		for(i = 0; i < mySDMarkerHeader.numMarkers; i++)
		{
			(myMarkerBlock + i)->markerType0 = 1;
			(myMarkerBlock + i)->markerType1 = 1;
			(myMarkerBlock + i)->position = myMarkerInfo->markPos[i];
			(myMarkerBlock + i)->text = 0;
			(myMarkerBlock + i)->cursorID = 24430;
			(myMarkerBlock + i)->markerID = i + 1;
			(myMarkerBlock + i)->textLength = 0;
			BlockMoveData((myMarkerBlock + i), (*markerHandle+offset), sizeof(SDShortMarker));
			offset = offset + sizeof(SDShortMarker) + (myMarkerBlock + i)->textLength;
		}
		HUnlock(markerHandle);
		ChangedResource(markerHandle);
		RemovePtr((Ptr)myMarkerBlock);
	}
	if(loopHandle!=0)
	{
		size = sizeof(mySDLoopHeader) + (myMarkerInfo->numLoops * sizeof(SDLoop));
		SetHandleSize(loopHandle,size);
		MoveHHi(loopHandle);
		HLock(loopHandle);
		mySDLoopHeader.version = 1;
		mySDLoopHeader.hScale = 0;
		mySDLoopHeader.vScale = 0;
		mySDLoopHeader.numLoops = myMarkerInfo->numLoops;
		BlockMoveData(&mySDLoopHeader, *loopHandle, sizeof(mySDLoopHeader));
		
		blockSize = mySDLoopHeader.numLoops * sizeof(SDLoop);
		myLoopBlock = (SDLoop *)NewPtr(blockSize);
		offset = sizeof(mySDLoopHeader);
		for(i = 0; i < mySDLoopHeader.numLoops; i++)
		{
			(myLoopBlock + i)->loopStart = myMarkerInfo->loopStart[i];
			(myLoopBlock + i)->loopEnd = myMarkerInfo->loopEnd[i];
			(myLoopBlock + i)->loopIndex = i + 1;
			(myLoopBlock + i)->loopSense = 117;
			(myLoopBlock + i)->channel = 0;
			BlockMoveData((myLoopBlock + i), (*loopHandle+offset), sizeof(SDLoop));
			offset = offset + sizeof(SDLoop);
		}
		HUnlock(loopHandle);
		ChangedResource(loopHandle);
		RemovePtr((Ptr)myLoopBlock);
	}
	CloseResFile(mySI->rFileNum);
	ReleaseResource(markerHandle);
	ReleaseResource(loopHandle);
	UseResFile(gAppFileNum);
	return(TRUE);
}

short
CreateMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	short	returnValue;
		
	switch(mySI->sfType)
	{
		case AIFF:
		case AIFC:
		case QKTMA:
			returnValue = CreateAIFFMarkers(mySI,myMarkerInfo);
			break;
		case SDII:
		case MMIX:
		case AUDMED:
			returnValue = CreateSDIIMarkers(mySI);
			break;
			case WAVE:
			returnValue = CreateWAVEMarkers(mySI,myMarkerInfo);
			break;
		case BICSF:
		case DSPD:
		case SUNAU:
		case MPEG:
		case NEXT:
		case TX16W:
		case SDI:
		case TEXT:
		case RAW:
			returnValue = TRUE;
			break;
	default:
			DrawErrorMessage("\pUnknown Sound File Type");
			returnValue = -1;
			break;
	}
	return(returnValue);
}


//	this modified version will put the INST and MARK chunks at the end of the file

short	CreateAIFFMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	long				i, writeSize, filePos, markerSize;
	AIFFInstrumentChunk		myInstrumentChunk;
	MarkerChunkHeader	myMarkerChunkHeader;
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	Byte	*blankMarkerChunk;
	
	/* Initialize chunks */
	/* No need to fill in any info, WriteMarkers() will do that */
	myInstrumentChunk.ckID = INSTRUMENTID;
	myInstrumentChunk.ckSize = sizeof(myInstrumentChunk) - sizeof(chunkHeader);
		
	myMarkerChunkHeader.ckID = MARKERID;
	markerSize = (((2 * myMarkerInfo->numLoops) + myMarkerInfo->numMarkers) 
		* (sizeof(AIFFShortMarker) +2)) + sizeof(myMarkerChunkHeader);
	/* 
	 * We need to zero out the marker area before writing, because WriteAIFFMarkers needs to
	 * read zero for the text length
	 */
	blankMarkerChunk = (Byte *)NewPtr(markerSize);
	for(i =0;i<markerSize;i++)
		blankMarkerChunk[i] = 0;
	myMarkerChunkHeader.ckSize =  markerSize - sizeof(chunkHeader);

	SetFPos(mySI->dFileNum, fsFromLEOF, 0);
	writeSize = markerSize;
	GetFPos(mySI->dFileNum, &filePos);
	FSWrite(mySI->dFileNum, &writeSize, blankMarkerChunk);
	
#ifdef SHOSXUB
	myMarkerChunkHeader.ckID = EndianU32_NtoB(myMarkerChunkHeader.ckID);
	myMarkerChunkHeader.ckSize = EndianS32_NtoB(myMarkerChunkHeader.ckSize);
#endif
	SetFPos(mySI->dFileNum, fsFromMark, -writeSize);
	writeSize = sizeof(myMarkerChunkHeader);
	FSWrite(mySI->dFileNum, &writeSize, &myMarkerChunkHeader);
	
#ifdef SHOSXUB
	myInstrumentChunk.ckID = EndianU32_NtoB(myInstrumentChunk.ckID);
	myInstrumentChunk.ckSize = EndianS32_NtoB(myInstrumentChunk.ckSize);
#endif
	SetFPos(mySI->dFileNum, fsFromStart, (filePos+markerSize));
	writeSize = sizeof(myInstrumentChunk);
	FSWrite(mySI->dFileNum, &writeSize, &myInstrumentChunk);
	
	DisposePtr((Ptr)blankMarkerChunk);
	return(TRUE);
}


short	CreateWAVEMarkers(SoundInfo *mySI,MarkerInfo *myMarkerInfo)
{
	long				writeSize, filePos, smplSize, cueSize, toBeSwapped;
	Byte				*blankSampleChunk, *blankCueChunk;
	WAVEInstChunk		myInstChunk;
	WAVESampleHeader	mySampHeader;
	WAVECueHeader		myCueHeader;
	
	struct
	{
		long	ckID;
		long	ckSize;
	}	chunkHeader;
	
	// Initialize chunks
	// no need to fill in any info, WriteMarkers() will do that
	myInstChunk.ckID = WAV_ID_INST;
	myInstChunk.ckSize = sizeof(WAVEInstChunk) - sizeof(chunkHeader);
#ifdef SHOSXUB
	myInstChunk.ckSize = EndianS32_NtoL(myInstChunk.ckSize);
#else				
	myInstChunk.ckSize = ByteSwapLong(myInstChunk.ckSize);
#endif
		
	// We need to zero out the SMPL area before writing.
	mySampHeader.ckID = WAV_ID_SAMPLE;
	smplSize = myMarkerInfo->numLoops * sizeof(WAVESampleLoop) + sizeof(mySampHeader);
	mySampHeader.ckSize =  smplSize - sizeof(chunkHeader);
#ifdef SHOSXUB
	mySampHeader.ckSize = EndianS32_NtoL(mySampHeader.ckSize);
#else				
	mySampHeader.ckSize = ByteSwapLong(mySampHeader.ckSize);
#endif
	blankSampleChunk = (Byte *)NewPtrClear(smplSize);
	
	// We need to zero out the CUE area before writing.
	mySampHeader.ckID = WAV_ID_CUE;
	cueSize = myMarkerInfo->numMarkers * sizeof(WAVECuePoint) + sizeof(myCueHeader);
	myCueHeader.ckSize =  cueSize - sizeof(chunkHeader);
#ifdef SHOSXUB
	myCueHeader.ckSize = EndianS32_NtoL(myCueHeader.ckSize);
#else				
	myCueHeader.ckSize = ByteSwapLong(myCueHeader.ckSize);
#endif
	toBeSwapped = myMarkerInfo->numMarkers;
#ifdef SHOSXUB
	myCueHeader.dwCuePoints = EndianS32_NtoL(toBeSwapped);
#else				
	myCueHeader.dwCuePoints = ByteSwapLong(toBeSwapped);
#endif
	blankCueChunk = (Byte *)NewPtrClear(cueSize);

	SetFPos(mySI->dFileNum, fsFromLEOF, 0);
	writeSize = smplSize;
	GetFPos(mySI->dFileNum, &filePos);	// note the original end of file.
	FSWrite(mySI->dFileNum, &writeSize, blankSampleChunk);
	
	SetFPos(mySI->dFileNum, fsFromMark, -writeSize);
	writeSize = sizeof(mySampHeader);
	FSWrite(mySI->dFileNum, &writeSize, &mySampHeader);
	
	SetFPos(mySI->dFileNum, fsFromLEOF, 0);
	writeSize = cueSize;
	GetFPos(mySI->dFileNum, &filePos);	// note the original end of file.
	FSWrite(mySI->dFileNum, &writeSize, blankCueChunk);
	
	SetFPos(mySI->dFileNum, fsFromMark, -writeSize);
	writeSize = sizeof(myCueHeader);
	FSWrite(mySI->dFileNum, &writeSize, &myCueHeader);
	
	SetFPos(mySI->dFileNum, fsFromLEOF, 0);
	writeSize = sizeof(myInstChunk);
	FSWrite(mySI->dFileNum, &writeSize, &myInstChunk);
	
	DisposePtr((Ptr)blankSampleChunk);
	DisposePtr((Ptr)blankCueChunk);
	return(TRUE);
}

short		
CreateSDIIMarkers(SoundInfo *mySI)
{
	Handle		markerHandle, loopHandle;

	/* Make new resources. Any size should do, since WriteMakers() should update the size. */
	markerHandle = NewHandle(16);
	loopHandle = NewHandle(16);
	mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec), fsCurPerm);
	UseResFile(mySI->rFileNum);
	AddResource(markerHandle,'sdML',1000,"\p");
	AddResource(loopHandle,'sdLL',1000,"\p");
	CloseResFile(mySI->rFileNum);
	ReleaseResource(markerHandle);
	ReleaseResource(loopHandle);
	UseResFile(gAppFileNum);
	return(TRUE);
}

void
CopyMarkers(void)
{
	InitMarkerStruct(&inMI);
	ReadMarkers(inSIPtr, &inMI);
	CreateMarkers(outSIPtr, &inMI);
	WriteMarkers(outSIPtr,&inMI);
}