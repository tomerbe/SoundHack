/*
	Sample code that demonstrates using the Movie Data Exhange import component
	to read a music track from an Apple CD-300 CD-ROM drive.

	9-02-1993 ¥ By Brigham Stevens

	Copyright © 1993 Apple Computer, Inc.  All rights reserved
	
	Written in THINK C as part of QTShrimpy project, a giant application that does
	lots of things with QuickTime.
*/
#include <QuickTime/QuickTimeComponents.h>
//#include <SoundComponents.h>
#include "SoundFile.h"
#include "Dialog.h"
#include "Menu.h"
#include "Misc.h"
#include "SoundHack.h"
#include "OpenSoundFile.h"
#include "QuickTimeImport.h"
#include "CarbonGlue.h"

extern MenuHandle gFileMenu, gImportMenu;
#ifdef powerc
MovieProgressUPP	gMyProgressProc;
#endif

extern short		gProcessEnabled, gProcessDisabled, gStopProcess;
short		gResRefNum;
WindowPtr 	gCDWindow;
Component	gImportComponents[32];	// only 32 components allowed

/*
	Call the audio-cd import component to add a sound track
	to the passed movie.  If the controller is not nil, then the controller will
	be notified that the movie changed.
	
	For more info:
	
	See the Movie Data Exchange chapter of Inside Macintosh: QuickTime Components
	See the Component Manager documentation
*/
#ifdef powerc
void	QuickTimeImportCallBackInit(void)
{
	gMyProgressProc = NewMovieProgressUPP((MovieProgressProcPtr)MyProgressProc);
}
#endif
void	CDAudioImporter(void)
{
	ComponentResult		err;
	FInfo				fndrInfo;
	FSSpec				cdFile, soundFile;
	Movie 				aMovie;
	Component			audioEater;
	ComponentDescription	audioEaterDes;
	MovieImportComponent eat2aiff;
	SHSFTypeList			typeList;
	SoundDescriptionHandle	sndDescHandle;
	SHStandardFileReply	reply;
	TimeValue			duration;
	Track				usedTrack;
	
	Boolean				abort;
	long				outFlags;
	short				sfDataFork, numTypes;
	
	gCDWindow = GetNewCWindow(CD_IMPORT_WIND,NIL_POINTER,(WindowPtr)MOVE_TO_FRONT);
	
	audioEater = GetAudioEater('trak');
	if(audioEater == nil)
		return;
	GetComponentInfo(audioEater, &audioEaterDes, nil, nil, nil);
	typeList[0] = audioEaterDes.componentSubType;
	numTypes = 1;
	SHStandardGetFile(nil, numTypes, typeList, &reply);
	cdFile = reply.sfFile;

	if(!reply.sfGood)
		return;
	
	// Find the CD to AIFF component and open it
	eat2aiff = OpenComponent(audioEater);
	if(!eat2aiff)
	{
		DrawErrorMessage("\pCould not find the import component");
		return;
	}

	sndDescHandle = (SoundDescriptionHandle)NewHandle(sizeof(SoundDescription));
	HLock((Handle)sndDescHandle);
	(*sndDescHandle)->descSize = sizeof(SoundDescription);	
	(*sndDescHandle)->resvd1 = 0;							
	(*sndDescHandle)->resvd2 = 0;
	(*sndDescHandle)->dataRefIndex = 1;
	(*sndDescHandle)->compressionID = 0;
	(*sndDescHandle)->packetSize = 0;
	(*sndDescHandle)->version = 0;								
	(*sndDescHandle)->revlevel = 0;
	(*sndDescHandle)->vendor = 0;  
	(*sndDescHandle)->dataFormat = 'twos';
	(*sndDescHandle)->numChannels = 2;							
	(*sndDescHandle)->sampleSize = 16;
	(*sndDescHandle)->sampleRate = (Fixed)(44100L * 65536L);	
	HUnlock((Handle)sndDescHandle);
	
	err = MovieImportSetSampleDescription(eat2aiff, (SampleDescriptionHandle)sndDescHandle, SoundMediaType);

	if(audioEaterDes.componentFlags & hasMovieImportUserInterface)
	{
		// Allow the user to change the start, sample size, and duration.
		err = MovieImportDoUserDialog(eat2aiff, &cdFile, nil, &abort);
		if(abort) 
			return;
	}
	
 	// Create a new movie file
	aMovie = nil;
	
	CreateQTAudioFile(&aMovie, &soundFile);

	// Set up a progress process
#ifdef powerc
	err = MovieImportSetProgressProc(eat2aiff, (MovieProgressUPP)gMyProgressProc, 999L);
#else
	err = MovieImportSetProgressProc(eat2aiff, (MovieProgressProcPtr)MyProgressProc, 999L);
#endif
	
	err = MovieImportFile(eat2aiff, 			// component
					 &cdFile, 					// file to be imported
					 aMovie, 					// the movie to import to
					 0,							// target track - ignored for this use
					 &usedTrack, 				// the track that was selected
					 0,							// starting time
					 &duration,					// tell us how long ya grabbed
					 movieImportCreateTrack,	// create a new track for us
					 &outFlags);				// tell us what you did (do we care?)

	CloseComponent(eat2aiff);
	DisposeMovie(aMovie);
	CloseMovieFile(gResRefNum);
	FSpOpenDF(&soundFile, fsCurPerm, &sfDataFork);
	if(err == 0)
	{
		FSpGetFInfo(&soundFile, &fndrInfo);
		fndrInfo.fdType = 'AIFF';
		err = FSpSetFInfo(&soundFile, &fndrInfo);
		err = FSClose(sfDataFork);
		OpenSoundFile(soundFile, FALSE);
		MenuUpdate();
	}
	else
	{
		err = FSClose(sfDataFork);
		err = FlushVol((StringPtr)NIL_POINTER,soundFile.vRefNum);
		err = FSpDelete(&soundFile);
		err = FlushVol((StringPtr)NIL_POINTER,soundFile.vRefNum);
	}
	DisposeWindow(gCDWindow);
}

void	AIFF2QTInPlace(SoundInfo *aSI)
{
	ComponentResult		err;
	FInfo				fndrInfo;
	Movie 				aMovie;
	Component			audioEater;
	ComponentDescription	audioEaterDes;
	MovieImportComponent eat2aiff;
	SoundDescriptionHandle	sndDescHandle;
	TimeValue			duration;
	Track				usedTrack, targetTrack;
	
	Boolean				newFile;
	long				outFlags, inFlags, createMovieFileFlags;
	short				resRefNum, resID;
	
	// Find the AIFF to QT component and open it
	if((audioEater = GetAudioEater('AIFF')) == nil)
	{
		DrawErrorMessage("\pCould not find the import component");
		return;
	}
/*	GetComponentInfo(audioEater, &audioEaterDes, nil, nil, nil);
	if((audioEaterDes.componentFlags & canMovieImportInPlace) != TRUE)
	{
		DrawErrorMessage("\pCould not convert in place");
		return;
	}*/
	if((eat2aiff = OpenComponent(audioEater)) == nil)
	{
		DrawErrorMessage("\pCould not open the import component");
		return;
	}

	sndDescHandle = (SoundDescriptionHandle)NewHandle(sizeof(SoundDescription));
	HLock((Handle)sndDescHandle);
	(*sndDescHandle)->descSize = sizeof(SoundDescription);	
	(*sndDescHandle)->resvd1 = (*sndDescHandle)->resvd2 = 0;
	(*sndDescHandle)->dataRefIndex = 1;
	(*sndDescHandle)->compressionID = (*sndDescHandle)->packetSize = (*sndDescHandle)->version = 0;								
	(*sndDescHandle)->revlevel = (*sndDescHandle)->vendor = 0;  
	(*sndDescHandle)->dataFormat = 'twos';
	(*sndDescHandle)->numChannels = aSI->nChans;							
	(*sndDescHandle)->sampleSize = (long)(aSI->frameSize * 8);
	(*sndDescHandle)->sampleRate = (Fixed)(aSI->sRate * 65536L);	
	HUnlock((Handle)sndDescHandle);
	
	err = MovieImportSetSampleDescription(eat2aiff, (SampleDescriptionHandle)sndDescHandle, SoundMediaType);

 	// Either we will open the already existing movie, open the track and tell MovieImportFile
 	// to use this track, or we will tell MovieImportFile to create an entirely new track.
	aMovie = nil;
	resID = 0;
	inFlags = 0;									// Clear All Bits
	
	if(OpenMovieFile(&(aSI->sfSpec), &resRefNum, fsCurPerm) == nil)
	{
		NewMovieFromFile(&aMovie, resRefNum, &resID, nil, nil, nil);		
		newFile = FALSE;
	}
	
	if(aMovie == nil)
	{
		// No movie, no track
		createMovieFileFlags = 0;			// Clear All Bits

		CreateMovieFile(&(aSI->sfSpec), 'TVOD', smCurrentScript, 
						createMovieFileFlags, &resRefNum, &aMovie);		
		targetTrack = nil;
		newFile = TRUE;
	}
	else
		targetTrack = GetMovieIndTrack(aMovie, 1);

	if(targetTrack == nil)
		inFlags = inFlags | movieImportCreateTrack;		// Set CreateTrack bit
	else
	{
		inFlags = inFlags | movieImportMustUseTrack;	// Set MustUseTrack bit
		inFlags = inFlags | movieImportInParallel;		// Set OverWrite bit
	}
//	gProcessEnabled = IMPORT_PROCESS;
//	MenuUpdate();
	err = MovieImportFile(eat2aiff, 			// component
					 &(aSI->sfSpec), 			// file to be imported
					 aMovie, 					// the movie to import to
					 targetTrack,				// target track
					 &usedTrack, 				// the track that was selected
					 0,							// starting time
					 &duration,					// tell us how long ya grabbed
					 inFlags,					// create a new track for us
					 &outFlags);				// tell us what you did (do we care?)
	

	if(newFile)
	{
		resID = 128;
		err = AddMovieResource(aMovie, resRefNum, &resID, aSI->sfSpec.name);
	}
	else
		err = UpdateMovieResource(aMovie, resRefNum, resID, aSI->sfSpec.name);
		
	CloseComponent(eat2aiff);
	DisposeMovie(aMovie);
	CloseMovieFile(resRefNum);
	if(err == 0)
	{
		FSpGetFInfo(&(aSI->sfSpec), &fndrInfo);
		fndrInfo.fdType = 'AIFF';
		err = FSpSetFInfo(&(aSI->sfSpec), &fndrInfo);
	}
//	gProcessEnabled = NO_PROCESS;
//	FinishProcess();
}


Component	GetAudioEater(OSType subType)
{
	Component				audioEater;
	ComponentDescription	cd;
		
	cd.componentType = MovieImportType;			// looking for an eater
	cd.componentSubType = subType;				// it will eat CD audio tracks
	cd.componentManufacturer = SoundMediaType;	// and xlates them to sound (AIFF file)
	cd.componentFlags = 0;						// no flags
	cd.componentFlagsMask = 0;					// no flag mask
	audioEater = (Component)nil;				// start the search at the beginning
	
	/* this gets the one we want, if it is available */
	audioEater = FindNextComponent(audioEater, &cd);
	return(audioEater);
}

pascal OSErr MyProgressProc(Movie theMovie, short message, short whatOperation, Fixed percentDone, long refcon)
{
	EventRecord	myEvent;
	Rect		myRect;
	float 		percentDoneFloat;
	Str255 		aStr, bStr;
	OSErr		returnValue;
	WindowPtr	whichWindow;
	short		thePart;
	
	returnValue = 0;
	ShowWindow(gCDWindow);	
	SelectWindow(gCDWindow);	
#if TARGET_API_MAC_CARBON == 1
	SetPort(GetWindowPort(gCDWindow));
	GetPortBounds(GetWindowPort(gCDWindow), &myRect);
#else
	SetPort(gCDWindow);
	myRect = gCDWindow->portRect;
#endif
	
	/*Draw the text */
	TextFont(kFontIDGeneva);
	TextSize(9);
	TextMode(srcCopy);
	
	percentDoneFloat = percentDone/655.36;
//	UpdateProcessWindow("\p", "\p", "\pquicktime import", percentDoneFloat * 0.01);
	FixToString(percentDoneFloat, aStr);
	StringAppend(aStr, "\p% Done", bStr);
	
	/* Time Scale */
	MoveTo(myRect.left + 5, ((3 * (myRect.bottom - myRect.top))/4 + 2));
	DrawString(bStr);
	MoveTo(myRect.left + 5, ((myRect.bottom - myRect.top)/4 + 5));
	DrawString("\pClick mouse in window to cancel CD import");
	
	if(EventAvail(mDownMask, &myEvent) == TRUE)
	{
			thePart = FindWindow(myEvent.where, &whichWindow);
		if(thePart == inContent && whichWindow == gCDWindow)
		{
			HideWindow(gCDWindow);
			returnValue = TRUE;
		}
	}
	
	if(percentDoneFloat >= 100.0)
		HideWindow(gCDWindow);
		
	HandleEvent();
	if(gStopProcess == true)
		returnValue = true;
	return(returnValue);
}

Movie GetMovieFromFile (FSSpec aFileSpec)
{
	OSErr	err;
	Movie	aMovie = nil;
	short	movieResFile;

	err = OpenMovieFile (&aFileSpec, &movieResFile, fsRdPerm);
	if (err == noErr) 
	{
		short		movieResID = 0;		/* want first movie */
		Str255		movieName;
		Boolean		wasChanged;
		
		err = NewMovieFromFile (&aMovie, movieResFile, &movieResID, movieName, newMovieActive, &wasChanged);
		CloseMovieFile (movieResFile);
	}
	return aMovie;
}

/*
	bring up standard file to locate an audio file
	and copy the FSSpec to the parameter.
	returns false if the user canceled.
*/


void 	
CreateQTAudioFile(Movie *theMovie, FSSpec *soundFile)
{
	SHStandardFileReply			theSFReply;
	short 						dataFork, resId = 0;
	OSErr 						err = noErr;
	
	gResRefNum = 0;
	SHStandardPutFile ("\pSoundFile name:", "\pImported CD Audio", &theSFReply);
	
	if(!theSFReply.sfGood)
		return;  
	StringCopy(theSFReply.sfFile.name, soundFile->name);
	soundFile->vRefNum = theSFReply.sfFile.vRefNum;
	soundFile->parID = theSFReply.sfFile.parID;
	
	// Delete this file if it exists
	err = FSpOpenDF(soundFile, fsCurPerm, &dataFork);
	if(err == noErr)
	{
		err = FSClose(dataFork);
		err = FlushVol((StringPtr)NIL_POINTER, soundFile->vRefNum);
		err = FSpDelete(soundFile);
		err = FlushVol((StringPtr)NIL_POINTER, soundFile->vRefNum);
	}

	err = CreateMovieFile(soundFile, 'SDHK', smCurrentScript, createMovieFileDeleteCurFile, 
							&gResRefNum, theMovie);
	err = AddMovieResource (*theMovie, gResRefNum, &resId, theSFReply.sfFile.name);
}
