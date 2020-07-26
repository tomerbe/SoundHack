// AppleCompression.c
// Wrapper for Sound Manger 3.2 Compression routines
//#include <SoundComponents.h>
#include <QuickTime/Movies.h>
#include "SoundFile.h"
#include "AppleCompression.h"

extern long			gNumberBlocks;
short 				*gPlaySams;

//A: Yes, a call to SoundConverterSetInfo is required, but first you'll have to retrieve the decompression settings
//  atom from the sound file. To do this, open the MP3 as a QuickTime movie using NewMovieFromFile. Use the
//  GetMediaSampleDescription routine to retrieve a description of the sample data, then use
//  GetSoundDescriptionExtension (passing in the sample description, and using the siDecompressionParams
//  selector as parameters) to extract the decompression atom list from the sound track. Once you have the decompression
//  atom list, call SoundConverterSetInfo using the siDecompressionParams selector as we did with the previous
//  call. Without this setup, the decoder won't know how to interpret the sample data.

OSErr MyGetSoundDescription(const FSSpec *inMP3file,
	SoundDescriptionV1Handle sourceSoundDescription, AudioFormatAtomPtr *outAudioAtom, TimeScale *timeScale) 
{
	Movie theMovie;
	Track theTrack;
	Media theMedia;
	short theRefNum;
	short theResID = 0;  // we want the first movie
	Boolean wasChanged;
	long length;

	OSErr err = noErr;
	theMovie = NULL;
	// open the movie file
	err = OpenMovieFile(inMP3file, &theRefNum, fsRdPerm);
	if (err) goto bail;
	// instantiate the movie
	err = NewMovieFromFile(&theMovie, theRefNum, &theResID, NULL, newMovieActive, &wasChanged);
	if (err) goto bail;
	CloseMovieFile(theRefNum);
	theRefNum = 0;
	// get the first sound track
	theTrack = GetMovieIndTrackType(theMovie, 1, SoundMediaType, movieTrackMediaType);
	if (theTrack != NULL)
	{
		// get the sound track media
		theMedia = GetTrackMedia(theTrack);
		if (theMedia != NULL) 
		{      
			Size size;
			Handle extension;

			*timeScale = GetMediaTimeScale(theMedia);
			length = GetMediaSampleCount(theMedia);
			// get the description of the sample data
			GetMediaSampleDescription(theMedia, 1, (SampleDescriptionHandle)sourceSoundDescription);
			err = GetMoviesError();
			extension = NewHandle(0);
			// get the "magic" decompression atom
			// This extension to the SoundDescription information stores
			// data specific to a given audio decompressor. Some audio
			// decompression algorithms require a set of out-of-stream
			// values to configure the decompressor.
			err = GetSoundDescriptionExtension((SoundDescriptionHandle)sourceSoundDescription,
						&extension, siDecompressionParams);
			if (noErr == err)
			{
				size = GetHandleSize(extension);
				HLock(extension);
				*outAudioAtom = (AudioFormatAtom*)NewPtr(size);
				err = MemError();
				// copy the atom data to our buffer...
				BlockMoveData(*extension, *outAudioAtom, size);
				HUnlock(extension);
			}
			else
			{
				// if it doesn't have an atom, that's ok
				*outAudioAtom = NULL;
				err = noErr;
			}

			DisposeHandle(extension);
		}
	}
	bail:
	if(theMovie != NULL)
		DisposeMovie(theMovie);
	if(length != 0)
	return err;
}

Boolean InitAppleCompression(SoundInfo *theSI, short direction)
{
    SoundComponentData  inputFormat, outputFormat;
    OSType				compFormat;
    OSErr               err;
    long 				targetBytes;
	SoundDescriptionV1Handle sourceSoundDescription;
	AudioFormatAtomPtr outAudioAtom;
	TimeScale timeScale;

	switch(theSI->packMode)
	{
		case SF_FORMAT_4_ADIMA:
    		compFormat = kIMACompression;
			break;
		case SF_FORMAT_MACE3:
			compFormat = kMACE3Compression;
			break;
		case SF_FORMAT_MACE6:
    		compFormat = kMACE6Compression;
			break;
		case SF_FORMAT_MPEG_I:
		case SF_FORMAT_MPEG_II:
		case SF_FORMAT_MPEG_III:
    		compFormat = kFullMPEGLay3Format;
			break;
	}    
    if(direction == SF_COMPRESS)
    {
#if defined (__i386__)
    	inputFormat.format = 'sowt';
#elif defined (__ppc__) || defined(__ppc64__)
    	inputFormat.format = 'twos';
#endif
    	outputFormat.format = compFormat;
    }
    else if(direction == SF_EXPAND)
    {
    	inputFormat.format = compFormat;
#if defined (__i386__)
    	outputFormat.format = 'sowt';
#elif defined (__ppc__) || defined(__ppc64__)
    	outputFormat.format = 'twos';
#endif
    }
	if(inputFormat.format == kFullMPEGLay3Format)
	{
		sourceSoundDescription = (SoundDescriptionV1Handle)NewHandle(0);
		MyGetSoundDescription(&(theSI->sfSpec), sourceSoundDescription, &outAudioAtom, &timeScale);
		inputFormat.format = (*sourceSoundDescription)->desc.dataFormat;
		
		outputFormat.sampleSize = inputFormat.sampleSize = (*sourceSoundDescription)->desc.sampleSize;
		outputFormat.sampleRate = inputFormat.sampleRate = (*sourceSoundDescription)->desc.sampleRate;
		DisposeHandle((Handle)sourceSoundDescription);
	}
	else
	{
		outputFormat.sampleSize = inputFormat.sampleSize = 16;
		outputFormat.sampleRate = inputFormat.sampleRate = theSI->sRate;
	}
	outputFormat.numChannels = inputFormat.numChannels = theSI->nChans;
	outputFormat.sampleCount = inputFormat.sampleCount = 0;
	outputFormat.buffer = inputFormat.buffer = nil;
	outputFormat.flags = inputFormat.flags = 0;
	outputFormat.reserved = inputFormat.reserved = 0;

	
    err = SoundConverterOpen(&inputFormat, &outputFormat, &(theSI->comp.sc));
    if (err != noErr)
        return(FALSE);
	
	if(compFormat == kFullMPEGLay3Format)
	{
		err = SoundConverterSetInfo(theSI->comp.sc, siDecompressionParams, outAudioAtom);
		if (siUnknownInfoType == err) 
		{
			// clear this error, the decompressor didn't
			// need the decompression atom and that's OK
			err = noErr;
		} 
	    if (err != noErr)
	        return(FALSE);
	}
	targetBytes = 128UL;
	theSI->comp.inputFrames = 0;
	theSI->comp.inputBytes = 0;
	theSI->comp.outputBytes = 0;
	do {
		targetBytes *= 2;
		err = SoundConverterGetBufferSizes(theSI->comp.sc, targetBytes, &(theSI->comp.inputFrames), 
				&(theSI->comp.inputBytes), &(theSI->comp.outputBytes));
	} while (notEnoughBufferSpace == err  && targetBytes < (MaxBlock() / 4));
	if (err != noErr)
        return(FALSE);

	// Open compression component and initialize memory (a little bigger for overflow)
    theSI->comp.inputPtr = NewPtrClear(theSI->comp.inputBytes * 3);
    theSI->comp.outputPtr = NewPtrClear(theSI->comp.outputBytes * 3);
	err = SoundConverterBeginConversion(theSI->comp.sc);
	if (err != noErr)
        return(FALSE);
    return(TRUE);
}    	

Boolean TermAppleCompression(SoundInfo *theSI)
{
    OSErr               err;
    theSI->comp.init = FALSE;
    err = SoundConverterClose(theSI->comp.sc);
    if(theSI->comp.inputPtr != NULL)
    {
	    DisposePtr(theSI->comp.inputPtr);
	    theSI->comp.inputPtr = NULL;
    }
    if(theSI->comp.outputPtr != NULL)
    {
	    DisposePtr(theSI->comp.outputPtr);
	    theSI->comp.outputPtr = NULL;
    }
    if (err != noErr)
        return(FALSE);
    return(TRUE);
}

UInt32 AppleCompressedRead(SoundInfo *theSI, UInt32 readBytes, long numFrames, Ptr outSamples)
{
	Movie theMovie;
	Track theTrack;
	Media theMedia;
	short theRefNum;
	short theResID = 0;	// we want the first movie
	Boolean wasChanged;
	long bytesRead;
	long frameRead;
	TimeValue realTime;
	
	OSErr err = noErr;
	
	// open the movie file
	err = OpenMovieFile(&(theSI->sfSpec), &theRefNum, fsRdPerm);

	// instantiate the movie
	err = NewMovieFromFile(&theMovie, theRefNum, &theResID, NULL, newMovieActive, &wasChanged);
	CloseMovieFile(theRefNum);
	theRefNum = 0;
		
	// get the first sound track
	theTrack = GetMovieIndTrackType(theMovie, 1, SoundMediaType, movieTrackMediaType);
	if(theTrack != NULL)
	{
		// get the sound track media
		theMedia = GetTrackMedia(theTrack);
		if(theMedia != NULL)
		{			
			// copy the sample data into our buffer
			Handle hTemp = NewHandle(0);

			err = GetMediaSample(theMedia, hTemp, readBytes*4, &bytesRead, theSI->timeValue, &realTime, 0, NULL, NULL, numFrames, &frameRead, NULL);
			if(readBytes > bytesRead)
				readBytes = bytesRead;
			if(readBytes < bytesRead && readBytes + 32 >= bytesRead)
				readBytes = bytesRead;
			HLock(hTemp);
			BlockMove(*hTemp,outSamples,readBytes);
			HUnlock(hTemp);
			DisposeHandle(hTemp);
		}

	}
	return(readBytes);
}

long AppleCompressed2Float(SoundInfo *theSI, long numFrames, float *floatSamL, float *floatSamR)
{
    OSErr				err;
    static TimeValue	lastTime;
    long				readSize, numFramesRead, dataLeft;
    short				shortSam;
   	Boolean				end;
   	
	// INITIALIZE VARIABLES 
	if(gNumberBlocks == 0 && theSI->comp.init != TRUE)
    {
    	theSI->comp.init = TRUE;
    	theSI->comp.curBufPos = 0;
		InitAppleCompression(theSI, SF_EXPAND);
		theSI->comp.filePosition = 0;
		lastTime = theSI->timeValue;
	}	
	numFramesRead = 0;
	
    		
	// EMPTY OLD OUTPUT BUFFER INTO FLOATSAM BUFFER(S)
	// current buffer position (theSI->comp.curBufPos) is set to zero when it is emptied
	// otherwise, there is still valid data in the compression output buffer
	// that needs to be flushed out. .00001
	if(theSI->comp.curBufPos != 0)
	{
		for(; (numFramesRead < numFrames) && (theSI->comp.curBufPos < theSI->comp.outputBytes); numFramesRead++)
		{
			shortSam = *(short *)(theSI->comp.outputPtr + theSI->comp.curBufPos);
			*(floatSamL + numFramesRead) = (float)shortSam * 0.000030517578125;
			theSI->comp.curBufPos += sizeof(short);
			if(floatSamL != floatSamR)	// STEREO
			{
				shortSam = *(short *)(theSI->comp.outputPtr + theSI->comp.curBufPos);
				*(floatSamR + numFramesRead) = (float)shortSam * 0.000030517578125;
				theSI->comp.curBufPos += sizeof(short);
			}
		}
		if(theSI->comp.curBufPos >= theSI->comp.outputBytes)
			theSI->comp.curBufPos = 0;
	}
	
	// UNSF_COMPRESS MORE SAMPLES IF NEEDED
	// now that the output buffer is empty, we can read and convert more samples
	// until the floatSam buffers are full
	end = FALSE;
	
	SetFPos(theSI->dFileNum, fsFromStart, theSI->comp.filePosition);
	while(numFramesRead < numFrames && end == FALSE)
	{
		theSI->dataEnd = theSI->numBytes - theSI->dataStart;
		dataLeft = theSI->dataEnd - theSI->comp.filePosition;
		readSize = theSI->comp.inputBytes;
		if(dataLeft < readSize)
			readSize = dataLeft;
		//U.B. - this should work without swapping - all bytes
		FSRead(theSI->dFileNum, &readSize, (theSI->comp.inputPtr));
  		theSI->comp.filePosition += readSize;
		GetFPos(theSI->dFileNum, &theSI->comp.filePosition);
		// inputFrames should not be set...
		if(readSize >= theSI->comp.inputBytes)
		{
			err = SoundConverterConvertBuffer(theSI->comp.sc, theSI->comp.inputPtr,
				theSI->comp.inputFrames, theSI->comp.outputPtr, &(theSI->comp.outputFrames),
				&(theSI->comp.outputBytes));
    		if(err != noErr)
        		return(0);
		}
		else if(readSize == 0)
		{
			// if we got no more input bytes, extract the rest with this function
   			err = SoundConverterEndConversion(theSI->comp.sc, theSI->comp.outputPtr, 
    				&(theSI->comp.outputFrames), &(theSI->comp.outputBytes));
    		if (err != noErr)
        		return(0);
        	end = TRUE;
    	}
    	else
    	{
    		// here readSize is smaller than theSI->comp.inputBytes, but not zero
    		theSI->comp.inputFrames = (readSize * theSI->comp.inputFrames)/theSI->comp.inputBytes;
    		
			err = SoundConverterConvertBuffer(theSI->comp.sc, theSI->comp.inputPtr,
				theSI->comp.inputFrames, theSI->comp.outputPtr, &(theSI->comp.outputFrames),
				&(theSI->comp.outputBytes));
    		if(err != noErr)
        		return(0);
		}
		for(; (numFramesRead < numFrames) && (theSI->comp.curBufPos < theSI->comp.outputBytes); numFramesRead++)
		{
			shortSam = *(short *)(theSI->comp.outputPtr + theSI->comp.curBufPos);
			*(floatSamL + numFramesRead) = (float)shortSam * 0.000030517578125;
			theSI->comp.curBufPos += sizeof(short);
			if(floatSamL != floatSamR)	// STEREO
			{
				shortSam = *(short *)(theSI->comp.outputPtr + theSI->comp.curBufPos);
				*(floatSamR + numFramesRead) = (float)shortSam * 0.000030517578125;
				theSI->comp.curBufPos += sizeof(short);
			}
		}
		theSI->timeValue += theSI->comp.outputFrames;
		if(theSI->comp.curBufPos >= theSI->comp.outputBytes)
			theSI->comp.curBufPos = 0;
    }
    if(end)
		TermAppleCompression(theSI);
	lastTime = theSI->timeValue;

    return(numFramesRead);
}


long Float2AppleCompressed(SoundInfo *theSI, long numFrames, float *floatSamL, float *floatSamR)
{
    OSErr				err;
    long				writeSize, numFramesCompressed, numBytesWrite;
    float				floatSam;
   	
	if(numFrames == 0 && theSI->comp.init == TRUE)
	{
		
   		err = SoundConverterEndConversion(theSI->comp.sc, theSI->comp.outputPtr, 
    			&(theSI->comp.outputFrames), &(theSI->comp.outputBytes));
    	if (err != noErr)
        	return(0);
        theSI->comp.init = FALSE;
		writeSize = theSI->comp.outputBytes;
		FSWrite(theSI->dFileNum, &writeSize, (theSI->comp.outputPtr));
		theSI->numBytes += writeSize;
		TermAppleCompression(theSI);
		return(0);
	}
	else if(numFrames == 0 && theSI->comp.init == FALSE)
		return(0);
	// INITIALIZE VARIABLES 
	if(gNumberBlocks == 0 && theSI->comp.init != TRUE)
    {
    	theSI->comp.init = TRUE;
		InitAppleCompression(theSI, SF_COMPRESS);
	}
	
	numBytesWrite = numFramesCompressed = 0;
    
    // In this function, I can just blast the samples into buffers, into
    // odd sized buffers too and just convert and write, nothing too
    // difficult here (or so he thinks!!!).
    
	// SF_COMPRESS MORE SAMPLES IF NEEDED
	// now that the output buffer is empty, we can write and convert more samples
	// until the floatSam buffers are full
	while(numFramesCompressed < numFrames)
	{
		for(theSI->comp.curBufPos = 0; (numFramesCompressed < numFrames) && (theSI->comp.curBufPos < theSI->comp.inputBytes); numFramesCompressed++)
		{
			floatSam = *(float *)(floatSamL + numFramesCompressed) * 32768.0;
			*(short *)(theSI->comp.inputPtr + theSI->comp.curBufPos) = (short)floatSam;
			theSI->comp.curBufPos += sizeof(short);
			if(floatSamL != floatSamR)	// STEREO
			{
				floatSam = *(float *)(floatSamR + numFramesCompressed) * 32768.0;
				*(short *)(theSI->comp.inputPtr + theSI->comp.curBufPos) = (short)floatSam;
				theSI->comp.curBufPos += sizeof(short);
			}
		}
		// inputFrames should be set...
		theSI->comp.inputFrames = (theSI->comp.curBufPos >> 1) / theSI->nChans;
		err = SoundConverterConvertBuffer(theSI->comp.sc, theSI->comp.inputPtr,
				theSI->comp.inputFrames, theSI->comp.outputPtr, &(theSI->comp.outputFrames),
				&(theSI->comp.outputBytes));
//		err = SoundConverterFillBuffer(theSI->comp.sc, MySoundConverterFillBufferDataUPP, NULL, theSI->comp.outputPtr,
//   outputBufferByteSize, &(theSI->comp.outputBytes), &(theSI->comp.outputFrames), &outputFlags);
    	if(err != noErr)
        	return(err);
		writeSize = theSI->comp.outputBytes;
		err = FSWrite(theSI->dFileNum, &writeSize, (theSI->comp.outputPtr));
		numBytesWrite += writeSize;
    	if(err != noErr)
        	return(err);
		if(theSI->comp.curBufPos >= theSI->comp.inputBytes)
			theSI->comp.curBufPos = 0;
    }
	return(numBytesWrite);
}

/*static pascal Boolean SoundConverterFillBufferDataProc(SoundComponentDataPtr *outData, void *inRefCon)
{
	SCFillBufferDataPtr pFillData = (SCFillBufferDataPtr)inRefCon;
	
	OSErr err;
							
	// if after getting the last chunk of data the total time is over the duration, we're done
	if (pFillData->getMediaAtThisTime >= pFillData->sourceDuration) {
		pFillData->isThereMoreSource = false;
		pFillData->compData.desc.buffer = NULL;
		pFillData->compData.desc.sampleCount = 0;
		pFillData->compData.bufferSize = 0;		
		pFillData->compData.commonFrameSize = 0;
	}
	
	if (pFillData->isThereMoreSource) {
	
		long	  sourceBytesReturned;
		long	  numberOfSamples;
		TimeValue sourceReturnedTime, durationPerSample;
		
		// in calling GetMediaSample, we'll get a buffer that consists of equal sized frames - the
		// degenerate case is only 1 frame -- for non-self-framed vbr formats (like AAC in QT 6.0)
		// we need to provide some more framing information - either the frameCount, frameSizeArray pair or
		// commonFrameSize field must be valid -- because we always get equal sized frames, we use
		// commonFrameSize and set the kExtendedSoundCommonFrameSizeValid flag -- if there is
		// only 1 frame then (common frame size == media sample size), if there are multiple frames,
		// then (common frame size == media sample size / number of frames).
		
		err = GetMediaSample(pFillData->sourceMedia,		// specifies the media for this operation
							 pFillData->hSource,			// function returns the sample data into this handle
							 pFillData->maxBufferSize,		// maximum number of bytes of sample data to be returned
							 &sourceBytesReturned,			// the number of bytes of sample data returned
							 pFillData->getMediaAtThisTime,	// starting time of the sample to be retrieved (must be in Media's TimeScale)
							 &sourceReturnedTime,			// indicates the actual time of the returned sample data
							 &durationPerSample,			// duration of each sample in the media
							 NULL,							// sample description corresponding to the returned sample data (NULL to ignore)
							 NULL,							// index value to the sample description that corresponds to the returned sample data (NULL to ignore)
							 0,								// maximum number of samples to be returned (0 to use a value that is appropriate for the media)
							 &numberOfSamples,				// number of samples it actually returned
							 NULL);							// flags that describe the sample (NULL to ignore)

		if ((noErr != err) || (sourceBytesReturned == 0)) {
			pFillData->isThereMoreSource = false;
			pFillData->compData.desc.buffer = NULL;
			pFillData->compData.desc.sampleCount = 0;
			pFillData->compData.bufferSize = 0;		
			pFillData->compData.commonFrameSize = 0;
			
			if ((err != noErr) && (sourceBytesReturned > 0))
				DebugStr("\pGetMediaSample - Failed in FillBufferDataProc");
		}
		
		pFillData->getMediaAtThisTime = sourceReturnedTime + (durationPerSample * numberOfSamples);		
		
		// we've specified kExtendedSoundSampleCountNotValid and the 'studly' Sound Converter will
		// take care of sampleCount for us, so while this is not required we fill out all the information
		// we have to simply demonstrate how this would be done
		// sampleCount is the number of PCM samples
		pFillData->compData.desc.sampleCount = numberOfSamples * durationPerSample;
		
		// kExtendedSoundBufferSizeValid was specified - make sure this field is filled in correctly
		pFillData->compData.bufferSize = sourceBytesReturned;

		// for VBR audio we specified the kExtendedSoundCommonFrameSizeValid flag - make sure this field is filled in correctly
		if (pFillData->isSourceVBR) pFillData->compData.commonFrameSize = sourceBytesReturned / numberOfSamples;
	}

	// set outData to a properly filled out ExtendedSoundComponentData struct
	*outData = (SoundComponentDataPtr)&pFillData->compData;
	
	return (pFillData->isThereMoreSource);
}

struct SoundComponentData {
	long 							flags;
	OSType 							format;
	short 							numChannels;
	short 							sampleSize;
	UnsignedFixed 					sampleRate;
	long 							sampleCount;
	Byte *							buffer;
	long 							reserved;
};
*/