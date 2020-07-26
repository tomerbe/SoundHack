/* 
 *	SoundHack- soon to be many things to many people
 *	Copyright ©1994 Tom Erbe - California Institute of the Arts
 *
 *	SoundHack.c - starting point, toolbox initialization, event manager.
 */
 
#if __MC68881__
#define __FASTMATH__
#define __HYBRIDMATH__
#endif	/* __MC68881__ */
#include <math.h>
#include <stdlib.h>
//#include <Gestalt.h>
//#include <SoundInput.h>
//#include <SoundComponents.h>
#include <QuickTime/Movies.h>
#include <QuickTime/QuickTimeComponents.h>
#include "SoundFile.h"
#include "OpenSoundFile.h"
#include "CreateSoundFile.h"
#include "CloseSoundFile.h"
#include "Dialog.h"
#include "MyAppleEvents.h"
#include "ChangeHeader.h"
#include "WriteHeader.h"
#include "Menu.h"
#include "FFT.h"
#include "Misc.h"
#include "Convolve.h"
#include "PhaseVocoder.h"
#include "Analysis.h"
#include "RingModulate.h"
#include "SampleRateConvert.h"
#include "Dynamics.h"
#include "DrawFunction.h"
#include "CopySoundFile.h"
#include "Gain.h"
#include "Markers.h"
#include "Mutate.h"
#include "Extract.h"
#include "Normalization.h"
#include "ShowWave.h"
#include "ShowSono.h"
#include "ShowSpect.h"
#include "MPEG.h"
#include "Play.h"
#include "Record.h"
#include "SNDResource.h"
#include "Binaural.h"
#include "SoundHack.h"
#include "QuickTimeImport.h"
#include "Preferences.h"
#include "Cursors.h"
#include "AppleCompression.h"
#include "CarbonGlue.h"

/* global variables */
DialogPtr	gDrawFunctionDialog, gPrefDialog;
EventRecord	gTheEvent;
FSSpec		nilFSSpec;
MenuHandle	gAppleMenu, gFileMenu, gEditMenu, gProcessMenu, gControlMenu,
			gSoundFileMenu, gBandsMenu, gScaleMenu, gTypeMenu, gFormatMenu,
			gWindowMenu, gMutateMenu, gDynamicsMenu, gExtractMenu, gSizeMenu, 
			gRateMenu, gChanMenu, gSigDispMenu, gSpectDispMenu, gSonoDispMenu,
			gImportMenu, gExportMenu, gInputMenu;
WindowPtr	gGrowWindow, gProcessWindow, gAboutWindow;
ProcessSerialNumber	gPSN;
PixPatHandle	gRedPP, gGreenPP;

#ifdef powerc
FileFilterUPP		gMPEGFileFilter;
#endif

Boolean		gOKtoOK, gNormalize, gStartUpScreen;
short		gDone, gMaxRow, gCurRow, gFileNum, gAppFileNum, gProcessDisabled, 
			gProcessEnabled, gStopProcess;
long		gSoundEnviron, gStartTicks, gNumFilesOpen;
float		*gFunction, Pi, twoPi;
SoundInfoPtr	inSIPtr, filtSIPtr, frontSIPtr, firstSIPtr, lastSIPtr, lastInSIPtr,
				outSIPtr, outSteadSIPtr, outTransSIPtr, outLSIPtr, outRSIPtr;
MarkerInfo	inMI, outMI;
extern	long	gIOSamplesAllocated;
long	gSystemVersion;

struct
{
	Boolean	soundPlay;
	Boolean	soundRecord;
	Boolean	soundCompress;
	Boolean	movie;
}	gGestalt;

extern ConvolveInfo	gCI;
extern PlayInfo	gPlayInfo;
//extern DoubleBufPlayInfo	gDBPI;
extern ControlHandle	gSliderCntl;
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
main(void) 
{
   long i;

	ToolBoxInit();
	gAppFileNum = CurResFile();
	HandleAboutWindow(0, nil);
	MenuBarInit();
	DialogInit();
	MaxApplZone();
	for(i = 0; i< 15; i++)
		MoreMasters();
#ifdef powerc
	RoutineDesInit();
#endif
	AppleEventInit();
	GlobalInit();
	OpenPreferences();
	if(CapabilityCheck())
	{
		if(gGestalt.movie)
		{
			EnterMovies();
			HandleAboutWindow(1, "\pQuickTime Loaded");
		}
		MenuUpdate();
		MainLoop();
	}
}

void
ToolBoxInit(void)
{
#if TARGET_API_MAC_CARBON == 0
	InitGraf(&(qd.thePort));
	InitFonts();
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(NIL_POINTER);
#endif
	InitCursor();
	FlushEvents(everyEvent, REMOVE_ALL_EVENTS);
}

short
CapabilityCheck(void)
{
	OSErr	status;
	long		response;
	OSErr		myErr;
	NumVersion	myVersion;

#ifdef powerc
	// this code, which is compiled for the powerpc, should not be run on a 68k machine
	// gestalt68k       = 1;
	// gestaltPowerPC   = 2;	
	status = Gestalt(gestaltSysArchitecture, &response);
	if(response == 1L)
	{
		StopAlert(NOTAPPC_ALERT, NIL_POINTER);
		return(false);
	}
#endif
#if __MC68881__
	status = Gestalt(gestaltFPUType, &response);
	if(response == 0L)
	{
		StopAlert(NO_FPU_ALERT, NIL_POINTER);
		return(false);
	}
	status = Gestalt(gestaltProcessorType, &response);
	if(response < 3L)
	{
		StopAlert(NO_FPU_ALERT, NIL_POINTER);
		return(false);
	}
#endif	/* __MC68881__ */


	status = Gestalt(gestaltCarbonVersion, &response);
	if(response <= 0x00000100)
	{
		StopAlert(NO_CARBON_ALERT, NIL_POINTER);
		return(false);
	}
	
	status = Gestalt(gestaltSystemVersion, &response);
	if((status!=noErr)||(response<0x0860))
	{
		StopAlert(BAD_SYS_ALERT, NIL_POINTER);
		return(false);
	}
	else
	{
		gSystemVersion = response;
		gGestalt.soundPlay = false;
		gGestalt.soundCompress = false;
		gGestalt.soundRecord = false;
		myVersion = SndSoundManagerVersion();
		myErr = Gestalt(gestaltSoundAttr, &response);
		if(myVersion.majorRev >= 0x03)
		{
			if(myVersion.minorAndBugRev >= 0x20)
				gGestalt.soundCompress = true;
			if(myErr == noErr && (0x0800 & response))
				gGestalt.soundPlay = true;
			if(myErr == noErr && (0x0020 & response))
				gGestalt.soundRecord = true;
		}
		else
		{
			gGestalt.soundRecord = false;
			myErr = Gestalt(gestaltHardwareAttr, &response);
			if(myErr == noErr && (0x0010 & response))
				gGestalt.soundPlay = true;
		}
		myErr = Gestalt (gestaltQuickTime, &response);
		if(myErr == noErr )
			gGestalt.movie = true;
		else
			gGestalt.movie = false;
		return(true);
	}
}

void
DialogInit(void)
{
	WindInfo *theWindInfo;
	
	HandleAboutWindow(1, "\pStarting Dialog Init");
	SetDialogFont(systemFont);
	gDrawFunctionDialog = GetNewDialog(DRAWFUNC_DLOG,NIL_POINTER,(WindowPtr)MOVE_TO_FRONT);
	
	theWindInfo = (WindInfo *)NewPtr(sizeof(WindInfo));
	theWindInfo->windType = PROCWIND;
	theWindInfo->structPtr = (Ptr)DRAWFUNC_DLOG;

#if TARGET_API_MAC_CARBON == 1
	SetWRefCon(GetDialogWindow(gDrawFunctionDialog), (long)theWindInfo);
#else
	SetWRefCon(gDrawFunctionDialog, (long)theWindInfo);
#endif

	gPrefDialog = GetNewDialog(PREF_DLOG,NIL_POINTER,(WindowPtr)MOVE_TO_FRONT);
	
	theWindInfo = (WindInfo *)NewPtr(sizeof(WindInfo));
	theWindInfo->windType = PROCWIND;
	theWindInfo->structPtr = (Ptr)PREF_DLOG;

#if TARGET_API_MAC_CARBON == 1
	SetWRefCon(GetDialogWindow(gPrefDialog), (long)theWindInfo);
#else
	SetWRefCon(gPrefDialog, (long)theWindInfo);
#endif

	HandleAboutWindow(1, "\pPreferences");

	HandleAboutWindow(1, "\pDialogs Initialized");
}

void
MenuBarInit(void)
{
	Handle	myMenuBar;
	
	myMenuBar = GetNewMBar(BASE_RES_ID);
	SetMenuBar(myMenuBar);
	gAppleMenu = GetMenuHandle(APPLE_MENU_ID);
	gFileMenu = GetMenuHandle(FILE_MENU_ID);
	gEditMenu = GetMenuHandle(EDIT_MENU_ID);
	gProcessMenu = GetMenuHandle(PROCESS_MENU_ID);
	gControlMenu = GetMenuHandle(CONTROL_MENU_ID);
	gSoundFileMenu = GetMenuHandle(SOUNDFILE_MENU_ID);
#if TARGET_API_MAC_CARBON == 0
	AppendResMenu(gAppleMenu, 'DRVR');
#endif
	DrawMenuBar();
	gBandsMenu = GetMenu(BANDS_MENU_ID);
	InsertMenu(gBandsMenu, NOT_A_NORMAL_MENU);
	gScaleMenu = GetMenu(SCALE_MENU_ID);
	InsertMenu(gScaleMenu, NOT_A_NORMAL_MENU);
	gTypeMenu = GetMenu(FILE_TYPE_MENU_ID);
	InsertMenu(gTypeMenu, NOT_A_NORMAL_MENU);
	gFormatMenu = GetMenu(FILE_FORMAT_MENU_ID);
	InsertMenu(gFormatMenu, NOT_A_NORMAL_MENU);
	gWindowMenu = GetMenu(WINDOW_MENU_ID);
	InsertMenu(gWindowMenu, NOT_A_NORMAL_MENU);
	gMutateMenu = GetMenu(MUTATE_MENU_ID);
	InsertMenu(gMutateMenu, NOT_A_NORMAL_MENU);
	gDynamicsMenu = GetMenu(DYNAMICS_MENU_ID);
	InsertMenu(gDynamicsMenu, NOT_A_NORMAL_MENU);
	gExtractMenu = GetMenu(EXTRACT_MENU_ID);
	InsertMenu(gExtractMenu, NOT_A_NORMAL_MENU);
	gChanMenu = GetMenu(CHAN_MENU_ID);
	InsertMenu(gChanMenu, NOT_A_NORMAL_MENU);
	gRateMenu = GetMenu(RATE_MENU_ID);
	InsertMenu(gRateMenu, NOT_A_NORMAL_MENU);
	gSizeMenu = GetMenu(SIZE_MENU_ID);
	InsertMenu(gSizeMenu, NOT_A_NORMAL_MENU);
	gSigDispMenu = GetMenu(SIGDISP_MENU_ID);
	InsertMenu(gSigDispMenu, NOT_A_NORMAL_MENU);
	gSpectDispMenu = GetMenu(SPECTDISP_MENU_ID);
	InsertMenu(gSpectDispMenu, NOT_A_NORMAL_MENU);
	gSonoDispMenu = GetMenu(SONODISP_MENU_ID);
	InsertMenu(gSonoDispMenu, NOT_A_NORMAL_MENU);
	gImportMenu = GetMenu(IMPORT_MENU_ID);
	InsertMenu(gImportMenu, NOT_A_NORMAL_MENU);
	gExportMenu = GetMenu(EXPORT_MENU_ID);
	InsertMenu(gExportMenu, NOT_A_NORMAL_MENU);
	gInputMenu = GetMenu(INPUT_MENU_ID);
	InsertMenu(gInputMenu, NOT_A_NORMAL_MENU);
	
	HandleAboutWindow(1, "\pMenus Initialized");

}

void
GlobalInit(void)
{
	Str255	dumbString;
	
	inSIPtr = nil;
	filtSIPtr = nil;
	outSIPtr = nil;
	outLSIPtr = nil;
	outRSIPtr = nil;
	outSteadSIPtr = nil;
	outTransSIPtr = nil;
	
	/* Set up common variables */
	gDone = false;
	gOKtoOK = true;
	gProcessEnabled = NO_PROCESS;
	gProcessDisabled = NO_PROCESS;
	gStopProcess = false;
	gStartUpScreen = true;
	gNumFilesOpen = 0;
	gStartTicks = TickCount();
	nilFSSpec.vRefNum = 0;
	nilFSSpec.parID = 0;
	StringCopy("\p", nilFSSpec.name);
	// set up nil pointers
	inMI.loopStart = nil;
	inMI.loopEnd = nil;
	inMI.markPos = nil;
	outMI.loopStart = nil;
	outMI.loopEnd = nil;
	outMI.markPos = nil;
	gIOSamplesAllocated = 0;
	// start out with 4 channels of 1024, just for the heck of it....
	AllocateSoundIOMemory(4, 1024);
	// set up the sound channel
	InitializePlay();
	SetPlayLooping(false);

	/* Set up constants */
	Pi = 4.0 * atan(1.0);
	twoPi = 8.0 * atan(1.0);
	InitFFTTable();
	InitProcessWindow();	// no better place to do it
	InitCursors();
	gRedPP = NewPixPat();
	gGreenPP = NewPixPat();
	GetCurrentProcess(&gPSN);
	gFunction = (float *)NewPtr(400 * sizeof(float));
	PickPhrase(dumbString);	
	HandleAboutWindow(1, dumbString);
	HandleAboutWindow(1, "\pFFT Tables Initialized");
}

#ifdef powerc
void
RoutineDesInit(void)
{
	HandleAboutWindow(1, "\pStarting Routine Descriptors Init");
	OpenSoundFileCallBackInit();
	CreateSoundFileCallBackInit();
	AppleEventCallBackInit();
	QuickTimeImportCallBackInit();
	HandleAboutWindow(1, "\pRoutine Descriptors Initialized");
}
#endif

void
MainLoop(void)
{
	while(gDone != true)
	{
//		YieldToAnyThread();
		if(gStartUpScreen)
		{
			if(TickCount()>gStartTicks+180)
			{
				HandleAboutWindow(2, nil);
				gStartUpScreen = false;
			}
		}
		HandleEvent();
	}
}

void
HandleEvent(void)
{
	char		theChar;
	OSErr		myErr;
	SoundInfoPtr	currentSIPtr;
	long 		i, tick;
	
	if(gProcessEnabled == NO_PROCESS)
		WaitNextEvent(everyEvent, &gTheEvent, SLEEP, NIL_MOUSE_REGION);
	else if(gProcessEnabled == PLAY_PROCESS || gProcessEnabled == FUDGE_PROCESS || gProcessEnabled == METER_PROCESS || gProcessEnabled == DRAW_PROCESS)
		WaitNextEvent(everyEvent, &gTheEvent, 3, NIL_MOUSE_REGION);
	else
	{
		WaitNextEvent(everyEvent, &gTheEvent, 1L, NIL_MOUSE_REGION);
	}
	if(IsDialogEvent(&gTheEvent) && (gProcessEnabled == PLAY_PROCESS || gProcessEnabled == FUDGE_PROCESS || gProcessEnabled == NO_PROCESS || gProcessEnabled == METER_PROCESS || gProcessEnabled == DRAW_PROCESS))
	{
		if(HandleDialogEvent(&gTheEvent))
			return;
	}
	
	switch(gTheEvent.what)
	{
		case nullEvent:
			if(gProcessEnabled != NO_PROCESS)
			{
				DrawProcessWindow();
				currentSIPtr = firstSIPtr;
				for(i = 0; i < gNumFilesOpen; i++)
				{
					if(currentSIPtr->infoUpdate == true)
						UpdateInfoWindow(currentSIPtr);
					currentSIPtr = (SoundInfoPtr)currentSIPtr->nextSIPtr;
				}
				HandleUpdateEvent();
				tick = TickCount();
				while(tick+20 > TickCount())
					HandleNullEvent();
			}
			break;
		case mouseDown:
			HandleMouseDown();
			break;
		case kHighLevelEvent :
			myErr = AEProcessAppleEvent(&gTheEvent);			
			break;
		case keyDown:
		case autoKey:
			theChar = gTheEvent.message & charCodeMask;
			if (( gTheEvent.modifiers & cmdKey) != 0)
				HandleMenuChoice(MenuKey(theChar));
			else if (( gTheEvent.modifiers & controlKey) != 0)
				HandleKeyDown((theChar), true);
			else
				HandleKeyDown(theChar, false);
			break;
		case updateEvt:
			DrawProcessWindow();
			HandleUpdateEvent();
			break;
		default:
			break;
	}
}

void	HandleNullEvent(void)
{
	Str255 tmpStr;
	
	switch(gProcessEnabled)
	{
		case NO_PROCESS:
			break;
		case COPY_PROCESS:
			SpinWart();
			CopyBlock();
			break;
		case SPLIT_PROCESS:
			SpinWart();
			SplitBlock();
			break;
		case PLAY_PROCESS:
			if(GetPlayState())
			{
				UpdatePlay();
				StringAppend("\pplay ¥ ", gPlayInfo.file->sfSpec.name, tmpStr);
				UpdateProcessWindow("\p", "\p", tmpStr, -1.0);
			}
			else
			{
				SetMenuItemText(gFileMenu, PLAY_ITEM, "\pPlay File (space bar)");
				SetPlayLooping(false);
				gProcessEnabled = NO_PROCESS;
				MenuUpdate();
				frontSIPtr->playPosition = 0;
				HandleProcessWindowEvent(gTheEvent.where, true);
				UpdateProcessWindow("\p", "\p", "\pidle", -1.0);
				DrawProcessWindow();
			}	
			break;
		case FIR_PROCESS:
			SpinWart();
			if(gCI.binaural)
			{
				ConvolveBlock();
				ConvolveBlock();
				ConvolveBlock();
				ConvolveBlock();
				ConvolveBlock();
				ConvolveBlock();
				ConvolveBlock();
				ConvolveBlock();
			}
			else
				ConvolveBlock();
			break;
		case RING_PROCESS:
			SpinWart();
			RingModBlock();
			break;
		case PVOC_PROCESS:
		//	SpinWart();
			PvocBlock();
			break;
		case ANAL_PROCESS:
			SpinWart();
			AnalBlock();
			break;
		case SYNTH_PROCESS:
			SpinWart();
			SynthBlock();
			break;
		case DYNAMICS_PROCESS:
			SpinWart();
			SpecGateBlock();
			break;
		case GAIN_PROCESS:
			SpinWart();
			GainBlock();
			break;
		case MUTATE_PROCESS:
			SpinWart();
			MutateBlock();
			break;
		case NORM_PROCESS:
			SpinWart();
			NormBlock();
			break;
		case SRATE_PROCESS:
			SpinWart();
			SRConvBlock();
			break;
		case EXTRACT_PROCESS:
			SpinWart();
			ExtractBlock();
			break;
		case MPEG_IMPORT_PROCESS:
//			SpinWart();
//			MPEGImportBlock();
			break;
//	display processes
		case METER_PROCESS:
			MeterUpdate();
			break;
		case DRAW_PROCESS:
			DrawTrackMouse();
			break;
		default:
			break;
	}
}

short	HandleDialogEvent(EventRecord *evp)
{
	DialogPtr	whichDlg;
	short	itemType, itemHit;
	unsigned long	tick, theDialog;
	Handle	itemHandle;
	Rect	itemRect;
	char	theKey;
	Boolean	myKey, flashIt, noEvent;
	WindInfo	*myWI;
	WindowPtr	tmpWindow;

	myKey = false;
	flashIt = false;
	if(evp->what == keyDown)
	{
		myKey = true;
		theKey =  evp->message & charCodeMask;
		if(evp->modifiers & cmdKey)	// Command Key Combos
		{
			if(theKey == '.')		// command-.
			{
				itemHit = 2;		// cancel
				flashIt = true;
			}
			else
				myKey = false;
		}
		else
		{
			if(theKey == 0x0d)		// return and enter
			{
				itemHit = 1;		// ok
				flashIt = true;
			}
			else if(theKey == 0x1b)	// escape
			{
				itemHit = 2;		// cancel
				flashIt = true;
			}
			else
				myKey = false;
		}
	}
	if(myKey == false)
	{
		noEvent = DialogSelect(evp, &whichDlg, &itemHit);
		if(noEvent == false)
				return(false);		/* no extra work needed if false ,  just return */
	}
	else
	{
#if TARGET_API_MAC_CARBON == 1
		tmpWindow = FrontWindow();
		whichDlg = GetDialogFromWindow(tmpWindow);
#else
		whichDlg = (DialogPtr)FrontWindow();
#endif
	}

	if(flashIt)
	{
		GetDialogItem(whichDlg, itemHit, &itemType, &itemHandle, &itemRect);
#if TARGET_API_MAC_CARBON == 1
		if(GetControlHilite((ControlHandle)itemHandle) == 0)
#else
		if((**(ControlHandle)itemHandle).contrlHilite == 0)
#endif
		{
			HiliteControl((ControlHandle)itemHandle,kControlButtonPart);
			Delay(8,&tick);
			HiliteControl((ControlHandle)itemHandle,0);
		}
	}
	
	if(gProcessEnabled == PLAY_PROCESS)
	{
		SetMenuItemText(gFileMenu, PLAY_ITEM, "\pPlay File (space bar)");
		StopPlay(false);
		SetPlayLooping(false);
		gProcessEnabled = NO_PROCESS;
		MenuUpdate();
	}


	/* Now list routines for each modeless dialogs */
#if TARGET_API_MAC_CARBON == 1
	myWI = (WindInfoPtr)GetWRefCon(GetDialogWindow(whichDlg));
#else
	myWI = (WindInfoPtr)GetWRefCon(whichDlg);
#endif
	theDialog = (long)(myWI->structPtr);
	switch(theDialog)
	{
		case BINAURAL_DLOG:
			HandleBinauralDialogEvent(itemHit);
			break;
		case DRAWFUNC_DLOG:
			HandleDrawFunctionDialogEvent(itemHit);
			break;
		case FIR_DLOG:
			HandleFIRDialogEvent(itemHit);
			break;
		case ANAL_DLOG:
			SynthAnalDialogEvent(itemHit);
			break;
		case SOUNDPICT_DLOG:
			SoundPICTDialogEvent(itemHit);
			break;
		case DYNAMICS_DLOG:
			HandleDynamicsDialogEvent(itemHit);
			break;
		case EXTRACT_DLOG:
			HandleExtractDialogEvent(itemHit);
			break;
		case GAIN_DLOG:
			HandleGainDialogEvent(itemHit);
			break;
		case MUTATE_DLOG:
			HandleMutateDialogEvent(itemHit);
			break;
		case PVOC_DLOG:
			HandlePvocDialogEvent(itemHit);
			break;
		case SRATE_DLOG:
			HandleSRConvDialogEvent(itemHit);
			break;
		case HEADER_DLOG:
			HandleHeaderDialogEvent(itemHit);
			break;
		case ERROR_DLOG:
			HandleErrorMessageEvent(itemHit);
			break;
		case MARKER_DLOG:
			HandleMarkerDialogEvent(itemHit);
			break;
		case RECORD_DLOG:
			HandleRecordDialogEvent(itemHit);
			break;
		case PREF_DLOG:
			HandlePrefDialogEvent(itemHit);
			break;
		default:
			return(false);
			break;
	}
	return(true);
}

void	HandleKeyDown(char theChar, Boolean control)
{
	Str255	tmpString;
	unsigned long controlValue;
	double frontLength, startTime;
	
	switch(theChar)
	{
		case ' ':
			if(gProcessEnabled == NO_PROCESS || gProcessEnabled == PLAY_PROCESS)
			{
				GetMenuItemText(gFileMenu, PLAY_ITEM, tmpString);
				if(EqualString(tmpString,"\pPlay File (space bar)",true,true))
				{
					if(gNumFilesOpen > 0)
					{
						controlValue = GetControlValue(gSliderCntl);
						frontLength = frontSIPtr->numBytes/(frontSIPtr->sRate * frontSIPtr->nChans * frontSIPtr->frameSize);
						startTime = (controlValue * frontLength)/420.0;
						SetMenuItemText(gFileMenu, PLAY_ITEM, "\pStop Play (space)");
						SetPlayLooping(control);
						StartPlay(frontSIPtr, startTime, 0.0);
						gProcessEnabled = PLAY_PROCESS;
						MenuUpdate();
					}
				}
				else
				{
					SetMenuItemText(gFileMenu, PLAY_ITEM, "\pPlay File (space bar)");
					StopPlay(false);
					SetPlayLooping(false);
//					UpdateProcessWindow("\p", "\p", "\pidle", 0.0);
//					gProcessEnabled = NO_PROCESS;
					MenuUpdate();
				}
			}
			break;
		case 0xd:
			if(gProcessEnabled == PLAY_PROCESS)
			{
				SetMenuItemText(gFileMenu, PLAY_ITEM, "\pPlay File (space bar)");
				StopPlay(false);
				SetPlayLooping(false);
//				UpdateProcessWindow("\p", "\p", "\pidle", 0.0);
//				gProcessEnabled = NO_PROCESS;
				MenuUpdate();
			}
			frontSIPtr->playPosition = 0;
			HandleProcessWindowEvent(gTheEvent.where, true);
			break;
	}
}

void
HandleMouseDown(void)
{
	WindowPtr	whichWindow;
	short		thePart;
	long		menuChoice, windSize;
	GrafPtr		oldPort;
	WindInfo	*myWI;
	RgnHandle	dragRgn;
	Rect		windowRect;
#if TARGET_API_MAC_CARBON == 0
	WindowPeek	updateWindowPeek;
#endif
	Rect tmpRect;

	
	thePart = FindWindow(gTheEvent.where, &whichWindow);
	switch(thePart)
	{
		case inMenuBar:
			menuChoice = MenuSelect(gTheEvent.where);
			HandleMenuChoice(menuChoice);
			break;
		case inSysWindow:
			SystemClick(&gTheEvent,whichWindow);
			break;
		case inGoAway:
			HandleCloseWindow(whichWindow);
			break;
		case inGrow:
			dragRgn = GetGrayRgn();
			HLock((Handle)dragRgn);
#if TARGET_API_MAC_CARBON == 1
			GetRegionBounds(dragRgn, &tmpRect);
			windSize = GrowWindow(whichWindow,gTheEvent.where, &tmpRect);
#else
			windSize = GrowWindow(whichWindow,gTheEvent.where,&((**dragRgn).rgnBBox));
#endif
			if (windSize != 0)
			{
				GetPort(&oldPort);
#if TARGET_API_MAC_CARBON == 1
				SetPort(GetWindowPort(whichWindow));
				GetPortBounds(GetWindowPort(whichWindow), &windowRect);
				EraseRect(&windowRect);
				SizeWindow(whichWindow,LoWord(windSize),HiWord(windSize),NORMAL_UPDATES);
				GetPortBounds(GetWindowPort(whichWindow), &windowRect);
				InvalWindowRect(whichWindow, &windowRect);
#else
				SetPort(whichWindow);
				EraseRect(&whichWindow->portRect);
				SizeWindow(whichWindow,LoWord(windSize),HiWord(windSize),NORMAL_UPDATES);
				InvalWindowRect(whichWindow, &whichWindow->portRect);
#endif
				SetPort(oldPort);
				gGrowWindow = whichWindow;
			}
			HUnlock((Handle)dragRgn);
			break;
		case inDrag:
			dragRgn = GetGrayRgn();
			HLock((Handle)dragRgn);
#if TARGET_API_MAC_CARBON == 1
			GetRegionBounds(dragRgn, &tmpRect);
			DragWindow(whichWindow,gTheEvent.where, &tmpRect);
#else
			DragWindow(whichWindow,gTheEvent.where,&((**dragRgn).rgnBBox));
#endif
			HUnlock((Handle)dragRgn);
			myWI = (WindInfoPtr)GetWRefCon(whichWindow);
			if(myWI != nil)
			{
				if(myWI->windType == INFOWIND)
				{
					frontSIPtr = (SoundInfo *)myWI->structPtr;
					if(gProcessEnabled != PLAY_PROCESS)
						HandleProcessWindowEvent(gTheEvent.where, true);
				}
			}
			SelectWindow(whichWindow);
			MenuUpdate();
			break;
		case inContent:
			SelectWindow(whichWindow);
			myWI = (WindInfoPtr)GetWRefCon(whichWindow);
			if(myWI != nil)
			{
				if(myWI->windType == INFOWIND)
				{
					frontSIPtr = (SoundInfo *)myWI->structPtr;
					if(gProcessEnabled != PLAY_PROCESS)
						HandleProcessWindowEvent(gTheEvent.where, true);
				}
			}
			else if(whichWindow == gProcessWindow)
				HandleProcessWindowEvent(gTheEvent.where, false);
			else if(whichWindow == gAboutWindow)
				HandleAboutWindow(2, nil);
			MenuUpdate();
			break;
		case inZoomIn:
		case inZoomOut:
			if (TrackBox(whichWindow,gTheEvent.where,thePart))
			{
				GetPort(&oldPort);
#if TARGET_API_MAC_CARBON == 1
				SetPort(GetWindowPort(whichWindow));
				GetPortBounds(GetWindowPort(whichWindow), &windowRect);
				EraseRect(&windowRect);
				ZoomWindow(whichWindow,thePart,LEAVE_IT_WHERE_IT_IS);
				GetPortBounds(GetWindowPort(whichWindow), &windowRect);
				InvalWindowRect(whichWindow, &windowRect);
#else
				SetPort(whichWindow);				
				EraseRect(&whichWindow->portRect);
				ZoomWindow(whichWindow,thePart,LEAVE_IT_WHERE_IT_IS);
				InvalWindowRect(whichWindow, &whichWindow->portRect);
#endif
				SetPort(oldPort);
				gGrowWindow = whichWindow;
			}
			break;
	}
}

void	HandleUpdateEvent(void)
{
#if TARGET_API_MAC_CARBON == 0
	WindowPeek	updateWindowPeek;
#endif
	WindInfo	*myWI;

	myWI = (WindInfoPtr)GetWRefCon((WindowPtr)gTheEvent.message);
#if TARGET_API_MAC_CARBON == 1
	if(GetWindowKind((WindowPtr)gTheEvent.message) == dialogKind)
#else
	updateWindowPeek = (WindowPeek)gTheEvent.message;
	if(updateWindowPeek->windowKind == dialogKind)
#endif
	{
#if TARGET_API_MAC_CARBON == 1
		BeginUpdate((WindowPtr)gTheEvent.message);
		DrawDialog(GetDialogFromWindow((WindowPtr)gTheEvent.message));
		EndUpdate((WindowPtr)gTheEvent.message);
		if(gNumFilesOpen > 0)
			UpdateInfoWindowFileType();
		if(gDrawFunctionDialog == GetDialogFromWindow((WindowPtr)gTheEvent.message))
		{
			DrawFunction();
		}
#else
		BeginUpdate((WindowPtr)gTheEvent.message);
		DrawDialog((DialogPtr)gTheEvent.message);
		EndUpdate((WindowPtr)gTheEvent.message);
		if(gNumFilesOpen > 0)
			UpdateInfoWindowFileType();
		if(gDrawFunctionDialog == (DialogPtr)gTheEvent.message)
		{
			DrawFunction();
		}
#endif
	}
	else if(myWI != nil)
	{
		if(myWI->windType == SPECTWIND)
		{
			BeginUpdate((WindowPtr)gTheEvent.message);
			UpdateSpectDisplay((SoundInfo *)myWI->structPtr);
			EndUpdate((WindowPtr)gTheEvent.message);
		}
		else if(myWI->windType == ANALWIND)
		{
			BeginUpdate((WindowPtr)gTheEvent.message);
			UpdateSpectDispWindow((SoundDisp *)myWI->structPtr);
			EndUpdate((WindowPtr)gTheEvent.message);
		}
		else if(myWI->windType == WAVEWIND)
		{
			BeginUpdate((WindowPtr)gTheEvent.message);
			DrawWaveBorders((SoundInfo *)myWI->structPtr);
			EndUpdate((WindowPtr)gTheEvent.message);
		}
		else if(myWI->windType == SONOWIND)
		{
			BeginUpdate((WindowPtr)gTheEvent.message);
			UpdateSonoDisplay((SoundInfo *)myWI->structPtr);
			EndUpdate((WindowPtr)gTheEvent.message);
		}
		else if(myWI->windType == INFOWIND)
		{
			BeginUpdate((WindowPtr)gTheEvent.message);
			UpdateInfoWindow((SoundInfo *)myWI->structPtr);
			EndUpdate((WindowPtr)gTheEvent.message);
		}
	}
	else if((WindowPtr)gTheEvent.message == gProcessWindow)
	{
		BeginUpdate((WindowPtr)gTheEvent.message);
		UpdateProcessWindow("\p", "\p", "\p", -1.0);
		DrawControls(gProcessWindow);
		EndUpdate((WindowPtr)gTheEvent.message);
	}
	else if((WindowPtr)gTheEvent.message == gAboutWindow)
	{
		BeginUpdate((WindowPtr)gTheEvent.message);
		HandleAboutWindow(1, nil);
		EndUpdate((WindowPtr)gTheEvent.message);
	}
	else
	{
		BeginUpdate((WindowPtr)gTheEvent.message);
		EndUpdate((WindowPtr)gTheEvent.message);
	}
}

void
HandleMenuChoice(long menuChoice)
{
	short	theMenu;
	short theItem;
	
	if(menuChoice != 0)
	{
		theMenu = HiWord(menuChoice);
		theItem = LoWord(menuChoice);
		switch(theMenu)
		{
			case APPLE_MENU_ID:
				HandleAppleChoice(theItem);
				break;
			case FILE_MENU_ID:
				HandleFileChoice(theItem);
				break;
			case EDIT_MENU_ID:
				HandleEditChoice(theItem);
				break;
			case PROCESS_MENU_ID:
				HandleProcessChoice(theItem);
				break;
			case SOUNDFILE_MENU_ID:
				HandleSoundChoice(theItem);
				break;
			case CONTROL_MENU_ID:
				HandleControlChoice(theItem);
				break;
			case SIGDISP_MENU_ID:
				HandleSigDispChoice(theItem);
				break;
			case SPECTDISP_MENU_ID:
				HandleSpectDispChoice(theItem);
				break;
			case SONODISP_MENU_ID:
				HandleSonoDispChoice(theItem);
				break;
			case IMPORT_MENU_ID:
				HandleImportChoice(theItem);
				break;
			case EXPORT_MENU_ID:
				HandleExportChoice(theItem);
				break;
		}
		HiliteMenu(0);
	}
}

void
HandleAppleChoice(short theItem)
{
	Str255	accName;
	short		accNumber;
	
	switch	(theItem)
	{
		case ABOUT_ITEM:
			HandleAboutWindow(1, nil);
			break;
		default:
			GetMenuItemText(gAppleMenu,theItem,accName);
			accNumber = OpenDeskAcc(accName);
			break;
	}
}

void
HandleFileChoice(short theItem)
{
	long	controlValue;
	long	i;
	Str255	tmpString;
	WindowPtr	foreWindow;
	double		startTime, frontLength;
	FInfo	fndrInfo;
	FSSpec	tmpFSSpec;
	
	switch(theItem)
	{
		case NEW_ITEM:
			FinishPlay();
			HandleRecordDialog();
			break;
		case OPEN_ITEM:
			FinishPlay();
			if(OpenSoundFile(nilFSSpec, false) != -1)
				MenuUpdate();
			break;
		case OPEN_ANY_ITEM:
			FinishPlay();
			if(OpenSoundFile(nilFSSpec, true) != -1)
				MenuUpdate();
			break;
		case CLOSE_ITEM:
			FinishPlay();
			foreWindow = FrontWindow();
			HandleCloseWindow(foreWindow);
			break;
		case CLOSE_EDIT_ITEM:
			FinishPlay();
			tmpFSSpec = frontSIPtr->sfSpec;
			CloseSoundFile(frontSIPtr);
			FlushVol("\p",tmpFSSpec.vRefNum);
			FSpGetFInfo(&tmpFSSpec,&fndrInfo);
			fndrInfo.fdCreator = gPreferences.editorID;
			fndrInfo.fdFlags = fndrInfo.fdFlags & 0xfeff;
			FSpSetFInfo(&tmpFSSpec,&fndrInfo);
			FlushVol("\p",tmpFSSpec.vRefNum);
			TouchFolder(tmpFSSpec.vRefNum, tmpFSSpec.parID);
			for(i=0; i<60; i++)
				HandleEvent();
			OpenSelection(&tmpFSSpec);
			break;
		case SAVE_ITEM:
			FinishPlay();
			inSIPtr = frontSIPtr;
			InitCopySoundFile();
			break;
		case SPLIT_ITEM:
			FinishPlay();
			inSIPtr = frontSIPtr;
			InitSplitSoundFile();
			break;
		case PLAY_ITEM:
			GetMenuItemText(gFileMenu, PLAY_ITEM, tmpString);
			if(EqualString(tmpString,"\pPlay File (space bar)",true,true))
			{
				controlValue = GetControlValue(gSliderCntl);
				frontLength = frontSIPtr->numBytes/(frontSIPtr->sRate * frontSIPtr->nChans * frontSIPtr->frameSize);
				startTime = (controlValue * frontLength)/420.0;
				SetMenuItemText(gFileMenu, PLAY_ITEM, "\pStop Play (space)");
				SetPlayLooping(false);
				StartPlay(frontSIPtr, startTime, 0.0);
				gProcessEnabled = PLAY_PROCESS;
				MenuUpdate();
			}
			else
			{
				SetMenuItemText(gFileMenu, PLAY_ITEM, "\pPlay File (space bar)");
				StopPlay(false);
				SetPlayLooping(false);
				gProcessEnabled = NO_PROCESS;
				MenuUpdate();
			}
			break;
		case QUIT_ITEM:
			FinishPlay();
			TerminateSoundHack();
			break;
	}
}

void
HandleImportChoice(short theItem)
{
	FinishPlay();
	switch(theItem)
	{
		case SND_IMPORT_ITEM:
			HandleImportDialog();
			break;
		case CD_IMPORT_ITEM:
			if(gSystemVersion > 0x1000)
				DrawErrorMessage("\pThere is no need to import CD tracks in OSX. Just open them like soundfiles.");
			else
				CDAudioImporter();
			break;
		case MPEG_IMPORT_ITEM:
//			InitMPEGImport();
			DrawErrorMessage("\pnot implemented yet");
			break;
	}
}

void
HandleExportChoice(short theItem)
{
	FinishPlay();
	switch(theItem)
	{
		case SND_EXPORT_ITEM:
			inSIPtr = frontSIPtr;
			HandleExportDialog();
			inSIPtr = nil;
			break;
		case MPEG_EXPORT_ITEM:
			DrawErrorMessage("\pnot implemented yet");
			break;
	}
}

void
HandleEditChoice(short theItem)
{
	switch(theItem)
	{
		case UNDO_ITEM:
			break;
		case CUT_ITEM:
			break;
		case COPY_ITEM:
			break;
		case PASTE_ITEM:
			break;
	}
}

void
HandleSoundChoice(short theItem)
{
	short	i;
	SoundInfo *mySI, *currentSI;
	Point	myPoint;
	
	myPoint.v = myPoint.h = 0;
	currentSI = firstSIPtr;
	for(i = 1; i<=theItem; i++)
	{
		mySI = currentSI;
		currentSI = (SoundInfo *)mySI->nextSIPtr;
	}
	frontSIPtr = mySI;
	SelectWindow(frontSIPtr->infoWindow);
	if(gProcessEnabled != PLAY_PROCESS)
		HandleProcessWindowEvent(myPoint, true);
	MenuUpdate();
}

void
HandleProcessChoice(short theItem)
{
	FinishPlay();
	inSIPtr = frontSIPtr;
	if((inSIPtr->sfType == TEXT || inSIPtr->sfType == RAW || inSIPtr->nChans >= 4)
			&& (theItem != HEADER_ITEM) && (theItem != MARKER_ITEM))
	{
		DrawErrorMessage("\pSoundHack cannot process text, quad or headerless soundfiles.");
		return;
	}
	switch(theItem)
	{
		case HEADER_ITEM:
			HandleHeaderDialog();
			break;
		case MARKER_ITEM:
			HandleMarkerDialog();
			break;
		case BINAURAL_ITEM:
			if(inSIPtr->nChans == 1)
				HandleBinauralDialog();
			else
				DrawErrorMessage("\pThe binaural filter works only on mono files.");
			break;
		case FIR_ITEM:
			HandleFIRDialog();
			break;
		case GAIN_ITEM:
			HandleGainDialog();
			break;
		case PVOC_ITEM:
			HandlePvocDialog();
			break;
		case ANAL_ITEM:
			SynthAnalDialogInit();
			break;
		case SOUNDPICT_ITEM:
			SoundPICTDialogInit();
			break;
		case NORM_ITEM:
			gNormalize = true;
			InitNormProcess(frontSIPtr);
			break;
		case SRATE_ITEM:
			HandleSRConvDialog();
			break;
		case DYNAMICS_ITEM:
			HandleDynamicsDialog();
			break;
		case MUTATE_ITEM:
			HandleMutateDialog();
			break;
		case EXTRACT_ITEM:
			HandleExtractDialog();
			break;
	}
}

void	HandleControlChoice(short theItem)
{
	if(gProcessEnabled == PLAY_PROCESS && gProcessDisabled != 0)
		return;
	FinishPlay();
	switch(theItem)
	{
		case PAUSE_ITEM:
			gProcessDisabled = gProcessEnabled;
			gProcessEnabled = NO_PROCESS;
			if(outSIPtr != nil)
			{
				WriteHeader(outSIPtr);
				UpdateInfoWindow(outSIPtr);
			}
			if(outLSIPtr != nil)
			{
				WriteHeader(outLSIPtr);
				UpdateInfoWindow(outLSIPtr);
			}
			if(outRSIPtr != nil)
			{
				WriteHeader(outRSIPtr);
				UpdateInfoWindow(outRSIPtr);
			}
			if(outSteadSIPtr != nil)
			{
				WriteHeader(outSteadSIPtr);
				UpdateInfoWindow(outSteadSIPtr);
			}
			if(outTransSIPtr != nil)
			{
				WriteHeader(outTransSIPtr);
				UpdateInfoWindow(outTransSIPtr);
			}
			MenuUpdate();
			break;
		case CONTINUE_ITEM:
			gProcessEnabled = gProcessDisabled;
			gProcessDisabled = NO_PROCESS;
			MenuUpdate();
			break;
		case STOP_ITEM:
			gStopProcess = true;
			if(gProcessEnabled == NO_PROCESS)
			{
				gProcessEnabled = gProcessDisabled;
				gProcessDisabled = NO_PROCESS;
			}
			break;
		case LOAD_SETINGS_ITEM:
			LoadSettings();
			break;
		case SAVE_SETTINGS_ITEM:
			SaveSettings();
			break;
		case DEFAULT_SETTINGS_ITEM:
			SetDefaults();
			break;
		case PREFERENCES_ITEM:
			HandlePrefDialog();
			break;
	}
}

void
HandleSigDispChoice(short theItem)
{	
	short curMark;
	switch(theItem)
	{
		case DISP_INPUT_ITEM:
			GetItemMark(gSigDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSigDispMenu, theItem, true );
			else
			{
				CheckItem(gSigDispMenu, theItem, false );
				if(inSIPtr != nil)
					DisposeWindWorld(&(inSIPtr->sigDisp));
			}
			break;
		case DISP_AUX_ITEM:
			GetItemMark(gSigDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSigDispMenu, theItem, true );
			else
			{
				CheckItem(gSigDispMenu, theItem, false );
				if(filtSIPtr != nil)
					DisposeWindWorld(&(filtSIPtr->sigDisp));
				else if(outSteadSIPtr != nil)
					DisposeWindWorld(&(outSteadSIPtr->sigDisp));
			}
			break;
		case DISP_OUTPUT_ITEM:
			GetItemMark(gSigDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSigDispMenu, theItem, true );
			else
			{
				CheckItem(gSigDispMenu, theItem, false );
				if(outSIPtr != nil)
					DisposeWindWorld(&(outSIPtr->sigDisp));
				else if(outTransSIPtr != nil)
					DisposeWindWorld(&(outTransSIPtr->sigDisp));
			}
			break;
	}
}

void
HandleSpectDispChoice(short theItem)
{	
	short curMark;
	switch(theItem)
	{
		case DISP_INPUT_ITEM:
			GetItemMark(gSpectDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSpectDispMenu, theItem, true );
			else
			{
				CheckItem(gSpectDispMenu, theItem, false );
				if(inSIPtr != nil)
					DisposeWindWorld(&(inSIPtr->spectDisp));
			}
			break;
		case DISP_AUX_ITEM:
			GetItemMark(gSpectDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSpectDispMenu, theItem, true );
			else
			{
				CheckItem(gSpectDispMenu, theItem, false );
				if(filtSIPtr != nil)
					DisposeWindWorld(&(filtSIPtr->spectDisp));
				else if(outSteadSIPtr != nil)
					DisposeWindWorld(&(outSteadSIPtr->spectDisp));
			}
			break;
		case DISP_OUTPUT_ITEM:
			GetItemMark(gSpectDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSpectDispMenu, theItem, true);
			else
			{
				CheckItem(gSpectDispMenu, theItem, false );
				if(outSIPtr != nil)
					DisposeWindWorld(&(outSIPtr->spectDisp));
				else if(outTransSIPtr != nil)
					DisposeWindWorld(&(outTransSIPtr->spectDisp));
			}
			break;
	}
}

void
HandleSonoDispChoice(short theItem)
{	
	short curMark;
	switch(theItem)
	{
		case DISP_INPUT_ITEM:
			GetItemMark(gSonoDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSonoDispMenu, theItem, true );
			else
			{
				CheckItem(gSonoDispMenu, theItem, false );
				if(inSIPtr != nil)
					DisposeWindWorld(&(inSIPtr->sonoDisp));
			}
			break;
		case DISP_AUX_ITEM:
			GetItemMark(gSonoDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSonoDispMenu, theItem, true );
			else
			{
				CheckItem(gSonoDispMenu, theItem, false );
				if(filtSIPtr != nil)
					DisposeWindWorld(&(filtSIPtr->sonoDisp));
				else if(outSteadSIPtr != nil)
					DisposeWindWorld(&(outSteadSIPtr->sonoDisp));
			}
			break;
		case DISP_OUTPUT_ITEM:
			GetItemMark(gSonoDispMenu, theItem, &curMark);
			if (curMark == noMark)
				CheckItem(gSonoDispMenu, theItem, true );
			else
			{
				CheckItem(gSonoDispMenu, theItem, false );
				if(outSIPtr != nil)
					DisposeWindWorld(&(outSIPtr->sonoDisp));
				else if(outTransSIPtr != nil)
					DisposeWindWorld(&(outTransSIPtr->sonoDisp));
			}
			break;
	}
}

void
HandleCloseWindow(WindowPtr whichWindow)
{
	WindInfo	*myWI;
	SoundInfoPtr	tmpSIPtr;
	WindowPtr	foreWindow;
	Str255 foreTitle, menuTitle;
#if TARGET_API_MAC_CARBON == 0
	WindowPeek	updateWindowPeek;
#endif

	myWI = (WindInfoPtr)GetWRefCon(whichWindow);
			
	if(myWI->windType == WAVEWIND)	
	{
		HideWindow(whichWindow);
		tmpSIPtr = (SoundInfo *)myWI->structPtr;
		CheckItem(gSigDispMenu, tmpSIPtr->sigDisp.spareB, false);
		DisposeWindWorld(&(tmpSIPtr->sigDisp));
	}
	else if(myWI->windType == SONOWIND)	
	{
		HideWindow(whichWindow);
		tmpSIPtr = (SoundInfo *)myWI->structPtr;
		CheckItem(gSonoDispMenu, tmpSIPtr->sonoDisp.spareB, false);
		DisposeWindWorld(&(tmpSIPtr->sonoDisp));
	}
	else if(myWI->windType == SPECTWIND)
	{
		HideWindow(whichWindow);
		tmpSIPtr = (SoundInfo *)myWI->structPtr;
		CheckItem(gSpectDispMenu, tmpSIPtr->spectDisp.spareB, false);
		DisposeWindWorld(&(tmpSIPtr->spectDisp));
	}
	else if(myWI->windType == INFOWIND)
	{
		tmpSIPtr = (SoundInfo *)myWI->structPtr;
		if(GetFilePlayState(tmpSIPtr))
			FinishPlay();
		CloseSoundFile(tmpSIPtr);
	}
	foreWindow = FrontWindow();
	if(foreWindow)
	{
		myWI = (WindInfoPtr)GetWRefCon(foreWindow);
		if(myWI)
		{
		if(myWI->windType == WAVEWIND || myWI->windType == SPECTWIND || myWI->windType == SONOWIND || myWI->windType == INFOWIND)	
		{
			EnableItem(gFileMenu, CLOSE_ITEM);
			GetWTitle(foreWindow, foreTitle);
			StringAppend("\pClose ", foreTitle, menuTitle);
			SetMenuItemText(gFileMenu, CLOSE_ITEM, menuTitle);
		}
		else
		{
			DisableItem(gFileMenu, CLOSE_ITEM);
			SetMenuItemText(gFileMenu, CLOSE_ITEM, "\pClose");
		}
		}
	}
	else
		DisableItem(gFileMenu, CLOSE_ITEM);
}

// This does a lot of GUI normalization
void
MenuUpdate(void)
{
	Boolean processing, spectral, pict, noClose;
	Point	myPoint;
	WindowPtr foreWindow;
	Str255 foreTitle, menuTitle;
	WindInfo	*myWI;
	Cursor		cursor;
#if TARGET_API_MAC_CARBON == 0
	WindowPeek	updateWindowPeek;
#endif
	
	myPoint.v = myPoint.h = 0;
	
	if(gNumFilesOpen == 0)
	{
		spectral = false;
		pict = false;
	}
	else
	{
		if(frontSIPtr->packMode == SF_FORMAT_SPECT_COMPLEX || frontSIPtr->packMode == SF_FORMAT_SPECT_AMP || frontSIPtr->packMode == SF_FORMAT_SPECT_AMPPHS
			|| frontSIPtr->packMode == SF_FORMAT_SPECT_AMPFRQ || frontSIPtr->packMode == SF_FORMAT_SPECT_MQ)
			spectral = true;
		else
			spectral = false;
			
		if(frontSIPtr->packMode == SF_FORMAT_PICT)
			pict = true;
		else
			pict = false;
	}
		
	if(gProcessEnabled == NO_PROCESS && gProcessDisabled == NO_PROCESS)
		processing = false;
	else
		processing = true;

	// Reset close menu for foremost item.
	noClose = false;
	foreWindow = FrontWindow();
	if(foreWindow)
	{
		SelectWindow(foreWindow);
		myWI = (WindInfoPtr)GetWRefCon(foreWindow);
		if(myWI != nil)
		{
			if(myWI->windType == WAVEWIND || myWI->windType == SPECTWIND ||
			   	myWI->windType == SONOWIND || (myWI->windType == INFOWIND && !processing))
			{
				EnableItem(gFileMenu, CLOSE_ITEM);
				GetWTitle(foreWindow, foreTitle);
				StringAppend("\pClose ", foreTitle, menuTitle);
				SetMenuItemText(gFileMenu, CLOSE_ITEM, menuTitle);
				if(gPreferences.editorID != 'SDHK')
					EnableItem(gFileMenu,CLOSE_EDIT_ITEM);
			}
			else
				noClose = true;
		}
		else
			noClose = true;

	}
	else
		noClose = true;
	if(noClose == true)
	{
		DisableItem(gFileMenu, CLOSE_EDIT_ITEM);
		DisableItem(gFileMenu, CLOSE_ITEM);
		SetMenuItemText(gFileMenu, CLOSE_ITEM, "\pClose");
	}
	
	// Make sure menus are enabled, only items are disabled
	EnableItem(gFileMenu, 0);
	EnableItem(gProcessMenu, 0);	
	EnableItem(gControlMenu, 0);
	EnableItem(gImportMenu, 0);
	EnableItem(gExportMenu, 0);
	
	// Enable File menu items
	// First, things which are always enabled
	EnableItem(gFileMenu, OPEN_ITEM);
	EnableItem(gFileMenu, OPEN_ANY_ITEM);
	EnableItem(gFileMenu, QUIT_ITEM);
	
	if(!processing)
	{
		EnableItem(gImportMenu, SND_IMPORT_ITEM);
		EnableItem(gImportMenu, MPEG_IMPORT_ITEM);
	}
	else
	{
		DisableItem(gImportMenu, SND_IMPORT_ITEM);
		DisableItem(gImportMenu, MPEG_IMPORT_ITEM);
	}
	if(gGestalt.movie && !processing)
		EnableItem(gImportMenu, CD_IMPORT_ITEM);
	else
		DisableItem(gImportMenu, CD_IMPORT_ITEM);


	if(gGestalt.soundRecord && !processing)
		EnableItem(gFileMenu, NEW_ITEM);
	else
		DisableItem(gFileMenu, NEW_ITEM);
		
	if(gNumFilesOpen && !processing && !spectral && !pict)
	{
		EnableItem(gFileMenu, SAVE_ITEM);
		if(frontSIPtr->nChans == MONO)
			DisableItem(gFileMenu, SPLIT_ITEM);
		else
			EnableItem(gFileMenu, SPLIT_ITEM);
		EnableItem(gExportMenu, SND_EXPORT_ITEM);
		EnableItem(gExportMenu, MPEG_EXPORT_ITEM);
	}
	else
	{
		DisableItem(gFileMenu, SAVE_ITEM);
		DisableItem(gFileMenu, SPLIT_ITEM);
		DisableItem(gExportMenu, SND_EXPORT_ITEM);
		DisableItem(gExportMenu, MPEG_EXPORT_ITEM);
	}
	
	if(gNumFilesOpen && gGestalt.soundPlay && (gProcessEnabled == NO_PROCESS || gProcessEnabled == PLAY_PROCESS) && !spectral && !pict)
		EnableItem(gFileMenu, PLAY_ITEM);
	else
		DisableItem(gFileMenu, PLAY_ITEM);
		
	
// Now onto the Hack menu
	if(gNumFilesOpen && !processing && !spectral && !pict)
	{
		EnableItem(gProcessMenu,HEADER_ITEM);
		EnableItem(gProcessMenu,MARKER_ITEM);
		if(frontSIPtr->nChans == MONO)
			EnableItem(gProcessMenu,BINAURAL_ITEM);
		else
			DisableItem(gProcessMenu,BINAURAL_ITEM);
		EnableItem(gProcessMenu,GAIN_ITEM);
		EnableItem(gProcessMenu,FIR_ITEM);
		EnableItem(gProcessMenu,PVOC_ITEM);	
		EnableItem(gProcessMenu,MUTATE_ITEM);
		EnableItem(gProcessMenu,ANAL_ITEM);
		SetMenuItemText(gProcessMenu,ANAL_ITEM,"\pSpectral Analysis...");
		EnableItem(gProcessMenu,SOUNDPICT_ITEM);
		EnableItem(gProcessMenu,NORM_ITEM);
		EnableItem(gProcessMenu,SRATE_ITEM);	
		EnableItem(gProcessMenu,DYNAMICS_ITEM);
		EnableItem(gProcessMenu,EXTRACT_ITEM);
	}
	else if(gNumFilesOpen && !processing && spectral)
	{
		DisableItem(gProcessMenu,HEADER_ITEM);
		DisableItem(gProcessMenu,MARKER_ITEM);
		DisableItem(gProcessMenu,BINAURAL_ITEM);
		DisableItem(gProcessMenu,GAIN_ITEM);
		DisableItem(gProcessMenu,FIR_ITEM);
		EnableItem(gProcessMenu,PVOC_ITEM);	
		EnableItem(gProcessMenu,MUTATE_ITEM);
		EnableItem(gProcessMenu,ANAL_ITEM);
		SetMenuItemText(gProcessMenu,ANAL_ITEM,"\pSpectral Resynthesis...");
		DisableItem(gProcessMenu,SOUNDPICT_ITEM);
		DisableItem(gProcessMenu,NORM_ITEM);
		DisableItem(gProcessMenu,SRATE_ITEM);	
		EnableItem(gProcessMenu,DYNAMICS_ITEM);
		EnableItem(gProcessMenu,EXTRACT_ITEM);
	}
	else if(gNumFilesOpen && !processing && pict)
	{
		DisableItem(gProcessMenu,HEADER_ITEM);
		DisableItem(gProcessMenu,MARKER_ITEM);
		DisableItem(gProcessMenu,BINAURAL_ITEM);
		DisableItem(gProcessMenu,GAIN_ITEM);
		DisableItem(gProcessMenu,FIR_ITEM);
		DisableItem(gProcessMenu,PVOC_ITEM);	
		DisableItem(gProcessMenu,MUTATE_ITEM);
		DisableItem(gProcessMenu,ANAL_ITEM);
		SetMenuItemText(gProcessMenu,ANAL_ITEM,"\pSpectral Resynthesis...");
		EnableItem(gProcessMenu,SOUNDPICT_ITEM);
		DisableItem(gProcessMenu,NORM_ITEM);
		DisableItem(gProcessMenu,SRATE_ITEM);	
		DisableItem(gProcessMenu,DYNAMICS_ITEM);
		DisableItem(gProcessMenu,EXTRACT_ITEM);
	}
	else
	{
	
		DisableItem(gProcessMenu,FIR_ITEM);
		DisableItem(gProcessMenu,GAIN_ITEM);
		DisableItem(gProcessMenu,BINAURAL_ITEM);
		DisableItem(gProcessMenu,DYNAMICS_ITEM);
		DisableItem(gProcessMenu,MUTATE_ITEM);	
		DisableItem(gProcessMenu,ANAL_ITEM);
		DisableItem(gProcessMenu,SOUNDPICT_ITEM);
		DisableItem(gProcessMenu,NORM_ITEM);
		DisableItem(gProcessMenu,SRATE_ITEM);	
		DisableItem(gProcessMenu,PVOC_ITEM);			
		DisableItem(gProcessMenu,EXTRACT_ITEM);
		DisableItem(gProcessMenu,HEADER_ITEM);
		DisableItem(gProcessMenu,MARKER_ITEM);
	}
	if(!gGestalt.movie)
		DisableItem(gProcessMenu,SOUNDPICT_ITEM);
	// the Control menu
	if(processing)
	{
		EnableItem(gControlMenu,SHOW_WAVE_ITEM);
		EnableItem(gControlMenu,SHOW_SPECT_ITEM);
		EnableItem(gControlMenu,SHOW_SONO_ITEM);
		DisableItem(gControlMenu,LOAD_SETINGS_ITEM);
		DisableItem(gControlMenu,SAVE_SETTINGS_ITEM);
		DisableItem(gControlMenu,DEFAULT_SETTINGS_ITEM);
		DisableItem(gControlMenu,PREFERENCES_ITEM);
		if(gProcessDisabled == UTIL_PROCESS)
		{
			DisableItem(gControlMenu,PAUSE_ITEM);
			DisableItem(gControlMenu,CONTINUE_ITEM);
			DisableItem(gControlMenu,STOP_ITEM);
		}
		else if(gProcessEnabled == NO_PROCESS)
		{
			DisableItem(gControlMenu,PAUSE_ITEM);
			EnableItem(gControlMenu,CONTINUE_ITEM);
			EnableItem(gControlMenu,STOP_ITEM);
		} 
		else if(gProcessEnabled == IMPORT_PROCESS)
		{
			DisableItem(gControlMenu,PAUSE_ITEM);
			DisableItem(gControlMenu,CONTINUE_ITEM);
			EnableItem(gControlMenu,STOP_ITEM);
		} 
		else
		{
			EnableItem(gControlMenu,PAUSE_ITEM);
			DisableItem(gControlMenu,CONTINUE_ITEM);
			EnableItem(gControlMenu,STOP_ITEM);
		}
	}
	else
	{
		DisableItem(gControlMenu,SHOW_WAVE_ITEM);		
		DisableItem(gControlMenu,SHOW_SPECT_ITEM);		
		DisableItem(gControlMenu,SHOW_SONO_ITEM);
		DisableItem(gControlMenu,PAUSE_ITEM);
		DisableItem(gControlMenu,CONTINUE_ITEM);
		DisableItem(gControlMenu,STOP_ITEM);
		EnableItem(gControlMenu,LOAD_SETINGS_ITEM);
		EnableItem(gControlMenu,SAVE_SETTINGS_ITEM);
		EnableItem(gControlMenu,DEFAULT_SETTINGS_ITEM);
		EnableItem(gControlMenu,PREFERENCES_ITEM);
	}		
	DrawMenuBar();
#if TARGET_API_MAC_CARBON == 1
	GetQDGlobalsArrow(&cursor);
	SetCursor(&cursor);
#else
	SetCursor(&qd.arrow);
#endif
	if(gNumFilesOpen > 0)
	{
		UpdateInfoWindowFileType();
		HandleProcessWindowEvent(myPoint, false);
	}
}

short
FinishProcess(void)
{
	SoundInfoPtr	playSIPtr, currentSIPtr;
	
	playSIPtr = nil;
	ShowSmiley();
	UpdateProcessWindow("\p00:00:00.000", "\p00:00:00.000", "\pidle", 0.0);
	HandleProcessWindowEvent(gTheEvent.where, true);
	DrawProcessWindow();
	if(outSIPtr != nil)
	{
		if(outSIPtr->packMode == SF_FORMAT_4_ADIMA || outSIPtr->packMode == SF_FORMAT_MACE3 || outSIPtr->packMode == SF_FORMAT_MACE6) 
			Float2AppleCompressed(outSIPtr, 0, nil, nil);
//		FlushWriteBuffer(outSIPtr);
		WriteHeader(outSIPtr);
		UpdateInfoWindow(outSIPtr);
		playSIPtr = outSIPtr;
	}
	if(outLSIPtr != nil)
	{
		if(outLSIPtr->packMode == SF_FORMAT_4_ADIMA || outLSIPtr->packMode == SF_FORMAT_MACE3 || outLSIPtr->packMode == SF_FORMAT_MACE6) 
			Float2AppleCompressed(outLSIPtr, 0, nil, nil);
//		FlushWriteBuffer(outLSIPtr);
		WriteHeader(outLSIPtr);
		UpdateInfoWindow(outLSIPtr);
		playSIPtr = outLSIPtr;
	}
	if(outRSIPtr != nil)
	{
		if(outRSIPtr->packMode == SF_FORMAT_4_ADIMA || outRSIPtr->packMode == SF_FORMAT_MACE3 || outRSIPtr->packMode == SF_FORMAT_MACE6) 
			Float2AppleCompressed(outRSIPtr, 0, nil, nil);
//		FlushWriteBuffer(outRSIPtr);
		WriteHeader(outRSIPtr);
		UpdateInfoWindow(outRSIPtr);
	}
	if(outSteadSIPtr != nil)
	{
		if(outSteadSIPtr->packMode == SF_FORMAT_4_ADIMA || outSteadSIPtr->packMode == SF_FORMAT_MACE3 || outSteadSIPtr->packMode == SF_FORMAT_MACE6) 
			Float2AppleCompressed(outSteadSIPtr, 0, nil, nil);
//		FlushWriteBuffer(outSteadSIPtr);
		WriteHeader(outSteadSIPtr);
		UpdateInfoWindow(outSteadSIPtr);
		playSIPtr = outSteadSIPtr;
	}
	if(outTransSIPtr != nil)
	{
		if(outTransSIPtr->packMode == SF_FORMAT_4_ADIMA || outTransSIPtr->packMode == SF_FORMAT_MACE3 || outTransSIPtr->packMode == SF_FORMAT_MACE6) 
			Float2AppleCompressed(outTransSIPtr, 0, nil, nil);
//		FlushWriteBuffer(outTransSIPtr);
		WriteHeader(outTransSIPtr);
		UpdateInfoWindow(outTransSIPtr);
	}
	if(inSIPtr != nil)
	{
		if(inSIPtr->packMode == SF_FORMAT_4_ADIMA || inSIPtr->packMode == SF_FORMAT_MACE3 || inSIPtr->packMode == SF_FORMAT_MACE6 
			|| inSIPtr->packMode == SF_FORMAT_MPEG_I || inSIPtr->packMode == SF_FORMAT_MPEG_II || inSIPtr->packMode == SF_FORMAT_MPEG_III ) 
		{
			if(inSIPtr->comp.init == TRUE)
			{
			   	SoundConverterEndConversion(inSIPtr->comp.sc, inSIPtr->comp.outputPtr, 
    				&(inSIPtr->comp.outputFrames), &(inSIPtr->comp.outputBytes));
    			TermAppleCompression(inSIPtr);
    		}
    	}
	}
	if(filtSIPtr != nil)
	{
		if(filtSIPtr->packMode == SF_FORMAT_4_ADIMA || filtSIPtr->packMode == SF_FORMAT_MACE3 || filtSIPtr->packMode == SF_FORMAT_MACE6 
			|| filtSIPtr->packMode == SF_FORMAT_MPEG_I || filtSIPtr->packMode == SF_FORMAT_MPEG_II || filtSIPtr->packMode == SF_FORMAT_MPEG_III ) 
		{
			if(filtSIPtr->comp.init == TRUE)
			{
			   	SoundConverterEndConversion(filtSIPtr->comp.sc, filtSIPtr->comp.outputPtr, 
    				&(filtSIPtr->comp.outputFrames), &(filtSIPtr->comp.outputBytes));
    			TermAppleCompression(filtSIPtr);
    		}
    	}
	}
 	// invalidate these pointers to remove alias from real soundfile struct
	RemoveSignalDisplays();
 	inSIPtr = filtSIPtr = outSIPtr = outLSIPtr = outRSIPtr = outSteadSIPtr = outTransSIPtr =  nil;
	gProcessEnabled = NO_PROCESS;
	gStopProcess = false;
	UpdateInfoWindowFileType();
	DrawMenuBar();
	if(gDone == PENDING)
		gDone = true;
	SelectWindow(lastSIPtr->infoWindow);
	MenuUpdate();
	if(gPreferences.procPlay && playSIPtr)
	{
	
		StopPlay(false);			// Stop any playing file*/
		SetMenuItemText(gFileMenu, PLAY_ITEM, "\pStop Play (space)");
		StartPlay(playSIPtr, 0.0, 0.0);
		gProcessEnabled = PLAY_PROCESS;
		MenuUpdate();
	}
	return(1);
}

void
RemoveSignalDisplays(void)
{
	CheckItem(gSigDispMenu, DISP_INPUT_ITEM, false);
	CheckItem(gSigDispMenu, DISP_AUX_ITEM, false);
	CheckItem(gSigDispMenu, DISP_OUTPUT_ITEM, false);
	CheckItem(gSonoDispMenu, DISP_INPUT_ITEM, false);
	CheckItem(gSonoDispMenu, DISP_AUX_ITEM, false);
	CheckItem(gSonoDispMenu, DISP_OUTPUT_ITEM, false);
	CheckItem(gSpectDispMenu, DISP_INPUT_ITEM, false);
	CheckItem(gSpectDispMenu, DISP_AUX_ITEM, false);
	CheckItem(gSpectDispMenu, DISP_OUTPUT_ITEM, false);
	if(inSIPtr != nil)
	{
		if(inSIPtr->sigDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(inSIPtr->sigDisp));
		if(inSIPtr->sonoDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(inSIPtr->sonoDisp));
		if(inSIPtr->spectDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(inSIPtr->spectDisp));
	}
	if(filtSIPtr != nil)
	{
		if(filtSIPtr->sigDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(filtSIPtr->sigDisp));
		if(filtSIPtr->sonoDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(filtSIPtr->sonoDisp));
		if(filtSIPtr->spectDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(filtSIPtr->spectDisp));
	}
	if(outSIPtr != nil)
	{
		if(outSIPtr->sigDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outSIPtr->sigDisp));
		if(outSIPtr->sonoDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outSIPtr->sonoDisp));
		if(outSIPtr->spectDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outSIPtr->spectDisp));
	}
	if(outLSIPtr != nil)
	{
		if(outLSIPtr->sigDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outLSIPtr->sigDisp));
		if(outLSIPtr->sonoDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outLSIPtr->sonoDisp));
		if(outLSIPtr->spectDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outLSIPtr->spectDisp));
	}
	if(outRSIPtr != nil)
	{
		if(outRSIPtr->sigDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outRSIPtr->sigDisp));
		if(outRSIPtr->sonoDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outRSIPtr->sonoDisp));
		if(outRSIPtr->spectDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outRSIPtr->spectDisp));
	}
	if(outSteadSIPtr != nil)
	{
		if(outSteadSIPtr->sigDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outSteadSIPtr->sigDisp));
		if(outSteadSIPtr->sonoDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outSteadSIPtr->sonoDisp));
		if(outSteadSIPtr->spectDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outSteadSIPtr->spectDisp));
	}
	if(outTransSIPtr != nil)
	{
		if(outTransSIPtr->sigDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outTransSIPtr->sigDisp));
		if(outTransSIPtr->sonoDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outTransSIPtr->sonoDisp));
		if(outTransSIPtr->spectDisp.windo != (WindowPtr)(-1L))
			DisposeWindWorld(&(outTransSIPtr->spectDisp));
	}
}

void
DisposeWindWorld(SoundDisp *disp)
{
	if(disp->windo != (WindowPtr)(-1L))
	{
		DisposeWindow(disp->windo);
		disp->windo = (WindowPtr)(-1L);
	}
	if(disp->offScreen)
		DisposeGWorld(disp->offScreen);
}

void
TerminateSoundHack(void)
{
	if(gProcessEnabled != NO_PROCESS)
	{
		gStopProcess = true;
		gDone = PENDING;
	}
	else
		gDone = true;
	TerminatePlay();
	SavePreferences();
}