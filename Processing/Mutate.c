#include <math.h>
#include <stdlib.h>
//#include <AppleEvents.h>
#include "SoundFile.h"
#include "OpenSoundFile.h"
#include "PhaseVocoder.h"
#include "CreateSoundFile.h"
#include "WriteHeader.h"
#include "SoundHack.h"
#include "Menu.h"
#include "Dialog.h"
#include "Misc.h"
#include "Mutate.h"
#include "Analysis.h"
#include "SpectFileIO.h"
#include "FFT.h"
#include "Windows.h"
#include "DrawFunction.h"
#include "ShowSpect.h"
#include "ShowSono.h"
#include "CarbonGlue.h"

extern MenuHandle	gAppleMenu, gFileMenu, gEditMenu, gProcessMenu,
					gControlMenu, gBandsMenu, gMutateMenu, gSigDispMenu, 
					gSpectDispMenu, gSonoDispMenu;
extern SoundInfoPtr	inSIPtr, filtSIPtr, outSIPtr, frontSIPtr;
extern DialogPtr	gDrawFunctionDialog;
DialogPtr	gMutateDialog;

extern short			gProcessEnabled, gProcessDisabled, gStopProcess, gProcNChans;
extern float		gScale, gScaleL, gScaleR, Pi, twoPi, gScaleDivisor;
extern long			gNumberBlocks;
extern Boolean		gNormalize;
extern FSSpec		nilFSSpec;

extern float		*gFunction;

extern long		gInPointer, gOutPointer, gInPosition;
extern float 	*analysisWindow, *synthesisWindow, *inputL, *inputR,
				*outputL, *outputR, *displaySpectrum, *lastPhaseInL, *lastPhaseInR;
extern struct
{
	Str255	editorName;
	long	editorID;
	short	openPlay;
	short	procPlay;
	short	defaultType;
	short	defaultFormat;
}	gPreferences;

long		gFInPointer, gFInPosition;
float		*targetInputL, *targetInputR, *targetCartSpectrum, *cartSpectrum,
			*sourceSpectrum, *targetSpectrum,
			*sourcejSpectrumL, *targetjSpectrumL, *mutantjSpectrumL,
			*sourcejSpectrumR, *targetjSpectrumR, *mutantjSpectrumR;
Boolean		*mutateDecisionA, *mutateDecisionB, *smallDecisionA, *smallDecisionB;
MutateInfo 	gMI;
PvocInfo	gMutPI;

void
HandleMutateDialog()
{
	WindInfo *theWindInfo;
	
	gMutateDialog = GetNewDialog(MUTATE_DLOG,NIL_POINTER,(WindowPtr)MOVE_TO_FRONT);

	theWindInfo = (WindInfo *)NewPtr(sizeof(WindInfo));
	theWindInfo->windType = PROCWIND;
	theWindInfo->structPtr = (Ptr)MUTATE_DLOG;

#if TARGET_API_MAC_CARBON == 1
	SetWRefCon(GetDialogWindow(gMutateDialog), (long)theWindInfo);
#else
	SetWRefCon(gMutateDialog, (long)theWindInfo);
#endif

	gProcessDisabled = FUDGE_PROCESS;
	// Rename Display MENUs
	SetMenuItemText(gSigDispMenu, DISP_INPUT_ITEM, "\pSource");
	SetMenuItemText(gSpectDispMenu, DISP_INPUT_ITEM, "\pSource");
	SetMenuItemText(gSonoDispMenu, DISP_INPUT_ITEM, "\pSource");
	SetMenuItemText(gSigDispMenu, DISP_AUX_ITEM, "\pTarget");
	SetMenuItemText(gSpectDispMenu, DISP_AUX_ITEM, "\pTarget");
	SetMenuItemText(gSonoDispMenu, DISP_AUX_ITEM, "\pTarget");
	SetMenuItemText(gSigDispMenu, DISP_OUTPUT_ITEM, "\pMutant");
	SetMenuItemText(gSpectDispMenu, DISP_OUTPUT_ITEM, "\pMutant");
	SetMenuItemText(gSonoDispMenu, DISP_OUTPUT_ITEM, "\pMutant");
	
	EnableItem(gSigDispMenu, DISP_INPUT_ITEM);
	EnableItem(gSpectDispMenu, DISP_INPUT_ITEM);
	EnableItem(gSonoDispMenu, DISP_INPUT_ITEM);
	EnableItem(gSigDispMenu, DISP_AUX_ITEM);
	EnableItem(gSpectDispMenu, DISP_AUX_ITEM);
	EnableItem(gSonoDispMenu, DISP_AUX_ITEM);
	EnableItem(gSigDispMenu, DISP_OUTPUT_ITEM);
	EnableItem(gSpectDispMenu, DISP_OUTPUT_ITEM);
	EnableItem(gSonoDispMenu, DISP_OUTPUT_ITEM);
	MenuUpdate();
		
	SetMutateDefaults();
#if TARGET_API_MAC_CARBON == 1
	ShowWindow(GetDialogWindow(gMutateDialog));
	SelectWindow(GetDialogWindow(gMutateDialog));
#else
	ShowWindow(gMutateDialog);
	SelectWindow(gMutateDialog);
#endif
}

void
HandleMutateDialogEvent(short itemHit)
{
	double	windowFactor = 1.0, inLength, tmpFloat;
	short		iteration;
	short	itemType; 
	static short	choiceBand = 4, choiceType;
	Boolean	okToOK;
	Rect	itemRect, dialogRect;
	Handle	itemHandle;
	Str255	tmpStr;
	WindInfo	*myWI;
	
	okToOK = FALSE;
	if(filtSIPtr != NULL)
	{
		if(filtSIPtr->dFileNum != -1)
			okToOK = TRUE;
	}
	
#if TARGET_API_MAC_CARBON == 1
	GetPortBounds(GetDialogPort(gMutateDialog), &dialogRect);
#else
	dialogRect = gMutateDialog->portRect;
#endif
	inLength = inSIPtr->numBytes/(inSIPtr->sRate * inSIPtr->nChans * inSIPtr->frameSize);

	switch(itemHit)
	{
		case M_BANDS_ITEM:
			GetDialogItem(gMutateDialog, M_BANDS_ITEM, &itemType, &itemHandle, &itemRect);
			choiceBand = GetControlValue((ControlHandle)itemHandle);
			gMutPI.points = 2<<(13 - choiceBand);
			break;
		case M_TYPE_ITEM:
			GetDialogItem(gMutateDialog, M_TYPE_ITEM, &itemType, &itemHandle, &itemRect);
			gMI.type = GetControlValue((ControlHandle)itemHandle);
			switch(gMI.type)
			{
				case USIM:
				case UUIM:
					HideDialogItem(gMutateDialog, M_BPERSI_FIELD);
					HideDialogItem(gMutateDialog, M_BPERSI_TEXT);
					break;
				case ISIM:
				case IUIM:
				case LCM:
				case LCMIUIM:
				case LCMUUIM:
					ShowDialogItem(gMutateDialog, M_BPERSI_FIELD);
					ShowDialogItem(gMutateDialog, M_BPERSI_TEXT);
					FixToString(gMI.bandPersistence, tmpStr);
					GetDialogItem(gMutateDialog, M_BPERSI_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpStr);
					break;
			}
			break;
		case M_OMEGA_FIELD:
			GetDialogItem(gMutateDialog, M_OMEGA_FIELD, &itemType, &itemHandle, &itemRect);
			GetDialogItemText(itemHandle, tmpStr);
			StringToFix(tmpStr,&tmpFloat);
			gMI.omega = tmpFloat;
			if(gMI.omega < 0.0)
				gMI.omega = 0.0;
			if(gMI.omega > 1.0)
				gMI.omega = 1.0;
			break;
		case M_FUNC_BOX:
			GetDialogItem(gMutateDialog, M_FUNC_BOX, &itemType, &itemHandle, &itemRect);
			if(GetControlValue((ControlHandle)itemHandle) == TRUE)
			{
				SetControlValue((ControlHandle)itemHandle,OFF);
				ShowDialogItem(gMutateDialog, M_OMEGA_FIELD);
				HideDialogItem(gMutateDialog, M_FUNC_BUTTON);
				gMutPI.useFunction = FALSE;
			} else
			{
				SetControlValue((ControlHandle)itemHandle,ON);
				for(iteration = 0; iteration < 400; iteration++)
					gFunction[iteration] = 0.5;
				HideDialogItem(gMutateDialog, M_OMEGA_FIELD);
				ShowDialogItem(gMutateDialog, M_FUNC_BUTTON);
				gMutPI.useFunction = TRUE;
			}
			break;
		case M_FUNC_BUTTON:
			HandleDrawFunctionDialog(1.0, 0.0, 1.0, "\p½:", inLength, FALSE);
			break;
		case M_BPERSI_FIELD:
			GetDialogItem(gMutateDialog, M_BPERSI_FIELD, &itemType, &itemHandle, &itemRect);
			GetDialogItemText(itemHandle, tmpStr);
			StringToFix(tmpStr,&tmpFloat);
			if(tmpFloat < 0.0)
				tmpFloat = 0.0;
			if(tmpFloat > 1.0)
				tmpFloat = 1.0;
			gMI.bandPersistence = tmpFloat;
			break;
		case M_EMPHA_FIELD:
			GetDialogItem(gMutateDialog, M_EMPHA_FIELD, &itemType, &itemHandle, &itemRect);
			GetDialogItemText(itemHandle, tmpStr);
			StringToFix(tmpStr,&tmpFloat);
			if(tmpFloat < -1.0)
				tmpFloat = -1.0;
				if(tmpFloat > 1.0)
					tmpFloat = 1.0;
				gMI.deltaEmphasis = tmpFloat;
				break;
			case M_SAV_FIELD:
				GetDialogItem(gMutateDialog, M_SAV_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpStr);
				StringToFix(tmpStr,&tmpFloat);
				if(tmpFloat < 0.0)
					tmpFloat = 0.0;
				if(tmpFloat > 1.0)
					tmpFloat = 1.0;
					gMI.sourceAbsoluteValue = tmpFloat;
				break;
			case M_TAV_FIELD:
				GetDialogItem(gMutateDialog, M_TAV_FIELD, &itemType, &itemHandle, &itemRect);
				GetDialogItemText(itemHandle, tmpStr);
				StringToFix(tmpStr,&tmpFloat);
				if(tmpFloat < 0.0)
					tmpFloat = 0.0;
				if(tmpFloat > 1.0)
					tmpFloat = 1.0;
				gMI.targetAbsoluteValue = tmpFloat;
				break;
			case M_ABSO_BOX:
				GetDialogItem(gMutateDialog, M_ABSO_BOX, &itemType, &itemHandle, &itemRect);
				if(GetControlValue((ControlHandle)itemHandle) == TRUE)
				{
					SetControlValue((ControlHandle)itemHandle,OFF);
					HideDialogItem(gMutateDialog, M_SAV_FIELD);
					HideDialogItem(gMutateDialog, M_SAV_TEXT);
					HideDialogItem(gMutateDialog, M_TAV_FIELD);
					HideDialogItem(gMutateDialog, M_TAV_TEXT);
					ShowDialogItem(gMutateDialog, M_EMPHA_FIELD);
					ShowDialogItem(gMutateDialog, M_EMPHA_TEXT);
					gMI.absolute = FALSE;
				}
				else
				{
					SetControlValue((ControlHandle)itemHandle,ON);
					HideDialogItem(gMutateDialog, M_EMPHA_FIELD);
					HideDialogItem(gMutateDialog, M_EMPHA_TEXT);
					FixToString(gMI.sourceAbsoluteValue, tmpStr);
					GetDialogItem(gMutateDialog, M_SAV_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpStr);
					ShowDialogItem(gMutateDialog, M_SAV_FIELD);
					ShowDialogItem(gMutateDialog, M_SAV_TEXT);
					FixToString(gMI.targetAbsoluteValue, tmpStr);
					GetDialogItem(gMutateDialog, M_TAV_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle, tmpStr);
					ShowDialogItem(gMutateDialog, M_TAV_FIELD);
					ShowDialogItem(gMutateDialog, M_TAV_TEXT);
					gMI.absolute = TRUE;
				}
				break;
			case M_SCALE_BOX:
				GetDialogItem(gMutateDialog, M_SCALE_BOX, &itemType, &itemHandle, &itemRect);
				if(GetControlValue((ControlHandle)itemHandle) == TRUE)
				{
					SetControlValue((ControlHandle)itemHandle,OFF);
					gMI.scale = FALSE;
				} else
				{
					SetControlValue((ControlHandle)itemHandle,ON);
					gMI.scale = TRUE;
				}
				break;
			case M_FILE_BUTTON:
				if(OpenSoundFile(nilFSSpec, FALSE) != -1)
				{
					if(frontSIPtr == inSIPtr)
					{
						DrawErrorMessage("\pYou can't use the source as the target.");
						break;
					}
					if((frontSIPtr->sfType == CS_PVOC || frontSIPtr->sfType == PICT) && (frontSIPtr->spectFrameSize != gMutPI.points))
					{
						DrawErrorMessage("\pIf the target is a spectral file, it must have the same number of bands as set.");
						break;
					}
					filtSIPtr = frontSIPtr;
					GetDialogItem(gMutateDialog, M_FILE_FIELD, &itemType, &itemHandle, &itemRect);
					SetDialogItemText(itemHandle,filtSIPtr->sfSpec.name);
					
					GetDialogItem(gMutateDialog, M_PROCESS_BUTTON, &itemType, &itemHandle, &itemRect);
					HiliteControl((ControlHandle)itemHandle, 0);
					if(inSIPtr->nChans == STEREO || filtSIPtr->nChans == STEREO)
						gProcNChans = STEREO;
					else
						gProcNChans = MONO;
					okToOK = TRUE;
				}
				break;
			case M_CANCEL_BUTTON:
				if(gProcessEnabled == DRAW_PROCESS)
				{
#if TARGET_API_MAC_CARBON == 1
					HideWindow(GetDialogWindow(gDrawFunctionDialog));
#else
					HideWindow(gDrawFunctionDialog);
#endif
					gProcessEnabled = NO_PROCESS;
				}
#if TARGET_API_MAC_CARBON == 1
				myWI = (WindInfoPtr)GetWRefCon(GetDialogWindow(gMutateDialog));
#else
				myWI = (WindInfoPtr)GetWRefCon(gMutateDialog);
#endif
				RemovePtr((Ptr)myWI);
				DisposeDialog(gMutateDialog);
				gMutateDialog = nil;
				gProcessDisabled = gProcessEnabled = NO_PROCESS;
				inSIPtr = nil;
				MenuUpdate();
				break;
			case M_PROCESS_BUTTON:
				if(okToOK)
				{
					if(gProcessEnabled == DRAW_PROCESS)
					{
#if TARGET_API_MAC_CARBON == 1
					HideWindow(GetDialogWindow(gDrawFunctionDialog));
#else
					HideWindow(gDrawFunctionDialog);
#endif
						gProcessEnabled = NO_PROCESS;
					}
#if TARGET_API_MAC_CARBON == 1
					myWI = (WindInfoPtr)GetWRefCon(GetDialogWindow(gMutateDialog));
#else
					myWI = (WindInfoPtr)GetWRefCon(gMutateDialog);
#endif
					RemovePtr((Ptr)myWI);
					DisposeDialog(gMutateDialog);
					gMutateDialog = nil;
					InitMutateProcess();
				}
				break;
		}
}

void
SetMutateDefaults()
{
	short	itemType, iteration;
	Rect	itemRect;
	Handle	itemHandle;
	double	tmpDouble;
	float	tmpFloat;
	long	tmpLong;
	Str255	tmpStr;

	gNormalize = FALSE;
	gScaleDivisor = 1.0;
	
	for(iteration = 0; iteration < 400; iteration++)
		gFunction[iteration] = 0.5;

	if(filtSIPtr != nil)
		filtSIPtr = nil;

#if TARGET_API_MAC_CARBON == 1
	SetPort(GetDialogPort(gMutateDialog));
#else
	SetPort((GrafPtr)gMutateDialog);
#endif
	if(inSIPtr->sfType == CS_PVOC || inSIPtr->sfType == PICT)
	{
		gMutPI.points = inSIPtr->spectFrameSize;
		gMutPI.decimation = gMutPI.interpolation = inSIPtr->spectFrameIncr;
		GetDialogItem(gMutateDialog, M_BANDS_ITEM, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle,255);
	}
	else
	{
		gMutPI.decimation = gMutPI.interpolation = gMutPI.points >> 2;
		GetDialogItem(gMutateDialog, M_BANDS_ITEM, &itemType, &itemHandle, &itemRect);
		HiliteControl((ControlHandle)itemHandle,0);
	}
	
	GetDialogItem(gMutateDialog, M_PROCESS_BUTTON, &itemType, &itemHandle, &itemRect);
	HiliteControl((ControlHandle)itemHandle, 255);
	
	switch(gMI.type)
	{
		case USIM:
		case UUIM:
			HideDialogItem(gMutateDialog, M_BPERSI_FIELD);
			HideDialogItem(gMutateDialog, M_BPERSI_TEXT);
			break;
		case ISIM:
		case IUIM:
		case LCM:
		case LCMIUIM:
		case LCMUUIM:
			ShowDialogItem(gMutateDialog, M_BPERSI_FIELD);
			ShowDialogItem(gMutateDialog, M_BPERSI_TEXT);
			break;
	}

	tmpDouble =	LLog2((double)gMutPI.points);
	tmpFloat = tmpDouble;
	tmpLong = (long)tmpFloat;	// 10 for 1024
	GetDialogItem(gMutateDialog, M_BANDS_ITEM, &itemType, &itemHandle, &itemRect);
	SetControlValue((ControlHandle)itemHandle, (14 - tmpLong));
	
	GetDialogItem(gMutateDialog, M_TYPE_ITEM, &itemType, &itemHandle, &itemRect);
	SetControlValue((ControlHandle)itemHandle, gMI.type);

	FixToString(gMI.omega, tmpStr);
	GetDialogItem(gMutateDialog, M_OMEGA_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpStr);
	
	if(gMutPI.useFunction == FALSE)
	{
		GetDialogItem(gMutateDialog, M_FUNC_BOX, &itemType, &itemHandle, &itemRect);
		SetControlValue((ControlHandle)itemHandle,OFF);
		ShowDialogItem(gMutateDialog, M_OMEGA_FIELD);
		HideDialogItem(gMutateDialog, M_FUNC_BUTTON);
	}
	else
	{
		GetDialogItem(gMutateDialog, M_FUNC_BOX, &itemType, &itemHandle, &itemRect);
		SetControlValue((ControlHandle)itemHandle,ON);
		HideDialogItem(gMutateDialog, M_OMEGA_FIELD);
		ShowDialogItem(gMutateDialog, M_FUNC_BUTTON);
	}
	if(gMI.absolute == TRUE)
	{
		GetDialogItem(gMutateDialog, M_ABSO_BOX, &itemType, &itemHandle, &itemRect);
		SetControlValue((ControlHandle)itemHandle,ON);
		HideDialogItem(gMutateDialog, M_EMPHA_TEXT);
		HideDialogItem(gMutateDialog, M_EMPHA_FIELD);
		ShowDialogItem(gMutateDialog, M_TAV_TEXT);
		ShowDialogItem(gMutateDialog, M_TAV_FIELD);
		ShowDialogItem(gMutateDialog, M_SAV_TEXT);
		ShowDialogItem(gMutateDialog, M_SAV_FIELD);
	}
	else
	{
		GetDialogItem(gMutateDialog, M_ABSO_BOX, &itemType, &itemHandle, &itemRect);
		SetControlValue((ControlHandle)itemHandle,OFF);
		ShowDialogItem(gMutateDialog, M_EMPHA_TEXT);
		ShowDialogItem(gMutateDialog, M_EMPHA_FIELD);
		HideDialogItem(gMutateDialog, M_TAV_TEXT);
		HideDialogItem(gMutateDialog, M_TAV_FIELD);
		HideDialogItem(gMutateDialog, M_SAV_TEXT);
		HideDialogItem(gMutateDialog, M_SAV_FIELD);
	}
	
	FixToString(gMI.sourceAbsoluteValue, tmpStr);
	GetDialogItem(gMutateDialog, M_SAV_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpStr);
	FixToString(gMI.targetAbsoluteValue, tmpStr);
	GetDialogItem(gMutateDialog, M_TAV_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpStr);
	
	GetDialogItem(gMutateDialog, M_SCALE_BOX, &itemType, &itemHandle, &itemRect);
	if(gMI.scale == FALSE)
		SetControlValue((ControlHandle)itemHandle,OFF);
	else
		SetControlValue((ControlHandle)itemHandle,ON);
	
	FixToString(gMI.bandPersistence, tmpStr);
	GetDialogItem(gMutateDialog, M_BPERSI_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpStr);
	FixToString(gMI.deltaEmphasis, tmpStr);
	GetDialogItem(gMutateDialog, M_EMPHA_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, tmpStr);
	GetDialogItem(gMutateDialog, M_FILE_FIELD, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, "\p");
}

short
InitMutateProcess()
{
	long	inSamples, filtSamples, n, sizeBlock, numSteps;
	float	ratio;
	double	numStepsFloat;	
	Str255	tmpStr, errStr;
	
	inSamples = inSIPtr->numBytes/(inSIPtr->frameSize*inSIPtr->nChans);
	filtSamples = filtSIPtr->numBytes/(filtSIPtr->frameSize*filtSIPtr->nChans);
	
	gMutPI.windowSize = gMutPI.points;
    gMutPI.halfPoints = gMutPI.points >> 1;
	
	if(gMI.scale == TRUE)
		ratio = (float)filtSamples/(float)inSamples;
	else
		ratio = 1.0;
	if(inSIPtr->sfType == CS_PVOC || inSIPtr->sfType == PICT)
	{
		gMutPI.interpolation = gMutPI.decimation = inSIPtr->spectFrameIncr;
		gMI.fDecimation = filtSIPtr->spectFrameIncr;
	}
	else
	{
		if(ratio <= 1.0)
		{
			gMutPI.decimation = gMutPI.interpolation = gMutPI.windowSize/8;
			gMI.fDecimation = (long)(gMutPI.interpolation * ratio);
		}
		else
		{
			gMI.fDecimation = gMutPI.windowSize/8;
			gMutPI.interpolation = gMutPI.decimation = (long)(gMI.fDecimation/ratio);
		}
	}
    
    if(gMI.fDecimation < gMutPI.decimation)
		sizeBlock = gMutPI.decimation;
	else
		sizeBlock = gMI.fDecimation;
		
	numStepsFloat = LLog2((double)(gMutPI.halfPoints+1));
	numSteps = (long)numStepsFloat * 3;

	outSIPtr = nil;
    outSIPtr = (SoundInfo *)NewPtr(sizeof(SoundInfo));
	if(gPreferences.defaultType == 0)
	{
		if(inSIPtr->sfType == CS_PVOC || inSIPtr->sfType == PICT)
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
	NameFile(inSIPtr->sfSpec.name, "\p", errStr);
	switch(gMI.type)
	{
		case ISIM:
			StringAppend(errStr, "\pISIM", tmpStr);
			break;
		case LCM:
			StringAppend(errStr, "\pLCM", tmpStr);
			break;
		case IUIM:
			StringAppend(errStr, "\pIUIM", tmpStr);
			break;
		case LCMIUIM:
			StringAppend(errStr, "\pLCIUIM", tmpStr);
			break;
		case LCMUUIM:
			StringAppend(errStr, "\pLCUUIM", tmpStr);
			break;
		case USIM:
			StringAppend(errStr, "\pUSIM", tmpStr);
			break;
		case UUIM:
			StringAppend(errStr, "\pUUIM", tmpStr);
			break;
	}
	NameFile(filtSIPtr->sfSpec.name, "\p", errStr);
	StringAppend(tmpStr, errStr, outSIPtr->sfSpec.name);
	outSIPtr->sRate = inSIPtr->sRate;
	outSIPtr->nChans = gProcNChans;
	outSIPtr->numBytes = 0;
	if(CreateSoundFile(&outSIPtr, SOUND_CUST_DIALOG) == -1)
	{
		gProcessDisabled = gProcessEnabled = NO_PROCESS;
		MenuUpdate();
		RemovePtr((Ptr)outSIPtr);
		outSIPtr = nil;
		return(-1);
	}
	WriteHeader(outSIPtr);
	UpdateInfoWindow(outSIPtr);
	SetOutputScale(outSIPtr->packMode);

//	Allocate memory
	
	smallDecisionA = smallDecisionB = mutateDecisionA = mutateDecisionB = nil;
	
	analysisWindow = synthesisWindow = inputL = cartSpectrum = nil;
	sourceSpectrum = sourcejSpectrumL = targetInputL = targetCartSpectrum = nil;
	targetSpectrum = targetjSpectrumL = mutantjSpectrumL = outputL = displaySpectrum = nil;
	sourcejSpectrumR = inputR = targetInputR = targetjSpectrumR = mutantjSpectrumR = nil;
	outputR = nil;

	smallDecisionA = (Boolean *)NewPtr(numSteps * sizeof(Boolean));
	smallDecisionB = (Boolean *)NewPtr(numSteps * sizeof(Boolean));
	
	mutateDecisionA = (Boolean *)NewPtr((gMutPI.halfPoints+1) * sizeof(Boolean));
	mutateDecisionB = (Boolean *)NewPtr((gMutPI.halfPoints+1) * sizeof(Boolean));
	
	analysisWindow = (float *)NewPtr(gMutPI.windowSize*sizeof(float));
	synthesisWindow = (float *)NewPtr(gMutPI.windowSize*sizeof(float));
	if(inSIPtr->sfType == CS_PVOC || inSIPtr->sfType == PICT)
	{
		lastPhaseInL = (float *)NewPtr((gMutPI.halfPoints+1) * sizeof(float));
		ZeroFloatTable(lastPhaseInL, (gMutPI.halfPoints+1));
		if(inSIPtr->nChans == STEREO)
		{
			lastPhaseInR = (float *)NewPtr((gMutPI.halfPoints+1) * sizeof(float));
			ZeroFloatTable(lastPhaseInR, (gMutPI.halfPoints+1));
		}
	}
	
	inputL = (float *)NewPtr(gMutPI.windowSize*sizeof(float));
	ZeroFloatTable(inputL, gMutPI.windowSize);
	cartSpectrum = (float *)NewPtr(gMutPI.points*sizeof(float));
	ZeroFloatTable(cartSpectrum, gMutPI.points);
	sourceSpectrum = (float *)NewPtr((gMutPI.points+2)*sizeof(float));
	ZeroFloatTable(sourceSpectrum, (gMutPI.points+2));
	sourcejSpectrumL = (float *)NewPtr((gMutPI.points+2)*sizeof(float));
	ZeroFloatTable(sourcejSpectrumL, (gMutPI.points+2));

	targetInputL = (float *)NewPtr(gMutPI.windowSize*sizeof(float));
	ZeroFloatTable(targetInputL, gMutPI.windowSize);
	targetCartSpectrum = (float *)NewPtr(gMutPI.points*sizeof(float));
	ZeroFloatTable(targetCartSpectrum, gMutPI.points);
	targetSpectrum = (float *)NewPtr((gMutPI.points+2)*sizeof(float));
	ZeroFloatTable(targetSpectrum, (gMutPI.points+2));
	targetjSpectrumL = (float *)NewPtr((gMutPI.points+2)*sizeof(float));
	ZeroFloatTable(targetjSpectrumL, (gMutPI.points+2));

	mutantjSpectrumL = (float *)NewPtr((gMutPI.points+2)*sizeof(float));
	ZeroFloatTable(mutantjSpectrumL, (gMutPI.points+2));
	outputL = (float *)NewPtr(gMutPI.windowSize*sizeof(float));
	ZeroFloatTable(outputL, gMutPI.windowSize);
	
	displaySpectrum = (float *)NewPtr((gMutPI.halfPoints+1)*sizeof(float));	/* For display of amplitude */

	if(inSIPtr->nChans == STEREO)
	{
		sourcejSpectrumR = (float *)NewPtr((gMutPI.points+2)*sizeof(float));
		ZeroFloatTable(sourcejSpectrumR, (gMutPI.points+2));
		inputR = (float *)NewPtr(gMutPI.windowSize*sizeof(float));
		ZeroFloatTable(inputR, gMutPI.windowSize);
	}
	else
	{
		sourcejSpectrumR = sourcejSpectrumL;
		inputR = inputL;
	}
	
	if(filtSIPtr->nChans == STEREO)
	{
		targetInputR = (float *)NewPtr(gMutPI.windowSize*sizeof(float));
		ZeroFloatTable(targetInputR, gMutPI.windowSize);
		targetjSpectrumR = (float *)NewPtr((gMutPI.points+2)*sizeof(float));
		ZeroFloatTable(targetjSpectrumR, (gMutPI.points+2));
	}
	else
	{
		targetInputR = targetInputL;
		targetjSpectrumR = targetjSpectrumL;
	}
	
	if(gProcNChans == STEREO)
	{
		mutantjSpectrumR = (float *)NewPtr((gMutPI.points+2)*sizeof(float));
		ZeroFloatTable(mutantjSpectrumR, (gMutPI.points+2));
		outputR = (float *)NewPtr(gMutPI.windowSize*sizeof(float));
		ZeroFloatTable(outputR, gMutPI.windowSize);
	}
	else
	{
		mutantjSpectrumR = mutantjSpectrumL;
		outputR = outputL;
	}
	
	
	if(outputL == 0 || mutantjSpectrumL == 0)
	{
		DrawErrorMessage("\pNot enough memory for processing");
		gProcessDisabled = gProcessEnabled = NO_PROCESS;
		MenuUpdate();
		DeAllocMutateMem();
		outSIPtr = nil;
		return(-1);
	}
	
	for(n = 0; n < (gMutPI.halfPoints+1); n++)
		mutateDecisionA[n] = mutateDecisionB[n] = FALSE;
	for(n = 0; n < numSteps; n++)
		smallDecisionA[n] = smallDecisionB[n] = FALSE;

	UpdateProcessWindow("\p", "\p", "\pmutating files", 0.0);
	
 	HammingWindow(analysisWindow, gMutPI.windowSize);
 	HammingWindow(synthesisWindow, gMutPI.windowSize);
    ScaleWindows(analysisWindow, synthesisWindow, gMutPI);

    gFInPointer = gOutPointer = gInPointer = -gMutPI.windowSize;
	gInPosition	= gFInPosition	= 0;
 	if(gMutPI.useFunction)
 	{
 		gMI.omega = InterpFunctionValue(0L, FALSE);
		FixToString(gMI.omega, errStr);	
		StringAppend("\pmutating files, index: ", errStr, tmpStr);
		UpdateProcessWindow("\p", "\p", tmpStr, 0.0);
	}

	gNumberBlocks = 0;	
	gProcessEnabled = MUTATE_PROCESS;
	gProcessDisabled = NO_PROCESS;
	MenuUpdate();
	return(0);
}

/*
 * main loop--perform phase vocoder analysis-resynthesis
 */
short
MutateBlock()
{
    short	curMark;
	long	numSampsRead, numTargetRead, pointer;
	float 	length;
	Str255	errStr, tmpStr;
	static long validSamples = -1L, targetValidSamples = -1L;
	static long	sOutPos;
	double	seconds;
	

	if(gStopProcess == TRUE)
	{
		DeAllocMutateMem();
		FinishProcess();
		gNumberBlocks = 0;
		return(-1);
	}
	if(gNumberBlocks == 0)
		sOutPos = outSIPtr->dataStart;
	SetFPos(outSIPtr->dFileNum, fsFromStart, sOutPos);

/*
 * increment times
 */
	gFInPointer += gMI.fDecimation;
	gInPointer += gMutPI.decimation;
	gOutPointer += gMutPI.interpolation;
	
/*
 * analysis for source: input gMutPI.decimation samples; window, fold and
 * rotate input samples into FFT buffer; take FFT; and convert to
 * amplitude-frequency (phase vocoder) form
 */
	if(inSIPtr->sfType == CS_PVOC || inSIPtr->sfType == PICT)
	{
		if(inSIPtr->packMode == SF_FORMAT_SPECT_AMPFRQ)
			numSampsRead = ReadCSAData(inSIPtr, sourceSpectrum, lastPhaseInL, gMutPI.halfPoints);
		else if(inSIPtr->packMode == SF_FORMAT_SPECT_AMPPHS)
			numSampsRead = ReadSHAData(inSIPtr, sourceSpectrum, LEFT);
		else if(inSIPtr->packMode == SF_FORMAT_SPECT_COMPLEX)
		{
			numSampsRead = ReadSDIFData(inSIPtr, cartSpectrum, sourceSpectrum, &seconds, LEFT);
			CartToPolar(cartSpectrum, sourceSpectrum, gMutPI.halfPoints);
		}
	}
	else
	{
		SetFPos(inSIPtr->dFileNum, fsFromStart, (inSIPtr->dataStart + gInPosition));
		numSampsRead = ShiftIn(inSIPtr, inputL, inputR, gMutPI.windowSize, gMutPI.decimation, &validSamples);
		WindowFold(inputL, analysisWindow, cartSpectrum, gInPointer, gMutPI.points, gMutPI.windowSize);
		RealFFT(cartSpectrum, gMutPI.halfPoints, TIME2FREQ);
		CartToPolar(cartSpectrum, sourceSpectrum, gMutPI.halfPoints);
	}
	if(numSampsRead > 0)
	{
		length = (numSampsRead + (gInPosition/(inSIPtr->nChans*inSIPtr->frameSize)))/inSIPtr->sRate;
		HMSTimeString(length, errStr);
		length = length / (inSIPtr->numBytes/(inSIPtr->sRate * inSIPtr->nChans * inSIPtr->frameSize));
		UpdateProcessWindow(errStr, "\p", "\p", length);
	}
	
	GetItemMark(gSpectDispMenu, DISP_INPUT_ITEM, &curMark);
	if(curMark != noMark)
		DisplaySpectrum(sourceSpectrum, displaySpectrum, gMutPI.halfPoints, inSIPtr, LEFT, 1.0, "\pSource Channel 1");
	GetItemMark(gSonoDispMenu, DISP_INPUT_ITEM, &curMark);
	if(curMark != noMark)
		DisplaySonogram(sourceSpectrum, displaySpectrum, gMutPI.halfPoints, inSIPtr, LEFT, 1.0, "\pSource Channel 1");
	
	if(filtSIPtr->sfType == CS_PVOC || filtSIPtr->sfType == PICT || filtSIPtr->sfType == SDIFF)
	{
		if(filtSIPtr->packMode == SF_FORMAT_SPECT_AMPFRQ)
			numSampsRead = ReadCSAData(filtSIPtr, targetSpectrum, lastPhaseInL, gMutPI.halfPoints);
		else if(filtSIPtr->packMode == SF_FORMAT_SPECT_AMPPHS)
			numSampsRead = ReadSHAData(filtSIPtr, targetSpectrum, LEFT);
		else if(filtSIPtr->packMode == SF_FORMAT_SPECT_COMPLEX)
		{
			numSampsRead = ReadSDIFData(filtSIPtr, targetCartSpectrum, targetSpectrum, &seconds, LEFT);
			CartToPolar(targetCartSpectrum, targetSpectrum, gMutPI.halfPoints);
		}
	}
	else
	{
		SetFPos(filtSIPtr->dFileNum, fsFromStart, (filtSIPtr->dataStart + gFInPosition));
		numTargetRead = ShiftIn(filtSIPtr, targetInputL, targetInputR, gMutPI.windowSize, gMI.fDecimation, &targetValidSamples);
		WindowFold(targetInputL, analysisWindow, targetCartSpectrum, gFInPointer, gMutPI.points, gMutPI.windowSize);
		RealFFT(targetCartSpectrum, gMutPI.halfPoints, TIME2FREQ);
		CartToPolar(targetCartSpectrum, targetSpectrum, gMutPI.halfPoints);
	}
	GetItemMark(gSpectDispMenu, DISP_AUX_ITEM, &curMark);
	if(curMark != noMark)
		DisplaySpectrum(targetSpectrum, displaySpectrum, gMutPI.halfPoints, filtSIPtr, LEFT, 1.0, "\pTarget Channel 1");
	GetItemMark(gSonoDispMenu, DISP_AUX_ITEM, &curMark);
	if(curMark != noMark)
		DisplaySonogram(targetSpectrum, displaySpectrum, gMutPI.halfPoints, filtSIPtr, LEFT, 1.0, "\pTarget Channel 1");

	MutateSpectrum((short)1, sourceSpectrum, sourcejSpectrumL, targetSpectrum, targetjSpectrumL, mutantjSpectrumL, gMI.omega);
	GetItemMark(gSpectDispMenu, DISP_OUTPUT_ITEM, &curMark);
	if(curMark != noMark)
		DisplaySpectrum(mutantjSpectrumL, displaySpectrum, gMutPI.halfPoints, outSIPtr, LEFT, 1.0, "\pMutant Channel 1");
	GetItemMark(gSonoDispMenu, DISP_OUTPUT_ITEM, &curMark);
	if(curMark != noMark)
		DisplaySonogram(mutantjSpectrumL, displaySpectrum, gMutPI.halfPoints, outSIPtr, LEFT, 1.0, "\pMutant Channel 1");
	
	PolarToCart(mutantjSpectrumL, cartSpectrum, gMutPI.halfPoints);
	RealFFT(cartSpectrum, gMutPI.halfPoints, FREQ2TIME);
	OverlapAdd(cartSpectrum, synthesisWindow, outputL, gOutPointer, gMutPI.points, gMutPI.windowSize);

	// Produce a new sourceSpectrum only if source is stereo
    if(inSIPtr->nChans == STEREO)
    {
		if(inSIPtr->sfType == CS_PVOC || inSIPtr->sfType == PICT || inSIPtr->sfType == SDIFF)
		{
			if(inSIPtr->packMode == SF_FORMAT_SPECT_AMPFRQ)
				numSampsRead = ReadCSAData(inSIPtr, sourceSpectrum, lastPhaseInR, gMutPI.halfPoints);
			else if(inSIPtr->packMode == SF_FORMAT_SPECT_AMPPHS)
				numSampsRead = ReadSHAData(inSIPtr, sourceSpectrum, RIGHT);
			else if(inSIPtr->packMode == SF_FORMAT_SPECT_COMPLEX)
			{
				numSampsRead = ReadSDIFData(inSIPtr, cartSpectrum, sourceSpectrum, &seconds, RIGHT);
				CartToPolar(cartSpectrum, sourceSpectrum, gMutPI.halfPoints);
			}
		}
		else
		{
			WindowFold(inputR, analysisWindow, cartSpectrum, gInPointer, gMutPI.points, gMutPI.windowSize);
			RealFFT(cartSpectrum, gMutPI.halfPoints, TIME2FREQ);
			CartToPolar(cartSpectrum, sourceSpectrum, gMutPI.halfPoints);
		}
		GetItemMark(gSpectDispMenu, DISP_INPUT_ITEM, &curMark);
		if(curMark != noMark)
			DisplaySpectrum(sourceSpectrum, displaySpectrum, gMutPI.halfPoints, inSIPtr, RIGHT, 1.0, "\pSource Channel 2");
		GetItemMark(gSonoDispMenu, DISP_INPUT_ITEM, &curMark);
		if(curMark != noMark)
			DisplaySonogram(sourceSpectrum, displaySpectrum, gMutPI.halfPoints, inSIPtr, RIGHT, 1.0, "\pSource Channel 2");
	}
	
	// Produce a new targetSpectrum only if target is stereo
	if(filtSIPtr->nChans == STEREO)
    {
		if(filtSIPtr->sfType == CS_PVOC || filtSIPtr->sfType == PICT)
		{
			if(filtSIPtr->packMode == SF_FORMAT_SPECT_AMPFRQ)
				numSampsRead = ReadCSAData(filtSIPtr, targetSpectrum, lastPhaseInR, gMutPI.halfPoints);
			else if(filtSIPtr->packMode == SF_FORMAT_SPECT_AMPPHS)
				numSampsRead = ReadSHAData(filtSIPtr, targetSpectrum, RIGHT);
			else if(filtSIPtr->packMode == SF_FORMAT_SPECT_COMPLEX)
			{
				numSampsRead = ReadSDIFData(filtSIPtr, targetCartSpectrum, targetSpectrum, &seconds, RIGHT);
				CartToPolar(targetCartSpectrum, targetSpectrum, gMutPI.halfPoints);
			}
		}
		else
		{
			WindowFold(targetInputR, analysisWindow, targetCartSpectrum, gFInPointer, gMutPI.points, gMutPI.windowSize);
			RealFFT(targetCartSpectrum, gMutPI.halfPoints, TIME2FREQ);
			CartToPolar(targetCartSpectrum, targetSpectrum, gMutPI.halfPoints);
		}
		GetItemMark(gSpectDispMenu, DISP_AUX_ITEM, &curMark);
		if(curMark != noMark)
			DisplaySpectrum(targetSpectrum, displaySpectrum, gMutPI.halfPoints, filtSIPtr, RIGHT, 1.0, "\pTarget Channel 2");
		GetItemMark(gSonoDispMenu, DISP_AUX_ITEM, &curMark);
		if(curMark != noMark)
			DisplaySonogram(targetSpectrum, displaySpectrum, gMutPI.halfPoints, filtSIPtr, RIGHT, 1.0, "\pTarget Channel 2");
	}
		
	if(gProcNChans == STEREO)
    {
		MutateSpectrum((short)2, sourceSpectrum, sourcejSpectrumR, targetSpectrum, targetjSpectrumR, mutantjSpectrumR, gMI.omega);
		GetItemMark(gSpectDispMenu, DISP_OUTPUT_ITEM, &curMark);
		if(curMark != noMark)
			DisplaySpectrum(mutantjSpectrumR, displaySpectrum, gMutPI.halfPoints, outSIPtr, RIGHT, 1.0, "\pMutant Channel 2");
		GetItemMark(gSonoDispMenu, DISP_OUTPUT_ITEM, &curMark);
		if(curMark != noMark)
			DisplaySonogram(mutantjSpectrumR, displaySpectrum, gMutPI.halfPoints, outSIPtr, RIGHT, 1.0, "\pMutant Channel 2");
	
		PolarToCart(mutantjSpectrumR, cartSpectrum, gMutPI.halfPoints);
		RealFFT(cartSpectrum, gMutPI.halfPoints, FREQ2TIME);
		OverlapAdd(cartSpectrum, synthesisWindow, outputR, gOutPointer, gMutPI.points, gMutPI.windowSize);
	}

	ShiftOut(outSIPtr, outputL, outputR, gOutPointer+gMutPI.interpolation, gMutPI.interpolation, gMutPI.windowSize);
	length = (outSIPtr->numBytes/(outSIPtr->frameSize*outSIPtr->nChans*outSIPtr->sRate));
	HMSTimeString(length, errStr);
	UpdateProcessWindow("\p", errStr, "\p", -1.0);

	if(numSampsRead == -2L || numTargetRead == -2L)
	{
		DeAllocMutateMem();
		FinishProcess();
		gNumberBlocks = 0;
		return(-1);
	}
	gNumberBlocks++;
	GetFPos(outSIPtr->dFileNum, &sOutPos);

 	gInPosition += inSIPtr->nChans * gMutPI.decimation * inSIPtr->frameSize;
 	gFInPosition += filtSIPtr->nChans * gMI.fDecimation * filtSIPtr->frameSize;
 	if(gMutPI.useFunction)
 	{
 		pointer = inSIPtr->nChans * gInPointer * inSIPtr->frameSize;
 		if(pointer < 0)
 			pointer = 0;
		gMI.omega = InterpFunctionValue(pointer, FALSE);
		FixToString(gMI.omega, errStr);	
		StringAppend("\pmutating files, index: ", errStr, tmpStr);
		UpdateProcessWindow("\p", "\p", tmpStr, -1.0);
	}
	return(0);
}

void
MutateSpectrum(short channel, float sourceSpectrum[], float sourcejSpectrum[], float targetSpectrum[], float targetjSpectrum[], float mutantjSpectrum[], float omega)
{
	long 	bandNumber, phaseIndex, ampIndex;
	float 	sourceAmpDelta, sourcePhaseDelta, targetAmpDelta, targetPhaseDelta,
			mutantAmpDelta, mutantPhaseDelta,
			mutantAmpResult, mutantPhaseResult;
	
	switch(gMI.type)
	{
		case ISIM:
		case LCM:
		case IUIM:
			PickMutateTable(omega, gMI.bandPersistence, gMutPI.halfPoints, mutateDecisionA, smallDecisionA);
			break;
		case LCMIUIM:
		case LCMUUIM:
			PickMutateTable(omega, gMI.bandPersistence, gMutPI.halfPoints, mutateDecisionA, smallDecisionA);
			PickMutateTable(omega, gMI.bandPersistence, gMutPI.halfPoints, mutateDecisionB, smallDecisionB);
			break;
		case USIM:
		case UUIM:
			break;
	}

    for (bandNumber = 0; bandNumber <= gMutPI.halfPoints; bandNumber++)
    {
		phaseIndex = (ampIndex = bandNumber<<1) + 1;
		if(gMI.absolute)
		{
			sourceAmpDelta = sourceSpectrum[ampIndex] - gMI.sourceAbsoluteValue;
			targetAmpDelta = targetSpectrum[ampIndex] - gMI.targetAbsoluteValue;
			mutantjSpectrum[ampIndex] = ((1.0 - omega) * gMI.sourceAbsoluteValue) + (omega * gMI.targetAbsoluteValue);
		}
		else
		{
			sourceAmpDelta = sourceSpectrum[ampIndex] - sourcejSpectrum[ampIndex];
			targetAmpDelta = targetSpectrum[ampIndex] - targetjSpectrum[ampIndex];
		}

		sourcePhaseDelta = sourceSpectrum[phaseIndex] - sourcejSpectrum[phaseIndex];
		targetPhaseDelta = targetSpectrum[phaseIndex] - targetjSpectrum[phaseIndex];
		// phase unwrapping
	   	while (sourcePhaseDelta > Pi)
			sourcePhaseDelta -= twoPi;
	   	while (sourcePhaseDelta < -Pi)
			sourcePhaseDelta += twoPi;
	   	while (targetPhaseDelta > Pi)
			targetPhaseDelta -= twoPi;
	    while (targetPhaseDelta < -Pi)
			targetPhaseDelta += twoPi;
			
		switch(gMI.type)
		{
			case ISIM:
				if(mutateDecisionA[bandNumber] == TRUE)
				{
					mutantAmpDelta = ISIMutate(targetAmpDelta);
					mutantPhaseDelta = ISIMutate(targetPhaseDelta);
				}
				else
				{
					mutantAmpDelta = sourceAmpDelta;
					mutantPhaseDelta = sourcePhaseDelta;
				}
				break;
			case IUIM:
				if(mutateDecisionA[bandNumber] == TRUE)
				{
					mutantAmpDelta = IUIMutate(sourceAmpDelta, targetAmpDelta);
					mutantPhaseDelta = IUIMutate(sourcePhaseDelta, targetPhaseDelta);
				}
				else
				{
					mutantAmpDelta = sourceAmpDelta;
					mutantPhaseDelta = sourcePhaseDelta;
				}
				break;
			case LCM:
				if(mutateDecisionA[bandNumber] == TRUE)
				{
					mutantAmpDelta = LCMutate(sourceAmpDelta, targetAmpDelta);
					mutantPhaseDelta = LCMutate(sourcePhaseDelta, targetPhaseDelta);
				}
				else
				{
					mutantAmpDelta = sourceAmpDelta;
					mutantPhaseDelta = sourcePhaseDelta;
				}
				break;
			case USIM:
				mutantAmpDelta = USIMutate(sourceAmpDelta, targetAmpDelta, omega);
				mutantPhaseDelta = USIMutate(sourcePhaseDelta, targetPhaseDelta, omega);
				break;
			case UUIM:
				mutantAmpDelta = UUIMutate(sourceAmpDelta, targetAmpDelta, omega);
				mutantPhaseDelta = UUIMutate(sourcePhaseDelta, targetPhaseDelta, omega);
				break;
			case LCMIUIM:
				if(mutateDecisionA[bandNumber] == TRUE)
				{
					mutantAmpDelta = LCMutate(sourceAmpDelta, targetAmpDelta);
					mutantPhaseDelta = LCMutate(sourcePhaseDelta, targetPhaseDelta);
				}
				else
				{
					mutantAmpDelta = sourceAmpDelta;
					mutantPhaseDelta = sourcePhaseDelta;
				}
				if(mutateDecisionB[bandNumber] == TRUE)
				{
					mutantAmpDelta = IUIMutate(mutantAmpDelta, targetAmpDelta);
					mutantPhaseDelta = IUIMutate(mutantPhaseDelta, targetPhaseDelta);
				}
				else
				{
					mutantAmpDelta = mutantAmpDelta;
					mutantPhaseDelta = mutantPhaseDelta;
				}
				break;
			case LCMUUIM:
				if(mutateDecisionA[bandNumber] == TRUE)
				{
					mutantAmpDelta = LCMutate(sourceAmpDelta, targetAmpDelta);
					mutantPhaseDelta = LCMutate(sourcePhaseDelta, targetPhaseDelta);
				}
				else
				{
					mutantAmpDelta = sourceAmpDelta;
					mutantPhaseDelta = sourcePhaseDelta;
				}
				mutantAmpDelta = UUIMutate(mutantAmpDelta, targetAmpDelta, omega);
				mutantPhaseDelta = UUIMutate(mutantPhaseDelta, targetPhaseDelta, omega);
				break;
			default:
				mutantAmpDelta = sourceAmpDelta;
				mutantPhaseDelta = sourcePhaseDelta;
				break;
		}
		
		/* 
		 * deltaEmphasis is above 0.0. Decrease the delta.
		 * The delta will have a gain that varies from 0.0 to 1.0,
		 * the Mj will have a gain of 1.0.
		 */
		if(gMI.deltaEmphasis >= 0.0)
			mutantAmpResult = mutantjSpectrum[ampIndex] + ((1.0 - gMI.deltaEmphasis) * mutantAmpDelta);
		/*
		 * deltaEmphasis is below 0.0. Decrease the previous mutant.
		 * The delta will have a gain of 1.0,
		 * the Mj will have a gain that varies from 0.0 to 1.0
		 */
		else
			mutantAmpResult = ((1.0 + gMI.deltaEmphasis) * mutantjSpectrum[ampIndex]) + mutantAmpDelta;
			
		if(mutantAmpResult < 0.0)
			mutantAmpResult = 0.0;
		mutantPhaseResult = mutantjSpectrum[phaseIndex] + mutantPhaseDelta;

	   	while (mutantPhaseResult > Pi)
			mutantPhaseResult -= twoPi;
	    while (mutantPhaseResult < -Pi)
			mutantPhaseResult += twoPi;
			
		mutantjSpectrum[ampIndex] = mutantAmpResult;
		mutantjSpectrum[phaseIndex] = mutantPhaseResult;

		if(inSIPtr->nChans == 2)
		{
			sourcejSpectrum[ampIndex] = sourceSpectrum[ampIndex];
			sourcejSpectrum[phaseIndex] = sourceSpectrum[phaseIndex];
		}
		else if(channel == gProcNChans)
		{
			sourcejSpectrum[ampIndex] = sourceSpectrum[ampIndex];
			sourcejSpectrum[phaseIndex] = sourceSpectrum[phaseIndex];
		}
		
		if(filtSIPtr->nChans == 2)
		{
			targetjSpectrum[ampIndex] = targetSpectrum[ampIndex];
			targetjSpectrum[phaseIndex] = targetSpectrum[phaseIndex];
		}
		else if(channel == gProcNChans)
		{
			targetjSpectrum[ampIndex] = targetSpectrum[ampIndex];
			targetjSpectrum[phaseIndex] = targetSpectrum[phaseIndex];
		}
	}
}

/* 
 * All of these mutation functions return the mutantDelta.
 * mutantDelta = mutant[n] - mutant[n-1]
 * mutant[n] = mutant[n-1] + mutantDelta
 */

/* irregular signed interval mutation */
float
ISIMutate(float targetDelta)
{	
	float mutantDelta;
	
	mutantDelta = targetDelta;
	return(mutantDelta);
}

/* irregular unsigned interval mutation - use after LCM */
float
IUIMutate(float sourceDelta, float targetDelta)
{	
	float mutantDelta;
		
	if(sourceDelta < 0.0)
		mutantDelta = -fabs(targetDelta);
	else if(sourceDelta >= 0.0)
		mutantDelta = fabs(targetDelta);
	return(mutantDelta);
}

/* linear contour mutation */
float
LCMutate(float sourceDelta, float targetDelta)
{	
	float mutantDelta;
		
	if(targetDelta < 0.0)
		mutantDelta = -fabs(sourceDelta);
	else if(targetDelta >= 0.0)
		mutantDelta = fabs(sourceDelta);
	return(mutantDelta);
}

/* uniform signed interval mutation */
float
USIMutate(float sourceDelta, float targetDelta, float omega)
{	
	float mutantDelta;
	
	mutantDelta = (sourceDelta + omega * (targetDelta - sourceDelta));
	return(mutantDelta);
}

/* uniform unsigned interval mutation - use after LCM */
float
UUIMutate(float sourceDelta, float targetDelta, float omega)
{	
	float mutantDelta;
	
	if(sourceDelta < 0.0)
		mutantDelta = -fabs(sourceDelta + omega * (targetDelta - sourceDelta));
	else if(sourceDelta >= 0.0)
		mutantDelta = fabs(sourceDelta + omega * (targetDelta - sourceDelta));
	return(mutantDelta);
}

/* 
 * This function will fill the mutateDecision with a random set of TRUEs and
 * FALSEs, depending on the omega which is a number between 0.0 and 1.0.
 */

void	PickMutateTable(float omega, float persist, long bands, Boolean decision[], Boolean smallDecision[])
{
	long oldCount, count, band, step, numSteps, nextDivision, n;
	unsigned long longSeed;
	unsigned short	shortSeed;
	double numStepsFloat, nextOctave, nextDivisionFloat;
	float normalRandom;
	
	numStepsFloat = LLog2((double)bands);
	numSteps = (long)numStepsFloat * 3;
	
	if(gNumberBlocks == 0)
	{
		GetDateTime(&longSeed);
		shortSeed = (unsigned)(longSeed/2);
		srand(shortSeed);
	}
	
	// randomly turn some of the bands off
	
	for(step = 0; step < numSteps; step++)
	{
		normalRandom = (float)rand()/(float)RAND_MAX;
		if(persist < normalRandom)
		{
			if(smallDecision[step] == TRUE)
					smallDecision[step] = FALSE;
		}
	}
	
	oldCount = 0;
	
	for(step = 0; step < numSteps; step++)
		if(smallDecision[step] == TRUE)
			oldCount++;
			
	
	count = (long)(omega * numSteps);
	
	if(oldCount<count)
		for(n = oldCount; n < count;)
		{
			normalRandom = (float)rand()/(float)RAND_MAX;
			step = (unsigned short)(normalRandom*(numSteps-1));
			if(smallDecision[step] == FALSE)
			{
				n++;
				smallDecision[step] = TRUE;
			}
		}
	else
		for(n = oldCount; n > count;)
		{
			normalRandom = (float)rand()/(float)RAND_MAX;
			step = (unsigned short)(normalRandom*(numSteps-1));
			if(smallDecision[step] == TRUE)
			{
				n--;
				smallDecision[step] = FALSE;
			}
		}
		
	nextOctave = -0.3333333333333;
	nextDivisionFloat = EExp2(nextOctave) * bands;
	nextDivision = (long)nextDivisionFloat;
	// copy smallDecision into third octave divisions of mutateDecision
	for(band = bands, step = (numSteps - 1); band >= 0; band--)
	{
		if(band <= nextDivision)
		{
			step--;
			nextOctave -= 0.3333333333333;
			nextDivisionFloat = EExp2(nextOctave) * bands;
			nextDivision = (long)nextDivisionFloat;
		}
		decision[band] = smallDecision[step];
	}
		
}

void
DeAllocMutateMem(void)
{
	OSErr error;
	
	if(analysisWindow)
		RemovePtr((Ptr)analysisWindow);
	error = MemError();
	if(synthesisWindow)
		RemovePtr((Ptr)synthesisWindow);
	error = MemError();
	if(inputL)
		RemovePtr((Ptr)inputL);
	error = MemError();
	if(targetInputL)
		RemovePtr((Ptr)targetInputL);
	error = MemError();
	if(cartSpectrum)
		RemovePtr((Ptr)cartSpectrum);
	error = MemError();
	if(targetCartSpectrum)
		RemovePtr((Ptr)targetCartSpectrum);
	error = MemError();
	if(sourceSpectrum)
		RemovePtr((Ptr)sourceSpectrum);
	error = MemError();
	if(targetSpectrum)
		RemovePtr((Ptr)targetSpectrum);
	error = MemError();
	if(sourcejSpectrumL)
		RemovePtr((Ptr)sourcejSpectrumL);
	error = MemError();
	if(targetjSpectrumL)
		RemovePtr((Ptr)targetjSpectrumL);	
	error = MemError();
	if(mutantjSpectrumL)
		RemovePtr((Ptr)mutantjSpectrumL);
	error = MemError();
	if(displaySpectrum)
		RemovePtr((Ptr)displaySpectrum);		
	error = MemError();
	if(outputL)
		RemovePtr((Ptr)outputL);
	error = MemError();
	if(mutateDecisionA)
		RemovePtr((Ptr)mutateDecisionA);
	error = MemError();
	if(mutateDecisionB)
		RemovePtr((Ptr)mutateDecisionB);
	if(smallDecisionA)
		RemovePtr((Ptr)smallDecisionA);
	error = MemError();
	if(smallDecisionB)
		RemovePtr((Ptr)smallDecisionB);
	error = MemError();

	if(inSIPtr->nChans == STEREO)
	{
		if(sourcejSpectrumR)
			RemovePtr((Ptr)sourcejSpectrumR);
		error = MemError();
		if(inputR)
			RemovePtr((Ptr)inputR);
		error = MemError();
	}

	if(filtSIPtr->nChans == STEREO)
	{
		if(targetjSpectrumR)
			RemovePtr((Ptr)targetjSpectrumR);	
		error = MemError();
		if(targetInputR)
			RemovePtr((Ptr)targetInputR);
		error = MemError();
	}

	if(gProcNChans == STEREO)
	{
		if(mutantjSpectrumR)
			RemovePtr((Ptr)mutantjSpectrumR);
		error = MemError();
		if(outputR)
			RemovePtr((Ptr)outputR);
		error = MemError();
	}
	smallDecisionA = smallDecisionB = mutateDecisionA = mutateDecisionB = nil;
	
	analysisWindow = synthesisWindow = inputL = cartSpectrum = nil;
	sourceSpectrum = sourcejSpectrumL = targetInputL = targetCartSpectrum = nil;
	targetSpectrum = targetjSpectrumL = mutantjSpectrumL = outputL = displaySpectrum = nil;
	sourcejSpectrumR = inputR = targetInputR = targetjSpectrumR = mutantjSpectrumR = nil;
	outputR = nil;
}
