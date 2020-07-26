/***************************************************************\
*   IEEE80.c							*
*   Convert between "double" and IEEE 80 bit format  		*
*   in machine independent manner.				*
*   Assumes array of char is a continuous region of 8 bit frames*
*   Assumes (unsigned long) has 32 useable bits			*
*   billg, dpwe @media.mit.edu					*
*   01aug91							*
*   19aug91  aldel/dpwe  workaround top bit problem in Ultrix   *
*                        cc's double->ulong cast		*
*   05feb92  dpwe/billg  workaround top bit problem in 		*
*                        THINKC4 + 68881 casting		*
\***************************************************************/

#include	"IEEE80.h"
//#include 	<stdio.h>	/* for stderr in myDoubleToUlong */

/* #define MAIN	1	/* to compile test routines */

#define ULPOW2TO31	((unsigned long)0x80000000L)
#define DPOW2TO31	((double)2147483648.0)	/* 2^31 */

/* have to deal with ulong's 32nd bit conditionally as double<->ulong casts 
   don't work in some C compilers */

static double   myUlongToDouble PARG((unsigned long ul));
static unsigned long myDoubleToUlong PARG((double val));

static double myUlongToDouble(unsigned long ul)
	{
	double val;
	
	/* in THINK_C, ulong -> double apparently goes via long, so can only 
	   apply to 31 bit numbers.  If 32nd bit is set, explicitly add on its
	   value */
	if(ul & ULPOW2TO31)
		val = DPOW2TO31 + (ul & (~ULPOW2TO31));
	else
		val = ul;
	return val;
	}

static unsigned long myDoubleToUlong(val)
	double	val;
	{
	unsigned long ul;
	
	/* cannot cast negative numbers into unsigned longs */
	if(val < 0)	
		{ 
	//	fprintf(stderr,"IEEE80:DoubleToUlong: val < 0\n"); 
		return(-1);
		}
	
	/* in ultrix 4.1's cc, double -> unsigned long loses the top bit, 
	   so we do the conversion only on the bottom 31 bits and set the 
	   last one by hand, if val is truly that big */
	/* should maybe test for val > (double)(unsigned long)0xFFFFFFFF ? */
	if(val < DPOW2TO31)
		 ul = (unsigned long)val;
	else
		 ul = ULPOW2TO31 | (unsigned long)(val-DPOW2TO31);
	return ul;
	}


/*
 * Convert IEEE 80 bit floating point to double.
 * Should be portable to all C compilers.
 */
double ieee_80_to_double(unsigned char *p)
{
char sign;
short exp = 0;
unsigned long mant1 = 0;
unsigned long mant0 = 0;

unsigned long *long1, *long2;
unsigned short *short1;
	
double val;

/*	short1 = p+0;
	long1 = p+2;
	long2 = p+6;
	*short1 = EndianU16_BtoN(*short1);
	*long1 = EndianU32_BtoN(*long1);
	*long2 = EndianU32_BtoN(*long2);*/
	
	exp = *p++;
	exp <<= 8;
	exp |= *p++;
	sign = (exp & 0x8000) ? 1 : 0;
	exp &= 0x7FFF;

	mant1 = *p++;
	mant1 <<= 8;
	mant1 |= *p++;
	mant1 <<= 8;
	mant1 |= *p++;
	mant1 <<= 8;
	mant1 |= *p++;

	mant0 = *p++;
	mant0 <<= 8;
	mant0 |= *p++;
	mant0 <<= 8;
	mant0 |= *p++;
	mant0 <<= 8;
	mant0 |= *p++;

	/* special test for all bits zero meaning zero 
	   - else pow(2,-16383) bombs */
	if(mant1 == 0 && mant0 == 0 && exp == 0 && sign == 0)
		return 0.0;
	else{
		val = myUlongToDouble(mant0) * pow(2.0,-63.0);
		val += myUlongToDouble(mant1) * pow(2.0,-31.0);
		val *= pow(2.0,((double) exp) - 16383.0);
		return sign ? -val : val;
		}
}

/*
 * Convert double to IEEE 80 bit floating point
 * Should be portable to all C compilers.
 * 19aug91 aldel/dpwe  covered for MSB bug in Ultrix 'cc'
 */

void double_to_ieee_80(val,p)
double val;
unsigned char *p;
{
char sign = 0;
short exp = 0;
unsigned long mant1 = 0;
unsigned long mant0 = 0;

	if(val < 0.0)	{  sign = 1;  val = -val; }
	
	if(val != 0.0)	/* val identically zero -> all elements zero */
		{
		exp = (short)(log(val)/log(2.0) + 16383.0);
		val *= pow(2.0, 31.0+16383.0-(double)exp);
		mant1 = myDoubleToUlong(val);
		val -= myUlongToDouble(mant1);
		val *= pow(2.0, 32.0);
		mant0 = myDoubleToUlong(val);
		}

	*p++ = ((sign<<7)|(exp>>8));
	*p++ = 0xFF & exp;
	*p++ = 0xFF & (mant1>>24);
	*p++ = 0xFF & (mant1>>16);
	*p++ = 0xFF & (mant1>> 8);
	*p++ = 0xFF & (mant1);
	*p++ = 0xFF & (mant0>>24);
	*p++ = 0xFF & (mant0>>16);
	*p++ = 0xFF & (mant0>> 8);
	*p++ = 0xFF & (mant0);

}

#ifdef MAIN

static void print_hex PARG((unsigned char *p, short n));

static void print_hex(p,n)
unsigned char *p;
short n;
{
long i;
	for (i = 0; i < n; i++) printf("%02X",*p++);
	printf(" ");
}

double tab[] = { 0, -1.0, 1.0, 2.0 , 4.0, 0.5, 0.25, 0.125, 
		     3.14159265358979323846, 10000.0, 22000.0,
		44100.0, 65536.0, 134072.768, 3540001.9, 4294967295.0};
#define NTAB (sizeof(tab)/sizeof(double))

main()
{
short i;
double val;
unsigned char eighty[10];
double *p80;

	p80 = (double *)eighty;
	
        /* for each number in the test table, print its actual value, 
	   its native hex representation, the 80 bit representation and 
	   the back-converted value (should be the same!) */
        /* I think the hex of PI to all 80 bits is 
	   4000 C90F DAA2 2168 C233 */
	for (i = 0; i < NTAB; i++) {
		printf("%8lf: ",tab[i]);
		print_hex((unsigned char *)(tab+i),sizeof(double));
		double_to_ieee_80((double) tab[i], eighty);
		print_hex(eighty, 10);
		val = ieee_80_to_double(eighty);
		printf("%lf\n",val);
	}
}

#endif
