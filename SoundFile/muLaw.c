#define MUCLIP  32635
#define BIAS    0x84
#define MUZERO  0x02
#define ZEROTRAP
#include "SoundFile.h"
//#include <AppleEvents.h>
#include "muLaw.h"

extern float gScaleL, gScaleR, gScale;

char exp_lut[128] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,
					 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
					 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
					 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
					 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
					 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
					 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
					 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};

short ulaw_decode[256] = {
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
     -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
     -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
     -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
     -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
     -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
     -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
      -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
      -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
      -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
      -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
      -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
       -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
     32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
     23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
     15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
     11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
      7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
      5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
      3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
      2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
      1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
      1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
       876,    844,    812,    780,    748,    716,    684,    652,
       620,    588,    556,    524,    492,    460,    428,    396,
       372,    356,    340,    324,    308,    292,    276,    260,
       244,    228,    212,    196,    180,    164,    148,    132,
       120,    112,    104,     96,     88,     80,     72,     64,
		56,     48,     40,     32,     24,     16,      8,      0 };

float
Ulaw2Float(unsigned char musam)
{
	float	floatsam;
	
	floatsam = (float)(ulaw_decode[musam]);
	return(floatsam);
}

short
Ulaw2Short(unsigned char musam)
{
	short shortsam;

	shortsam = ulaw_decode[musam];
	return(shortsam);
}

void
Ulaw2ShortBlock(unsigned char *muBlock, short *shortBlock, long size)
{
	long i;
	
	for(i=0;i<size;i++)
		*(shortBlock+i) = ulaw_decode[*(muBlock+i)];
}

unsigned char 
Short2Ulaw(short shortsam)
{
    register long	longsam;
    register short	sign;
    short				sample, exponent, mantissa;
    char			ulawbyte;
    extern  char    exp_lut[128];	/* mulaw encoding table */

	if ((longsam = shortsam) < 0)	/* if sample negative	*/
	{
		sign = 0x80;
		longsam = - longsam;		/*  make abs, save sign	*/
	}
	else
		sign = 0;
			
	if (longsam > MUCLIP)			/* out of range? */
		longsam = MUCLIP;       	/*   clip	 */
	   		
	sample = longsam + BIAS;
	exponent = exp_lut[( sample >> 8 ) & 0x7F];
	mantissa = (sample >> (exponent+3)) & 0x0F;
	ulawbyte = ~(sign |(exponent << 4)|mantissa);
	return(ulawbyte);
}
