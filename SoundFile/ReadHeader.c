// ReadHeader.c		All of the header reading routines. Spectral and sound.
#if powerc
#include <fp.h>
#endif	/*powerc*/

#include <stdio.h>
#include <stdlib.h>
#include <QuickTime/Movies.h>
#include "IEEE80.h"
#include "SoundFile.h"
#include "SpectralHeader.h"
#include "ReadHeader.h"
#include "SoundHack.h"
#include "Macros.h"
#include "Misc.h"
#include "ByteSwap.h"
#include "OpenSoundFile.h"
#include "Dialog.h"
#include "AppleCompression.h"
#include "sdif.h"

/* globals */

extern short	gAppFileNum;
extern long		gSpectPICTWidth;
extern struct
{
	Boolean	soundPlay;
	Boolean	soundRecord;
	Boolean	soundCompress;
	Boolean	movie;
}	gGestalt;

short
ReadSDIHeader(SoundInfo *mySI)
{
	long			dFileEOF, readSize;
	SDIHeader		myHeader;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myHeader);
	FSRead(mySI->dFileNum, &readSize, &myHeader);
#ifdef SHOSXUB
	myHeader.HeaderSize = EndianU16_BtoN(myHeader.HeaderSize);
	myHeader.SampRate = EndianS32_BtoN(myHeader.SampRate);
	myHeader.FileSize = EndianS32_BtoN(myHeader.FileSize);
#endif
	if(myHeader.HeaderSize != 1336)
		return(-1);
	mySI->packMode = SF_FORMAT_16_LINEAR;
	mySI->frameSize = 2;
	mySI->sRate = (float)myHeader.SampRate;
	mySI->nChans = 1;
	mySI->dataStart = myHeader.HeaderSize;
	mySI->numBytes = myHeader.FileSize;
	GetEOF(mySI->dFileNum, &dFileEOF);
	mySI->dataEnd = dFileEOF;
	return(TRUE);
}

short
ReadSDIIHeader(SoundInfo *mySI)
{
	long		intSRate;
	Handle		frameSizeHandle, sRateHandle, nChansHandle;
	Str255		tmpStr;
	
	mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec),fsCurPerm);
	if(mySI->rFileNum == -1)
		return(-1);
	UseResFile(mySI->rFileNum);
	frameSizeHandle = GetNamedResource('STR ',"\psample-size");
	if(frameSizeHandle==0)
		return(-1);
	sRateHandle = GetNamedResource('STR ',"\psample-rate");
	if(sRateHandle==0)
		return(-1);
	nChansHandle = GetNamedResource('STR ',"\pchannels");
	if(nChansHandle==0)
		return(-1);
		
	HLock(nChansHandle);
	StringCopy((StringPtr)*nChansHandle, tmpStr);
	StringToNum(tmpStr, &mySI->nChans);
	HUnlock(nChansHandle);

	HLock(sRateHandle);
	StringCopy((StringPtr)*sRateHandle, tmpStr);
	if(mySI->sfType == DSPD)
	{
		StringToNum(tmpStr, &intSRate);
		mySI->sRate = (float)intSRate;
	}
	else
		StringToFix(tmpStr, &mySI->sRate);
	HUnlock(sRateHandle);

	HLock(frameSizeHandle);
	StringCopy((StringPtr)*frameSizeHandle, tmpStr);
	StringToFix(tmpStr, &mySI->frameSize);
	HUnlock(frameSizeHandle);
	
	CloseResFile(mySI->rFileNum);
	ReleaseResource(frameSizeHandle);
	ReleaseResource(sRateHandle);
	ReleaseResource(nChansHandle);
	
	UseResFile(gAppFileNum);
	mySI->dataStart = 0L;
	if(mySI->frameSize == 4.0)
		mySI->packMode = SF_FORMAT_32_LINEAR;
	else if(mySI->frameSize == 3.0)
		mySI->packMode = SF_FORMAT_24_LINEAR;
	else if(mySI->frameSize == 2.0)
		mySI->packMode = SF_FORMAT_16_LINEAR;
	else
		mySI->packMode = SF_FORMAT_8_LINEAR;
	GetEOF(mySI->dFileNum, &mySI->numBytes);
	mySI->dataEnd = mySI->numBytes;
	return(TRUE);
}

short
ReadAIFFHeader(SoundInfo *mySI)
{
	short	soundFound = FALSE, commonFound = FALSE, peakFound = FALSE;
	long	readSize, filePos;
	OSErr	error;
	FormChunk		myFormChunk;
	AIFFCommonChunk		myCommonChunk;
	SoundDataChunkHeader	mySoundDataChunk;
	PeakChunkHeader	myPeakChunkHeader;
	PositionPeak aPositionPeak;
	int i;
	unsigned long *longPtr;
	
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
	/* Loop through the Chunks until finding the Common Chunk and Sound Chunk */
	while(commonFound == FALSE || soundFound == FALSE || peakFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		error = FSRead(mySI->dFileNum, &readSize, &chunkHeader);
#ifdef SHOSXUB
		chunkHeader.ckID = EndianU32_BtoN(chunkHeader.ckID);
		chunkHeader.ckSize = EndianU32_BtoN(chunkHeader.ckSize);
#endif
		if((error == eofErr) && commonFound && soundFound)
		{
			if(peakFound == FALSE)
				mySI->peak = mySI->peakFL = mySI->peakFR = mySI->peakRL = mySI->peakRR = 1.0;
			return(TRUE);
		}
		else if((error == eofErr) && (commonFound == FALSE || soundFound == FALSE))
			return(-1);
			
		switch(chunkHeader.ckID)
		{
			case COMMONID:
				readSize = sizeof(myCommonChunk);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSRead(mySI->dFileNum, &readSize, &myCommonChunk);
#ifdef SHOSXUB
				myCommonChunk.numChannels = EndianS16_BtoN(myCommonChunk.numChannels);
				myCommonChunk.sampleSize = EndianS16_BtoN(myCommonChunk.sampleSize);
				myCommonChunk.numSampleFrames = EndianU32_BtoN(myCommonChunk.numSampleFrames);
#endif
				mySI->sRate = ieee_80_to_double(myCommonChunk.sampleRate.Bytes);
				mySI->nChans = myCommonChunk.numChannels;
				switch(myCommonChunk.sampleSize)
				{
					case AIFF_FORMAT_LINEAR_8:
						mySI->frameSize = 1;
						mySI->packMode = SF_FORMAT_8_LINEAR;
						break;
					case AIFF_FORMAT_LINEAR_16:
						mySI->frameSize = 2;
						mySI->packMode = SF_FORMAT_16_LINEAR;
						break;
					case AIFF_FORMAT_LINEAR_24:
						mySI->frameSize = 3;
						mySI->packMode = SF_FORMAT_24_LINEAR;
						break;
					case AIFF_FORMAT_LINEAR_32:
						mySI->frameSize = 4;
						mySI->packMode = SF_FORMAT_32_LINEAR;
						break;
				}
				mySI->numBytes = (long)(myCommonChunk.numSampleFrames
					*mySI->frameSize*mySI->nChans);
				commonFound = TRUE;
				break;
			case SOUNDID:
				// set filePos to point just after chunkHeader
				GetFPos(mySI->dFileNum, &filePos);
				// back up before chunkHeader
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				// grab the offset and chunksize put in sound struct.
				readSize = sizeof(SoundDataChunkHeader);
				FSRead(mySI->dFileNum, &readSize, &mySoundDataChunk);
#ifdef SHOSXUB
				mySoundDataChunk.offset = EndianU32_BtoN(mySoundDataChunk.offset);
#endif
				mySI->dataStart = filePos + mySoundDataChunk.offset + 8;
				// the end of chunk is ckSize after the chunkHeader
				filePos += chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromStart, filePos);
				mySI->dataEnd = filePos;
				soundFound = TRUE;
				break;
			case PEAKID:
				// set filePos to point just after chunkHeader
				GetFPos(mySI->dFileNum, &filePos);
				if(commonFound == TRUE)
				{
					mySI->peak = 0.0;
					readSize = sizeof(PeakChunkHeader);
					SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
					FSRead(mySI->dFileNum, &readSize, &myPeakChunkHeader);
					for(i = 0; i < myCommonChunk.numChannels; i++)
					{
						readSize = sizeof(PositionPeak);
						FSRead(mySI->dFileNum, &readSize, &aPositionPeak);
#ifdef SHOSXUB
						longPtr = &(aPositionPeak.value);
						*longPtr = EndianU32_BtoN(*longPtr);
						aPositionPeak.position = EndianU32_BtoN(aPositionPeak.position);
#endif
						switch(i)
						{
							case 0:
								mySI->peakFL = aPositionPeak.value;
								break;
							case 1:
								mySI->peakFR = aPositionPeak.value;
								break;
							case 2:
								mySI->peakRL = aPositionPeak.value;
								break;
							case 3:
								mySI->peakRR = aPositionPeak.value;
								break;
						}
						if(aPositionPeak.value > mySI->peak)
							mySI->peak = aPositionPeak.value;
					}
				}
				filePos += chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromStart, filePos);
				peakFound = TRUE;
				break;
			case NAMEID:
			{
				char testChar;
				GetFPos(mySI->dFileNum, &filePos);
				readSize = sizeof(char);
				FSRead(mySI->dFileNum, &readSize, &testChar);
				if(testChar == '|')
				mySI->packMode = SF_FORMAT_3DO_CONTENT;
				filePos += chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromStart, filePos);
				break;
			}
			case MARKERID:
			case INSTRUMENTID:
			case MIDIDATAID:
			case AUDIORECORDINGID:
			case APPLICATIONSPECIFICID:
			case COMMENTID:
			case AUTHORID:
			case COPYRIGHTID:
			case ANNOTATIONID:
			case FORMVERID:
			case JUNKID:
			default:
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
		}

	}

	return(TRUE);
}

short
ReadAIFCHeader(SoundInfo *mySI)
{
	Str255				errStr, tmpStr;
	short				soundFound = FALSE, commonFound = FALSE, nameFound = FALSE;
	Boolean				appSpecNeeded = TRUE, foulSE16stench = FALSE, peakFound = FALSE;
	long				readSize, filePos, i, tmpLong;
	FormChunk			myFormChunk;
	AIFCFormatVersionChunk	myFormatVersionChunk;
	AIFCCommonChunk		myCommonChunk;
	SoundDataChunkHeader		mySoundDataChunk;
	PeakChunkHeader		myPeakChunkHeader;
	PositionPeak		aPositionPeak;
	OSType				myOSType;
	OSErr				error;
	unsigned long		*longPtr;
	
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
	/* Loop through the Chunks until finding the Common Chunk and Sound Chunk */
	while(commonFound == FALSE || soundFound == FALSE || appSpecNeeded == TRUE || peakFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		error = FSRead(mySI->dFileNum, &readSize, &chunkHeader);
#ifdef SHOSXUB
		chunkHeader.ckID = EndianU32_BtoN(chunkHeader.ckID);
		chunkHeader.ckSize = EndianU32_BtoN(chunkHeader.ckSize);
#endif
		if((error == eofErr) && commonFound && soundFound)
		{
			if(peakFound == FALSE)
				mySI->peak = mySI->peakFL = mySI->peakFR = mySI->peakRL = mySI->peakRR = 1.0;
			return(TRUE);
		}
		else if((error == eofErr) && (commonFound == FALSE || soundFound == FALSE))
			return(-1);
			
		switch(chunkHeader.ckID)
		{
			case FORMVERID:
				readSize = sizeof(myFormatVersionChunk);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSRead(mySI->dFileNum, &readSize, &myFormatVersionChunk);
#ifdef SHOSXUB
				myFormatVersionChunk.timestamp = EndianU32_BtoN(myFormatVersionChunk.timestamp);
#endif
				if(myFormatVersionChunk.timestamp != AIFCVERSION1)
				{
					DrawErrorMessage("\pUnsupported AIFC format");
					return(-1);
				}
				break;
			case COMMONID:
				readSize = sizeof(myCommonChunk);
				GetFPos(mySI->dFileNum, &filePos);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSRead(mySI->dFileNum, &readSize, &myCommonChunk);
#ifdef SHOSXUB
				myCommonChunk.numChannels = EndianS16_BtoN(myCommonChunk.numChannels);
				myCommonChunk.sampleSize = EndianS16_BtoN(myCommonChunk.sampleSize);
				myCommonChunk.numSampleFrames = EndianU32_BtoN(myCommonChunk.numSampleFrames);
				myCommonChunk.compressionType = EndianU32_BtoN(myCommonChunk.compressionType);
#endif
				mySI->sRate = ieee_80_to_double(myCommonChunk.sampleRate.Bytes);
				mySI->nChans = myCommonChunk.numChannels;
				for(i=0; i<=myCommonChunk.compressionName[0]; i++)
					mySI->compName[i] = myCommonChunk.compressionName[i];
				switch(myCommonChunk.compressionType)
				{
					case kIMACompression:
						if(gGestalt.soundCompress == FALSE)
						{
							DrawErrorMessage("\pThis is an IMA4 compressed file. SoundHack requires Sound Manager 3.2 to read IMA4 files");
							return(-1);
						}
						else
						{
							mySI->frameSize = 0.53125;
							mySI->packMode = SF_FORMAT_4_ADIMA;
						}
						break;
					case kMACE3Compression:
						if(gGestalt.soundCompress == FALSE)
						{
							DrawErrorMessage("\pThis is a MACE3 compressed file. SoundHack requires Sound Manager 3.2 to read MACE3 files");
							return(-1);
						}
						else
						{
							mySI->frameSize = 1.0/3.0;
							mySI->packMode = SF_FORMAT_MACE3;
						}
						break;
					case kMACE6Compression:
						if(gGestalt.soundCompress == FALSE)
						{
							DrawErrorMessage("\pThis is a MACE6 compressed file.SoundHack requires Sound Manager 3.2 to read MACE6 files");
							return(-1);
						}
						else
						{
							mySI->frameSize = 1.0/6.0;
							mySI->packMode = SF_FORMAT_MACE6;
						}
						break;
					case k8BitOffsetBinaryFormat:
						mySI->frameSize = 1.0;
						mySI->packMode = SF_FORMAT_8_UNSIGNED;
						break;
					case k16BitBigEndianFormat:
						mySI->frameSize = 2.0;
						mySI->packMode = SF_FORMAT_16_LINEAR;
						break;
					case k16BitLittleEndianFormat:
						mySI->frameSize = 2.0;
						mySI->packMode = SF_FORMAT_16_SWAP;
						break;
					case AIFC_ID_ADDVI:
						mySI->frameSize = 0.5;
						mySI->packMode = SF_FORMAT_4_ADDVI;
						break;
					case kULawCompression:
						mySI->frameSize = 1.0;
						mySI->packMode = SF_FORMAT_8_MULAW;
						break;
					case kALawCompression:
						mySI->frameSize = 1.0;
						mySI->packMode = SF_FORMAT_8_ALAW;
						break;
					case k24BitFormat:
						mySI->frameSize = 3.0;
						mySI->packMode = SF_FORMAT_24_COMP;
						break;
					case k32BitFormat:
						mySI->frameSize = 4.0;
						mySI->packMode = SF_FORMAT_32_COMP;
						break;
					case AIFC_ID_FLT32:
					case kFloat32Format:
						mySI->frameSize = 4.0;
						mySI->packMode = SF_FORMAT_32_FLOAT;
						break;
					case kFloat64Format:
					case k32BitLittleEndianFormat:
					case kCDXA4Compression:
					case kCDXA2Compression:
					case kMicrosoftADPCMFormat:
					case kDVIIntelIMAFormat:
					case kDVAudioFormat:
					case kQDesignCompression:
					case kQUALCOMMCompression:
					case kMPEGLayer3Format:
					case kFullMPEGLay3Format:
						DrawErrorMessage("\pSoundHack can't read this type of compression");
						return(-1);
						break;
					case kSoundNotCompressed:
						switch(myCommonChunk.sampleSize)
						{
							case AIFF_FORMAT_LINEAR_8:
								mySI->frameSize = 1.0;
								mySI->packMode = SF_FORMAT_8_LINEAR;
								break;
							case AIFF_FORMAT_LINEAR_16:
								mySI->frameSize = 2.0;
								mySI->packMode = SF_FORMAT_16_LINEAR;
								break;
							case AIFF_FORMAT_LINEAR_24:
								mySI->frameSize = 3.0;
								mySI->packMode = SF_FORMAT_24_LINEAR;
								break;
							case AIFF_FORMAT_LINEAR_32:
								mySI->frameSize = 4.0;
								mySI->packMode = SF_FORMAT_32_LINEAR;
								break;
						}
						break;
					}
				if(mySI->packMode != SF_FORMAT_32_FLOAT)
					appSpecNeeded = FALSE;
				tmpLong = (long)(myCommonChunk.numSampleFrames * mySI->frameSize * mySI->nChans);
				if(mySI->packMode == SF_FORMAT_4_ADIMA && ((float)tmpLong/(float)(myFormChunk.ckSize) < .95))
				{
					foulSE16stench = TRUE;
					mySI->numBytes = (long)(myCommonChunk.numSampleFrames * 34 * mySI->nChans);
				}
				else
					mySI->numBytes = tmpLong;
				commonFound = TRUE;
				filePos += chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromStart, filePos);
				break;
			case SOUNDID:
				// set filePos to point just after chunkHeader
				GetFPos(mySI->dFileNum, &filePos);
				// back up before chunkHeader
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				// grab the offset and chunksize put in sound struct.
				readSize = sizeof(SoundDataChunkHeader);
				FSRead(mySI->dFileNum, &readSize, &mySoundDataChunk);
#ifdef SHOSXUB
				mySoundDataChunk.offset = EndianU32_BtoN(mySoundDataChunk.offset);
#endif
				mySI->dataStart = filePos + mySoundDataChunk.offset + 8;
				// the end of chunk is ckSize after the chunkHeader
				filePos += chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromStart, filePos);
				mySI->dataEnd = filePos;
				soundFound = TRUE;
				break;
			case PEAKID:
				// set filePos to point just after chunkHeader
				GetFPos(mySI->dFileNum, &filePos);
				if(commonFound == TRUE)
				{
					mySI->peak = 0.0;
					readSize = sizeof(PeakChunkHeader);
					SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
					FSRead(mySI->dFileNum, &readSize, &myPeakChunkHeader);
					for(i = 0; i < myCommonChunk.numChannels; i++)
					{
						readSize = sizeof(PositionPeak);
						FSRead(mySI->dFileNum, &readSize, &aPositionPeak);
#ifdef SHOSXUB
						longPtr = &(aPositionPeak.value);
						*longPtr = EndianU32_BtoN(*longPtr);
						aPositionPeak.position = EndianU32_BtoN(aPositionPeak.position);
#endif
						switch(i)
						{
							case 0:
								mySI->peakFL = aPositionPeak.value;
								break;
							case 1:
								mySI->peakFR = aPositionPeak.value;
								break;
							case 2:
								mySI->peakRL = aPositionPeak.value;
								break;
							case 3:
								mySI->peakRR = aPositionPeak.value;
								break;
						}
						if(aPositionPeak.value > mySI->peak)
							mySI->peak = aPositionPeak.value;
					}
				}
				filePos += chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromStart, filePos);
				peakFound = TRUE;
				break;
			case APPLICATIONSPECIFICID:
				// set filePos to point just after chunkHeader
				GetFPos(mySI->dFileNum, &filePos);
				// see if the OSTypeID is right for gain values
				readSize = sizeof(myOSType);
				FSRead(mySI->dFileNum, &readSize, &myOSType);
				if(myOSType == 'pErF')
				{
					if(chunkHeader.ckSize >= 8)
					{
						readSize = sizeof(float);
						FSRead(mySI->dFileNum, &readSize, &mySI->peak);
					}
					if(chunkHeader.ckSize >= 12)
					{
						readSize = sizeof(float);
						FSRead(mySI->dFileNum, &readSize, &mySI->peakFL);
					}
					if(chunkHeader.ckSize >= 16)
					{
						readSize = sizeof(float);
						FSRead(mySI->dFileNum, &readSize, &mySI->peakFR);
					}
					if(chunkHeader.ckSize >= 20)
					{
						readSize = sizeof(float);
						FSRead(mySI->dFileNum, &readSize, &mySI->peakRL);
					}
					if(chunkHeader.ckSize >= 24)
					{
						readSize = sizeof(float);
						FSRead(mySI->dFileNum, &readSize, &mySI->peakRR);
					}
				}
				
				// the end of chunk is ckSize after the chunkHeader
				filePos += chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromStart, filePos);
				appSpecNeeded = FALSE;
				break;
			case NAMEID:
			{
				char testChar;
				GetFPos(mySI->dFileNum, &filePos);
				readSize = sizeof(char);
				FSRead(mySI->dFileNum, &readSize, &testChar);
				if(testChar == '|')
				mySI->packMode = SF_FORMAT_3DO_CONTENT;
				filePos += chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromStart, filePos);
				break;
			}
			case MARKERID:
			case INSTRUMENTID:
			case MIDIDATAID:
			case AUDIORECORDINGID:
			case COMMENTID:
			case AUTHORID:
			case COPYRIGHTID:
			case ANNOTATIONID:
			case JUNKID:
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
			default:
				if(commonFound == TRUE && soundFound == TRUE)
					return(TRUE);
				else
					return(FALSE);
				break;
		}
	}
	return(TRUE);
}

short
ReadWAVEHeader(SoundInfo *mySI)
{
	short				soundFound = FALSE, commonFound = FALSE;
	short				sampleSize;
	long				readSize, filePos;
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
	/* Loop through the Chunks until finding the Format Chunk and Sound Chunk */
	while(commonFound == FALSE || soundFound == FALSE)
	{
		readSize = sizeof(chunkHeader);
		FSRead(mySI->dFileNum, &readSize, &chunkHeader);
#ifdef SHOSXUB
		chunkHeader.ckID = EndianU32_BtoN(chunkHeader.ckID);
		chunkHeader.ckSize = EndianU32_LtoN(chunkHeader.ckSize);
#endif
		switch(chunkHeader.ckID)
		{
			case WAV_ID_FORMAT:
				readSize = sizeof(myFormatChunk);
				GetFPos(mySI->dFileNum, &filePos);
				SetFPos(mySI->dFileNum, fsFromMark, -(sizeof(chunkHeader)));
				FSRead(mySI->dFileNum, &readSize, &myFormatChunk);
#ifdef SHOSXUB
				chunkHeader.ckSize = myFormatChunk.ckSize = EndianS32_LtoN(myFormatChunk.ckSize);
				myFormatChunk.wFormatTag = EndianS16_LtoN(myFormatChunk.wFormatTag);
				myFormatChunk.wChannels = EndianS16_LtoN(myFormatChunk.wChannels);
				myFormatChunk.dwSamplePerSec = EndianS32_LtoN(myFormatChunk.dwSamplePerSec);
				myFormatChunk.dwAvgBytesPerSec = EndianS32_LtoN(myFormatChunk.dwAvgBytesPerSec);
				myFormatChunk.wBlockAlign = EndianS16_LtoN(myFormatChunk.wBlockAlign);
#else
				chunkHeader.ckSize = myFormatChunk.ckSize = ByteSwapLong(myFormatChunk.ckSize);
				myFormatChunk.wFormatTag = ByteSwapShort(myFormatChunk.wFormatTag);
				myFormatChunk.wChannels = ByteSwapShort(myFormatChunk.wChannels);
				myFormatChunk.dwSamplePerSec = ByteSwapLong(myFormatChunk.dwSamplePerSec);
				myFormatChunk.dwAvgBytesPerSec = ByteSwapLong(myFormatChunk.dwAvgBytesPerSec);
				myFormatChunk.wBlockAlign = ByteSwapShort(myFormatChunk.wBlockAlign);
#endif
				mySI->sRate = (float)myFormatChunk.dwSamplePerSec;
				mySI->nChans = myFormatChunk.wChannels;
				switch(myFormatChunk.wFormatTag)
				{
					case WAV_FORMAT_PCM:
//						readSize = 2;
//						FSRead(mySI->dFileNum, &readSize, &sampleSize);
#ifdef SHOSXUB
						mySI->frameSize = EndianS16_LtoN(myFormatChunk.sampleSize)/8.0;
#else
						mySI->frameSize = ByteSwapShort(myFormatChunk.sampleSize)/8.0;
#endif
						if(mySI->frameSize == 1)
							mySI->packMode = SF_FORMAT_8_UNSIGNED;
						else if(mySI->frameSize == 2)
							mySI->packMode = SF_FORMAT_16_SWAP;
						else if(mySI->frameSize == 3)
							mySI->packMode = SF_FORMAT_24_SWAP;
						else if(mySI->frameSize == 4)
							mySI->packMode = SF_FORMAT_32_SWAP;
						break;
					case IBM_FORMAT_MULAW:
						mySI->frameSize = 1;
						mySI->packMode = SF_FORMAT_8_MULAW;
						break;
					case IBM_FORMAT_ALAW:
						mySI->frameSize = 1;
						mySI->packMode = SF_FORMAT_8_ALAW;
					case IBM_FORMAT_ADDVI:
					case WAV_FORMAT_ADDVI:
					case WAV_FORMAT_ADIMA:
					default:
//						DrawErrorMessage("\pUnknown data format. Only 8 and 16 bit PCM format WAVE files supported.");
						return(-1);
						break;
				}
				commonFound = TRUE;
				SetFPos(mySI->dFileNum, fsFromStart, filePos+chunkHeader.ckSize);
				break;
			case WAV_ID_DATA:
				/* We only want to get the offset for start of sound and the number of bytes */
#ifdef SHOSXUB
				mySI->numBytes = chunkHeader.ckSize;
#else
				mySI->numBytes = chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				GetFPos(mySI->dFileNum, &filePos);
				mySI->dataStart = filePos;
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				mySI->dataEnd = filePos;
				soundFound = TRUE;
				break;
			default:
#ifndef SHOSXUB
				chunkHeader.ckSize = ByteSwapLong(chunkHeader.ckSize);
#endif
				filePos = chunkHeader.ckSize;
				EVENUP(filePos);
				SetFPos(mySI->dFileNum, fsFromMark, filePos);
				break;
		}

	}

	return(TRUE);
}

/* NeXT and Sun have the same header */
short	ReadNEXTHeader(SoundInfo *mySI)
{
	Str255			errStr;
	char			*info;
	long			dFileEOF, readSize, theFilePos;
	float			peak;
	NeXTSoundInfo	myNeXTSI;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myNeXTSI);
	FSRead(mySI->dFileNum, &readSize, &myNeXTSI);
#ifdef SHOSXUB
	myNeXTSI.magic = EndianS32_BtoN(myNeXTSI.magic);
	myNeXTSI.dataLocation = EndianS32_BtoN(myNeXTSI.dataLocation);
	myNeXTSI.dataSize = EndianS32_BtoN(myNeXTSI.dataSize);
	myNeXTSI.dataFormat = EndianS32_BtoN(myNeXTSI.dataFormat);
	myNeXTSI.samplingRate = EndianS32_BtoN(myNeXTSI.samplingRate);
	myNeXTSI.channelCount = EndianS32_BtoN(myNeXTSI.channelCount);
#endif
	if(myNeXTSI.magic != NEXTMAGIC)
		return(-1);
	switch(myNeXTSI.dataFormat)
	{
		case NEXT_FORMAT_LINEAR_8:
			mySI->packMode = SF_FORMAT_8_LINEAR;
			mySI->frameSize = 1;
			break;
		case NEXT_FORMAT_LINEAR_16:
			mySI->packMode = SF_FORMAT_16_LINEAR;
			mySI->frameSize = 2;
			break;
		case NEXT_FORMAT_LINEAR_24:
			mySI->frameSize = 3;
			mySI->packMode = SF_FORMAT_24_LINEAR;
			break;
		case NEXT_FORMAT_LINEAR_32:
			mySI->frameSize = 4;
			mySI->packMode = SF_FORMAT_32_LINEAR;
			break;
		case NEXT_FORMAT_FLOAT:
			mySI->packMode = SF_FORMAT_32_FLOAT;
			mySI->frameSize = 4;
			break;
		case NEXT_FORMAT_MULAW_8:
			mySI->packMode = SF_FORMAT_8_MULAW;
			mySI->frameSize = 1;
			break;
		case NEXT_FORMAT_ALAW_8:
			mySI->packMode = SF_FORMAT_8_ALAW;
			mySI->frameSize = 1;
			break;
		default:
			DrawErrorMessage("\pUnrecognized packmode");
			NumToString(myNeXTSI.dataFormat, errStr);
			DrawErrorMessage(errStr);
			return(-1);
	}
	mySI->sRate = (float)myNeXTSI.samplingRate;
	mySI->nChans = myNeXTSI.channelCount;
	mySI->dataStart = myNeXTSI.dataLocation;
	GetEOF(mySI->dFileNum, &dFileEOF);
	mySI->numBytes = dFileEOF - myNeXTSI.dataLocation;
	mySI->dataEnd = dFileEOF;
	
	// read in the info string
	GetFPos(mySI->dFileNum, &theFilePos);
	readSize = myNeXTSI.dataLocation - theFilePos;
	info = (char *)NewPtr(readSize);
	FSRead(mySI->dFileNum, &readSize, info);
	if(info[0] != 0)
	{
		if(info[0] == 'P')
		{
			if(sscanf(info, "Peak %f", &peak))
				mySI->peak = mySI->peakFL = mySI->peakFR = mySI->peakRL = mySI->peakRR = peak;
			else
				mySI->peak = mySI->peakFL = mySI->peakFR = mySI->peakRL = mySI->peakRR = 1.0;
		}
	}
	DisposePtr((Ptr)info);
	return(TRUE);
}
	
short
ReadBICSFHeader(SoundInfo *mySI)
{
	long			dFileEOF, readSize;
	Boolean			codeLeft;
	BICSFSoundInfo	myBICSFSI;
	BICSFCodeHeader myCodeHeader;
	short			codePosition;
	long			*longPtr;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myBICSFSI);
	FSRead(mySI->dFileNum, &readSize, &myBICSFSI);
#ifdef SHOSXUB
	myBICSFSI.magic = EndianU32_BtoN(myBICSFSI.magic);
	longPtr = &(myBICSFSI.srate);
	*longPtr	= EndianU32_BtoN(*longPtr);
	myBICSFSI.chans = EndianU32_BtoN(myBICSFSI.chans);
	myBICSFSI.packMode = EndianU32_BtoN(myBICSFSI.packMode);
#endif
	if(myBICSFSI.magic != BICSFMAGIC)
		return(-1);
	switch(myBICSFSI.packMode)
	{
		case BICSF_FORMAT_LINEAR_8:
			mySI->packMode = SF_FORMAT_8_LINEAR;
			break;
		case BICSF_FORMAT_LINEAR_16:
			mySI->packMode = SF_FORMAT_16_LINEAR;
			break;
		case BICSF_FORMAT_FLOAT:
			mySI->packMode = SF_FORMAT_32_FLOAT;
			break;
		default:
			DrawErrorMessage("\pUnrecognized pack mode.");
			return(-1);
	}
	mySI->sRate = myBICSFSI.srate;
	mySI->nChans = myBICSFSI.chans;
	mySI->frameSize = (double)myBICSFSI.packMode;
	mySI->dataStart = sizeof(myBICSFSI);
	GetEOF(mySI->dFileNum, &dFileEOF);
	mySI->numBytes = dFileEOF - sizeof(myBICSFSI);
	mySI->dataEnd = dFileEOF;

	// we are gonna look for some amp codes if float
	codeLeft = TRUE;
	codePosition = 0;
	if(myBICSFSI.packMode == BICSF_FORMAT_FLOAT)
		while(codeLeft)
		{
			BlockMoveData(&myBICSFSI.codes[codePosition], &myCodeHeader, sizeof(myCodeHeader));
#ifdef SHOSXUB
			myCodeHeader.code = EndianS16_BtoN(myCodeHeader.code);
			myCodeHeader.bsize = EndianS16_BtoN(myCodeHeader.bsize);
#endif		
			codePosition += sizeof(myCodeHeader);
			if(myCodeHeader.code == BICSF_CODE_END)
				break;
			else if(myCodeHeader.code == BICSF_CODE_MAXAMP)
			{
				BlockMoveData(&myBICSFSI.codes[codePosition], &mySI->peakFL, sizeof(float));
				codePosition += sizeof(float);
#ifdef SHOSXUB
				longPtr = &(mySI->peakFL);
				*longPtr	= EndianU32_BtoN(*longPtr);
#endif
				mySI->peak = mySI->peakFL;
				if(mySI->nChans > 1)
				{
					BlockMoveData(&myBICSFSI.codes[codePosition], &mySI->peakFR, sizeof(float));
#ifdef SHOSXUB
				longPtr = &(mySI->peakFR);
				*longPtr	= EndianU32_BtoN(*longPtr);
#endif
					codePosition += sizeof(float);
					if(mySI->peakFR > mySI->peak)
						mySI->peak = mySI->peakFR;
				}
				if(mySI->nChans > 2)
				{
					BlockMoveData(&myBICSFSI.codes[codePosition], &mySI->peakRL, sizeof(float));
#ifdef SHOSXUB
				longPtr = &(mySI->peakRL);
				*longPtr	= EndianU32_BtoN(*longPtr);
#endif
					codePosition += sizeof(float);
					if(mySI->peakRL > mySI->peak)
						mySI->peak = mySI->peakRL;
				}
				if(mySI->nChans > 1)
				{
					BlockMoveData(&myBICSFSI.codes[codePosition], &mySI->peakRR, sizeof(float));
#ifdef SHOSXUB
				longPtr = &(mySI->peakRR);
				*longPtr	= EndianU32_BtoN(*longPtr);
#endif
					codePosition += sizeof(float);
					if(mySI->peakRR > mySI->peak)
						mySI->peak = mySI->peakRR;
				}
				break;
			}
			else
			{
				codePosition += myCodeHeader.bsize;
				if(codePosition >= 1008)
					codeLeft = FALSE;
			}
		}
			
				
	return(TRUE);
}

short
ReadTX16WHeader(SoundInfo *mySI)
{
	long			dFileEOF, readSize;
	TX16WHeader		myTX16WHeader;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myTX16WHeader);
	FSRead(mySI->dFileNum, &readSize, &myTX16WHeader);
	if(myTX16WHeader.filetype[0] != 'L' 
		|| myTX16WHeader.filetype[1] != 'M'
		|| myTX16WHeader.filetype[2] != '8'
		|| myTX16WHeader.filetype[3] != '9'
		|| myTX16WHeader.filetype[4] != '5'
		|| myTX16WHeader.filetype[5] != '3'
		)
		return(-1);
	mySI->packMode = SF_FORMAT_TX16W;
	switch(myTX16WHeader.sample_rate)
	{
		case 1:
			mySI->sRate = 33000.0;
			break;
		case 2:
			mySI->sRate = 50000.0;
			break;
		case 3:
			mySI->sRate = 16000.0;
			break;
	}
	mySI->nChans = 1;
	mySI->frameSize = 1.5;
	mySI->dataStart = sizeof(myTX16WHeader);
	GetEOF(mySI->dFileNum, &dFileEOF);
	mySI->numBytes = dFileEOF - sizeof(myTX16WHeader);
	mySI->dataEnd = dFileEOF;

	return(TRUE);
}


short
ReadLemurHeader(SoundInfo *mySI)
{
	long			dFileEOF, readSize, *longPtr;
	LemurHeader		myLemurSI;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myLemurSI);
	FSRead(mySI->dFileNum, &readSize, &myLemurSI);
	// endian conversions
#ifdef SHOSXUB
	myLemurSI.formatNumber = EndianS16_BtoN(myLemurSI.formatNumber);
	myLemurSI.headerLength = EndianS16_BtoN(myLemurSI.headerLength);
	myLemurSI.FFTlength = EndianS16_BtoN(myLemurSI.FFTlength);
	myLemurSI.originalFormatNumber = EndianS16_BtoN(myLemurSI.originalFormatNumber);
	myLemurSI.numberOfSamples = EndianS32_BtoN(myLemurSI.numberOfSamples);
	myLemurSI.numberOfFrames = EndianS32_BtoN(myLemurSI.numberOfFrames);
	longPtr = &(myLemurSI.mainLobeWidth);	*longPtr	= EndianU32_BtoN(*longPtr);
	longPtr = &(myLemurSI.sidelobeAttenuation);	*longPtr	= EndianU32_BtoN(*longPtr);
	longPtr = &(myLemurSI.analysisFrameLength);	*longPtr	= EndianU32_BtoN(*longPtr);
	longPtr = &(myLemurSI.analysisThreshold);	*longPtr	= EndianU32_BtoN(*longPtr);
	longPtr = &(myLemurSI.analysisRange);	*longPtr	= EndianU32_BtoN(*longPtr);
	longPtr = &(myLemurSI.frequencyDrift);	*longPtr	= EndianU32_BtoN(*longPtr);
	longPtr = &(myLemurSI.analysisSampleRate);	*longPtr	= EndianU32_BtoN(*longPtr);
#endif
	if(myLemurSI.formatNumber != 4000L || myLemurSI.formatNumber != 4001L)
		return(-1);

	mySI->packMode = SF_FORMAT_SPECT_MQ;
//	mySI->frameSize = 
	mySI->sRate = (float)myLemurSI.analysisSampleRate;
	mySI->nChans = MONO;
	mySI->dataStart = sizeof(myLemurSI);
	GetEOF(mySI->dFileNum, &dFileEOF);
	mySI->numBytes = dFileEOF - mySI->dataStart;
	mySI->dataEnd = dFileEOF;
	return(TRUE);
}

short	ReadPVAHeader(SoundInfo *mySI)
{
	long			dFileEOF, readSize, *longPtr;
	CsoundHeader	myCSHeader;
	
	SetFPos(mySI->dFileNum, fsFromStart, 0L);
	readSize = sizeof(myCSHeader);
	FSRead(mySI->dFileNum, &readSize, &myCSHeader);
#ifdef SHOSXUB
	myCSHeader.magic = EndianS32_BtoN(myCSHeader.magic);
	myCSHeader.headBsize = EndianS32_BtoN(myCSHeader.headBsize);
	myCSHeader.dataBsize = EndianS32_BtoN(myCSHeader.dataBsize);
	myCSHeader.dataFormat = EndianS32_BtoN(myCSHeader.dataFormat);
	myCSHeader.channels = EndianS32_BtoN(myCSHeader.channels);
	myCSHeader.frameSize = EndianS32_BtoN(myCSHeader.frameSize);
	myCSHeader.frameIncr = EndianS32_BtoN(myCSHeader.frameIncr);
	myCSHeader.frameBsize = EndianS32_BtoN(myCSHeader.frameBsize);
	myCSHeader.frameFormat = EndianS32_BtoN(myCSHeader.frameFormat);
	myCSHeader.freqFormat = EndianS32_BtoN(myCSHeader.freqFormat);
	longPtr = &(myCSHeader.samplingRate);	*longPtr	= EndianU32_BtoN(*longPtr);
	longPtr = &(myCSHeader.minFreq);	*longPtr	= EndianU32_BtoN(*longPtr);
	longPtr = &(myCSHeader.maxFreq);	*longPtr	= EndianU32_BtoN(*longPtr);
#endif
	
	if(myCSHeader.frameIncr == 0)
		return(-1);
	mySI->sRate = myCSHeader.samplingRate;
	mySI->nChans = myCSHeader.channels;
	mySI->dataStart = sizeof(myCSHeader);
	mySI->frameSize = (myCSHeader.frameSize * sizeof(float))/myCSHeader.frameIncr;
	mySI->spectFrameSize = myCSHeader.frameSize;
	mySI->spectFrameIncr = myCSHeader.frameIncr;
	GetEOF(mySI->dFileNum, &dFileEOF);
	mySI->numBytes = dFileEOF - mySI->dataStart;
	mySI->dataEnd = dFileEOF;
	if(myCSHeader.magic == SHA_MAGIC)
	{
		mySI->packMode = SF_FORMAT_SPECT_AMPPHS;
		return(TRUE);
	}
	else if(myCSHeader.magic == CSA_MAGIC)
	{
		if(myCSHeader.frameFormat == PVA_PVOC)
			mySI->packMode = SF_FORMAT_SPECT_AMPFRQ;
		else if(myCSHeader.frameFormat == PVA_POLAR)
			mySI->packMode = SF_FORMAT_SPECT_AMPPHS;
		return(TRUE);
	}
	else
		return(-1);	
}

// A PICT doesn't have a header, so we will just make up all SoundInfo values!
short ReadPICTHeader(SoundInfo *mySI)
{
	PicHandle	aPictHandle;
	FInfo		fndrInfo;
	long		numberPICTs, height, width, readSize, endOfFile, error;
	short		resError;
	
	FSpGetFInfo(&(mySI->sfSpec),&fndrInfo);	// read the file type
	if(fndrInfo.fdType == 'SCRN' || fndrInfo.fdType == 'CSCR')
	{
		mySI->rFileNum = FSpOpenResFile(&(mySI->sfSpec),fsCurPerm);
		if(mySI->rFileNum == -1)
		{
			resError = ResError();
			return(-1);
		}
		UseResFile(mySI->rFileNum);
		numberPICTs = Count1Resources('PICT');
		if(numberPICTs == 0)					// we need to make sure there are PICTs in here
		{
			CloseResFile(mySI->rFileNum);
			UseResFile(gAppFileNum);
			return(-1);
		}
		aPictHandle = (PicHandle)Get1IndResource('PICT',1);
		if(aPictHandle==0)
		{
			CloseResFile(mySI->rFileNum);
			UseResFile(gAppFileNum);
			return(-1);
		}
	}
	else
	{
		error = FSpOpenDF(&(mySI->sfSpec),fsCurPerm, &mySI->dFileNum);
		aPictHandle = (PicHandle)NewHandle(sizeof(Picture));
		GetEOF(mySI->dFileNum, &endOfFile);
		readSize = endOfFile - 512;
		SetHandleSize((Handle)aPictHandle, readSize);
		HLock((Handle)aPictHandle);
		SetFPos(mySI->dFileNum,fsFromStart,512);
        error = FSRead(mySI->dFileNum, &readSize, (*aPictHandle));
		HUnlock((Handle)aPictHandle);
		numberPICTs = 1;
	}

	MoveHHi((Handle)aPictHandle);
	HLock((Handle)aPictHandle);
	// find size of picture for FFT size
	height = (**aPictHandle).picFrame.bottom - (**aPictHandle).picFrame.top;
	width = (**aPictHandle).picFrame.right - (**aPictHandle).picFrame.left;
	HUnlock((Handle)aPictHandle);
	if(fndrInfo.fdType == 'SCRN' || fndrInfo.fdType == 'CSCR')
	{
		CloseResFile(mySI->rFileNum);
		ReleaseResource((Handle)aPictHandle);
		UseResFile(gAppFileNum);
	}
	else
	{
		DisposeHandle((Handle)aPictHandle);
		FSClose(mySI->dFileNum);
	}
	for(mySI->spectFrameSize = 1; mySI->spectFrameSize < ((height-1) * 2);)
		mySI->spectFrameSize <<= 1;
	mySI->spectFrameIncr = mySI->spectFrameSize/8;
	mySI->sRate = 44100.0;
	mySI->nChans = MONO;
	mySI->frameSize = 2; // bogus info
	mySI->numBytes = mySI->spectFrameIncr * width * numberPICTs * mySI->frameSize;
	mySI->dataStart = 0L;
	mySI->packMode = SF_FORMAT_PICT;
	gSpectPICTWidth = width;
	return(TRUE);
}

// most SoundInfo stuff can be gathered from the movie
short ReadMovieHeader(SoundInfo *mySI)
{
	Boolean		gotVideo;
	PicHandle	aPictHandle;
	long		numberFrames, height, width, error, i;
	Movie		theMovie;
	Track		theTrack;
	Media		theMedia;
	TimeValue	time;
	OSType		myMediaType;
	
	error = OpenMovieFile (&(mySI->sfSpec), &mySI->rFileNum, fsCurPerm);
	if (error == noErr) 
	{
		short	movieResID = 0;	// want first movie */	
		Str255	movieName;
		Boolean wasChanged;
		
		error = NewMovieFromFile (&theMovie, mySI->rFileNum, &movieResID,
					movieName, newMovieActive, &wasChanged);
		
	}
	/* create a track and PICT media */
	gotVideo = false;
	for (i = 1; ((i <= GetMovieTrackCount(theMovie)) && (!gotVideo)); i++)
	{
		theTrack = GetMovieIndTrack(theMovie, i);
		theMedia = GetTrackMedia(theTrack);
		GetMediaHandlerDescription(theMedia, &myMediaType, nil, nil);
		if (myMediaType == VideoMediaType)
			gotVideo = true;
	}
	if(gotVideo == false) 
	{
		CloseMovieFile(mySI->rFileNum);
		DisposeMovie(theMovie);
		mySI->spectFrameSize = 0;
		mySI->spectFrameIncr = mySI->spectFrameSize/8;
		mySI->sRate = 44100.0;
		mySI->nChans = MONO;
		mySI->frameSize = 2; // bogus info
		mySI->numBytes = 0;
		mySI->dataStart = 0L;
		mySI->packMode = SF_FORMAT_PICT;
		return(TRUE);
	}
		
	GetTrackNextInterestingTime(theTrack, nextTimeMediaSample, 0, 1, &time, nil);
	aPictHandle = GetTrackPict(theTrack, time);
	if(aPictHandle == 0) 
	{
//		DrawErrorMessage("\pNot enough memory to read QuickTimeª movie");
		CloseMovieFile(mySI->rFileNum);
		DisposeMovie(theMovie);
		mySI->spectFrameSize = 0;
		mySI->spectFrameIncr = mySI->spectFrameSize/8;
		mySI->sRate = 44100.0;
		mySI->nChans = MONO;
		mySI->frameSize = 2; // bogus info
		mySI->numBytes = 0;
		mySI->dataStart = 0L;
		mySI->packMode = SF_FORMAT_PICT;
		return(-1L);
	}
	MoveHHi((Handle)aPictHandle);
	HLock((Handle)aPictHandle);
	
	// find size of picture for FFT size
	height = (**aPictHandle).picFrame.bottom - (**aPictHandle).picFrame.top;
	width = (**aPictHandle).picFrame.right - (**aPictHandle).picFrame.left;
	HUnlock((Handle)aPictHandle);
	DisposeHandle((Handle)aPictHandle);
	numberFrames = GetMediaSampleCount(theMedia);
	
	CloseMovieFile(mySI->rFileNum);
	DisposeMovie(theMovie);
	
	for(mySI->spectFrameSize = 1; mySI->spectFrameSize < ((height-1) * 2);)
		mySI->spectFrameSize <<= 1;
	mySI->spectFrameIncr = mySI->spectFrameSize/8;
	mySI->sRate = 44100.0;
	mySI->nChans = MONO;
	mySI->frameSize = 2; // bogus info
	mySI->numBytes = mySI->spectFrameIncr * width * numberFrames * mySI->frameSize;
	mySI->dataStart = 0L;
	mySI->packMode = SF_FORMAT_PICT;
	gSpectPICTWidth = width;
	return(TRUE);
}

// using quicktime to gather sound information
short ReadMPEGHeader(SoundInfo *mySI)
{
	SoundDescriptionV1Handle sourceSoundDescription;
	AudioFormatAtomPtr outAudioAtom;
	long dFileEOF;
	OSErr err;
	
	if(OpenSoundFindType(mySI->sfSpec) != MPEG)
		return(-1);
	sourceSoundDescription = (SoundDescriptionV1Handle)NewHandle(0);
	err = MyGetSoundDescription(&(mySI->sfSpec), sourceSoundDescription, &outAudioAtom, &mySI->timeScale);
	mySI->peak = mySI->peakFL = mySI->peakFR = mySI->peakRL = mySI->peakRR = 1.0;
	mySI->spectFrameSize = 0;
	mySI->spectFrameIncr = mySI->spectFrameSize/8;
	mySI->sRate = (*sourceSoundDescription)->desc.sampleRate >> 16;
	mySI->nChans = (*sourceSoundDescription)->desc.numChannels;
	mySI->frameSize = (float)(*sourceSoundDescription)->bytesPerPacket/(*sourceSoundDescription)->samplesPerPacket;
	mySI->dataStart = 0L;
	GetEOF(mySI->dFileNum, &dFileEOF);
	mySI->dataEnd = dFileEOF;
	mySI->numBytes = mySI->dataEnd - mySI->dataStart;
	mySI->packMode = SF_FORMAT_MPEG_III;
	DisposeHandle((Handle)sourceSoundDescription);
	if(err == noErr)
		return(TRUE);
	else 
		return(-1);

}


short ReadSDIFHeader(SoundInfo *mySI)
{
	FILE *				file;
	SDIF_FrameHeader fh;
	SDIF_MatrixHeader mh;
	SDIFresult r;
	sdif_int32 streamID;
	unsigned long	framePosition;
	float	sampleRateF;
	float	frameSecondsF;
	float	fftSizeF;
	float	dummyF;
	double	sampleRateD;
	double	frameSecondsD;
	double	fftSizeD;
	long	numFramesRead = 0;
	float	firstFrameTime;
	long	seekBytes, dFileEOF;
	char	path[255];

	if(OpenSoundFindType(mySI->sfSpec) != SDIFF)
		return(-1);

	FSS2Path(path, &(mySI->sfSpec));
	FSpOpenDF(&(mySI->sfSpec), fsCurPerm, &mySI->dFileNum);
	file = fopen(path, "wb");
	fseek(file, 0, SEEK_SET);
	if(SDIF_BeginRead(file) != ESDIF_SUCCESS)
		return(-1);
	fgetpos(file, &framePosition);
	while ((r = SDIF_ReadFrameHeader(&fh, file)) == ESDIF_SUCCESS) 
	{
		if(SDIF_Char4Eq(fh.frameType, "1STF"))
		{
        	if ((r = SDIF_ReadMatrixHeader(&mh, file)) == ESDIF_SUCCESS) 
        	{
				if(SDIF_Char4Eq(mh.matrixType, "ISTF"))
				{
					if(mh.matrixDataType == SDIF_FLOAT32)
					{
						if (r = SDIF_Read4(&sampleRateF,1,file)) return(-1);
						if (r = SDIF_Read4(&frameSecondsF,1,file)) return(-1);
						if (r = SDIF_Read4(&fftSizeF,1,file)) return(-1);
						if (r = SDIF_Read4(&dummyF,1,file)) return(-1);
						if(fh.streamID == 0)
						{
							if(numFramesRead == 0)
							{
								mySI->sRate = sampleRateF;
								mySI->nChans = 1;
								mySI->dataStart = framePosition;
								mySI->spectFrameSize = fftSizeF;
								GetEOF(mySI->dFileNum, &dFileEOF);
								mySI->numBytes = dFileEOF - mySI->dataStart;
								mySI->dataEnd = dFileEOF;
								mySI->packMode = SF_FORMAT_SPECT_COMPLEX;
								firstFrameTime = fh.time;
							}
							else
							{
								mySI->spectFrameIncr = (fh.time - firstFrameTime) * sampleRateF;
								// frameSize represents the amount of data for each sample
								// so we need to divide the actual frameSize by the
								// increment (which is the number of new samples per fft)
								mySI->frameSize = seekBytes;
								// each frame has
								// a frame header 			- 8
								// header for info matrix 	- sizeof(mh) 
								// data in info matrix		- 4 * sizeof(float)
								// header for data matrix	- sizeof(mh)
								// data in data matrix		- already accounted for
								mySI->frameSize += 8 + sizeof(mh) + (4 * sizeof(float)) + sizeof(mh);
								mySI->frameSize = mySI->frameSize/mySI->spectFrameIncr;
							}
							numFramesRead++;
						}
						else
							mySI->nChans++;
						if(numFramesRead > 1)
							return(TRUE);
						else
						{
							// read the 1STF matrix in order to get to the next frame
        					if ((r = SDIF_ReadMatrixHeader(&mh, file)) == ESDIF_SUCCESS) 
        					{
								if(SDIF_Char4Eq(mh.matrixType, "1STF"))
								{
									if(mh.matrixDataType == SDIF_FLOAT32)
									{
										seekBytes = mh.rowCount * mh.columnCount * sizeof(float);
										fseek(file, seekBytes, SEEK_CUR);
									}
									else if(mh.matrixDataType == SDIF_FLOAT64)
									{
										seekBytes = mh.rowCount * mh.columnCount * sizeof(double);
										fseek(file, seekBytes, SEEK_CUR);
									}
								}
							}
						}
					}
					else if(mh.matrixDataType == SDIF_FLOAT64)
					{
						if (r = SDIF_Read8(&sampleRateD,1,file)) return(-1);
						if (r = SDIF_Read8(&frameSecondsD,1,file)) return(-1);
						if (r = SDIF_Read8(&fftSizeD,1,file)) return(-1);
						mySI->sRate = sampleRateD;
						mySI->nChans = 1;
						mySI->dataStart = framePosition;
						mySI->spectFrameSize = fftSizeD;
						GetEOF(mySI->dFileNum, &dFileEOF);
						mySI->numBytes = dFileEOF - mySI->dataStart;
						mySI->dataEnd = dFileEOF;
						mySI->packMode = SF_FORMAT_SPECT_COMPLEX;
						mySI->spectFrameIncr = frameSecondsD * sampleRateD;
						mySI->frameSize = ((mySI->spectFrameSize>>1) + 1) * sizeof(double);
						// each frame has
						// a frame header 			- fh.size + 8
						// header for info matrix 	- sizeof(mh) 
						// data in info matrix		- 3 * sizeof(double)
						// header for data matrix	- sizeof(mh)
						// data in data matrix		- already accounted for
						mySI->frameSize += fh.size + 8 + sizeof(mh) + (3 * sizeof(double)) + sizeof(mh);
						mySI->frameSize = mySI->frameSize/mySI->spectFrameIncr;
						return(TRUE);
					}
					else
						return(-1);
				}
			}
			else
				return(-1);
		}
		else
		{ 
			if (r = SDIF_SkipFrame(&fh, file)) 
				return(-1);
			fgetpos(file, &framePosition);
		}
	}
	return(-1);
}