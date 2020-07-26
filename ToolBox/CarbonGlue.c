// CarbonGlue.c - substitutes for old OS9/8/7 calls which no longer exist
// include in Carbon projects

#include <string.h>
#include <stdlib.h>
#include "SoundFile.h"
#include "CarbonGlue.h"
#include "Misc.h"
#define kPopupCommand 			1
#define kAllowDismissCheck		2
// the requested dimensions for our sample open customization area:
#define kCustomWidth			100
#define kCustomHeightInset		70
#define kCustomHeight			55

extern struct
{
	Boolean	soundPlay;
	Boolean	soundRecord;
	Boolean	soundCompress;
	Boolean	movie;
}	gGestalt;
ControlHandle gFormatPopup;
ControlHandle gEncodingPopup;
extern MenuHandle gFormatMenu;
Boolean	gCreateSoundCanDismiss;
short	gCreateSoundLastTryWidth;
short	gCreateSoundLastTryHeight;
SoundInfoPtr	gCreateSoundInfoPtr;
Handle		gFormatDITL;

pascal void MaxApplZone(void){}

pascal void SystemClick (const EventRecord *theEvent, WindowRef theWindow){}

pascal SInt16 OpenDeskAcc(ConstStr255Param deskAccName){}

pascal void CheckItem(MenuHandle theMenu, SInt16 item, Boolean checked)
{
	CheckMenuItem (theMenu, item, checked);
}

pascal void EnableItem(MenuHandle theMenu, SInt16 item)
{
	EnableMenuItem(theMenu, item);
}

pascal void DisableItem(MenuHandle theMenu, SInt16 item)
{
	DisableMenuItem(theMenu, item);
}

//path2fss makes an FSSpec from a path with or without a filename
int Path2FSS(FSSpec *fss, char *path)
{
  char buf[256];
  char *p = &buf[1];
  strcpy(p, path); //convert to Str255	
  buf[0] = strlen(p);

  return(FSMakeFSSpec(0, 0, (unsigned char *)buf, fss)); //== noErr
}


//fss2path takes the FSSpec of a file, folder or volume and returns it's path 
void FSS2Path(char *path, FSSpec *fss)
{
  int l;             //fss->name contains name of last item in path
  for(l=0; l<(fss->name[0]); l++) path[l] = fss->name[l + 1]; 
  path[l] = 0;

  if(fss->parID != fsRtParID) //path is more than just a volume name
  { 
    int i, len;
    CInfoPBRec pb;
    
    pb.dirInfo.ioNamePtr = fss->name;
    pb.dirInfo.ioVRefNum = fss->vRefNum;
    pb.dirInfo.ioDrParID = fss->parID;
    do
    {
		pb.dirInfo.ioFDirIndex = -1;  //get parent directory name
		pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;   
		if(PBGetCatInfoSync(&pb) != noErr) break;

		len = fss->name[0] + 1;
		for(i=l; i>=0;  i--) path[i + len] = path[i];
		for(i=1; i<len; i++) path[i - 1] = fss->name[i]; //add to start of path
		path[i - 1] = ':';
		l += len;
	} while(pb.dirInfo.ioDrDirID != fsRtDirID); //while more directory levels
  }
}

// these only work if there are no return pointers expected
unsigned char *c2pstr(char *s)
{
	Str255 dst;
	CopyCStringToPascal(s, dst);
	memcpy(s, dst, 256);
//	return(dst);
}

char *p2cstr(Str255 s)
{
	char dst[256];
	CopyPascalStringToC(s, dst);
	memcpy(s, dst, 256);
//	return(dst);
}

// this only works if there is no call-back function
pascal void SHStandardGetFile(
	RoutineDescriptor *fileFilter, 
	short numTypes, 
	SHSFTypeList typeList,
	SHStandardFileReply *sfreply)
{
	NavDialogOptions    dialogOptions;
	NavReplyRecord	navReply;
	NavTypeListHandle	navList;
	FInfo finderInfo;
	OSErr anErr;
	
	int i;
	if(fileFilter != nil)
		return;
	anErr = NavGetDefaultDialogOptions(&dialogOptions);
	dialogOptions.dialogOptionFlags |= kNavSelectDefaultLocation;
	
	if(numTypes == -1)
	{
		navList = (NavTypeListHandle)GetResource('open', 128);
		dialogOptions.dialogOptionFlags |=  kNavSelectAllReadableItem;
	}
	else 
	{
		navList = (NavTypeListHandle)NewHandle(sizeof(NavTypeList) + sizeof(OSType)*(numTypes - 1));
		HLock((Handle)navList);
		(*navList)->componentSignature = kNavGenericSignature;
		(*navList)->osTypeCount = numTypes;
    	for(i = 0; i < numTypes; i++)
    	{
    		(*navList)->osType[i] = typeList[i];
    	}
   		HUnlock((Handle)navList);
    }
	NavChooseFile(nil, &navReply, &dialogOptions, nil, nil, nil, navList, nil);
	DisposeHandle((Handle)navList);
	sfreply->sfGood = navReply.validRecord;
	sfreply->sfReplacing = navReply.replacing;
	sfreply->sfScript = navReply.keyScript;
	if(navReply.validRecord)
	{
		//  Deal with multiple file selection
		long    count;

		AECountItems(&(navReply.selection), &count);
		// Set up index for file list
		for (i = 1; i <= count; i++)
		{
		    AEKeyword   theKeyword;
		    DescType    actualType;
		    Size        actualSize;
		    FSSpec      documentFSSpec;
		    
		    // Get a pointer to selected file
		    anErr = AEGetNthPtr(&(navReply.selection), i,
		                        typeFSS, &theKeyword,
		                        &actualType,&documentFSSpec,
		                        sizeof(documentFSSpec),
		                        &actualSize);
		    if (anErr == noErr)
		    {
		    	sfreply->sfFile = documentFSSpec;
				FSpGetFInfo(&documentFSSpec, &finderInfo);
		    	sfreply->sfType = finderInfo.fdType;
		    	sfreply->sfFlags = finderInfo.fdFlags;
		    	sfreply->sfIsFolder = false;
		    	sfreply->sfIsVolume = false;
		    }
		}
	}
	//  Dispose of NavReplyRecord, resources, descriptors
	anErr = NavDisposeReply(&navReply);
}

pascal void SHStandardPutFile(
	ConstStr255Param prompt,
	ConstStr255Param defaultName,
	SHStandardFileReply *sfreply)
{
	NavReplyRecord	navReply;
	FInfo finderInfo;
	OSErr anErr;

	NavDialogOptions    dialogOptions;
	anErr = NavGetDefaultDialogOptions(&dialogOptions);
	memcpy(dialogOptions.savedFileName, defaultName, 256);
    anErr = NavPutFile(nil, &navReply, &dialogOptions, nil, nil, 'SDHK', nil);
	sfreply->sfGood = navReply.validRecord;
	sfreply->sfReplacing = navReply.replacing;
	sfreply->sfScript = navReply.keyScript;
    if (anErr == noErr && navReply.validRecord)
    {
		AEKeyword   theKeyword;
		DescType    actualType;
		Size        actualSize;
		FSSpec      documentFSSpec;

		anErr = AEGetNthPtr(&(navReply.selection), 1, typeFSS,
		                 &theKeyword, &actualType,
		                 &documentFSSpec, sizeof(documentFSSpec),
		                 &actualSize );
		if (anErr == noErr)
		{
		    sfreply->sfFile = documentFSSpec;
			FSpGetFInfo(&documentFSSpec, &finderInfo);
		    sfreply->sfType = finderInfo.fdType;
		    sfreply->sfFlags = finderInfo.fdFlags;
		    sfreply->sfIsFolder = false;
		    sfreply->sfIsVolume = false;
		    navReply.translationNeeded = false;
			anErr = NavCompleteSave(&navReply, kNavTranslateInPlace);
		}
		anErr = NavDisposeReply(&navReply);
    }
}

// For OpenSoundFile.c
void	OpenSoundFileCallBackInit(){}

void OpenSoundGetFile(
	short numTypes, 
	SHSFTypeList typeList, 
	SHStandardFileReply *openReply, 
	long *openCount, 
	FSSpecPtr *files)
{
	NavDialogOptions    dialogOptions;
	NavObjectFilterUPP  filterProc = NewNavObjectFilterUPP(OpenSoundCarbonFileFilter);
	OSErr               anErr = noErr;

	//  Specify default options for dialog box
	anErr = NavGetDefaultDialogOptions(&dialogOptions);
	if (anErr == noErr)
	{
		//  Adjust the options to fit our needs
		//  Set default location option
		dialogOptions.dialogOptionFlags |= kNavSelectDefaultLocation;
		//  Set preview option
		dialogOptions.dialogOptionFlags |= kNavAllowPreviews;

		// make descriptor for default location
		if (anErr == noErr)
		{
			// Get 'open' resource. A nil handle being returned is OK,
			// this simply means no automatic file filtering.
			NavTypeListHandle typeList = (NavTypeListHandle)GetResource('open', 128);
			NavReplyRecord reply;

			// Call NavGetFile() with specified options and
			// declare our app-defined functions and type list
			anErr = NavGetFile (nil, &reply, &dialogOptions,
						nil, nil, filterProc, typeList, nil);
			*openCount = 0;
			if (anErr == noErr && reply.validRecord)
			{
				anErr = AECountItems(&(reply.selection), openCount);
				// Set up index for file list
				if (anErr == noErr)
				{
					long index;
					long size = *openCount * sizeof(FSSpec);
					*files = (FSSpec *)NewPtr(size);
					for (index = 1; index <= *openCount; index++)
					{
						AEKeyword   theKeyword;
						DescType    actualType;
						Size        actualSize;
						FSSpec      documentFSSpec;

						// Get a pointer to selected file
						anErr = AEGetNthPtr(&(reply.selection), index,
						                   typeFSS, &theKeyword,
						                   &actualType,&documentFSSpec,
						                   sizeof(documentFSSpec),
						                   &actualSize);
						if (anErr == noErr)
						{
							(*files)[index - 1] = documentFSSpec;
						}
					}
				}
		      //  Dispose of NavReplyRecord, resources, descriptors
		      anErr = NavDisposeReply(&reply);
		  }
		  if (typeList != NULL)
		  {
		      ReleaseResource( (Handle)typeList);
		  }
		}
	}
	DisposeNavObjectFilterUPP(filterProc);
}

pascal Boolean OpenSoundCarbonFileFilter(
				AEDesc* theItem, 
				void* info, 
                NavCallBackUserData callBackUD,
				NavFilterModes filterMode)
{
	OSErr theErr = noErr;
	NavFileOrFolderInfo* theInfo = (NavFileOrFolderInfo*)info;
	AEKeyword   theKeyword;
	DescType    actualType;
	Size        actualSize;
	FSSpec      soundFSSpec;
	Str255		testString;
	int			i;
    
	if (theItem->descriptorType == typeFSS)
	{
		
        if (theInfo->isFolder)
        	return(true);
		if(theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'Sd2f' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'AIFF' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'AIFC' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'DSPs' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'MSND' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'RIFF' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == '.WAV' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'IRCM' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'NxTS' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'LMAN' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'DATA' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'ULAW' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'SCRN' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'CSCR' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'PICT' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'TEXT' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'MooV' 
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'WAVE'
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'SFIL'
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'MPG3'
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'MPEG'
			|| theInfo->fileAndFolder.fileInfo.finderInfo.fdType == 'SDIF'
			)
			return(true);
		theErr = 0;
		theErr = AEGetDescData(theItem, &soundFSSpec, sizeof(soundFSSpec));

		for(i = 1; i <= 3; i++)
			testString[i] = soundFSSpec.name[i+(soundFSSpec.name[0] - 3)];
		testString[0] = 3;
		if(EqualString(testString,"\p.au",false,true))
			return(true);
		if(EqualString(testString,"\p.sf",false,true))
			return(true);
		for(i = 1; i <= 4; i++)
			testString[i] = soundFSSpec.name[i+(soundFSSpec.name[0] - 4)];
		testString[0] = 4;
		if(EqualString(testString,"\p.snd",false,true))
			return(true);
		if(EqualString(testString,"\p.mp3",false,true))
			return(true);
		if(EqualString(testString,"\p.irc",false,true))
			return(true);
		if(EqualString(testString,"\p.wav",false,true))
			return(true);
		if(EqualString(testString,"\p.pvc",false,true))
			return(true);
		if(EqualString(testString,"\p.aif",false,true))
			return(true);
		if(EqualString(testString,"\p.aic",false,true))
			return(true);
		testString[0] = 2;
		if(EqualString(testString,"\p.W",false,true))
			return(true);
		if(EqualString(testString,"\p.w",false,true))
			return(true);
		for(i = 1; i <= 5; i++)
			testString[i] = soundFSSpec.name[i+(soundFSSpec.name[0] - 5)];
		testString[0] = 5;
		if(EqualString(testString,"\p.aiff",false,true))
			return(true);
		if(EqualString(testString,"\p.cdda",false,true))
			return(true);
		if(EqualString(testString,"\p.aifc",false,true))
			return(true);
		if(EqualString(testString,"\p.sdif",false,true))
			return(true);
	}
    return(false);
}

// For CreateSoundFile.c
void	CreateSoundFileCallBackInit(){}

void	CreateSoundPutFile(
			Str255 saveStr, 
			Str255 fileName, 
			SHStandardFileReply *createReply,
			SoundInfoPtr	*soundInfo)
{
	OSErr               anErr = noErr;
	NavReplyRecord      reply;
	NavDialogOptions    dialogOptions;
	OSType              fileTypeToSave = 'TEXT';
	OSType              creatorType = 'SDHK';
	NavEventUPP         eventProc = NewNavEventUPP(CreateSoundEventProc);
	Str255				menuTitle;
	NavMenuItemSpec 	menuItems[13];
	long				i;	

	gCreateSoundCanDismiss = false;

	gCreateSoundInfoPtr = *soundInfo;
	
	anErr = NavGetDefaultDialogOptions(&dialogOptions);
	if (anErr == noErr)
	{
		//  One way to get the name for the file to be saved.
		StringCopy(fileName,  dialogOptions.savedFileName);
		StringCopy("\pSoundHack",  dialogOptions.clientName);
		dialogOptions.dialogOptionFlags &= ~kNavAllowStationery;
		dialogOptions.dialogOptionFlags |= kNavNoTypePopup;
		dialogOptions.dialogOptionFlags |= kNavDontAutoTranslate;
		dialogOptions.dialogOptionFlags |= kNavDontAddTranslateItems;
		dialogOptions.preferenceKey = 0;

		anErr = NavPutFile(nil, &reply, &dialogOptions, eventProc, fileTypeToSave, creatorType, &i);
		createReply->sfGood = reply.validRecord;
		if (anErr == noErr && reply.validRecord)
		{
			AEKeyword   theKeyword;
			DescType    actualType;
			Size        actualSize;
			FSSpec      documentFSSpec;

			anErr = AEGetNthPtr(&(reply.selection), 1, typeFSS,
			                 &theKeyword, &actualType,
			                 &documentFSSpec, sizeof(documentFSSpec),
			                 &actualSize );
			if (anErr == noErr)
			{
				createReply->sfReplacing = reply.replacing;
				switch(gCreateSoundInfoPtr->sfType)
				{
					case AIFF:
					case QKTMA:
						createReply->sfType = 'AIFF';
						break;
					case AIFC:
						createReply->sfType = 'AIFC';
						break;
					case AUDMED:
					case SDII:
						createReply->sfType = 'Sd2f';
						break;
					case DSPD:
						createReply->sfType = 'DSPs';
						break;
					case MMIX:
						createReply->sfType = 'MSND';
						break;
					case BICSF:
						createReply->sfType = 'IRCM';
						break;
					case NEXT:
					case SUNAU:
						createReply->sfType = 'NxTS';
						break;
					case WAVE:
						createReply->sfType = 'WAVE';
						break;
					case RAW:
						createReply->sfType = 'DATA';
						break;
					case TEXT:
						createReply->sfType = 'TEXT';
						break;
					default:
						gCreateSoundInfoPtr->sfType = AIFF;
						createReply->sfType = 'AIFF';
						break;
						
				}
				createReply->sfFile = documentFSSpec;
				createReply->sfScript = reply.keyScript;
				createReply->sfIsFolder = false;
				createReply->sfIsVolume = false;
		    	reply.translationNeeded = false;

				if ( anErr == noErr)
				{
					// Always call NavCompleteSave() to complete
					anErr = NavCompleteSave(&reply,
					                        kNavTranslateInPlace);
				}
			}
			(void) NavDisposeReply(&reply);
		}
		DisposeNavEventUPP(eventProc);
	}
}


pascal void CreateSoundEventProc(	NavEventCallbackMessage callBackSelector, 
								NavCBRecPtr callBackParms, 
								NavCallBackUserData callBackUD )
{

	if ( callBackUD != 0 && callBackParms != NULL )
		switch ( callBackSelector )
		{
			case kNavCBEvent:
				CreateSoundHandleNormalEvents( callBackParms, callBackUD );
				break;
			case kNavCBCustomize:
				CreateSoundHandleCustomizeEvent( callBackParms );
				break;
			case kNavCBStart:
				CreateSoundHandleNewFileStartEvent( callBackParms );
				break;
			case kNavCBPopupMenuSelect:	// Signifies that a popup menu selection was made
				CreateSoundHandleShowPopupMenuSelect( callBackParms );
				break;
			// all of the other events
			case kNavCBTerminate:	// The navigation dialog is closing down
				CreateSoundHandleTerminateEvent( callBackParms );
				break;
			case kNavCBAdjustRect:	// The navigation dialog is being resized
				MoveControl(gFormatPopup, callBackParms->customRect.left + 5, callBackParms->customRect.top + 5);
				MoveControl(gEncodingPopup, callBackParms->customRect.left + 5, callBackParms->customRect.top + 30);
				break;
			case kNavCBNewLocation:	// User has chosen a new location in the browser
			case kNavCBShowDesktop:	// User has navigated to the desktop
			case kNavCBSelectEntry:	// User has made a selection in the browser
			case kNavCBAccept:	// User has accepted the navigation dialog
			case kNavCBCancel:	// User has cancelled the navigation dialog
			case kNavCBAdjustPreview:	// Preview button clicked or preview was resized
			case kNavCBOpenSelection:
				break;		
		}
			
}

void CreateSoundHandleTerminateEvent( NavCBRecPtr callBackParms )
{
	DisposeControl(gFormatPopup);
	DisposeControl(gEncodingPopup);
}

void CreateSoundHandleNewFileStartEvent( NavCBRecPtr callBackParms )
{
	OSErr	theErr = noErr;
	UInt32 	version = 0;
	int i;
	
	gFormatPopup = GetNewControl(136, callBackParms->window);
	gEncodingPopup = GetNewControl(137, callBackParms->window);
	theErr = NavCustomControl(callBackParms->context, kNavCtlAddControl, gFormatPopup);
	theErr = NavCustomControl(callBackParms->context, kNavCtlAddControl, gEncodingPopup);
	MoveControl(gFormatPopup, callBackParms->customRect.left + 5, callBackParms->customRect.top + 5);
	MoveControl(gEncodingPopup, callBackParms->customRect.left + 5, callBackParms->customRect.top + 30);
	if(gCreateSoundInfoPtr->sfType > TEXT)
	{
		gCreateSoundInfoPtr->sfType = AIFF;
		gCreateSoundInfoPtr->packMode = SF_FORMAT_16_LINEAR;
	}
	SetControlValue(gFormatPopup,gCreateSoundInfoPtr->sfType);
	SetControlValue(gEncodingPopup,gCreateSoundInfoPtr->packMode);
	for(i = 1; i <= 16; i++)
		DisableItem(gFormatMenu, i);
	switch(gCreateSoundInfoPtr->sfType)
	{
		case AIFF:
		case SDII:
		case QKTMA:
		case AUDMED:
		case MMIX:
			EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_24_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_32_LINEAR);
			break;
		case AIFC:
			EnableItem(gFormatMenu,SF_FORMAT_4_ADDVI);
			EnableItem(gFormatMenu,SF_FORMAT_8_MULAW);
			EnableItem(gFormatMenu,SF_FORMAT_8_ALAW);
			EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_24_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_32_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_32_FLOAT);
			if(gGestalt.soundCompress == TRUE)
			{
				EnableItem(gFormatMenu,SF_FORMAT_4_ADIMA);
				EnableItem(gFormatMenu,SF_FORMAT_MACE3);
				EnableItem(gFormatMenu,SF_FORMAT_MACE6);
			}
			break;
		case DSPD:
			EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
			break;
		case BICSF:
			EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_32_FLOAT);
			break;
		case SUNAU:
		case NEXT:
			EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_24_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_32_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_32_FLOAT);
			EnableItem(gFormatMenu,SF_FORMAT_8_MULAW);
			EnableItem(gFormatMenu,SF_FORMAT_8_ALAW);
			break;
		case WAVE:
			EnableItem(gFormatMenu,SF_FORMAT_8_UNSIGNED);
			EnableItem(gFormatMenu,SF_FORMAT_16_SWAP);
			EnableItem(gFormatMenu,SF_FORMAT_24_SWAP);
			EnableItem(gFormatMenu,SF_FORMAT_32_SWAP);
			break;
		case RAW:
			EnableItem(gFormatMenu,SF_FORMAT_4_ADDVI);
			EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_8_UNSIGNED);
			EnableItem(gFormatMenu,SF_FORMAT_8_MULAW);
			EnableItem(gFormatMenu,SF_FORMAT_8_ALAW);
			EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_16_SWAP);
			EnableItem(gFormatMenu,SF_FORMAT_24_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_32_LINEAR);
			EnableItem(gFormatMenu,SF_FORMAT_32_FLOAT);
			if(gGestalt.soundCompress == TRUE)
			{
				EnableItem(gFormatMenu,SF_FORMAT_4_ADIMA);
				EnableItem(gFormatMenu,SF_FORMAT_MACE3);
				EnableItem(gFormatMenu,SF_FORMAT_MACE6);
			}
		case TEXT:
			EnableItem(gFormatMenu,SF_FORMAT_TEXT);
			break;
		default:
			break;
	}
}

void CreateSoundHandleNormalEvents( NavCBRecPtr callBackParms, NavCallBackUserData callBackUD )
{
#pragma unused( callBackUD )

	WindowPtr pWindow = NULL;

	switch( callBackParms->eventData.eventDataParms.event->what )
	{
			case mouseDown:
				CreateSoundHandleCustomMouseDown( callBackParms );
				break;
			
			case updateEvt:
			{
				pWindow = (WindowPtr)callBackParms->eventData.eventDataParms.event->message;
				// do something with the window?
				break;
			}
			
			default:
				break;
	}
}

void CreateSoundHandleShowPopupMenuSelect( NavCBRecPtr callBackParms )
{
	;		
}

void CreateSoundHandleCustomMouseDown( NavCBRecPtr callBackParms )
{
	OSErr			theErr = noErr;
	ControlHandle	whichControl = NULL;			
	Point 			where = callBackParms->eventData.eventDataParms.event->where;	
	short			theItem = 0;	
	UInt16 			firstItem = 0;
	short			realItem = 0;
	short			partCode = 0;
	int				i;
	UInt32 			version = 0;
		
	version = NavLibraryVersion( );
	
	// check for version 1.0, the 'itemHit' param field is not available in version 1.0:
	if ( version < 0x01108000 )
	{					
		GetMouse( &where );	// use the current mouse coordinates for proper tracking:
		theItem = FindDialogItem( (DialogPtr)callBackParms->window, where );	// get the item number of the control
	}
	else
	{
		// use the event data to obtain the mouse coordinates:
		theItem = callBackParms->eventData.itemHit - 1;	// Nav 1.1 givies us the "itemHit"
		where = callBackParms->eventData.eventDataParms.event->where;
		GlobalToLocal( &where );
	}
	partCode = FindControl( where, callBackParms->window, &whichControl );	// finally get the control itself
	

	if( whichControl == gEncodingPopup && theErr == noErr )
	{
		TrackControl(gEncodingPopup, where, (ControlActionUPP)-1);
		gCreateSoundInfoPtr->packMode = GetControlValue(gEncodingPopup);
	}
	else if(whichControl == gFormatPopup && theErr == noErr)
	{
		TrackControl(gFormatPopup, where, (ControlActionUPP)-1);
		gCreateSoundInfoPtr->sfType = GetControlValue(gFormatPopup);
		for(i = 1; i <= 16; i++)
			DisableItem(gFormatMenu, i);
		switch(gCreateSoundInfoPtr->sfType)
		{
			case AIFF:
			case SDII:
			case QKTMA:
			case AUDMED:
			case MMIX:
				EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_24_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_32_LINEAR);
				gCreateSoundInfoPtr->packMode = SF_FORMAT_16_LINEAR;
				break;
			case AIFC:
				EnableItem(gFormatMenu,SF_FORMAT_4_ADDVI);
				EnableItem(gFormatMenu,SF_FORMAT_8_MULAW);
				EnableItem(gFormatMenu,SF_FORMAT_8_ALAW);
				EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_24_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_32_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_32_FLOAT);
				if(gGestalt.soundCompress == TRUE)
				{
					EnableItem(gFormatMenu,SF_FORMAT_4_ADIMA);
					EnableItem(gFormatMenu,SF_FORMAT_MACE3);
					EnableItem(gFormatMenu,SF_FORMAT_MACE6);
				}
				gCreateSoundInfoPtr->packMode = SF_FORMAT_16_LINEAR;
				break;
			case DSPD:
				EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
				gCreateSoundInfoPtr->packMode = SF_FORMAT_16_LINEAR;
				break;
			case BICSF:
				EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_32_FLOAT);
				gCreateSoundInfoPtr->packMode = SF_FORMAT_32_FLOAT;
				break;
			case SUNAU:
			case NEXT:
				EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_24_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_32_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_32_FLOAT);
				EnableItem(gFormatMenu,SF_FORMAT_8_MULAW);
				EnableItem(gFormatMenu,SF_FORMAT_8_ALAW);
				gCreateSoundInfoPtr->packMode = SF_FORMAT_32_FLOAT;
				break;
			case WAVE:
				EnableItem(gFormatMenu,SF_FORMAT_8_UNSIGNED);
				EnableItem(gFormatMenu,SF_FORMAT_16_SWAP);
				EnableItem(gFormatMenu,SF_FORMAT_24_SWAP);
				EnableItem(gFormatMenu,SF_FORMAT_32_SWAP);
				gCreateSoundInfoPtr->packMode = SF_FORMAT_16_SWAP;
				break;
			case RAW:
				EnableItem(gFormatMenu,SF_FORMAT_4_ADDVI);
				EnableItem(gFormatMenu,SF_FORMAT_8_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_8_UNSIGNED);
				EnableItem(gFormatMenu,SF_FORMAT_8_MULAW);
				EnableItem(gFormatMenu,SF_FORMAT_8_ALAW);
				EnableItem(gFormatMenu,SF_FORMAT_16_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_16_SWAP);
				EnableItem(gFormatMenu,SF_FORMAT_24_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_32_LINEAR);
				EnableItem(gFormatMenu,SF_FORMAT_32_FLOAT);
				if(gGestalt.soundCompress == TRUE)
				{
					EnableItem(gFormatMenu,SF_FORMAT_4_ADIMA);
					EnableItem(gFormatMenu,SF_FORMAT_MACE3);
					EnableItem(gFormatMenu,SF_FORMAT_MACE6);
				}
				gCreateSoundInfoPtr->packMode = SF_FORMAT_16_LINEAR;
			case TEXT:
				EnableItem(gFormatMenu,SF_FORMAT_TEXT);
				gCreateSoundInfoPtr->packMode = SF_FORMAT_TEXT;
				break;
			default:
				break;
		}
		SetControlValue(gEncodingPopup,gCreateSoundInfoPtr->packMode);
	}
}

void CreateSoundHandleCustomizeEvent( NavCBRecPtr callBackParms )
{							
	// here are the desired dimensions for our custom area:
	short	neededWidth = callBackParms->customRect.left + kCustomWidth;
	short 	neededHeight = 30;	
	UInt32 	version = 0;
	
	version = NavLibraryVersion( );

	// this feature only available in v2.0 or greator: don't show the bevel box
	if ( version < 0x02008000 ) 
		neededHeight = callBackParms->customRect.top + kCustomHeightInset;
	else
		neededHeight = callBackParms->customRect.top + kCustomHeight;	
	
	// check to see if this is the first round of negotiations:
	if ( callBackParms->customRect.right == 0 && callBackParms->customRect.bottom == 0 )
	{
		// it is, so tell NavServices what dimensions we want:
		callBackParms->customRect.right = neededWidth;
		callBackParms->customRect.bottom = neededHeight;
	}
	else
	{
		// we are in the middle of negotiating:
		if ( gCreateSoundLastTryWidth != callBackParms->customRect.right )
			if ( callBackParms->customRect.right < neededWidth )	// is NavServices width too small for us?
				callBackParms->customRect.right = neededWidth;

		if ( gCreateSoundLastTryHeight != callBackParms->customRect.bottom )	// is NavServices height too small for us?
			if ( callBackParms->customRect.bottom < neededHeight )
				callBackParms->customRect.bottom = neededHeight;
	}
	
	// remember our last size so the next time we can re-negotiate:
	gCreateSoundLastTryWidth = callBackParms->customRect.right;
	gCreateSoundLastTryHeight = callBackParms->customRect.bottom;
}
