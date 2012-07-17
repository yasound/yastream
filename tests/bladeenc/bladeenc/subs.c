/*
			(c) Copyright 1998, 1999 - Tord Jansson
			=======================================

		This file is part of the BladeEnc MP3 Encoder, based on
		ISO's reference code for MPEG Layer 3 compression, and might
		contain smaller or larger sections that are directly taken
		from ISO's reference code.

		All changes to the ISO reference code herein are either
		copyrighted by Tord Jansson (tord.jansson@swipnet.se)
		or sublicensed to Tord Jansson by a third party.

	BladeEnc is free software; you can redistribute this file
	and/or modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

*/

#include "common.h"
#include "encoder.h"

/*****************************************************************************
 ************************** Start of Subroutines *****************************
 *****************************************************************************/

/*****************************************************************************
 * FFT computes fast fourier transform of BLKSIZE samples of data            *
 *   uses decimation-in-frequency algorithm described in "Digital            *
 *   Signal Processing" by Oppenheim and Schafer, refer to pages 304         *
 *   (flow graph) and 330-332 (Fortran program in problem 5)                 *
 *   to get the inverse fft, change line 20 from                             *
 *                 w_imag[L] = -sin(PI/le1);                                 *
 *                          to                                               *
 *                 w_imag[L] = sin(PI/le1);                                  *
 *                                                                           *
 *   required constants:                                                     *
 *         #define      PI          3.14159265358979                         *
 *         #define      BLKSIZE     1024                                     *
 *         #define      LOGBLKSIZE  10                                       *
 *         #define      BLKSIZE_S   256                                      *
 *         #define      LOGBLKSIZE_S 8                                       *
 *                                                                           *
 *****************************************************************************/
#define      BLKSIZE_S   256
#define      LOGBLKSIZE_S 8

#define	      BLKSIZEV2	(BLKSIZE>>1)

#define		BLKSIZE_SV2 (BLKSIZE_S>>1)

int fInit_fft_1024;

/* remember: polar will _not_ be ordered correctly! look into l3psy to see
   how to reorder it */

void fft_1024(rectcomplex rect[BLKSIZE], polarcomplex polar[BLKSIZE])
{
    static double  w_real[BLKSIZEV2], w_imag[BLKSIZEV2];
    int            i,j,k,L,SKIP;
    int            ip, le,le1;
    double         t_real, t_imag, t1_real, t1_imag;

    if(fInit_fft_1024==0) 
    {
	for(L=0; L<(BLKSIZEV2); L++)
	{
	    w_real[L] = cos((double)L*PI/(BLKSIZEV2));
            w_imag[L] = -sin((double)L*PI/(BLKSIZEV2));
	}          
	fInit_fft_1024++;
    }

    for(L=0; L<(LOGBLKSIZE-1); L++)
    {
	le = 1 << (LOGBLKSIZE-L);
	le1 = le >> 1;

	for(j=0; j<le1; j++)
	{
	    SKIP = j << L;
    	    for(i=j; i<BLKSIZE; i+=le)
	    {
    		ip = i + le1;
		t_real = rect[i].real + rect[ip].real;
		t_imag = rect[i].imag + rect[ip].imag;
		t1_real = rect[i].real - rect[ip].real;
		rect[ip].imag = rect[i].imag - rect[ip].imag;
		rect[i].real = t_real;
		rect[i].imag = t_imag;
		rect[ip].real = t1_real*w_real[SKIP] - rect[ip].imag*w_imag[SKIP];
		rect[ip].imag = rect[ip].imag*w_real[SKIP] + t1_real*w_imag[SKIP];
	    }
	}
    }
    /* special case: L = M-1; all Wn = 1 */
    for(i=0; i<BLKSIZE; i+=2)
    {
	ip = i + 1;
	t_real = rect[i].real + rect[ip].real;
	t_imag = rect[i].imag + rect[ip].imag;
	t1_real = rect[i].real - rect[ip].real;
	t1_imag = rect[i].imag - rect[ip].imag;
    
	polar[i].energy = t_real*t_real + t_imag*t_imag;
	if(polar[i].energy <= 0.0005)
	{
	    polar[i].phi = 0;
	    polar[i].energy = 0.0005;
	}
	else 
	    polar[i].phi = atan2((double) t_imag,(double) t_real);

	polar[ip].energy = t1_real*t1_real + t1_imag*t1_imag;
	if(polar[ip].energy == 0)
	    polar[ip].phi = 0;
	else 
	    polar[ip].phi = atan2((double) t1_imag,(double) t1_real);
    }
}



int fInit_fft_256;

/* remember: polar will _not_ be ordered correctly! look into l3psy to see
   how to reorder it */

void fft_256(rectcomplex rect[BLKSIZE], polarcomplex polar[BLKSIZE])
{
    static double  w_real[BLKSIZE_SV2], w_imag[BLKSIZE_SV2];
    int            i,j,k,L,SKIP;
    int            ip, le,le1;
    double         t_real, t_imag, t1_real, t1_imag;

    if(fInit_fft_256==0) 
    {
	for(L=0; L<(BLKSIZE_SV2); L++)
	{
	    w_real[L] = cos((double)L*PI/(BLKSIZE_SV2));
	    w_imag[L] = -sin((double)L*PI/(BLKSIZE_SV2));
	}          
	fInit_fft_256++;
    }

    for(L=0; L<(LOGBLKSIZE_S-1); L++)
    {
	le = 1 << (LOGBLKSIZE_S-L);
	le1 = le >> 1;

	for(j=0; j<le1; j++)
	{
	    SKIP = j << L;
            for(i=j; i<BLKSIZE_S; i+=le)
	    {
    		ip = i + le1;
    		t_real = rect[i].real + rect[ip].real;
    		t_imag = rect[i].imag + rect[ip].imag;
    		t1_real = rect[i].real - rect[ip].real;
    		rect[ip].imag = rect[i].imag - rect[ip].imag;
    		rect[i].real = t_real;
    		rect[i].imag = t_imag;
    		rect[ip].real = t1_real*w_real[SKIP] - rect[ip].imag*w_imag[SKIP];
    		rect[ip].imag = rect[ip].imag*w_real[SKIP] + t1_real*w_imag[SKIP];
	    }
	}
    }

    /* special case: L = M-1; all Wn = 1 */
    for(i=0; i<BLKSIZE_S; i+=2)
    {
	ip = i + 1;
	t_real = rect[i].real + rect[ip].real;
	t_imag = rect[i].imag + rect[ip].imag;
	t1_real = rect[i].real - rect[ip].real;
	t1_imag = rect[i].imag - rect[ip].imag;
    
	polar[i].energy = t_real*t_real + t_imag*t_imag;
	if(polar[i].energy <= 0.0005)
	{
	    polar[i].phi = 0;
	    polar[i].energy = 0.0005;
	}
	else 
	    polar[i].phi = atan2((double) t_imag,(double) t_real);

	polar[ip].energy = t1_real*t1_real + t1_imag*t1_imag;
	if(polar[ip].energy == 0)
	    polar[ip].phi = 0;
	else 
	    polar[ip].phi = atan2((double) t1_imag,(double) t1_real);
    }
}
