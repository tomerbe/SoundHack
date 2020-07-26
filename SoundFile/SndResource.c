#if __MC68881__
#define __FASTMATH__
#define __HYBRIDMATH__
#endif	/* __MC68881__ */
#include <math.h>
//#include <AppleEvents.h>
//#include <Sound.h>
//#include <SoundInput.h>
#include "SoundFile.h"
#include "CreateSoundFile.h"
#include "WriteHeader.h"
#include "Dialog.h"
#include "Menu.h"
#include "Misc.h"
#include "ByteSwap.h"
#include "muLaw.h"
#include "ALaw.h"
#include "SoundHack.h"
#include "OpenSoundFile.h"
#include "SNDResource.h"
#include "CarbonGlue.h"

/* Globals */

DialogPtr			gImportDialog, gExportDialog;
extern MenuHandle	gAppleMenu, gFileMenu, gEditMenu, gProcessMenu, gControlMenu;
extern SoundInfoPtr	inSIPtr;
 
extern short		gAppFileNum;
char	*aCharBlock;
short	*aShortBlock;
float	*aFloatBlock;
long	*aLongBlock;

SoundInfo		importSI;

void
HandleExportDialog(void)
{
	short	itemType;
	short		dialogDone=FALSE;
	short	itemHit;
	Rect	itemRect;
	Handle	itemHandle;
	double	startTime, endTime, maxTime, inTime;
	long	memory;
	Str255	timeStr;

	/* Initialize Variables */

	gExportDialog = GetNewDialog(EXPORT_DLOG,NIL_POINTER,(WindowPtr)MOVE_TO_FRONT);
	
	memory = FreeMem();
	inTime = inSIPtr->numBytes/(inSIPtr->sRate * inSIPtr->frameSize * inSIPtr->nChans);
	startTime = 0.0;
	endTime = (memory * 0.7)/(inSIPtr->sRate * (inSIPtr->frameSize + 1) * inSIPtr->nChans);
	if(endTime > inTime)
		endTime = inTime;
	maxTime = endTime - startTime;
		
	/* Initialize dialog */
#if TARGET_API_MAC_CARBON == 1
	SetPort(GetDialogPort(gExportDialog));
#else
	SetPort(gExportDialog);
#endif
	HMSTimeString(startTime, timeStr);
	GetDialogItem(gExportDialog, E_START_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, timeStr);
	HMSTimeString(endTime, timeStr);
	GetDialogItem(gExportDialog, E_END_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, timeStr);
	HMSTimeString(maxTime, timeStr);
	GetDialogItem(gExportDialog, E_MAX_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, timeStr);
#if TARGET_API_MAC_CARBON == 1
	ShowWindow(GetDialogWindow(gExportDialog));
	SelectWindow(GetDialogWindow(gExportDialog));
#else
	ShowWindow(gExportDialog);
	SelectWindow(gExportDialog);
#endif
	SetDialogCancelItem(gExportDialog, E_CANCEL_BUTTON);
	while(dialogDone == FALSE)
	{
		ModalDialog(NIL_POINTER,&itemHit);
		switch(itemHit)
		{
			case E_START_FIELD:
				GetDialogItem(gExportDialog,E_START_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, timeStr);
				StringToFix(timeStr, &startTime);
				break;
			case E_END_FIELD:
				GetDialogItem(gExportDialog,E_END_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, timeStr);
				StringToFix(timeStr, &endTime);
				break;
			case E_CANCEL_BUTTON:
				MenuUpdate();
				dialogDone = TRUE;
				break;
			case E_OK_BUTTON:
				if(endTime > inTime)
					endTime = inTime;
				if((endTime - startTime)>maxTime)
				{
					endTime = startTime + maxTime;
					HMSTimeString(endTime, timeStr);
					GetDialogItem(gExportDialog, E_END_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, timeStr);
					DrawErrorMessage("\pEnd set to maximum");
				}
				ExportSndResource(startTime, endTime);
				MenuUpdate();
				dialogDone = TRUE;
				break;
		}
	}
	DisposeDialog(gExportDialog);
}

short
ExportSndResource(double startTime, double endTime)
{	
	short		baseFreq;
	short	headerLen;
	long	numBytes, readSize, i;
	unsigned long	sampleRate, resFileNum;

	Handle	mySndHandle;
	Point	myPoint;
	SHStandardFileReply	reply;
	FSSpec	resSpec;
	Str255	resFiName;
		
	myPoint.h = 100;
	myPoint.v = 100;
	
	NameFile(inSIPtr->sfSpec.name, "\pSFIL", resFiName);
	SHStandardPutFile("\pSave SoundFile as:", resFiName, &reply);
	if(reply.sfGood == FALSE)
		return(-1);
	
	resSpec = reply.sfFile;
	FSpCreate(&resSpec, 'SDHK', 'sfil', reply.sfScript);
	FSpCreateResFile(&resSpec, 'SDHK', 'sfil', reply.sfScript);
	resFileNum = FSpOpenResFile(&resSpec, fsCurPerm);
	UseResFile(resFileNum);
	
	DrawDialog(gExportDialog);
	
	sampleRate = (unsigned long)(inSIPtr->sRate * 65536);
	baseFreq = 60; /*Middle C*/
	numBytes = (long)((endTime - startTime) * inSIPtr->nChans * inSIPtr->sRate);
	mySndHandle = NewHandle(128); /*An Extended Header should take 64 */
	AddResource(mySndHandle,'snd ',8192, resFiName); /*give resource same name as file*/
	SetupSndHeader((SndListHandle)mySndHandle, (short)inSIPtr->nChans, sampleRate, 8, 'NONE', baseFreq, numBytes, &headerLen);
	SetHandleSize(mySndHandle, (numBytes + headerLen));
	
	SetFPos(inSIPtr->dFileNum, fsFromStart, inSIPtr->dataStart);
	readSize = numBytes * inSIPtr->frameSize;
	if(inSIPtr->frameSize == 1.0)
	{
		aCharBlock = (char *)NewPtr(readSize);
		if(aCharBlock == 0)
		{
			DrawErrorMessage("\pNot Enough Memory");
			return(-1);
		}
		FSRead(inSIPtr->dFileNum, &readSize, aCharBlock);
	}
	else if(inSIPtr->frameSize == 2.0)
	{
		aShortBlock = (short *)NewPtr(readSize);
		if(aShortBlock == 0)
		{
			DrawErrorMessage("\pNot Enough Memory");
			return(-1);
		}
		FSRead(inSIPtr->dFileNum, &readSize, aShortBlock);
	}
	else if(inSIPtr->packMode == SF_FORMAT_32_FLOAT)
	{
		aFloatBlock = (float *)NewPtr(readSize);
		if(aFloatBlock == 0)
		{
			DrawErrorMessage("\pNot Enough Memory");
			return(-1);
		}
		FSRead(inSIPtr->dFileNum, &readSize, aFloatBlock);
	}
	else if(inSIPtr->packMode == SF_FORMAT_32_LINEAR || inSIPtr->packMode == SF_FORMAT_32_COMP)
	{
		aLongBlock = (long *)NewPtr(readSize);
		if(aLongBlock == 0)
		{
			DrawErrorMessage("\pNot Enough Memory");
			return(-1);
		}
		FSRead(inSIPtr->dFileNum, &readSize, aLongBlock);
	}
	
	HLock(mySndHandle);
	numBytes = readSize/inSIPtr->frameSize;
	switch(inSIPtr->packMode)
	{
		case SF_FORMAT_16_LINEAR:
			for(i = 0; i < numBytes; i++)
				*(*mySndHandle + headerLen + i) = (char)((*(aShortBlock+i)>>8) + 128);
			break;
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
			for(i = 0; i < numBytes; i++)
				*(*mySndHandle + headerLen + i) = (char)((*(aLongBlock+i)>>24) + 128);
			break;
		case SF_FORMAT_16_SWAP:
			for(i = 0; i < numBytes; i++)
				*(*mySndHandle + headerLen + i) = (char)((ByteSwapShort(*(aShortBlock+i))>>8) + 128);
			break;
		case SF_FORMAT_8_LINEAR:
			for(i = 0; i < numBytes; i++)
				*(*mySndHandle + headerLen + i) = (char)(*(aCharBlock+i) + 128);
			break;	
		case SF_FORMAT_8_UNSIGNED:
			for(i = 0; i < numBytes; i++)
				*(*mySndHandle + headerLen + i) = *(aCharBlock+i);
			break;	
		case SF_FORMAT_32_FLOAT:
			for(i = 0; i < numBytes; i++)
				*(*mySndHandle + headerLen + i) = (char)((*(aFloatBlock+i) + 1.0) * 128.0);
			break;
		case SF_FORMAT_8_MULAW:
			for(i = 0; i < numBytes; i++)
				*((*mySndHandle) + headerLen + i) = (char)((Ulaw2Short(*(aCharBlock+i)) >> 8) + 128);
			break;
		case SF_FORMAT_8_ALAW:
			for(i = 0; i < numBytes; i++)
				*((*mySndHandle) + headerLen + i) = (char)((Alaw2Short(*(aCharBlock+i)) >> 8) + 128);
			break;
	}
	HUnlock(mySndHandle);
	CloseResFile(resFileNum);
	ReleaseResource(mySndHandle);
	UseResFile(gAppFileNum);
	if(inSIPtr->frameSize == 1.0)
		RemovePtr((Ptr)aCharBlock);
	else if(inSIPtr->frameSize == 2.0)
		RemovePtr((Ptr)aShortBlock);
	else if(inSIPtr->packMode == SF_FORMAT_32_FLOAT)
		RemovePtr((Ptr)aFloatBlock);
	else if(inSIPtr->packMode == SF_FORMAT_32_LINEAR || inSIPtr->packMode == SF_FORMAT_32_COMP)
		RemovePtr((Ptr)aLongBlock);
	return(TRUE);
}

short
HandleImportDialog(void)
{
	short		dialogDone=FALSE;
	short	itemType, numTypes, mySndID, itemHit;
	unsigned long	resource, resFileNum, numResources;
	long			error;
	
	Handle		itemHandle, mySndHandle;
	Rect		itemRect;
	ResType		myResType;
	SHStandardFileReply		reply;
	SHSFTypeList	typeList;
	SndChannelPtr	myChannel;
	Str255		errStr, myResName;
		

	/* Initialize Variables */
	gImportDialog = GetNewDialog(IMPORT_DLOG,NIL_POINTER,(WindowPtr)MOVE_TO_FRONT);	

	/* Open resource file */
	numTypes = -1;		/* Open all types */
	myResType = 'snd ';
	resource = 1;		/* Look for first numbered resource */
	
	UseResFile(gAppFileNum);
	SHStandardGetFile(NIL_FILE_FILTER,numTypes,typeList,&reply);
	if(reply.sfGood == FALSE)
		return(-1);
	resFileNum = FSpOpenResFile(&reply.sfFile, fsCurPerm);
	UseResFile(resFileNum);
	
	/* Find first sound resource */
	numResources = Count1Resources(myResType);
	if(numResources == 0)
	{
		DrawErrorMessage("\pThere are no 'snd ' resources in this file");
		return(-1);
	}
	mySndHandle = Get1IndResource(myResType, resource);
	HLock(mySndHandle);
	GetDialogItem(gImportDialog, IP_OK_BUTTON, &itemType, &itemHandle, &itemRect);
	if(ReadSndResourceInfo(mySndHandle))
		HiliteControl((ControlHandle)itemHandle, 0);
	else
		HiliteControl((ControlHandle)itemHandle, 255);
	HUnlock(mySndHandle);
	GetResInfo(mySndHandle, &mySndID, &myResType, myResName);
	GetDialogItem(gImportDialog, IP_NAME_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, myResName);
	HMSTimeString(importSI.sRate, errStr);
	GetDialogItem(gImportDialog, IP_RATE_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, errStr);
	NumToString(importSI.nChans, errStr);
	GetDialogItem(gImportDialog, IP_CHAN_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, errStr);
	GetDialogItem(gImportDialog, IP_TYPE_FIELD, &itemType, &itemHandle, &itemRect);
	if(importSI.sfType == MAC)
		SetDialogItemText(itemHandle, "\pStandard");
	else if(importSI.sfType == MACTWO)
		SetDialogItemText(itemHandle, "\pHypercard SND");
	else if(importSI.sfType == MACEXT)
		SetDialogItemText(itemHandle, "\pExtended");
	else if(importSI.sfType == MACCOMP)
		SetDialogItemText(itemHandle, "\pCompressed");
	else
		SetDialogItemText(itemHandle, "\pNo Sampled Sound");
	NumToString(mySndID, errStr);
	GetDialogItem(gImportDialog, IP_ID_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, errStr);
	
	/* Initialize dialog */
#if TARGET_API_MAC_CARBON == 1
	SetPort(GetDialogPort(gImportDialog));
	ShowWindow(GetDialogWindow(gImportDialog));
	SelectWindow(GetDialogWindow(gImportDialog));
#else
	SetPort(gImportDialog);
	ShowWindow(gImportDialog);
	SelectWindow(gImportDialog);
#endif
	SetDialogCancelItem(gImportDialog, IP_CANCEL_BUTTON);
	while(dialogDone == FALSE)
	{
		ModalDialog(NIL_POINTER,&itemHit);
		switch(itemHit)
		{
			case IP_NEXT_FIELD:
				resource++;
				if(resource > numResources)
					resource = numResources;
				mySndHandle = Get1IndResource(myResType, resource);
				HLock(mySndHandle);
				GetDialogItem(gImportDialog, IP_OK_BUTTON, &itemType, &itemHandle, &itemRect);
				if(ReadSndResourceInfo(mySndHandle))
					HiliteControl((ControlHandle)itemHandle, 0);
				else
					HiliteControl((ControlHandle)itemHandle, 255);
				HUnlock(mySndHandle);
				GetResInfo (mySndHandle, &mySndID, &myResType, myResName);
				NumToString(mySndID, errStr);
				GetDialogItem(gImportDialog, IP_ID_FIELD, &itemType, &itemHandle, &itemRect);
				SetDialogItemText(itemHandle, errStr);
				HMSTimeString(importSI.sRate, errStr);
				GetDialogItem(gImportDialog, IP_RATE_FIELD, &itemType, &itemHandle, &itemRect);
				SetDialogItemText(itemHandle, errStr);
				NumToString(importSI.nChans, errStr);
				GetDialogItem(gImportDialog, IP_CHAN_FIELD, &itemType, &itemHandle, &itemRect);
				SetDialogItemText(itemHandle, errStr);
				GetDialogItem(gImportDialog, IP_TYPE_FIELD, &itemType, &itemHandle, &itemRect);
				if(importSI.sfType == MAC)
					SetDialogItemText(itemHandle, "\pStandard");
				else if(importSI.sfType == MACTWO)
					SetDialogItemText(itemHandle, "\pHypercard SND");
				else if(importSI.sfType == MACEXT)
					SetDialogItemText(itemHandle, "\pExtended");
				else if(importSI.sfType == MACCOMP)
					SetDialogItemText(itemHandle, "\pCompressed");
				else
					SetDialogItemText(itemHandle, "\pNo Sampled Sound");
				GetDialogItem(gImportDialog, IP_NAME_FIELD, &itemType, &itemHandle, &itemRect);
				SetDialogItemText(itemHandle, myResName);
				break;
			case IP_PREV_FIELD:
				resource--;
				if(resource < 1)
					resource = 1;
				mySndHandle = Get1IndResource(myResType, resource);
				HLock(mySndHandle);
				GetDialogItem(gImportDialog, IP_OK_BUTTON, &itemType, &itemHandle, &itemRect);
				if(ReadSndResourceInfo(mySndHandle))
					HiliteControl((ControlHandle)itemHandle, 0);
				else
					HiliteControl((ControlHandle)itemHandle, 255);
				HUnlock(mySndHandle);
				GetResInfo (mySndHandle, &mySndID, &myResType, myResName);
				NumToString(mySndID, errStr);
				GetDialogItem(gImportDialog, IP_ID_FIELD, &itemType, &itemHandle, &itemRect);
				SetDialogItemText(itemHandle, errStr);
				HMSTimeString(importSI.sRate, errStr);
				GetDialogItem(gImportDialog, IP_RATE_FIELD, &itemType, &itemHandle, &itemRect);
				SetDialogItemText(itemHandle, errStr);
				NumToString(importSI.nChans, errStr);
				GetDialogItem(gImportDialog, IP_CHAN_FIELD, &itemType, &itemHandle, &itemRect);
				SetDialogItemText(itemHandle, errStr);
				GetDialogItem(gImportDialog, IP_TYPE_FIELD, &itemType, &itemHandle, &itemRect);
				if(importSI.sfType == MAC)
					SetDialogItemText(itemHandle, "\pStandard");
				else if(importSI.sfType == MACTWO)
					SetDialogItemText(itemHandle, "\pHypercard SND");
				else if(importSI.sfType == MACEXT)
					SetDialogItemText(itemHandle, "\pExtended");
				else if(importSI.sfType == MACCOMP)
					SetDialogItemText(itemHandle, "\pCompressed");
				else
					SetDialogItemText(itemHandle, "\pNo Sampled Sound");
				GetDialogItem(gImportDialog, IP_NAME_FIELD, &itemType, &itemHandle, &itemRect);
				SetDialogItemText(itemHandle, myResName);
				break;
			case IP_PLAY_BUTTON:
				myChannel = 0L;
				error = SndNewChannel(&myChannel, sampledSynth, 0, nil);
				if(error != 0)
				{
					DrawErrorMessage("\pError Allocating Sound Channel");
					break;
				}
 				SndPlay(myChannel, (SndListHandle) mySndHandle, FALSE);
				SndDisposeChannel(myChannel, FALSE);
				break;
			case IP_CANCEL_BUTTON:
				MenuUpdate();
				dialogDone = TRUE;
				break;
			case IP_OK_BUTTON:
				ImportSndResource(mySndHandle);
				ReleaseResource(mySndHandle);
				UseResFile(gAppFileNum);
				MenuUpdate();
				dialogDone = TRUE;
				break;
		}
	}
	DisposeDialog(gImportDialog);
	gImportDialog = nil;
	return(TRUE);
}

short
ImportSndResource(Handle mySndHandle)
{	
	long			headerLen, writeSize, i;
	short			tmpShort, mySndID;
	extern short		gAppFileNum;
	char			charTemp;

	Point			myPoint;
	ResType			myResType;
	Str255			myResName;
	SoundInfoPtr	importedSIPtr;
		
	myPoint.h = 100;
	myPoint.v = 100;
	
	GetResInfo(mySndHandle, &mySndID, &myResType, myResName);
	
	UseResFile(gAppFileNum);
	
	importedSIPtr = nil;
	importedSIPtr = (SoundInfo *)NewPtr(sizeof(SoundInfo));
	importedSIPtr->sfType = AIFF;
	importedSIPtr->packMode = SF_FORMAT_16_LINEAR;
	NameFile(myResName, "\pAIFF", importedSIPtr->sfSpec.name);
	importedSIPtr->sRate = importSI.sRate;
	importedSIPtr->nChans = importSI.nChans;
	importedSIPtr->numBytes = 0;
	if(CreateSoundFile(&importedSIPtr, SOUND_CUST_DIALOG) == -1)
	{
		MenuUpdate();
		RemovePtr((Ptr)importedSIPtr);
		importedSIPtr = nil;
		return(-1);
	}
	WriteHeader(importedSIPtr);
	UpdateInfoWindow(importedSIPtr);
	SetFPos(importedSIPtr->dFileNum, fsFromStart, importedSIPtr->dataStart);
	writeSize = importSI.numBytes * importedSIPtr->frameSize;
	if(importedSIPtr->frameSize == 1.0)
	{
		aCharBlock = (char *)NewPtr(writeSize);
		if(aCharBlock == 0)
		{
			DrawErrorMessage("\pNot Enough Memory");
			return(-1);
		}
	}
	else if(importedSIPtr->frameSize == 2.0)
	{
		aShortBlock = (short *)NewPtr(writeSize);
		if(aShortBlock == 0)
		{
			DrawErrorMessage("\pNot Enough Memory");
			return(-1);
		}
	}
	else if(importedSIPtr->packMode == SF_FORMAT_32_FLOAT)
	{
		aFloatBlock = (float *)NewPtr(writeSize);
		if(aFloatBlock == 0)
		{
			DrawErrorMessage("\pNot Enough Memory");
			return(-1);
		}
	}
	else if(importedSIPtr->packMode == SF_FORMAT_32_LINEAR ||  importedSIPtr->packMode == SF_FORMAT_32_COMP)
	{
		aLongBlock = (long *)NewPtr(writeSize);
		if(aLongBlock == 0)
		{
			DrawErrorMessage("\pNot Enough Memory");
			return(-1);
		}
	}
	headerLen = importSI.dataStart;
	HLock(mySndHandle);
	switch(importedSIPtr->packMode)
	{
		case SF_FORMAT_16_LINEAR:
			for(i = 0; i < importSI.numBytes; i++)
				*(aShortBlock+i) = (short)((*(*mySndHandle + headerLen + i) ^ 0x80) << 8);
			FSWrite(importedSIPtr->dFileNum, &writeSize, aShortBlock);
			break;
		case SF_FORMAT_32_COMP:
		case SF_FORMAT_32_LINEAR:
			for(i = 0; i < importSI.numBytes; i++)
				*(aLongBlock+i) = (long)((*(*mySndHandle + headerLen + i) ^ 0x80) << 24);
			FSWrite(importedSIPtr->dFileNum, &writeSize, aLongBlock);
			break;
		case SF_FORMAT_16_SWAP:
			for(i = 0; i < importSI.numBytes; i++)
			{
				tmpShort = (short)((*(*mySndHandle + headerLen + i) ^ 0x80) << 8);
				*(aShortBlock+i) = ByteSwapShort(tmpShort);
			}
			FSWrite(importedSIPtr->dFileNum, &writeSize, aShortBlock);
			break;
		case SF_FORMAT_8_LINEAR:
			for(i = 0; i < importSI.numBytes; i++)
				aCharBlock[i] = (char)(*(*mySndHandle + headerLen + i) ^ 0x80);
			FSWrite(importedSIPtr->dFileNum, &writeSize, aCharBlock);
			break;	
		case SF_FORMAT_8_UNSIGNED:
			for(i = 0; i < importSI.numBytes; i++)
				aCharBlock[i] = *(*mySndHandle + headerLen + i);
			FSWrite(importedSIPtr->dFileNum, &writeSize, aCharBlock);
			break;	
		case SF_FORMAT_32_FLOAT:
			for(i = 0; i < importSI.numBytes; i++)
			{
				charTemp = (char)(*(*mySndHandle + headerLen + i) ^ 0x80);
				*(aFloatBlock+i) = charTemp/128.0;
			}
			FSWrite(importedSIPtr->dFileNum, &writeSize, aFloatBlock);
			break;
		case SF_FORMAT_8_MULAW:
			for(i = 0; i < importSI.numBytes; i++)
				*(aCharBlock+i) = (char)Short2Ulaw((*(*mySndHandle + headerLen + i) ^ 0x80)<<8);
			FSWrite(importedSIPtr->dFileNum, &writeSize, aCharBlock);
			break;
		case SF_FORMAT_8_ALAW:
			for(i = 0; i < importSI.numBytes; i++)
				*(aCharBlock+i) = (char)Short2Alaw((*(*mySndHandle + headerLen + i) ^ 0x80)<<8);
			FSWrite(importedSIPtr->dFileNum, &writeSize, aCharBlock);
			break;
	}
	HUnlock(mySndHandle);
	importedSIPtr->numBytes = writeSize;
	WriteHeader(importedSIPtr);	/* Update header - really only needed for AIFF */
	UpdateInfoWindow(importedSIPtr);
	
	if(importedSIPtr->frameSize == 1.0)
		RemovePtr((Ptr)aCharBlock);
	if(importedSIPtr->frameSize == 2.0)
		RemovePtr((Ptr)aShortBlock);
	if(importedSIPtr->packMode == SF_FORMAT_32_FLOAT)
		RemovePtr((Ptr)aFloatBlock);
	if(importedSIPtr->packMode == SF_FORMAT_32_LINEAR || importedSIPtr->packMode == SF_FORMAT_32_COMP)
		RemovePtr((Ptr)aLongBlock);
	importedSIPtr = nil;
	MenuUpdate();
	return(TRUE);
}

/*
 * We will read the info into the importSI SoundInfo structure
 */
short
ReadSndResourceInfo(Handle mySndHandle)
{
	unsigned long offset;
	SoundHeader	mySH;
	ErbeExtSoundHeader myEH;
	ErbeCmpSoundHeader myCH;
	
	/* Look for type two sound resources first */
	if(*(*mySndHandle+1) == 2)
	{
		/* Find header type using offset based on number of synths */
		offset = (*(*mySndHandle+5) * 8);
		/* Standard header */ 
		if(*(*mySndHandle+offset+26) == 0)
		{
			BlockMoveData((*mySndHandle+offset+6), &mySH, 22);
			importSI.sRate = (float)(mySH.sampleRate/65536.0);
			importSI.dataStart = offset + 28;
			importSI.numBytes = mySH.length;
			importSI.packMode = SF_FORMAT_8_UNSIGNED;
			importSI.nChans = MONO;
			importSI.frameSize = 1;
			importSI.sfType = MACTWO;
			if(importSI.sRate < 0.0)
				importSI.sRate += 65536.0;
			return(TRUE);
		}
		else
			return(FALSE);
	}
	/* Now look at sampled synth (5) files */
	else if(*(*mySndHandle+5) == 5)
	{
		/* Find header type using offset based on number of synths */
		offset = (*(*mySndHandle+11) * 8);
		/* Standard header */ 
		if(*(*mySndHandle+offset+32) == 0)
		{
			BlockMoveData((*mySndHandle+offset+12), &mySH, 22);
			importSI.sRate = (float)(mySH.sampleRate/65536.0);
			importSI.dataStart = offset + 34;
			importSI.numBytes = mySH.length;
			importSI.packMode = SF_FORMAT_8_UNSIGNED;
			importSI.nChans = MONO;
			importSI.frameSize = 1;
			importSI.sfType = MAC;
		}
		// More fixed up broken apple ex80 code.
		else if(*(*mySndHandle+offset+32) == -1)
		{
			BlockMoveData((*mySndHandle+offset+12), &myEH, 64);
			importSI.sRate = (float)(myEH.sampleRate/65536.0);
			importSI.dataStart = offset + 76;
			importSI.numBytes = myEH.numChannels * myEH.numFrames * (myEH.sampleSize >> 3);
			importSI.nChans = myEH.numChannels;
			importSI.frameSize = myEH.sampleSize >> 3;
			if(importSI.frameSize == 1.0)
				importSI.packMode = SF_FORMAT_8_UNSIGNED;
			else
				importSI.packMode = SF_FORMAT_16_LINEAR;
			importSI.sfType = MACEXT;
		}
		// More fixed up broken apple ex80 code.
		else if(*(*mySndHandle+offset+32) == -2)
		{
			BlockMoveData((*mySndHandle+offset+12), &myCH, 64);
			importSI.sRate = (float)(myCH.sampleRate/65536.0);
			importSI.dataStart = offset + 76;
			importSI.numBytes = myCH.numFrames;
			importSI.packMode = SF_FORMAT_8_UNSIGNED;
			importSI.nChans = myCH.numChannels;
			importSI.frameSize = 1;
			importSI.sfType = MACCOMP;
			return(FALSE);
		}
		/* Print out other types */
		else
		{
			DrawErrorMessage("\pSoundHack doesn't know about this snd resource type");
			importSI.sfType = 0;
			return(FALSE);
		}
		if(importSI.sRate < 0.0)
			importSI.sRate += 65536.0;
		return(TRUE);
	}
	else
	{
		importSI.sfType = 0;
		return(FALSE);
	}
}

