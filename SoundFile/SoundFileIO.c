/*
 *	SoundHackª
 *	Copyright ©1994 Tom Erbe - CalArts
 *
 *	SoundFileIO.c - functions to read and write blocks of sound to any packmode of
 *	monaural or sterephonic soundfile. The blocks are in floating point representation
 *	with a range of -1.0 to 1.0 (usually).
 *
 *	Functions:
 *	SetOutputScale()	- set up all the scaling globals
 *	ReadStereoBlock()	- Read two blocks of floats from a soundfile with any number
 *						  of channels. It will fill both blocks with the same numbers
 *						  if the file being read is monaural. It returns the number of
 *						  frames read.
 *	ReadMonoBlock()		- Read a blocks of floats from a monaural soundfile. It returns
 *						  the number of frames read.
 *	WriteStereoBlock()	- Write two blocks of floats to a stereophonic soundfile. It
 *						  returns the number of frames written.
 *	ReadMonoBlock()		- Write a blocks of floats to a monaural soundfile. It
 *						  returns the number of frames written.
 */


#include <string.h>
#include <stdio.h> 
#include <stdlib.h>
//#include <AppleEvents.h>
//#include <SoundComponents.h>
#include <QuickTime/Movies.h>
#include "SoundFile.h"
#include "SoundHack.h"
#include "ByteSwap.h"
#include "ADPCMDVI.h"
#include "AppleCompression.h"
#include "muLaw.h"
#include "ALaw.h"
#include "Misc.h"
#include "Menu.h"
#include "Dialog.h"
#include "ShowWave.h"


extern	short	ulaw_decode[];
short	*shortBlock;
char	*charBlock;
long	*longBlock;
float	*floatBlock;
long	gIOSamplesAllocated;
extern	short	gProcessEnabled, gStopProcess;
extern	long	gNumberBlocks;
extern	float	gScale, gScaleL, gScaleR, gScaleDivisor;
extern	MenuHandle	gSigDispMenu;
extern SoundInfoPtr	inSIPtr, filtSIPtr, outSIPtr, outSteadSIPtr, outTransSIPtr;


void
AllocateSoundIOMemory(short channels, long frames)
{
	long samples;
	
	samples = frames * channels;
	if(gIOSamplesAllocated == samples)
		return;
	else if(gIOSamplesAllocated == 0)
	{
		charBlock = (char *)malloc(sizeof(char)*samples);
		shortBlock = (short *)malloc(sizeof(short)*samples);
		longBlock = (long *)malloc(sizeof(long)*samples);
		floatBlock = (float *)malloc(sizeof(float)*samples);
		gIOSamplesAllocated = samples;
	}
	else if(samples > gIOSamplesAllocated || samples < (gIOSamplesAllocated >> 3))
	{
		charBlock = (char *)realloc((void *)charBlock, sizeof(char)*samples);
		shortBlock = (short *)realloc(shortBlock, sizeof(short)*samples);
		longBlock = (long *)realloc(longBlock, sizeof(long)*samples);
		floatBlock = (float *)realloc(floatBlock, sizeof(float)*samples);
		gIOSamplesAllocated = samples;
	}
}

void
SetOutputScale(long packMode)
{
	switch(packMode)
	{
		case SF_FORMAT_8_LINEAR:
		case SF_FORMAT_8_UNSIGNED:
			gScaleL = gScaleR = gScale = gScaleDivisor * 128.0;
			break;
		case SF_FORMAT_4_ADDVI:
		case SF_FORMAT_8_MULAW:
		case SF_FORMAT_8_ALAW:
		case SF_FORMAT_16_LINEAR:
		case SF_FORMAT_3DO_CONTENT:
		case SF_FORMAT_16_SWAP:
			gScaleL = gScaleR = gScale = gScaleDivisor * 32768.0;
			break;
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_COMP:
		case SF_FORMAT_24_SWAP:
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
		case SF_FORMAT_32_SWAP:
			gScaleL = gScaleR = gScale = gScaleDivisor * 2147483648.0;
			break;
		case SF_FORMAT_32_FLOAT:
		case SF_FORMAT_TEXT:
			gScaleL = gScaleR = gScale = gScaleDivisor;
			break;
	}
}
void SetSecondsPosition(SoundInfo *mySI, double seconds)
{
	unsigned long position, frames;
	
	switch(mySI->packMode)
	{
		case SF_FORMAT_4_ADIMA:
		case SF_FORMAT_MACE3:
		case SF_FORMAT_MACE6:
		case SF_FORMAT_MPEG_I:
		case SF_FORMAT_MPEG_II:
		case SF_FORMAT_MPEG_III:
				mySI->timeValue = seconds * mySI->timeScale;
		default:
			frames = seconds * mySI->sRate;
			position = frames * mySI->nChans * mySI->frameSize;
			SetFPos(mySI->dFileNum, fsFromStart, (mySI->dataStart + position));
			break;
	}
}


/*
 * This function will read two blocks of floats from a soundfile with
 * any packmode and number of channels. It will fill both blocks with
 * the same samples if the file being read is mono
 */
	
long	ReadMonoBlock(SoundInfo *mySI, long numSamples, float *block)
{
	long	i, j, readSize, charReadSize, position, dataLeft, tmpLong;
	Str255	numStr, beginStr, endStr;
	char	c;
	short	curMark, shortSam;
	unsigned short uShortSam;
	unsigned long *longSwap;
	float	beginTime, endTime;
	double	tmpFloat, oneOver2147483648, oneOver32768, oneOver128, oneOver2048;
	static long	lastLongSam;
	Boolean	init = FALSE;
	OSErr	error;
	unsigned short EORvalue = 170;
	
	oneOver2147483648 = 1.0/2147483648.0;	// 32-bit
	oneOver32768 = 1.0/32768.0;
	oneOver2048 = 1.0/2048.0;	// 12 bit
	oneOver128 = 1.0/128.0;
	if(gNumberBlocks == 0)
		lastLongSam = 0;
	
	AllocateSoundIOMemory(1, numSamples);
	GetFPos(mySI->dFileNum, &position);
	mySI->dataEnd = mySI->numBytes - mySI->dataStart;
	dataLeft = mySI->dataEnd - position;
	readSize = numSamples * mySI->frameSize;
	if(dataLeft < readSize)
		readSize = dataLeft;
	switch(mySI->packMode)
	{
// read into charBlock - no swabbing necessary
		case SF_FORMAT_4_ADDVI:
		{
		/*	This is a real mess with all these local block variables */
			short shortOne, shortTwo;
			static long lastEstimate, lastStepSize, lastStepIndex;
			
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = j = 0; i < numSamples; i += 2, j++)
			{
					if(i == 0 && gNumberBlocks == 0)
						init = TRUE;
					else
						init = FALSE;
				ADDVIDecode(*(charBlock+j), &shortOne, &shortTwo, mySI->nChans, init);
				block[i] = shortOne * oneOver32768;
				block[i+1] = shortTwo * oneOver32768;
			}
			break;
		}
// all swabbing in AppleCompression.c
		case SF_FORMAT_4_ADIMA:
		case SF_FORMAT_MACE3:
		case SF_FORMAT_MACE6:
		case SF_FORMAT_MPEG_I:
		case SF_FORMAT_MPEG_II:
		case SF_FORMAT_MPEG_III:
			numSamples = AppleCompressed2Float(mySI, numSamples, block, block);
			readSize = mySI->nChans * mySI->frameSize * numSamples;
			break;
// read into charBlock - no swabbing necessary
		case SF_FORMAT_8_LINEAR:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = 0; i < numSamples; i++)
				block[i] = *(charBlock+i) * oneOver128;
			break;	
// read into charBlock - no swabbing necessary
		case SF_FORMAT_8_UNSIGNED:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = 0; i < numSamples; i++)
				block[i] = (char)(*(charBlock+i) ^ 0x80) * oneOver128;
			break;	
// read into charBlock - no swabbing necessary
		case SF_FORMAT_8_MULAW:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = 0; i < numSamples; i++)
				block[i] = Ulaw2Float(*(charBlock+i)) * oneOver32768;
			break;
// read into charBlock - no swabbing necessary
		case SF_FORMAT_8_ALAW:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = 0; i < numSamples; i++)
				block[i] = Alaw2Float(*(charBlock+i)) * oneOver32768;
			break;
		case SF_FORMAT_3DO_CONTENT:
			for(i = 0; i < numSamples; i++)
			{
				shortSam = *(shortBlock+i);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				short tmpHi = shortSam & 0xFF00;
				short tmpLo = (shortSam & 0x00FF) ^ EORvalue;
				short tmpLoHi = tmpLo | tmpHi;
				block[i] = tmpLoHi * oneOver32768;
			}
			break;
		case SF_FORMAT_TX16W:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			for(i = 0, j = 0; i < numSamples; i+=4, j+=3)
			{
				uShortSam = *(shortBlock+j);
#ifdef SHOSXUB
				uShortSam = EndianU16_BtoN(uShortSam);
#endif
				unsigned char uc1 = (uShortSam & 0xff00) >> 8;
				unsigned char uc2 = (uShortSam & 0x00ff);
				uShortSam = *(shortBlock+j+1);
#ifdef SHOSXUB
				uShortSam = EndianU16_BtoN(uShortSam);
#endif
				unsigned char uc3 = (uShortSam & 0xff00) >> 8;
				unsigned char uc4 = (uShortSam & 0x00ff);
				uShortSam = *(shortBlock+j+2);
#ifdef SHOSXUB
				uShortSam = EndianU16_BtoN(uShortSam);
#endif
				unsigned char uc5 = (uShortSam & 0xff00) >> 8;
				unsigned char uc6 = (uShortSam & 0x00ff);
	    		unsigned short s1 = (unsigned short) (uc1 << 4) | (((uc2 >> 4) & 017));
	    		unsigned short s2 = (unsigned short) (uc3 << 4) | (( uc2 & 017 ));
	    		unsigned short s3 = (unsigned short) (uc4 << 4) | (((uc5 >> 4) & 017));
	    		unsigned short s4 = (unsigned short) (uc6 << 4) | (( uc5 & 017 ));
				short sampleOne = s1 << 4;
				short sampleTwo = s2 << 4;
				short sampleThree = s3 << 4;
				short sampleFour = s4 << 4;
				block[i] = sampleOne * oneOver32768;
				block[i+1] = sampleTwo * oneOver32768;
				block[i+2] = sampleThree * oneOver32768;
				block[i+3] = sampleFour * oneOver32768;
			}
			break;
		case SF_FORMAT_16_LINEAR:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			for(i = 0; i < numSamples; i++)
			{
				shortSam = *(shortBlock+i);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				block[i] = shortSam * oneOver32768;
			}
			break;
		case SF_FORMAT_16_SWAP:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			for(i = 0; i < numSamples; i++)
			{
				shortSam = *(shortBlock+i);
#ifdef SHOSXUB
				shortSam = EndianS16_LtoN(shortSam);
#else
				shortSam = ByteSwapShort(shortSam);
#endif
				block[i] = shortSam * oneOver32768;
			}
			break;
// swap - yikes
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_COMP:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
#ifdef SHOSXUB
#if defined (__i386__)
			for(i = j = 0; i < numSamples; i += 4, j += 3)
			{
				tmpLong = (EndianU32_BtoN(*(longBlock+j) & 0x00ffffffL));
				block[i] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  ((EndianU32_BtoN(*(longBlock+j) & 0xff000000L)) << 24) 
						+ ((EndianU32_BtoN(*(longBlock+j+1) & 0x0000ffffL)) >> 8);
				block[i+1] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  ((EndianU32_BtoN(*(longBlock+j+1) & 0xffff0000L)) << 16) 
						+ ((EndianU32_BtoN(*(longBlock+j+2) & 0x000000ffL)) >> 16);
				block[i+2] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  (EndianU32_BtoN(*(longBlock+j+2) & 0xffffff00L)) << 8;
				block[i+3] = tmpLong * oneOver2147483648;
			}
#elif defined (__ppc__) || defined(__ppc64__)
			for(i = j = 0; i < numSamples; i += 4, j += 3)
			{
				tmpLong = *(longBlock+j) & 0xffffff00L;
				block[i] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = ((*(longBlock+j) & 0x000000ffL) << 24) + ((*(longBlock+j+1) & 0xffff0000L) >> 8);
				block[i+1] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = ((*(longBlock+j+1) & 0x0000ffffL) << 16) + ((*(longBlock+j+2) & 0xff000000L) >> 16);
				block[i+2] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = ((*(longBlock+j+2) & 0x00ffffffL) << 8);
				block[i+3] = tmpLong * oneOver2147483648;
			}
#endif
#else
			for(i = j = 0; i < numSamples; i += 4, j += 3)
			{
				tmpLong = *(longBlock+j) & 0xffffff00L;
				block[i] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = ((*(longBlock+j) & 0x000000ffL) << 24) + ((*(longBlock+j+1) & 0xffff0000L) >> 8);
				block[i+1] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = ((*(longBlock+j+1) & 0x0000ffffL) << 16) + ((*(longBlock+j+2) & 0xff000000L) >> 16);
				block[i+2] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = ((*(longBlock+j+2) & 0x00ffffffL) << 8);
				block[i+3] = tmpLong * oneOver2147483648;
			}
#endif
			break;
		case SF_FORMAT_24_SWAP:
		// big end -    A1B1C1A2 B2C2A3B3 C3A4B4C4
		// little end - C1B1A1C2 B2A2C3B3 A3C4B4A4
		// le swapped - C2A1B1C1 B3C3A2B2 A4B4C4A3

			error = FSRead(mySI->dFileNum, &readSize, longBlock);
#ifdef SHOSXUB
#if defined (__i386__)
			for(i = j = 0; i < numSamples; i += 4, j += 3)
			{
				tmpLong = (*(longBlock+j) & 0x00ffffffL) << 8;
				block[i] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = ((*(longBlock+j) & 0xff000000L) >> 16) + ((*(longBlock+j+1) & 0x0000ffffL) << 16);
				block[i+1] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = ((*(longBlock+j+1) & 0xffff0000L) >> 8) + ((*(longBlock+j+2) & 0x000000ffL) << 24);
				block[i+2] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong = (*(longBlock+j+2) & 0xffffff00L);
				block[i+3] = tmpLong * oneOver2147483648;
			}
#elif defined (__ppc__) || defined(__ppc64__)
			for(i = j = 0; i < numSamples; i += 4, j += 3)
			{
				tmpLong = (EndianU32_LtoN(*(longBlock+j)) & 0x00ffffffL) << 8;
				block[i] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  ((EndianU32_LtoN(*(longBlock+j)) & 0xff000000L) >> 16) 
						+ ((EndianU32_LtoN(*(longBlock+j+1)) & 0x0000ffffL) << 16);
				block[i+1] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  ((EndianU32_LtoN(*(longBlock+j+1)) & 0xffff0000L) >> 8) 
						+ ((EndianU32_LtoN(*(longBlock+j+2)) & 0x000000ffL) << 24);
				block[i+2] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  ((EndianU32_LtoN(*(longBlock+j+2)) & 0xffffff00L));
				block[i+3] = tmpLong * oneOver2147483648;
			}
#endif
#else
			for(i = j = 0; i < numSamples; i += 4, j += 3)
			{
				tmpLong = (ByteSwapLong(*(longBlock+j)) & 0x00ffffffL) << 8;
				block[i] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  ((ByteSwapLong(*(longBlock+j)) & 0xff000000L) >> 16) 
						+ ((ByteSwapLong(*(longBlock+j+1)) & 0x0000ffffL) << 16);
				block[i+1] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  ((ByteSwapLong(*(longBlock+j+1)) & 0xffff0000L) >> 8) 
						+ ((ByteSwapLong(*(longBlock+j+2)) & 0x000000ffL) << 24);
				block[i+2] = tmpLong * oneOver2147483648;
				if(i == numSamples)
					break;
				tmpLong =  ((ByteSwapLong(*(longBlock+j+2)) & 0xffffff00L));
				block[i+3] = tmpLong * oneOver2147483648;
			}
#endif
			break;
		case SF_FORMAT_32_SWAP:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
				for(i = 0; i < numSamples; i++)
#ifdef SHOSXUB
					block[i] = EndianS32_LtoN(*(longBlock+i)) * oneOver2147483648;
#else
					block[i] = ByteSwapLong(*(longBlock+i)) * oneOver2147483648;
#endif
			break;
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
				for(i = 0; i < numSamples; i++)
#ifdef SHOSXUB
					block[i] = EndianS32_BtoN(*(longBlock+i)) * oneOver2147483648;
#else
					block[i] = *(longBlock+i) * oneOver2147483648;
#endif
			break;
		case SF_FORMAT_32_FLOAT:
			error = FSRead(mySI->dFileNum, &readSize, floatBlock);
			for(i = 0; i < numSamples; i++)
			{
#ifdef SHOSXUB
				longSwap = floatBlock+i;
				*longSwap = EndianU32_BtoN(*longSwap);
#endif
				block[i] = *(floatBlock+i);
			}
			break;
		case SF_FORMAT_TEXT:
			charReadSize = 1;
			mySI->frameSize = 1;
			for(j = 0; j < numSamples; j++)
			{
				numStr[0] = 0;
				for(i = 0; i < 255; i++)
				{
					error = FSRead(mySI->dFileNum, &charReadSize, &c);
					if(c == 0xd)
						break;
					numStr[0]++;
					numStr[i+1] = c;
				}
				StringToFix(numStr, &tmpFloat);
				block[j] = tmpFloat;
				if(charReadSize == 0)
					numSamples = j;
				readSize = numSamples;
			}
			break;
	}
	if(mySI->packMode != SF_FORMAT_4_ADIMA && mySI->packMode != SF_FORMAT_MACE3 && mySI->packMode != SF_FORMAT_MACE6
		 && mySI->packMode != SF_FORMAT_MPEG_III && mySI->packMode != SF_FORMAT_MPEG_II && mySI->packMode != SF_FORMAT_MPEG_I)
		numSamples = readSize/mySI->frameSize;
	curMark = noMark;
	if(mySI == inSIPtr)
		GetItemMark(gSigDispMenu, DISP_INPUT_ITEM, &curMark);
	else if(mySI == filtSIPtr)
		GetItemMark(gSigDispMenu, DISP_AUX_ITEM, &curMark);
	if((curMark != noMark) & (gProcessEnabled != COPY_PROCESS) & (gProcessEnabled != NORM_PROCESS))
	{
		GetFPos(mySI->dFileNum, &position);
		dataLeft = position - mySI->dataStart;
		endTime = (dataLeft/(mySI->frameSize*mySI->nChans*mySI->sRate));
		beginTime = endTime - (numSamples/mySI->sRate);
		HMSTimeString(beginTime, beginStr);
		HMSTimeString(endTime, endStr);
		DisplayMonoWave(block, mySI, numSamples, beginStr, endStr);
	}
	if(numSamples >= 256)
	{
		BlockMove(block ,mySI->infoSamps, 256*sizeof(float));		
		mySI->infoUpdate = true;
	}
	else
	{
		BlockMove(mySI->infoSamps+numSamples, mySI->infoSamps, (256-numSamples)*sizeof(float));
		BlockMove(block ,mySI->infoSamps+(256-numSamples), numSamples*sizeof(float));		
		mySI->infoUpdate = true;
	}
	return(numSamples);
}

long
ReadStereoBlock(SoundInfo *mySI, long numSamples, float blockL[], float blockR[])
{
	long	i, j, readSize, charReadSize, position, dataLeft, tmpLong;
	Str255	numStr, beginStr, endStr;
	char	c;
	float	beginTime, endTime;
	short	curMark;
	double	tmpFloat, oneOver128, oneOver32768, oneOver2147483648, 	oneOver2048;
	static	long lastLongSamL, lastLongSamR;
	Boolean	init = FALSE;
	OSErr	error;
	unsigned short EORvalue = 170;
	short shortSam;
	unsigned short uShortSam;
	unsigned long *longSwap;
	
	oneOver2147483648 = 1.0/2147483648.0;	// 32-bit
	oneOver2048 = 1.0/2048.0;	// 12 bit
	oneOver32768 = 1.0/32768.0;				// 16-bit
	oneOver128 = 1.0/128.0;					// 8-bit
	if(gNumberBlocks == 0)
		lastLongSamL = lastLongSamR = 0;
	
	AllocateSoundIOMemory(2, numSamples);
	GetFPos(mySI->dFileNum, &position);
	mySI->dataEnd = mySI->numBytes - mySI->dataStart;
	dataLeft = mySI->dataEnd - position;
	readSize = numSamples * (mySI->nChans * mySI->frameSize);
	if(dataLeft < readSize)
		readSize = dataLeft;
	switch(mySI->packMode)
	{
// read into charBlock - no swabbing necessary
		case SF_FORMAT_4_ADDVI:
		{
		/* The format here is interleaved nibbles for stereo. */
			short shortOne, shortTwo;
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			if(mySI->nChans == STEREO)
				for(i = 0; i < numSamples; i++)
				{
					if(i == 0 && gNumberBlocks == 0)
						init = TRUE;
					else
						init = FALSE;
					ADDVIDecode(*(charBlock+i), &shortOne, &shortTwo, mySI->nChans, init);
					blockL[i] = shortOne * oneOver32768;
					blockR[i] = shortTwo * oneOver32768;

				}
			else
				for(i = j = 0; i < numSamples; i += 2, j++)
				{
					if(i == 0 && gNumberBlocks == 0)
						init = TRUE;
					else
						init = FALSE;
					ADDVIDecode(*(charBlock+j), &shortOne, &shortTwo, mySI->nChans, init);
					blockL[i] = shortOne * oneOver32768;
					blockL[i+1] = shortTwo * oneOver32768;
					blockR[i] = blockL[i];
					blockR[i+1] = blockL[i+1];
				}
			break;
		}
		case SF_FORMAT_4_ADIMA:
		case SF_FORMAT_MACE3:
		case SF_FORMAT_MACE6:
		case SF_FORMAT_MPEG_I:
		case SF_FORMAT_MPEG_II:
		case SF_FORMAT_MPEG_III:
			numSamples = AppleCompressed2Float(mySI, numSamples, blockL, blockR);
			readSize = mySI->nChans * mySI->frameSize * numSamples;
			break;
// read into charBlock - no swabbing necessary
		case SF_FORMAT_8_LINEAR:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			if(mySI->nChans == STEREO)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
					blockL[i] = *(charBlock+j) * oneOver128;
					blockR[i] = *(charBlock+j+1) * oneOver128;
				}
			else
				for(i = 0; i < numSamples; i++)
				{
					blockL[i] = *(charBlock+i) * oneOver128;
					blockR[i] = *(charBlock+i) * oneOver128;
				}
			break;	
// read into charBlock - no swabbing necessary
		case SF_FORMAT_8_UNSIGNED:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			if(mySI->nChans == STEREO)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
					blockL[i] = (char)(*(charBlock+j) ^ 0x80) * oneOver128;
					blockR[i] = (char)(*(charBlock+j+1) ^ 0x80) * oneOver128;
				}
			else
				for(i = 0; i < numSamples; i++)
				{
					blockL[i] = (char)(*(charBlock+i) ^ 0x80) * oneOver128;
					blockR[i] = (char)(*(charBlock+i) ^ 0x80) * oneOver128;
				}
			break;	
// read into charBlock - no swabbing necessary
		case SF_FORMAT_8_MULAW:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			if(mySI->nChans == STEREO)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
					blockL[i] = Ulaw2Float(*(charBlock+j)) * oneOver32768;
					blockR[i] = Ulaw2Float(*(charBlock+j+1)) * oneOver32768;
				}
			else
				for(i = 0; i < numSamples; i++)
				{
					blockL[i] = Ulaw2Float(*(charBlock+i)) * oneOver32768;
					blockR[i] = Ulaw2Float(*(charBlock+i)) * oneOver32768;
				}
			break;
// read into charBlock - no swabbing necessary
		case SF_FORMAT_8_ALAW:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			if(mySI->nChans == STEREO)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
					blockL[i] = Alaw2Float(*(charBlock+j)) * oneOver32768;
					blockR[i] = Alaw2Float(*(charBlock+j+1)) * oneOver32768;
				}
			else
				for(i = 0; i < numSamples; i++)
				{
					blockL[i] = Alaw2Float(*(charBlock+i)) * oneOver32768;
					blockR[i] = Alaw2Float(*(charBlock+i)) * oneOver32768;
				}
			break;
		case SF_FORMAT_3DO_CONTENT:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			if(mySI->nChans == STEREO)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
					shortSam = *(shortBlock+j);
#ifdef SHOSXUB
					shortSam = EndianS16_BtoN(shortSam);
#endif
					short tmpHi = shortSam & 0xFF00;
					short tmpLo = (shortSam & 0x00FF) ^ EORvalue;
					short tmpLoHi = tmpLo | tmpHi;
					blockL[i] = tmpLoHi * oneOver32768;
					
					shortSam = *(shortBlock+j+1);
#ifdef SHOSXUB
					shortSam = EndianS16_BtoN(shortSam);
#endif
					tmpHi = shortSam & 0xFF00;
					tmpLo = (shortSam & 0x00FF) ^ EORvalue;
					tmpLoHi = tmpLo | tmpHi;
					blockR[i] = tmpLoHi * oneOver32768;
				}
			else
				for(i = 0; i < numSamples; i++)
				{
					shortSam = *(shortBlock+i);
#ifdef SHOSXUB
					shortSam = EndianS16_BtoN(shortSam);
#endif
					short tmpHi = shortSam & 0xFF00;
					short tmpLo = (shortSam & 0x00FF) ^ EORvalue;
					short tmpLoHi = tmpLo | tmpHi;
					blockR[i] = blockL[i] = tmpLoHi * oneOver32768;
				}
			break;
		case SF_FORMAT_16_LINEAR:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			if(mySI->nChans == STEREO)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
					shortSam = *(shortBlock+j);
#ifdef SHOSXUB
					shortSam = EndianS16_BtoN(shortSam);
#endif
					blockL[i] = shortSam * oneOver32768;
					shortSam = *(shortBlock+j+1);
#ifdef SHOSXUB
					shortSam = EndianS16_BtoN(shortSam);
#endif
					blockR[i] = shortSam * oneOver32768;
				}
			else
				for(i = 0; i < numSamples; i++)
				{
					shortSam = *(shortBlock+i);
#ifdef SHOSXUB
					shortSam = EndianS16_BtoN(shortSam);
#endif
					blockR[i] = blockL[i] = shortSam * oneOver32768;
				}
			break;
		case SF_FORMAT_16_SWAP:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			if(mySI->nChans == STEREO)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
#ifdef SHOSXUB
					blockL[i] = EndianS16_LtoN(*(shortBlock+j)) * oneOver32768;
					blockR[i] = EndianS16_LtoN(*(shortBlock+j+1)) * oneOver32768;
#else
					blockL[i] = ByteSwapShort(*(shortBlock+j)) * oneOver32768;
					blockR[i] = ByteSwapShort(*(shortBlock+j+1)) * oneOver32768;
#endif
				}
			else
				for(i = 0; i < numSamples; i++)
				{
#ifdef SHOSXUB
					blockL[i] = EndianS16_LtoN(*(shortBlock+i)) * oneOver32768;
					blockR[i] = EndianS16_LtoN(*(shortBlock+i)) * oneOver32768;
#else
					blockL[i] = ByteSwapShort(*(shortBlock+i)) * oneOver32768;
					blockR[i] = ByteSwapShort(*(shortBlock+i)) * oneOver32768;
#endif
				}
			break;
		case SF_FORMAT_TX16W:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			for(i = 0, j = 0; i < numSamples; i+=4, j+=3)
			{
				uShortSam = *(shortBlock+j);
#ifdef SHOSXUB
				uShortSam = EndianU16_BtoN(uShortSam);
#endif
				unsigned char uc1 = (uShortSam & 0xff00) >> 8;
				unsigned char uc2 = (uShortSam & 0x00ff);
				uShortSam = *(shortBlock+j+1);
#ifdef SHOSXUB
				uShortSam = EndianU16_BtoN(uShortSam);
#endif
				unsigned char uc3 = (uShortSam & 0xff00) >> 8;
				unsigned char uc4 = (uShortSam & 0x00ff);
				uShortSam = *(shortBlock+j+2);
#ifdef SHOSXUB
				uShortSam = EndianU16_BtoN(uShortSam);
#endif
				unsigned char uc5 = (uShortSam & 0xff00) >> 8;
				unsigned char uc6 = (uShortSam & 0x00ff);
	    		unsigned short s1 = (unsigned short) (uc1 << 4) | (((uc2 >> 4) & 017));
	    		unsigned short s2 = (unsigned short) (uc3 << 4) | (( uc2 & 017 ));
	    		unsigned short s3 = (unsigned short) (uc4 << 4) | (((uc5 >> 4) & 017));
	    		unsigned short s4 = (unsigned short) (uc6 << 4) | (( uc5 & 017 ));
				short sampleOne = s1 << 4;
				short sampleTwo = s2 << 4;
				short sampleThree = s3 << 4;
				short sampleFour = s4 << 4;
				blockL[i] = blockR[i] = sampleOne * oneOver32768;
				blockL[i+1] = blockR[i+1] = sampleTwo * oneOver32768;
				blockL[i+2] = blockR[i+2] = sampleThree * oneOver32768;
				blockL[i+3] = blockR[i+3] = sampleFour * oneOver32768;
			}
			break;
		case SF_FORMAT_24_COMP:
		case SF_FORMAT_24_LINEAR:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
			if(mySI->nChans == 2)
			{
#ifdef SHOSXUB
#if defined (__i386__)
				for(i = j = 0; i < numSamples; i += 2, j += 3)
				{
					tmpLong = (EndianU32_BtoN(*(longBlock+j) & 0x00ffffffL));
					blockL[i] = tmpLong * oneOver2147483648;
					tmpLong =  ((EndianU32_BtoN(*(longBlock+j) & 0xff000000L)) << 24) 
						+ ((EndianU32_BtoN(*(longBlock+j+1) & 0x0000ffffL)) >> 8);
					blockR[i] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong =  ((EndianU32_BtoN(*(longBlock+j+1) & 0xffff0000L)) << 16) 
						+ ((EndianU32_BtoN(*(longBlock+j+2) & 0x000000ffL)) >> 16);
					blockL[i+1] = tmpLong * oneOver2147483648;
					tmpLong =  (EndianU32_BtoN(*(longBlock+j+2) & 0xffffff00L)) << 8;
					blockR[i+1] = tmpLong * oneOver2147483648;
				}
#elif defined (__ppc__) || defined(__ppc64__)
				for(i = j = 0; i < numSamples; i += 2, j += 3)
				{
					tmpLong = *(longBlock+j) & 0xffffff00L;
					blockL[i] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j) & 0x000000ffL) << 24) + ((*(longBlock+j+1) & 0xffff0000L) >> 8);
					blockR[i] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong = ((*(longBlock+j+1) & 0x0000ffffL) << 16) + ((*(longBlock+j+2) & 0xff000000L) >> 16);
					blockL[i+1] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j+2) & 0x00ffffffL) << 8);
					blockR[i+1] = tmpLong * oneOver2147483648;
				}
#endif
#else
				for(i = j = 0; i < numSamples; i += 2, j += 3)
				{
					tmpLong = *(longBlock+j) & 0xffffff00L;
					blockL[i] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j) & 0x000000ffL) << 24) + ((*(longBlock+j+1) & 0xffff0000L) >> 8);
					blockR[i] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong = ((*(longBlock+j+1) & 0x0000ffffL) << 16) + ((*(longBlock+j+2) & 0xff000000L) >> 16);
					blockL[i+1] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j+2) & 0x00ffffffL) << 8);
					blockR[i+1] = tmpLong * oneOver2147483648;
				}
#endif
			}
			else
			{
#ifdef SHOSXUB
#if defined (__i386__)
				for(i = j = 0; i < numSamples; i += 4, j += 3)
				{
					tmpLong = (EndianU32_BtoN(*(longBlock+j) & 0x00ffffffL));
					blockR[i] = blockL[i] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong =  ((EndianU32_BtoN(*(longBlock+j) & 0xff000000L)) << 24) 
						+ ((EndianU32_BtoN(*(longBlock+j+1) & 0x0000ffffL)) >> 8);
					blockR[i+1] = blockL[i+1] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong =  ((EndianU32_BtoN(*(longBlock+j+1) & 0xffff0000L)) << 16) 
						+ ((EndianU32_BtoN(*(longBlock+j+2) & 0x000000ffL)) >> 16);
					blockR[i+2] = blockL[i+2] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong =  (EndianU32_BtoN(*(longBlock+j+2) & 0xffffff00L)) << 8;
					blockR[i+3] = blockL[i+3] = tmpLong * oneOver2147483648;
				}
#elif defined (__ppc__) || defined(__ppc64__)
				for(i = j = 0; i < numSamples; i += 4, j += 3)
				{
					tmpLong = *(longBlock+j) & 0xffffff00L;
					blockR[i] = blockL[i] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong = ((*(longBlock+j) & 0x000000ffL) << 24) + ((*(longBlock+j+1) & 0xffff0000L) >> 8);
					blockR[i+1] = blockL[i+1] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong = ((*(longBlock+j+1) & 0x0000ffffL) << 16) + ((*(longBlock+j+2) & 0xff000000L) >> 16);
					blockR[i+2] = blockL[i+2] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong = ((*(longBlock+j+2) & 0x00ffffffL) << 8);
					blockR[i+3] = blockL[i+3] = tmpLong * oneOver2147483648;
				}
#endif
#else
				for(i = j = 0; i < numSamples; i += 4, j += 3)
				{
					tmpLong = *(longBlock+j) & 0xffffff00L;
					blockR[i] = blockL[i] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong = ((*(longBlock+j) & 0x000000ffL) << 24) + ((*(longBlock+j+1) & 0xffff0000L) >> 8);
					blockR[i+1] = blockL[i+1] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong = ((*(longBlock+j+1) & 0x0000ffffL) << 16) + ((*(longBlock+j+2) & 0xff000000L) >> 16);
					blockR[i+2] = blockL[i+2] = tmpLong * oneOver2147483648;
					if(i == numSamples)
						break;
					tmpLong = ((*(longBlock+j+2) & 0x00ffffffL) << 8);
					blockR[i+3] = blockL[i+3] = tmpLong * oneOver2147483648;
				}
#endif
			}
			break;
		case SF_FORMAT_24_SWAP:
		// big end -    A1B1C1A2 B2C2A3B3 C3A4B4C4
		// little end - C1B1A1C2 B2A2C3B3 A3C4B4A4
		// le swapped - C2A1B1C1 B3C3A2B2 A4B4C4A3
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
			if(mySI->nChans == 2)
			{
#ifdef SHOSXUB
				if (('1234' >> 24) == '1')
				{
					for(i = j = 0; i < numSamples; i += 2, j += 3)
					{
						tmpLong = (*(longBlock+j) & 0x00ffffffL) << 8;
						blockL[i] = tmpLong * oneOver2147483648;
						tmpLong = ((*(longBlock+j) & 0xff000000L) >> 16) + ((*(longBlock+j+1) & 0x0000ffffL) << 16);
						blockR[i] = tmpLong * oneOver2147483648;
						if(i == numSamples)
							break;
						tmpLong = ((*(longBlock+j+1) & 0xffff0000L) >> 8) + ((*(longBlock+j+2) & 0x000000ffL) << 24);
						blockL[i+1] = tmpLong * oneOver2147483648;
						tmpLong = (*(longBlock+j+2) & 0xffffff00L);
						blockR[i+1] = tmpLong * oneOver2147483648;
					}
				}
				else if(('4321' >> 24) == '1')
#endif
				{
					for(i = j = 0; i < numSamples; i += 2, j += 3)
					{
						tmpLong = (ByteSwapLong(*(longBlock+j)) & 0x00ffffffL) << 8;
						blockL[i] = tmpLong * oneOver2147483648;
						tmpLong =  ((ByteSwapLong(*(longBlock+j)) & 0xff000000L) >> 16) 
							+ ((ByteSwapLong(*(longBlock+j+1)) & 0x0000ffffL) << 16);
						blockR[i] = tmpLong * oneOver2147483648;
						if(i == numSamples)
							break;
						tmpLong =  ((ByteSwapLong(*(longBlock+j+1)) & 0xffff0000L) >> 8) 
							+ ((ByteSwapLong(*(longBlock+j+2)) & 0x000000ffL) << 24);
						blockL[i+1] = tmpLong * oneOver2147483648;
						tmpLong =  ((ByteSwapLong(*(longBlock+j+2)) & 0xffffff00L));
						blockR[i+1] = tmpLong * oneOver2147483648;
					}
				}
			}
			else
			{
#ifdef SHOSXUB
				if (('1234' >> 24) == '1')
				{
					for(i = j = 0; i < numSamples; i += 4, j += 3)
					{
						tmpLong = (*(longBlock+j) & 0x00ffffffL) << 8;
						blockR[i] = blockL[i] = tmpLong * oneOver2147483648;
						if(i == numSamples)
							break;
						tmpLong = ((*(longBlock+j) & 0xff000000L) >> 16) + ((*(longBlock+j+1) & 0x0000ffffL) << 16);
						blockR[i+1] = blockL[i+1] = tmpLong * oneOver2147483648;
						if(i == numSamples)
							break;
						tmpLong = ((*(longBlock+j+1) & 0xffff0000L) >> 8) + ((*(longBlock+j+2) & 0x000000ffL) << 24);
						blockR[i+2] = blockL[i+2] = tmpLong * oneOver2147483648;
						if(i == numSamples)
							break;
						tmpLong = (*(longBlock+j+2) & 0xffffff00L);
						blockR[i+3] = blockL[i+3] = tmpLong * oneOver2147483648;
					}
				}
				else if(('4321' >> 24) == '1')
#endif
				{
					for(i = j = 0; i < numSamples; i += 4, j += 3)
					{
						tmpLong = (ByteSwapLong(*(longBlock+j)) & 0x00ffffffL) << 8;
						blockR[i] = blockL[i] = tmpLong * oneOver2147483648;
						if(i == numSamples)
							break;
						tmpLong =  ((ByteSwapLong(*(longBlock+j)) & 0xff000000L) >> 16) 
							+ ((ByteSwapLong(*(longBlock+j+1)) & 0x0000ffffL) << 16);
						blockR[i+1] = blockL[i+1] = tmpLong * oneOver2147483648;
						if(i == numSamples)
							break;
						tmpLong =  ((ByteSwapLong(*(longBlock+j+1)) & 0xffff0000L) >> 8) 
							+ ((ByteSwapLong(*(longBlock+j+2)) & 0x000000ffL) << 24);
						blockR[i+2] = blockL[i+2] = tmpLong * oneOver2147483648;
						if(i == numSamples)
							break;
						tmpLong =  ((ByteSwapLong(*(longBlock+j+2)) & 0xffffff00L));
						blockR[i+3] = blockL[i+3] = tmpLong * oneOver2147483648;
					}
				}
			}
			break;
		case SF_FORMAT_32_SWAP:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
			if(mySI->nChans == 2)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
#ifdef SHOSXUB
					blockL[i] = EndianS32_LtoN(*(longBlock+j)) * oneOver2147483648;
					blockR[i] = EndianS32_LtoN(*(longBlock+j+1)) * oneOver2147483648;
#else
					blockL[i] = ByteSwapLong(*(longBlock+j)) * oneOver2147483648;
					blockR[i] = ByteSwapLong(*(longBlock+j+1)) * oneOver2147483648;
#endif
				}
			else
				for(i = 0; i < numSamples; i++)
				{
#ifdef SHOSXUB
					blockL[i] = EndianS32_LtoN(*(longBlock+i)) * oneOver2147483648;
					blockR[i] = EndianS32_LtoN(*(longBlock+i)) * oneOver2147483648;
#else
					blockL[i] = ByteSwapLong(*(longBlock+i)) * oneOver2147483648;
					blockR[i] = ByteSwapLong(*(longBlock+i)) * oneOver2147483648;
#endif
				}
			break;
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
			if(mySI->nChans == 2)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
#ifdef SHOSXUB
					blockL[i] = EndianS32_BtoN(*(longBlock+j)) * oneOver2147483648;
					blockR[i] = EndianS32_BtoN(*(longBlock+j+1)) * oneOver2147483648;
#else
					blockL[i] = *(longBlock+j) * oneOver2147483648;
					blockR[i] = *(longBlock+j+1) * oneOver2147483648;
#endif
				}
			else
				for(i = 0; i < numSamples; i++)
				{
#ifdef SHOSXUB
					blockL[i] = EndianS32_BtoN(*(longBlock+i)) * oneOver2147483648;
					blockR[i] = EndianS32_BtoN(*(longBlock+i)) * oneOver2147483648;
#else
					blockL[i] = *(longBlock+i) * oneOver2147483648;
					blockR[i] = *(longBlock+i) * oneOver2147483648;
#endif
				}
			break;
		case SF_FORMAT_32_FLOAT:
			error = FSRead(mySI->dFileNum, &readSize, floatBlock);
			if(mySI->nChans == 2)
				for(i = j = 0; i < numSamples; i++, j += 2)
				{
#ifdef SHOSXUB
					longSwap = floatBlock+j;
					*longSwap = EndianU32_BtoN(*longSwap);
#endif
					blockL[i] = *(floatBlock+j);
#ifdef SHOSXUB
					longSwap = floatBlock+j+1;
					*longSwap = EndianU32_BtoN(*longSwap);
#endif
					blockR[i] = *(floatBlock+j+1);
				}
			else
				for(i = 0; i < numSamples; i++)
				{
#ifdef SHOSXUB
					longSwap = floatBlock+i;
					*longSwap = EndianU32_BtoN(*longSwap);
#endif
					blockL[i] = *(floatBlock+i);
					blockR[i] = *(floatBlock+i);
				}
			break;
		case SF_FORMAT_TEXT:
			charReadSize = 1;
			mySI->frameSize = 1;
			for(j = 0; j < numSamples; j++)
			{
				numStr[0] = 0;
				for(i = 0; i < 255; i++)
				{
					error = FSRead(mySI->dFileNum, &charReadSize, &c);
					if(c == 0xd)
						break;
					numStr[0]++;
					numStr[i+1] = c;
				}
				StringToFix(numStr, &tmpFloat);
				blockL[j] = tmpFloat;
				if(mySI->nChans == STEREO)
				{
					numStr[0] = 0;
					for(i = 0; i < 255; i++)
					{
						error = FSRead(mySI->dFileNum, &charReadSize, &c);
						if(c == 0xd)
							break;
						numStr[0]++;
						numStr[i+1] = c;
					}
					StringToFix(numStr, &tmpFloat);
					blockR[j] = tmpFloat;
				}
				else
					blockR[j] = blockL[j];
				if(charReadSize == 0)
					numSamples = j;
				readSize = numSamples * mySI->nChans;
			}
			break;

	}
	if(mySI->packMode != SF_FORMAT_4_ADIMA && mySI->packMode != SF_FORMAT_MACE3 && mySI->packMode != SF_FORMAT_MACE6
		&& mySI->packMode != SF_FORMAT_MPEG_III && mySI->packMode != SF_FORMAT_MPEG_II && mySI->packMode != SF_FORMAT_MPEG_I)
		numSamples = readSize/(mySI->nChans * mySI->frameSize);
	curMark = noMark;
	if(mySI == inSIPtr)
		GetItemMark(gSigDispMenu, DISP_INPUT_ITEM, &curMark);
	else if(mySI == filtSIPtr)
		GetItemMark(gSigDispMenu, DISP_AUX_ITEM, &curMark);
	if((curMark != noMark) & (gProcessEnabled != COPY_PROCESS) & (gProcessEnabled != NORM_PROCESS))
	{
		GetFPos(mySI->dFileNum, &position);
		dataLeft = position - mySI->dataStart;
		endTime = (dataLeft/(mySI->frameSize*mySI->nChans*mySI->sRate));
		beginTime = endTime - (numSamples/mySI->sRate);
		HMSTimeString(beginTime, beginStr);
		HMSTimeString(endTime, endStr);
		if(mySI->nChans == MONO)
			DisplayMonoWave(blockL, mySI, numSamples, beginStr, endStr);
		else
			DisplayStereoWave(blockL, blockR, mySI, numSamples, beginStr, endStr);
	}
	if(numSamples >= 128)
	{
		BlockMove(blockL ,mySI->infoSamps, 128*sizeof(float));		
		BlockMove(blockR ,mySI->infoSamps+128, 128*sizeof(float));		
		mySI->infoUpdate = true;
	}
	else
	{
		BlockMove(mySI->infoSamps+numSamples, mySI->infoSamps, (128-numSamples)*sizeof(float));
		BlockMove(blockL ,mySI->infoSamps+(128-numSamples), numSamples*sizeof(float));		
		BlockMove(mySI->infoSamps+numSamples+128, mySI->infoSamps+128, (128-numSamples)*sizeof(float));
		BlockMove(blockR ,mySI->infoSamps+(256-numSamples), numSamples*sizeof(float));		
		mySI->infoUpdate = true;
	}
	return(numSamples);
}

long
ReadQuadBlock(SoundInfo *mySI, long numSamples, float blockL[], float blockR[], float block3[], float block4[])
{
	long	i, j, readSize, charReadSize, position, dataLeft, tmpLong;
	Str255	numStr, beginStr, endStr;
	char	c;
	float	beginTime, endTime;
	short	curMark;
	short	shortSam;
	unsigned long *longSwap;
	double	tmpFloat, oneOver128, oneOver32768, oneOver2147483648;
	static	long lastLongSamL, lastLongSamR, lastLongSam3, lastLongSam4;
	Boolean	init = FALSE;
	OSErr	error;
	unsigned short EORvalue = 170;
	
	oneOver2147483648 = 1.0/2147483648.0;	// 32-bit
	oneOver32768 = 1.0/32768.0;				// 16-bit
	oneOver128 = 1.0/128.0;					// 8-bit
	if(gNumberBlocks == 0)
		lastLongSamL = lastLongSamR = lastLongSam3 = lastLongSam4 = 0; 
	
	AllocateSoundIOMemory(4, numSamples);
	GetFPos(mySI->dFileNum, &position);
	mySI->dataEnd = mySI->numBytes - mySI->dataStart;
	dataLeft = mySI->dataEnd - position;
	readSize = numSamples * (mySI->nChans * mySI->frameSize);
	if( dataLeft < readSize)
		readSize = dataLeft;
	switch(mySI->packMode)
	{
		case SF_FORMAT_4_ADDVI:
		{
		/* The format here is interleaved nibbles for stereo & quad. */
			short shortOne, shortTwo, shortThree, shortFour;
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
				if(i == 0 && gNumberBlocks == 0)
					init = TRUE;
				else
					init = FALSE;
				ADDVIDecode(*(charBlock+j), &shortOne, &shortTwo, 2, init);
				ADDVIDecode(*(charBlock+j+1), &shortThree, &shortFour, 2, init);
				blockL[i] = shortOne * oneOver32768;
				blockR[i] = shortTwo * oneOver32768;
				block3[i] = shortThree * oneOver32768;
				block4[i] = shortFour * oneOver32768;
			}
			break;
		}
		case SF_FORMAT_8_LINEAR:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
				blockL[i] = *(charBlock+j) * oneOver128;
				blockR[i] = *(charBlock+j+1) * oneOver128;
				block3[i] = *(charBlock+j+2) * oneOver128;
				block4[i] = *(charBlock+j+3) * oneOver128;
			}
			break;	
		case SF_FORMAT_8_UNSIGNED:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
				blockL[i] = (char)(*(charBlock+j) ^ 0x80) * oneOver128;
				blockR[i] = (char)(*(charBlock+j+1) ^ 0x80) * oneOver128;
				block3[i] = (char)(*(charBlock+j+2) ^ 0x80) * oneOver128;
				block4[i] = (char)(*(charBlock+j+3) ^ 0x80) * oneOver128;
			}
			break;	
		case SF_FORMAT_8_MULAW:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
				blockL[i] = Ulaw2Float(*(charBlock+j)) * oneOver32768;
				blockR[i] = Ulaw2Float(*(charBlock+j+1)) * oneOver32768;
				block3[i] = Ulaw2Float(*(charBlock+j+2)) * oneOver32768;
				block4[i] = Ulaw2Float(*(charBlock+j+3)) * oneOver32768;
			}
			break;
		case SF_FORMAT_8_ALAW:
			error = FSRead(mySI->dFileNum, &readSize, charBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
				blockL[i] = Alaw2Float(*(charBlock+j)) * oneOver32768;
				blockR[i] = Alaw2Float(*(charBlock+j+1)) * oneOver32768;
				block3[i] = Alaw2Float(*(charBlock+j+2)) * oneOver32768;
				block4[i] = Alaw2Float(*(charBlock+j+3)) * oneOver32768;
			}
			break;
		case SF_FORMAT_3DO_CONTENT:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
				shortSam = *(shortBlock+j);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				short tmpHi = shortSam & 0xFF00;
				short tmpLo = (shortSam & 0x00FF) ^ EORvalue;
				short tmpLoHi = tmpLo | tmpHi;
				blockL[i] = tmpLoHi * oneOver32768;
				
				shortSam = *(shortBlock+j+1);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				tmpHi = shortSam & 0xFF00;
				tmpLo = (shortSam & 0x00FF) ^ EORvalue;
				tmpLoHi = tmpLo | tmpHi;
				blockR[i] = tmpLoHi * oneOver32768;
				shortSam = *(shortBlock+j+2);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				tmpHi = shortSam & 0xFF00;
				tmpLo = (shortSam & 0x00FF) ^ EORvalue;
				tmpLoHi = tmpLo | tmpHi;
				block3[i] = tmpLoHi * oneOver32768;
				shortSam = *(shortBlock+j+3);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				tmpHi = shortSam & 0xFF00;
				tmpLo = (shortSam & 0x00FF) ^ EORvalue;
				tmpLoHi = tmpLo | tmpHi;
				block4[i] = tmpLoHi * oneOver32768;
			}
			break;
		case SF_FORMAT_16_LINEAR:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
				shortSam = *(shortBlock+j);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				blockL[i] = shortSam * oneOver32768;
				shortSam = *(shortBlock+j+1);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				blockR[i] = shortSam * oneOver32768;
				shortSam = *(shortBlock+j+2);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				block3[i] = shortSam * oneOver32768;
				shortSam = *(shortBlock+j+3);
#ifdef SHOSXUB
				shortSam = EndianS16_BtoN(shortSam);
#endif
				block4[i] = shortSam * oneOver32768;
			}
			break;
		case SF_FORMAT_16_SWAP:
			error = FSRead(mySI->dFileNum, &readSize, shortBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
#ifdef SHOSXUB
				blockL[i] = EndianS16_LtoN(*(shortBlock+j)) * oneOver32768;
				blockR[i] = EndianS16_LtoN(*(shortBlock+j+1)) * oneOver32768;
				block3[i] = EndianS16_LtoN(*(shortBlock+j+2)) * oneOver32768;
				block4[i] = EndianS16_LtoN(*(shortBlock+j+3)) * oneOver32768;
#else
				blockL[i] = ByteSwapShort(*(shortBlock+j)) * oneOver32768;
				blockR[i] = ByteSwapShort(*(shortBlock+j+1)) * oneOver32768;
				block3[i] = ByteSwapShort(*(shortBlock+j+2)) * oneOver32768;
				block4[i] = ByteSwapShort(*(shortBlock+j+3)) * oneOver32768;
#endif
			}
			break;
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_COMP:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
#ifdef SHOSXUB
			if (('1234' >> 24) == '1')
				for(i = j = 0; i < numSamples; i ++, j += 3)
				{
					tmpLong = (EndianU32_BtoN(*(longBlock+j) & 0x00ffffffL));
					blockL[i] = tmpLong * oneOver2147483648;
					tmpLong =  ((EndianU32_BtoN(*(longBlock+j) & 0xff000000L)) << 24) 
							+ ((EndianU32_BtoN(*(longBlock+j+1) & 0x0000ffffL)) >> 8);
					blockR[i] = tmpLong * oneOver2147483648;
					tmpLong =  ((EndianU32_BtoN(*(longBlock+j+1) & 0xffff0000L)) << 16) 
							+ ((EndianU32_BtoN(*(longBlock+j+2) & 0x000000ffL)) >> 16);
					block3[i] = tmpLong * oneOver2147483648;
					tmpLong =  (EndianU32_BtoN(*(longBlock+j+2) & 0xffffff00L)) << 8;
					block4[i] = tmpLong * oneOver2147483648;
				}
			else if(('4321' >> 24) == '1')
#endif
				for(i = j = 0; i < numSamples; i ++, j += 3)
				{
					tmpLong = *(longBlock+j) & 0xffffff00L;
					blockL[i] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j) & 0x000000ffL) << 24) + ((*(longBlock+j+1) & 0xffff0000L) >> 8);
					blockR[i] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j+1) & 0x0000ffffL) << 16) + ((*(longBlock+j+2) & 0xff000000L) >> 16);
					block3[i] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j+2) & 0x00ffffffL) << 8);
					block4[i] = tmpLong * oneOver2147483648;
				}
			break;
		case SF_FORMAT_24_SWAP:
		// big end -    A1B1C1A2 B2C2A3B3 C3A4B4C4
		// little end - C1B1A1C2 B2A2C3B3 A3C4B4A4
		// le swapped - C2A1B1C1 B3C3A2B2 A4B4C4A3
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
#ifdef SHOSXUB
			if (('1234' >> 24) == '1')
				for(i = j = 0; i < numSamples; i ++, j += 3)
				{
					tmpLong = (*(longBlock+j) & 0x00ffffffL) << 8;
					blockL[i] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j) & 0xff000000L) >> 16) + ((*(longBlock+j+1) & 0x0000ffffL) << 16);
					blockR[i] = tmpLong * oneOver2147483648;
					tmpLong = ((*(longBlock+j+1) & 0xffff0000L) >> 8) + ((*(longBlock+j+2) & 0x000000ffL) << 24);
					block3[i] = tmpLong * oneOver2147483648;
					tmpLong = (*(longBlock+j+2) & 0xffffff00L);
					block4[i] = tmpLong * oneOver2147483648;
				}
			else if(('4321' >> 24) == '1')
				for(i = j = 0; i < numSamples; i++, j += 3)
				{
					tmpLong = (EndianU32_LtoN(*(longBlock+j)) & 0x00ffffffL) << 8;
					blockL[i] = tmpLong * oneOver2147483648;
					tmpLong =  ((EndianU32_LtoN(*(longBlock+j)) & 0xff000000L) >> 16) 
						+ ((EndianU32_LtoN(*(longBlock+j+1)) & 0x0000ffffL) << 16);
					blockR[i] = tmpLong * oneOver2147483648;
					tmpLong =  ((EndianU32_LtoN(*(longBlock+j+1)) & 0xffff0000L) >> 8) 
						+ ((EndianU32_LtoN(*(longBlock+j+2)) & 0x000000ffL) << 24);
					block3[i] = tmpLong * oneOver2147483648;
					tmpLong =  ((EndianU32_LtoN(*(longBlock+j+2)) & 0xffffff00L));
					block4[i] = tmpLong * oneOver2147483648;
				}
#else
			for(i = j = 0; i < numSamples; i++, j += 3)
			{
				tmpLong = (ByteSwapLong(*(longBlock+j)) & 0x00ffffffL) << 8;
				blockL[i] = tmpLong * oneOver2147483648;
				tmpLong =  ((ByteSwapLong(*(longBlock+j)) & 0xff000000L) >> 16) 
					+ ((ByteSwapLong(*(longBlock+j+1)) & 0x0000ffffL) << 16);
				blockR[i] = tmpLong * oneOver2147483648;
				tmpLong =  ((ByteSwapLong(*(longBlock+j+1)) & 0xffff0000L) >> 8) 
					+ ((ByteSwapLong(*(longBlock+j+2)) & 0x000000ffL) << 24);
				block3[i] = tmpLong * oneOver2147483648;
				tmpLong =  ((ByteSwapLong(*(longBlock+j+2)) & 0xffffff00L));
				block4[i] = tmpLong * oneOver2147483648;
			}
#endif
			break;
		case SF_FORMAT_32_SWAP:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
#ifdef SHOSXUB
				blockL[i] = EndianS32_LtoN(*(longBlock+j)) * oneOver2147483648;
				blockR[i] = EndianS32_LtoN(*(longBlock+j+1)) * oneOver2147483648;
				block3[i] = EndianS32_LtoN(*(longBlock+j+2)) * oneOver2147483648;
				block4[i] = EndianS32_LtoN(*(longBlock+j+3)) * oneOver2147483648;
#else
				blockL[i] = ByteSwapLong(*(longBlock+j)) * oneOver2147483648;
				blockR[i] = ByteSwapLong(*(longBlock+j+1)) * oneOver2147483648;
				block3[i] = ByteSwapLong(*(longBlock+j+2)) * oneOver2147483648;
				block4[i] = ByteSwapLong(*(longBlock+j+3)) * oneOver2147483648;
#endif
			}
			break;
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
			error = FSRead(mySI->dFileNum, &readSize, longBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
#ifdef SHOSXUB
				blockL[i] = EndianS32_BtoN(*(longBlock+j)) * oneOver2147483648;
				blockR[i] = EndianS32_BtoN(*(longBlock+j+1)) * oneOver2147483648;
				block3[i] = EndianS32_BtoN(*(longBlock+j+2)) * oneOver2147483648;
				block4[i] = EndianS32_BtoN(*(longBlock+j+3)) * oneOver2147483648;
#else
				blockL[i] = *(longBlock+j) * oneOver2147483648;
				blockR[i] = *(longBlock+j+1) * oneOver2147483648;
				block3[i] = *(longBlock+j+2) * oneOver2147483648;
				block4[i] = *(longBlock+j+3) * oneOver2147483648;
#endif
			}
			break;
		case SF_FORMAT_32_FLOAT:
			error = FSRead(mySI->dFileNum, &readSize, floatBlock);
			for(i = j = 0; i < numSamples; i++, j += 4)
			{
#ifdef SHOSXUB
				longSwap = floatBlock+j;
				*longSwap = EndianU32_BtoN(*longSwap);
#endif
				blockL[i] = *(floatBlock+j);
#ifdef SHOSXUB
				longSwap = floatBlock+j+1;
				*longSwap = EndianU32_BtoN(*longSwap);
#endif
				blockR[i] = *(floatBlock+j+1);
#ifdef SHOSXUB
				longSwap = floatBlock+j+2;
				*longSwap = EndianU32_BtoN(*longSwap);
#endif
				block3[i] = *(floatBlock+j+2);
#ifdef SHOSXUB
				longSwap = floatBlock+j+3;
				*longSwap = EndianU32_BtoN(*longSwap);
#endif
				block4[i] = *(floatBlock+j+3);
			}
			break;
		case SF_FORMAT_TEXT:
			charReadSize = 1;
			mySI->frameSize = 1;
			for(j = 0; j < numSamples; j++)
			{
				numStr[0] = 0;
				for(i = 0; i < 255; i++)
				{
					error = FSRead(mySI->dFileNum, &charReadSize, &c);
					if(c == 0xd)
						break;
					numStr[0]++;
					numStr[i+1] = c;
				}
				StringToFix(numStr, &tmpFloat);
				blockL[j] = tmpFloat;
				numStr[0] = 0;
				for(i = 0; i < 255; i++)
				{
					error = FSRead(mySI->dFileNum, &charReadSize, &c);
					if(c == 0xd)
						break;
					numStr[0]++;
					numStr[i+1] = c;
				}
				StringToFix(numStr, &tmpFloat);
				blockR[j] = tmpFloat;
				numStr[0] = 0;
				for(i = 0; i < 255; i++)
				{
					error = FSRead(mySI->dFileNum, &charReadSize, &c);
					if(c == 0xd)
						break;
					numStr[0]++;
					numStr[i+1] = c;
				}
				StringToFix(numStr, &tmpFloat);
				block3[j] = tmpFloat;
				numStr[0] = 0;
				for(i = 0; i < 255; i++)
				{
					error = FSRead(mySI->dFileNum, &charReadSize, &c);
					if(c == 0xd)
						break;
					numStr[0]++;
					numStr[i+1] = c;
				}
				StringToFix(numStr, &tmpFloat);
				block4[j] = tmpFloat;
				if(charReadSize == 0)
					numSamples = j;
				readSize = numSamples * mySI->nChans;
			}
			break;
	}
	numSamples = readSize/(mySI->nChans * mySI->frameSize);
	curMark = noMark;
	if(mySI == inSIPtr)
		GetItemMark(gSigDispMenu, DISP_INPUT_ITEM, &curMark);
	else if(mySI == filtSIPtr)
		GetItemMark(gSigDispMenu, DISP_AUX_ITEM, &curMark);
	if((curMark != noMark) & (gProcessEnabled != COPY_PROCESS) & (gProcessEnabled != NORM_PROCESS))
	{
		GetFPos(mySI->dFileNum, &position);
		dataLeft = position - mySI->dataStart;
		endTime = (dataLeft/(mySI->frameSize*mySI->nChans*mySI->sRate));
		beginTime = endTime - (numSamples/mySI->sRate);
		HMSTimeString(beginTime, beginStr);
		HMSTimeString(endTime, endStr);
		if(mySI->nChans == MONO)
			DisplayMonoWave(blockL, mySI, numSamples, beginStr, endStr);
		else
			DisplayStereoWave(blockL, blockR, mySI, numSamples, beginStr, endStr);
	}
	if(numSamples >= 64)
	{
		BlockMove(blockL ,mySI->infoSamps, 64*sizeof(float));		
		BlockMove(blockR ,mySI->infoSamps+64, 64*sizeof(float));		
		BlockMove(block3 ,mySI->infoSamps+128, 64*sizeof(float));		
		BlockMove(block4 ,mySI->infoSamps+192, 64*sizeof(float));
		mySI->infoUpdate = true;
	}
	else
	{
		BlockMove(blockL ,mySI->infoSamps, numSamples*sizeof(float));		
		BlockMove(blockR ,mySI->infoSamps+64, numSamples*sizeof(float));		
		BlockMove(block3 ,mySI->infoSamps+128, numSamples*sizeof(float));		
		BlockMove(block4 ,mySI->infoSamps+192, numSamples*sizeof(float));
		mySI->infoUpdate = true;
	}
	return(numSamples);
}

/*
 * This function will read two blocks of floats from a soundfile with
 * any packmode and number of channels. It will fill both blocks with
 * the same samples if the file being read is mono
 */
	
long
WriteStereoBlock(SoundInfo *mySI, long numSamples, float blockL[], float blockR[])
{
	Str255	numStr, beginStr, endStr;
	long	error, i, j, writeSize, charWriteSize, longSam, longSamB;
	float	beginTime, endTime;
	short	shortSam, curMark;
	unsigned long *longSwap;
	static long	lastLongSamL, lastLongSamR;
	Boolean	init = FALSE;
	unsigned long endianTest;

	
	if(gNumberBlocks == 0)
		lastLongSamL = lastLongSamR = 0;

	AllocateSoundIOMemory(2, numSamples);
	writeSize = numSamples * mySI->frameSize * mySI->nChans;
	switch(mySI->packMode)
	{
		case SF_FORMAT_4_ADDVI:
		{
			short shortOne, shortTwo;
			for(i = 0; i < numSamples; i++)
			{
				if(i == 0 && gNumberBlocks == 0)	/* set initial values */
					init = TRUE;
				else
					init = FALSE;
				shortOne = (short)(blockL[i] * gScaleL);
				shortTwo = (short)(blockR[i] * gScaleR);
				*(charBlock+i) = ADDVIEncode(shortOne, shortTwo, mySI->nChans, init);
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		}
		case SF_FORMAT_4_ADIMA:
		case SF_FORMAT_MACE3:
		case SF_FORMAT_MACE6:
			writeSize = Float2AppleCompressed(mySI, numSamples, blockL, blockR);
			if(numSamples < 0)
			{
				error = numSamples;
				writeSize = numSamples = 0;
			}
			else
				error = 0;
			break;
		case SF_FORMAT_8_LINEAR:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
				*(charBlock+j) = (char)(blockL[i]*gScaleL);
				*(charBlock+j+1) = (char)(blockR[i]*gScaleR);
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		case SF_FORMAT_8_UNSIGNED:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
				*(charBlock+j) = (char)(blockL[i] * gScaleL) ^ (char)0x80;
				*(charBlock+j+1) = (char)(blockR[i] * gScaleR) ^ (char)0x80;
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		case SF_FORMAT_8_MULAW:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
				shortSam = (short)(blockL[i]*gScaleL);
				*(charBlock+j) = Short2Ulaw(shortSam);
				shortSam = (short)(blockR[i]*gScaleR);
				*(charBlock+j+1) = Short2Ulaw(shortSam);
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		case SF_FORMAT_8_ALAW:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
				shortSam = (short)(blockL[i]*gScaleL);
				*(charBlock+j) = Short2Alaw(shortSam);
				shortSam = (short)(blockR[i]*gScaleR);
				*(charBlock+j+1) = Short2Alaw(shortSam);
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		case SF_FORMAT_16_LINEAR:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
				longSam = (long)(blockL[i]*gScaleL);
				longSam = longSam < -32768L ? -32768L : longSam;
				longSam = longSam > 32767L ? 32767L : longSam;
				*(shortBlock+j) = (short)longSam;
#ifdef SHOSXUB
				*(shortBlock+j) = EndianS16_NtoB(*(shortBlock+j));
#endif
				longSam = (long)(blockR[i]*gScaleR);
				longSam = longSam < -32768L ? -32768L : longSam;
				longSam = longSam > 32767L ? 32767L : longSam;
				*(shortBlock+j+1) = (short)longSam;
#ifdef SHOSXUB
				*(shortBlock+j+1) = EndianS16_NtoB(*(shortBlock+j+1));
#endif
			}
			error = FSWrite(mySI->dFileNum, &writeSize, shortBlock);
			break;
		case SF_FORMAT_16_SWAP:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
#ifdef SHOSXUB
				*(shortBlock+j) = EndianS16_NtoL((short)(blockL[i]*gScaleL));
				*(shortBlock+j+1) = EndianS16_NtoL((short)(blockR[i]*gScaleR));
#else
				*(shortBlock+j) = ByteSwapShort((short)(blockL[i]*gScaleL));
				*(shortBlock+j+1) = ByteSwapShort((short)(blockR[i]*gScaleR));
#endif
			}
			error = FSWrite(mySI->dFileNum, &writeSize, shortBlock);
			break;
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_COMP:
#ifdef SHOSXUB
#if defined (__i386__)
				for(i = j = 0; i < numSamples; i += 2, j += 3)
				{
					longSam = (long)(blockL[i]*gScaleL);
					longSamB = (long)(blockR[i]*gScaleR);
					*(longBlock+j) = EndianU32_NtoB((longSam & 0xffffff00L) | (longSamB & 0xff000000L)>>24);
					if((i+1) == numSamples)
					{
						longSam = (long)(blockR[i]*gScaleR);
						*(longBlock+j+1) = EndianU32_NtoB((longSam & 0x00ffff00L)<<8);
					}
					else
					{
						longSam = (long)(blockR[i]*gScaleR);
						longSamB = (long)(blockL[i+1]*gScaleL);
						*(longBlock+j+1) = EndianU32_NtoB((longSam & 0x00ffff00L)<<8 | (longSamB & 0xffff0000L)>>16);
						longSam = (long)(blockL[i+1]*gScaleL);
						longSamB = (long)(blockR[i+1]*gScaleR);
						*(longBlock+j+2) = EndianU32_NtoB((longSam & 0x0000ff00L)<<16 | (longSamB & 0xffffff00L)>>8);
					}
				}
#elif defined (__ppc__) || defined(__ppc64__)
				for(i = j = 0; i < numSamples; i += 2, j += 3)
				{
					longSam = (long)(blockL[i]*gScaleL);
					longSamB = (long)(blockR[i]*gScaleR);
					*(longBlock+j) = longSam & 0xffffff00L | (longSamB & 0xff000000L)>>24;
					if((i+1) == numSamples)
					{
						longSam = (long)(blockR[i]*gScaleR);
						*(longBlock+j+1) = (longSam & 0x00ffff00L)<<8;
					}
					else
					{
						longSam = (long)(blockR[i]*gScaleR);
						longSamB = (long)(blockL[i+1]*gScaleL);
						*(longBlock+j+1) = (longSam & 0x00ffff00L)<<8 | (longSamB & 0xffff0000L)>>16;
						longSam = (long)(blockL[i+1]*gScaleL);
						longSamB = (long)(blockR[i+1]*gScaleR);
						*(longBlock+j+2) = (longSam & 0x0000ff00L)<<16 | (longSamB & 0xffffff00L)>>8;
					}
				}
#endif
#else
				for(i = j = 0; i < numSamples; i += 2, j += 3)
				{
					longSam = (long)(blockL[i]*gScaleL);
					longSamB = (long)(blockR[i]*gScaleR);
					*(longBlock+j) = longSam & 0xffffff00L | (longSamB & 0xff000000L)>>24;
					if((i+1) == numSamples)
					{
						longSam = (long)(blockR[i]*gScaleR);
						*(longBlock+j+1) = (longSam & 0x00ffff00L)<<8;
					}
					else
					{
						longSam = (long)(blockR[i]*gScaleR);
						longSamB = (long)(blockL[i+1]*gScaleL);
						*(longBlock+j+1) = (longSam & 0x00ffff00L)<<8 | (longSamB & 0xffff0000L)>>16;
						longSam = (long)(blockL[i+1]*gScaleL);
						longSamB = (long)(blockR[i+1]*gScaleR);
						*(longBlock+j+2) = (longSam & 0x0000ff00L)<<16 | (longSamB & 0xffffff00L)>>8;
					}
				}
#endif
			error = FSWrite(mySI->dFileNum, &writeSize, longBlock);
			break;
		case SF_FORMAT_24_SWAP:
#ifdef SHOSXUB
#if defined (__i386__)
				for(i = j = 0; i < numSamples; i += 2, j += 3)
				{
					longSam = (long)(blockL[i]*gScaleL);
					longSamB = (long)(blockR[i]*gScaleR);
					*(longBlock+j) = (longSam & 0xffffff00L)>>8 | (longSamB & 0x0000ff00L)<<16;
					if((i+1) == numSamples)
					{
						longSam = (long)(blockR[i]*gScaleR);
						*(longBlock+j+1) = (longSam & 0xffff0000L)>>16;
					}
					else
					{
						longSam = (long)(blockR[i]*gScaleR);
						longSamB = (long)(blockL[i+1]*gScaleL);
						*(longBlock+j+1) = (longSam & 0xffff0000L)>>16 | (longSamB & 0x00ffff00L)<<16;
						longSam = (long)(blockL[i+1]*gScaleL);
						longSamB = (long)(blockR[i+1]*gScaleR);
						*(longBlock+j+2) = (longSam & 0xff000000L)>>24 | (longSamB & 0xffffff00L);
					}
				}
#elif defined (__ppc__) || defined(__ppc64__)
				for(i = j = 0; i < numSamples; i += 2, j += 3)
				{
					longSam = (long)(blockL[i]*gScaleL);
					longSamB = (long)(blockR[i]*gScaleR);
					*(longBlock+j) = EndianU32_NtoL((longSam & 0xffffff00L)>>8 | (longSamB & 0x0000ff00L)<<16);
					if((i+1) == numSamples)
					{
						longSam = (long)(blockR[i]*gScaleR);
						*(longBlock+j+1) = EndianU32_NtoL((longSam & 0xffff0000L)>>16);
					}
					else
					{
						longSam = (long)(blockR[i]*gScaleR);
						longSamB = (long)(blockL[i+1]*gScaleL);
						*(longBlock+j+1) = EndianU32_NtoL((longSam & 0xffff0000L)>>16 | (longSamB & 0x00ffff00L)<<8);
						longSam = (long)(blockL[i+1]*gScaleL);
						longSamB = (long)(blockR[i+1]*gScaleR);
						*(longBlock+j+2) = EndianU32_NtoL((longSam & 0xff000000L)>>24 | (longSamB & 0xffffff00L));
					}
				}
#endif
#else
			for(i = j = 0; i < numSamples; i += 2, j += 3)
			{
				longSam = (long)(blockL[i]*gScaleL);
				longSamB = (long)(blockR[i]*gScaleR);
				*(longBlock+j) = ByteSwapLong((longSam & 0xffffff00L)>>8 | (longSamB & 0x0000ff00L)<<16);
				if((i+1) == numSamples)
				{
					longSam = (long)(blockR[i]*gScaleR);
					*(longBlock+j+1) = ByteSwapLong((longSam & 0xffff0000L)>>16);
				}
				else
				{
					longSam = (long)(blockR[i]*gScaleR);
					longSamB = (long)(blockL[i+1]*gScaleL);
					*(longBlock+j+1) = ByteSwapLong((longSam & 0xffff0000L)>>16 | (longSamB & 0x00ffff00L)<<8);
					longSam = (long)(blockL[i+1]*gScaleL);
					longSamB = (long)(blockR[i+1]*gScaleR);
					*(longBlock+j+2) = ByteSwapLong((longSam & 0xff000000L)>>24 | (longSamB & 0xffffff00L));
				}
			}
#endif
			error = FSWrite(mySI->dFileNum, &writeSize, longBlock);
			break;
		case SF_FORMAT_32_SWAP:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
#ifdef SHOSXUB
				*(longBlock+j) = EndianU32_NtoL((long)(blockL[i]*gScaleL));
				*(longBlock+j+1) = EndianU32_NtoL((long)(blockR[i]*gScaleR));
#else
				*(longBlock+j) = ByteSwapLong((long)(blockL[i]*gScaleL));
				*(longBlock+j+1) = ByteSwapLong((long)(blockR[i]*gScaleR));
#endif
			}
			error = FSWrite(mySI->dFileNum, &writeSize, longBlock);
			break;
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
#ifdef SHOSXUB
				*(longBlock+j) = EndianU32_NtoB((long)(blockL[i]*gScaleL));
				*(longBlock+j+1) = EndianU32_NtoB((long)(blockR[i]*gScaleR));
#else
				*(longBlock+j) = (long)(blockL[i]*gScaleL);
				*(longBlock+j+1) = (long)(blockR[i]*gScaleR);
#endif
			}
			error = FSWrite(mySI->dFileNum, &writeSize, longBlock);
			break;
		case SF_FORMAT_32_FLOAT:
			for(i = j = 0; i < numSamples; i++, j += 2)
			{
				if(blockL[i] > mySI->peakFL)
					mySI->peakFL = blockL[i]; 
				else if(blockL[i] < -mySI->peakFL)
					mySI->peakFL = -blockL[i]; 
				if(blockR[i] > mySI->peakFR)
					mySI->peakFR = blockR[i]; 
				else if(blockR[i] < -mySI->peakFR)
					mySI->peakFR = -blockR[i]; 
				*(floatBlock+j) = blockL[i];
				*(floatBlock+j+1) = blockR[i];
#ifdef SHOSXUB
				longSwap = floatBlock+j;
				*longSwap = EndianU32_NtoB(*longSwap);
				longSwap = floatBlock+j+1;
				*longSwap = EndianU32_NtoB(*longSwap);
#endif
			}
			if(mySI->peakFL > mySI->peakFR)
				mySI->peak = mySI->peakFL;
			else
				mySI->peak = mySI->peakFR;
			error = FSWrite(mySI->dFileNum, &writeSize, floatBlock);
			break;
		case SF_FORMAT_TEXT:
			charWriteSize = 1;
			mySI->frameSize = 1;
			for(j = 0; j < numSamples; j++)
			{
				FixToString(blockL[j], numStr);
				numStr[0]++;
				numStr[numStr[0]] = 0xd;
				charWriteSize = numStr[0];
				error = FSWrite(mySI->dFileNum, &charWriteSize, numStr+1);
				FixToString(blockR[j], numStr);
				numStr[0]++;
				numStr[numStr[0]] = 0xd;
				charWriteSize = numStr[0];
				error = FSWrite(mySI->dFileNum, &charWriteSize, numStr+1);
				if(charWriteSize == 0)
					numSamples = j;
				writeSize = numSamples * mySI->nChans;
			}
			break;
		default:
			break;
	}
	if(error == dskFulErr)
	{
		gStopProcess = TRUE;
		DrawErrorMessage("\pDisk Full!");
	}
	else if(error != 0)
		DrawErrorMessage("\pWrite Error");
	mySI->numBytes += writeSize;
	numSamples = writeSize/(mySI->nChans * mySI->frameSize);
	curMark = noMark;
	if(mySI == outSIPtr || mySI == outTransSIPtr)
		GetItemMark(gSigDispMenu, DISP_OUTPUT_ITEM, &curMark);
	else if(mySI == outSteadSIPtr)
		GetItemMark(gSigDispMenu, DISP_AUX_ITEM, &curMark);
	if((curMark != noMark) & (gProcessEnabled != COPY_PROCESS) & (gProcessEnabled != NORM_PROCESS))
	{
		endTime = (mySI->numBytes/(mySI->frameSize*mySI->nChans*mySI->sRate));
		beginTime = endTime - (numSamples/mySI->sRate);
		HMSTimeString(beginTime, beginStr);
		HMSTimeString(endTime, endStr);
		DisplayStereoWave(blockL, blockR, mySI, numSamples, beginStr, endStr);
	}
	if(numSamples >= 128)
	{
		BlockMove(blockL ,mySI->infoSamps, 128*sizeof(float));		
		BlockMove(blockR ,mySI->infoSamps+128, 128*sizeof(float));		
		mySI->infoUpdate = true;
	}	
	else
	{
		BlockMove(mySI->infoSamps+numSamples, mySI->infoSamps, (128-numSamples)*sizeof(float));
		BlockMove(blockL ,mySI->infoSamps+(128-numSamples), numSamples*sizeof(float));		
		BlockMove(mySI->infoSamps+numSamples+128, mySI->infoSamps+128, (128-numSamples)*sizeof(float));
		BlockMove(blockR ,mySI->infoSamps+(256-numSamples), numSamples*sizeof(float));		
		mySI->infoUpdate = true;
	}	
	return(numSamples);
}

long
WriteMonoBlock(SoundInfo *mySI, long numSamples, float block[])
{
	Str255	numStr, beginStr, endStr;
	long	error, i, j, writeSize, charWriteSize, longSam, longSamB;
	float	beginTime, endTime, floatSam;
	short	shortSam, curMark;
	unsigned long *longSwap;
	static long	lastLongSam;
	Boolean	init = FALSE;
	
	if(gNumberBlocks == 0)
		lastLongSam = 0L;

	AllocateSoundIOMemory(1, numSamples);
	writeSize = numSamples * mySI->frameSize;
	switch(mySI->packMode)
	{
		case SF_FORMAT_4_ADDVI:
		{
			short shortOne, shortTwo;
			
			for(i = j = 0; i < numSamples; i += 2, j++)
			{
				if(i == 0 && gNumberBlocks == 0)	/* set initial values */
					init = TRUE;
				else
					init = FALSE;
				
				shortOne = (short)(block[i] * gScale);
				shortTwo = (short)(block[i+1] * gScale);
				*(charBlock+j) = ADDVIEncode(shortOne, shortTwo, mySI->nChans, init);
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
//			BlockADDVIEncode(charBlock, block, block, numSamples, mySI->nChans);
//			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		}
		case SF_FORMAT_4_ADIMA:
		case SF_FORMAT_MACE3:
		case SF_FORMAT_MACE6:
			writeSize = Float2AppleCompressed(mySI, numSamples, block, block);
			if(numSamples < 0)
			{
				error = numSamples;
				writeSize = numSamples = 0;
			}
			else
				error = 0;
			break;
		case SF_FORMAT_8_LINEAR:
			for(i = 0; i < numSamples; i++)
			{
				floatSam = block[i] * gScale;
				if(floatSam >= 0.0)
					floatSam += 0.5;
				else
					floatSam -= 0.5;
				*(charBlock+i) = (char)floatSam;
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		case SF_FORMAT_8_UNSIGNED:
			for(i = 0; i < numSamples; i++)
				*(charBlock+i) = (char)(block[i] * gScaleL) ^ (char)0x80;
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		case SF_FORMAT_8_MULAW:
			for(i = 0; i < numSamples; i++)
			{
				shortSam = (short)(block[i]*gScale);
				*(charBlock+i) = Short2Ulaw(shortSam);
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		case SF_FORMAT_8_ALAW:
			for(i = 0; i < numSamples; i++)
			{
				shortSam = (short)(block[i]*gScale);
				*(charBlock+i) = Short2Alaw(shortSam);
			}
			error = FSWrite(mySI->dFileNum, &writeSize, charBlock);
			break;
		case SF_FORMAT_16_LINEAR:
			for(i = 0; i < numSamples; i++)
			{
				longSam = block[i] * gScale;
				longSam = longSam < -32768L ? -32768L : longSam;
				longSam = longSam > 32767L ? 32767L : longSam;
				*(shortBlock+i) = (short)longSam;
#ifdef SHOSXUB
				*(shortBlock+i) = EndianS16_NtoB(*(shortBlock+i));
#endif
			}
			error = FSWrite(mySI->dFileNum, &writeSize, shortBlock);
			break;
		case SF_FORMAT_16_SWAP:
			for(i = 0; i < numSamples; i++)
			{
				floatSam = block[i] * gScale;
				if(floatSam >= 0.0)
					floatSam += 0.5;
				else
					floatSam -= 0.5;
#ifdef SHOSXUB
				*(shortBlock+i) = EndianS16_NtoL((short)floatSam);
#else
				*(shortBlock+i) = ByteSwapShort((short)floatSam);
#endif
			}
			error = FSWrite(mySI->dFileNum, &writeSize, shortBlock);
			break;
		case SF_FORMAT_TX16W:
			for(i = 0, j = 0; i < numSamples; i+=4, j+=3)
			{
				short sampleOne = block[i] * gScale;
				short sampleTwo = block[i+1] * gScale;
				short sampleThree = block[i+2] * gScale;
				short sampleFour = block[i+3] * gScale;
	    		unsigned short s2 = sampleTwo >> 4;
	    		unsigned short s3 = sampleThree >> 4;
	    		unsigned short s4 = sampleFour >> 4;
				unsigned char uc1 = ((unsigned short)sampleOne & 0xff00) >> 8;
				unsigned char uc2 = ((unsigned short)sampleOne & 0x00f0) | (((unsigned short)sampleTwo & 0x00f0) >> 4);
				unsigned char uc3 = ((unsigned short)sampleTwo & 0xff00) >> 8;
				unsigned char uc4 = ((unsigned short)sampleThree & 0xff00) >> 8;
				unsigned char uc5 = ((unsigned short)sampleThree & 0x00f0) | (((unsigned short)sampleFour & 0x00f0) >> 4);
				unsigned char uc6 = ((unsigned short)sampleFour & 0xff00) >> 8;
				
				*(unsigned short *)(shortBlock+j) = ((unsigned short)uc1 << 8) | (unsigned short)uc2;
				*(unsigned short *)(shortBlock+j+1) = ((unsigned short)uc3 << 8) | (unsigned short)uc4;
				*(unsigned short *)(shortBlock+j+2) = ((unsigned short)uc5 << 8) | (unsigned short)uc6;
#ifdef SHOSXUB
				*(shortBlock+j) = EndianU16_NtoB(*(shortBlock+j));
				*(shortBlock+j+1) = EndianU16_NtoB(*(shortBlock+j+1));
				*(shortBlock+j+1) = EndianU16_NtoB(*(shortBlock+j+2));
#endif
			}
			error = FSWrite(mySI->dFileNum, &writeSize, shortBlock);
			break;
		case SF_FORMAT_24_LINEAR:
		case SF_FORMAT_24_COMP:
#ifdef SHOSXUB
#if defined (__i386__)
			for(i = j = 0; i < numSamples; i += 4, j += 3)
			{
				// fill in the first byte
				if((i+1) == numSamples)
				{
					longSam = (long)(block[i]*gScale);
					*(longBlock+j) = EndianU32_NtoB((longSam & 0xffffff00L));
				}
				else
				{
					longSam = (long)(block[i]*gScale);
					longSamB = (long)(block[i+1]*gScale);
					*(longBlock+j) = EndianU32_NtoB((longSam & 0xffffff00L) | (longSamB & 0xff000000L)>>24);
				}
				// the second byte
				if((i+2) == numSamples)
				{
					longSam = (long)(block[i+1]*gScale);
					*(longBlock+j+1) = EndianU32_NtoB((longSam & 0x00ffff00L)<<8);
				}
				else
				{
					longSam = (long)(block[i+1]*gScale);
					longSamB = (long)(block[i+2]*gScale);
					*(longBlock+j+1) = EndianU32_NtoB((longSam & 0x00ffff00L)<<8 | (longSamB & 0xffff0000L)>>16);
				}
				if((i+3) == numSamples)
				{
					longSam = (long)(block[i+2]*gScale);
					*(longBlock+j+2) = EndianU32_NtoB((longSam & 0x0000ff00L)<<16);
				}
				else
				{
					longSam = (long)(block[i+2]*gScale);
					longSamB = (long)(block[i+3]*gScale);
					*(longBlock+j+2) = EndianU32_NtoB((longSam & 0x0000ff00L)<<16 | (longSamB & 0xffffff00L)>>8);
				}
			}
#elif defined (__ppc__) || defined(__ppc64__)
				for(i = j = 0; i < numSamples; i += 4, j += 3)
				{
					if((i+1) == numSamples)
					{
						longSam = (long)(block[i]*gScale);
						*(longBlock+j) = longSam & 0xffffff00L;
					}
					else
					{
						longSam = (long)(block[i]*gScale);
						longSamB = (long)(block[i+1]*gScale);
						*(longBlock+j) = longSam & 0xffffff00L | (longSamB & 0xff000000L)>>24;
					}
					if((i+2) == numSamples)
					{
						longSam = (long)(block[i+1]*gScale);
						*(longBlock+j+1) = (longSam & 0x00ffff00L)<<8;
					}
					else
					{
						longSam = (long)(block[i+1]*gScale);
						longSamB = (long)(block[i+2]*gScale);
						*(longBlock+j+1) = (longSam & 0x00ffff00L)<<8 | (longSamB & 0xffff0000L)>>16;
					}
					if((i+3) == numSamples)
					{
						longSam = (long)(block[i+2]*gScale);
						*(longBlock+j+2) = (longSam & 0x0000ff00L)<<16;
					}
					else
					{
						longSam = (long)(block[i+2]*gScale);
						longSamB = (long)(block[i+3]*gScale);
						*(longBlock+j+2) = (longSam & 0x0000ff00L)<<16 | (longSamB & 0xffffff00L)>>8;
					}
				}
#endif
#else
				for(i = j = 0; i < numSamples; i += 4, j += 3)
				{
					if((i+1) == numSamples)
					{
						longSam = (long)(block[i]*gScale);
						*(longBlock+j) = longSam & 0xffffff00L;
					}
					else
					{
						longSam = (long)(block[i]*gScale);
						longSamB = (long)(block[i+1]*gScale);
						*(longBlock+j) = longSam & 0xffffff00L | (longSamB & 0xff000000L)>>24;
					}
					if((i+2) == numSamples)
					{
						longSam = (long)(block[i+1]*gScale);
						*(longBlock+j+1) = (longSam & 0x00ffff00L)<<8;
					}
					else
					{
						longSam = (long)(block[i+1]*gScale);
						longSamB = (long)(block[i+2]*gScale);
						*(longBlock+j+1) = (longSam & 0x00ffff00L)<<8 | (longSamB & 0xffff0000L)>>16;
					}
					if((i+3) == numSamples)
					{
						longSam = (long)(block[i+2]*gScale);
						*(longBlock+j+2) = (longSam & 0x0000ff00L)<<16;
					}
					else
					{
						longSam = (long)(block[i+2]*gScale);
						longSamB = (long)(block[i+3]*gScale);
						*(longBlock+j+2) = (longSam & 0x0000ff00L)<<16 | (longSamB & 0xffffff00L)>>8;
					}
				}
#endif
			error = FSWrite(mySI->dFileNum, &writeSize, longBlock);
			break;
		case SF_FORMAT_24_SWAP:
		// needs work
		// ABCABCABCABC A1B1C1A2 B2C2A3B3 C3A4B4C4 A2C1B1A1 B3A3C2B2 CBAC
		// CBACBACBACBA                            C1B1A1C2 B2A2C3B3 ACBA
#ifdef SHOSXUB
#if defined (__i386__)
				for(i = j = 0; i < numSamples; i += 4, j += 3)
				{
					if((i+1) == numSamples)
					{
						longSam = (long)(block[i]*gScale);
						*(longBlock+j) = (longSam & 0xffffff00L)>>8;
					}
					else
					{
						longSam = (long)(block[i]*gScale);
						longSamB = (long)(block[i+1]*gScale);
						*(longBlock+j) = (longSam & 0xffffff00L)>>8 | (longSamB & 0x0000ff00L)<<16;
					}
					if((i+2) == numSamples)
					{
						longSam = (long)(block[i+1]*gScale);
						*(longBlock+j+1) = (longSam & 0xffff0000L)>>16;
					}
					else
					{
						longSam = (long)(block[i+1]*gScale);
						longSamB = (long)(block[i+2]*gScale);
						*(longBlock+j+1) = (longSam & 0xffff0000L)>>16 | (longSamB & 0x00ffff00L)<<16;
					}
					if((i+3) == numSamples)
					{
						longSam = (long)(block[i+2]*gScale);
						*(longBlock+j+2) = (longSam & 0xff000000L)>>24;
					}
					else
					{
						longSam = (long)(block[i+2]*gScale);
						longSamB = (long)(block[i+3]*gScale);
						*(longBlock+j+2) = (longSam & 0xff000000L)>>24 | (longSamB & 0xffffff00L);
					}
				}
#elif defined (__ppc__) || defined(__ppc64__)
				for(i = j = 0; i < numSamples; i += 4, j += 3)
				{
					// fill in the first byte
					if((i+1) == numSamples)
					{
						longSam = (long)(block[i]*gScale);
						*(longBlock+j) = EndianU32_NtoL((longSam & 0xffffff00L)>>8);
					}
					else
					{
						longSam = (long)(block[i]*gScale);
						longSamB = (long)(block[i+1]*gScale);
						*(longBlock+j) = EndianU32_NtoL((longSam & 0xffffff00L)>>8 | (longSamB & 0x0000ff00L)<<16);
					}
					// the second byte
					if((i+2) == numSamples)
					{
						longSam = (long)(block[i+1]*gScale);
						*(longBlock+j+1) = EndianU32_NtoL((longSam & 0xffff0000L)>>16);
					}
					else
					{
						longSam = (long)(block[i+1]*gScale);
						longSamB = (long)(block[i+2]*gScale);
						*(longBlock+j+1) = EndianU32_NtoL((longSam & 0xffff0000L)>>16 | (longSamB & 0x00ffff00L)<<8);
					}
					if((i+3) == numSamples)
					{
						longSam = (long)(block[i+2]*gScale);
						*(longBlock+j+2) = EndianU32_NtoL((longSam & 0xff000000L)>>24);
					}
					else
					{
						longSam = (long)(block[i+2]*gScale);
						longSamB = (long)(block[i+3]*gScale);
						*(longBlock+j+2) = EndianU32_NtoL((longSam & 0xff000000L)>>24 | (longSamB & 0xffffff00L));
					}
				}
#endif
#else
			for(i = j = 0; i < numSamples; i += 4, j += 3)
			{
				// fill in the first byte
				if((i+1) == numSamples)
				{
					longSam = (long)(block[i]*gScale);
					*(longBlock+j) = ByteSwapLong((longSam & 0xffffff00L)>>8);
				}
				else
				{
					longSam = (long)(block[i]*gScale);
					longSamB = (long)(block[i+1]*gScale);
					*(longBlock+j) = ByteSwapLong((longSam & 0xffffff00L)>>8 | (longSamB & 0x0000ff00L)<<16);
				}
				// the second byte
				if((i+2) == numSamples)
				{
					longSam = (long)(block[i+1]*gScale);
					*(longBlock+j+1) = ByteSwapLong((longSam & 0xffff0000L)>>16);
				}
				else
				{
					longSam = (long)(block[i+1]*gScale);
					longSamB = (long)(block[i+2]*gScale);
					*(longBlock+j+1) = ByteSwapLong((longSam & 0xffff0000L)>>16 | (longSamB & 0x00ffff00L)<<8);
				}
				if((i+3) == numSamples)
				{
					longSam = (long)(block[i+2]*gScale);
					*(longBlock+j+2) = ByteSwapLong((longSam & 0xff000000L)>>24);
				}
				else
				{
					longSam = (long)(block[i+2]*gScale);
					longSamB = (long)(block[i+3]*gScale);
					*(longBlock+j+2) = ByteSwapLong((longSam & 0xff000000L)>>24 | (longSamB & 0xffffff00L));
				}
			}
#endif
			error = FSWrite(mySI->dFileNum, &writeSize, longBlock);
			break;
		case SF_FORMAT_32_SWAP:
			for(i = 0; i < numSamples; i++)
#ifdef SHOSXUB
				*(longBlock+i) = EndianU32_NtoL((long)(block[i]*gScale));
#else
				*(longBlock+i) = ByteSwapLong((long)(block[i]*gScale));
#endif
			error = FSWrite(mySI->dFileNum, &writeSize, longBlock);
			break;
		case SF_FORMAT_32_LINEAR:
		case SF_FORMAT_32_COMP:
			for(i = 0; i < numSamples; i++)
#ifdef SHOSXUB
				*(longBlock+i) = EndianU32_NtoB((long)(block[i]*gScale));
#else
				*(longBlock+i) = (long)(block[i]*gScale);
#endif
			error = FSWrite(mySI->dFileNum, &writeSize, longBlock);
			break;
		case SF_FORMAT_32_FLOAT:
			for(i = 0; i < numSamples; i++)
			{
				if(block[i] > mySI->peakFL)
					mySI->peakFL = block[i];
				else if(block[i] < -mySI->peakFL)
					mySI->peakFL = -block[i];
				*(floatBlock+i) = block[i];
#ifdef SHOSXUB
				longSwap = floatBlock+i;
				*longSwap = EndianU32_NtoB(*longSwap);
#endif
			}
			mySI->peak = mySI->peakFL;
			error = FSWrite(mySI->dFileNum, &writeSize, floatBlock);
			break;
		case SF_FORMAT_TEXT:
			charWriteSize = 1;
			mySI->frameSize = 1;
			for(j = 0; j < numSamples; j++)
			{
				FixToString(block[j], numStr);
				numStr[0]++;
				numStr[numStr[0]] = 0xd;
				charWriteSize = numStr[0];
				error = FSWrite(mySI->dFileNum, &charWriteSize, numStr+1);
				if(charWriteSize == 0)
					numSamples = j;
				writeSize = numSamples * mySI->nChans;
			}
			break;
	}
	if(error == dskFulErr)
	{
		gStopProcess = TRUE;
		DrawErrorMessage("\pDisk Full!");
	}
	else if(error != 0)
		DrawErrorMessage("\pWrite Error");
	mySI->numBytes += writeSize;
	numSamples = writeSize/mySI->frameSize;
	curMark = noMark;
	if(mySI == outSIPtr || mySI == outTransSIPtr)
		GetItemMark(gSigDispMenu, DISP_OUTPUT_ITEM, &curMark);
	else if(mySI == outSteadSIPtr)
		GetItemMark(gSigDispMenu, DISP_AUX_ITEM, &curMark);
	if((curMark != noMark) & (gProcessEnabled != COPY_PROCESS) & (gProcessEnabled != NORM_PROCESS) & (gProcessEnabled != SPLIT_PROCESS))
	{
		endTime = (mySI->numBytes/(mySI->frameSize*mySI->nChans*mySI->sRate));
		beginTime = endTime - (numSamples/mySI->sRate);
		HMSTimeString(beginTime, beginStr);
		HMSTimeString(endTime, endStr);
		DisplayMonoWave(block, mySI, numSamples, beginStr, endStr);
	}
	if(numSamples >= 256)
	{
		BlockMove(block ,mySI->infoSamps, 256*sizeof(float));		
		mySI->infoUpdate = true;
	}
	else
	{
		BlockMove(mySI->infoSamps+numSamples, mySI->infoSamps, (256-numSamples)*sizeof(float));
		BlockMove(block ,mySI->infoSamps+(256-numSamples), numSamples*sizeof(float));		
		mySI->infoUpdate = true;
	}	
	return(numSamples);
}

/*void WriteBuffered(SoundInfo *mySI, long *numBytes, char * block)
{
	long size;
	
	if(mySI->writeBlock == NULL)
	{
		mySI->writeBlock = malloc(WRITEBYTES);
		mySI->writePos = 0;
	}
	if(*numBytes > WRITEBYTES)
		FSWrite(mySI->dFileNum, numBytes, block);
	else
	{
		if(mySI->writePos + *numBytes >= WRITEBYTES)
		{
			memcpy((mySI->writeBlock + mySI->writePos), block, WRITEBYTES - mySI->writePos);
			size = WRITEBYTES;
			FSWrite(mySI->dFileNum, &size, mySI->writeBlock);
			if(mySI->writePos + *numBytes > WRITEBYTES)
			{
				memcpy(mySI->writeBlock, (block + (WRITEBYTES - mySI->writePos)), *numBytes - (WRITEBYTES - mySI->writePos));
				mySI->writePos = (*numBytes - (WRITEBYTES - mySI->writePos));
			}
			else
				mySI->writePos = 0;
		}
		else
		{
			memcpy((mySI->writeBlock + mySI->writePos), block, *numBytes);
			mySI->writePos += *numBytes;
		}
	}
}

void FlushWriteBuffer(SoundInfo *mySI)
{
	long size;

	size = mySI->writePos;
	if(size == 0)
		return;
	FSWrite(mySI->dFileNum, &size, mySI->writeBlock);
	mySI->writePos = 0;
}*/
