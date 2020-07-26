/*
 *	SoundHackª
 *	Copyright ©1994 Tom Erbe
 *
 *  SANE MUST BE REMOVED TO DO ANYTHING ON THE POWER PC!!!!!!
 *
 *	Misc.c - Various string manipulation hacks for pascal style strings. Also, all of soundhack's
 *	SANE functions used to live here. These functions have been rewritten to use the ANSI math 
 *	and stdio routines instead.
 *
 *	Functions:
 *	LLog2()			a log2 function from ANSI functions.
 *	EExp2()			an exp2 function from ANSI functions.
 *	StringCopy()	Copies one pascal string to another.
 *	StringAppend()	Takes two input strings, concatenates them, puts the result into
 *					an output string.
 *	FixToString()	Like NumToString, but for floating point numbers. Creates a string
 *					with 6 digits to the right of the decimal.
 *	StringToFix()	The reverse.
 *	ZeroFloatTable()	This just clears a floating point array.
 */
#if __MC68881__
#define __FASTMATH__
#define __HYBRIDMATH__
#endif	/* __MC68881__ */
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include "SoundFile.h"
//#include <AppleEvents.h>
#include "Misc.h"
#include "CarbonGlue.h"


void	ColorBrighten(RGBColor *color)
{
	if(color->blue >= color->red && color->green >= color->red)
	{
		color->red = 0;
		return;
	}
	if(color->red >= color->blue && color->green >= color->blue)
	{
		color->blue = 0;
		return;
	}
	if(color->blue >= color->green && color->red >= color->green)
	{
		color->green = 0;
		return;
	}
}

OSErr
TouchFolder( short vRefNum , long parID )
{
	CInfoPBRec rec ;
	Str63 name ;
	short err ;

    rec.hFileInfo.ioNamePtr = name ;
    name [ 0 ] = 0 ;
    rec.hFileInfo.ioVRefNum = vRefNum ;
    rec.hFileInfo.ioDirID = parID ;
    rec.hFileInfo.ioFDirIndex = -1 ;
    rec.hFileInfo.ioFVersNum = 0 ;
    err = PBGetCatInfoSync ( & rec ) ;
    if ( err ) {
        return err ;
    }
    GetDateTime ( & rec.dirInfo.ioDrMdDat ) ;
    rec.hFileInfo.ioVRefNum = vRefNum ;
    rec.hFileInfo.ioDirID = parID ;
    rec.hFileInfo.ioFDirIndex = -1 ;
    rec.hFileInfo.ioFVersNum = 0 ;
    rec.hFileInfo.ioNamePtr [ 0 ] = 0 ;
    err = PBSetCatInfoSync ( & rec ) ;
    return err ;
}

double
LLog2(double x)
{
	double ret;
	
	ret = log10(x) / log10(2.0);
	return(ret);
}

double
EExp2(double x)
{
	double ret;

	ret = pow(2.0, x);
	return(ret);
}

void	HMSTimeString(double seconds, Str255 timeStr)
{
	int hours, minutes, intsec, fracsec;
	
	hours = seconds/3600.0;
	seconds -= hours * 3600.0;
	minutes = seconds/60.0;
	seconds -= minutes * 60.0;
	intsec = seconds;
	seconds -= intsec;
	fracsec = seconds * 1000.0;
	sprintf((char *)timeStr, "%02d:%02d:%02d.%03d", hours, minutes, intsec, fracsec);
	c2pstr((char *)timeStr);
}

void	NameFile(Str255 inStr, Str255 procStr, Str255 outStr)
{
	short i,j;
	Str255 tmpStr;
	
	tmpStr[0] = inStr[0];

	for(i = 1; i <= inStr[0]; i++)
	{
//		if(inStr[i] > 64 && inStr[i] < 90)
//			tmpStr[i] = inStr[i] + 32;
//		else
			tmpStr[i] = inStr[i];
	}

	for(i = 1, j = 1; i <= tmpStr[0] && j < 32; i++, j++)
	{
		if(tmpStr[i] == ' ' || tmpStr[i] == '.')
			j--;
		else
			outStr[j] = tmpStr[i];
	}
	for(i = 1; i <= procStr[0] && j < 32; i++, j++)
			outStr[j] = procStr[i];
	outStr[0] = j - 1;
}

void	RemoveMenuChars(Str255 stringIn, Str255 stringOut)
{
	short i, j;
	for(i = 1, j = 1, stringOut[0] = 0; i <= stringIn[0]; i++)
	{
		if(stringIn[i] != ';' && stringIn[i] != 0x0d && stringIn[i] != '^' && 
		   stringIn[i] != '!' && stringIn[i] != '<' && stringIn[i] != '/' && 
		   stringIn[i] != '(' && stringIn[i] != '-')
		{
			stringOut[j] = stringIn[i];
			stringOut[0]++;
			j++;
		}
	}
}

void
StringCopy(Str255 stringIn, Str255 stringOut)
{
	short i;
	for(i = 0; i <= stringIn[0]; i++)
		stringOut[i] = stringIn[i];
}

void
StringAppend(Str255 stringHead, Str255 stringTail, Str255 stringOut)
{
	short i;
	
	stringOut[0] = stringHead[0] + stringTail[0];
	for(i = 1; i <= stringHead[0]; i++)
		stringOut[i] = stringHead[i];
	for(i = 1; i <= stringTail[0]; i++)
		stringOut[i+stringHead[0]] = stringTail[i];
}

void	
StringToFix(Str255 s, double *f)
{
	int i;
	Str255 s2;
	for(i = 0; i<=s[0]; i++)
		s2[i] = s[i];
	p2cstr(s2);
	sscanf((char *)s2, "%lf", f);
}

void
FixToString(double f, Str255 s)
{	
	Str255 s2;
	short error, i;
	error = sprintf((char *)s2, "%7.6f", f);
	c2pstr((char *)s2);
	for(i = 0; i<=s2[0]; i++)
		s[i] = s2[i];
}

void
FixToString3(double f, Str255 s)
{	
	short error;
	error = sprintf((char *)s, "%4.3f", f);
	c2pstr((char *)s);
}

void
FixToString12(double f, Str255 s)
{
	sprintf((char *)s, "%12.6f", f);
	c2pstr((char *)s);
}

void
ZeroFloatTable(float A[], long n)
{
	long i;
	
	for(i=0; i<n; i++)
		A[i] = 0.0;
}

void	PickPhrase(Str255 str)
{
	unsigned long	longSeed;
	short	shortSeed, choice;
	float normalRandom;
		
	GetDateTime(&longSeed);
	shortSeed = (unsigned)(longSeed/2);
	srand(shortSeed);
	normalRandom = (float)rand()/(float)RAND_MAX;
	normalRandom = (float)rand()/(float)RAND_MAX;
	choice = (short)(normalRandom * 32767.0);
	if (choice < 4096)
		StringCopy("\pWashing The GnomeMobile", str);
	else if(choice < 8192)
		StringCopy("\pIs Guava A Donut?", str);
	else if(choice < 12288)
		StringCopy("\pThank You Eloy Anzola", str);
	else if(choice < 16384)
		StringCopy("\pLet's Build A Car", str);
	else if(choice < 20480)
		StringCopy("\pThank You Akira Rabelais", str);
	else if (choice < 24576)
		StringCopy("\pThank You Phil Burk", str);
	else if (choice < 28672)
		StringCopy("\pCan't Stop The Music", str);
	else
		StringCopy("\pDedicated to Betsy Edwards", str);
}

void
RemovePtr(Ptr killMePtr)
{
	if(killMePtr != nil)
		DisposePtr(killMePtr);
}