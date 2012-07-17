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



/*____ rebuffer_audio() __________________________________________________*/

void rebuffer_audio( short buffer[2][1152], short * insamp, unsigned long samples_read, int stereo )
{
  int j;

  if(stereo == 2)
	{ 

		for(j=0;j<1152;j++) 
		{
	    buffer[0][j] = insamp[2*j];
	    buffer[1][j] = insamp[2*j+1];
		}
  }
  else
	{		
		for(j=0;j<1152;j++)
		{
			buffer[0][j] = insamp[j];
			buffer[1][j] = 0;
		}
  }

  return;
}


/************************************************************************
*
* create_ana_filter()
*
* PURPOSE:  Calculates the analysis filter bank coefficients
*
* SEMANTICS:
* Calculates the analysis filterbank coefficients and rounds to the
* 9th decimal place accuracy of the filterbank tables in the ISO
* document.  The coefficients are stored in #filter#

************************************************************************/

void create_ana_filter(double filter[SBLIMIT][64])
{
   register int i,k;

   for (i=0; i<32; i++)
      for (k=0; k<64; k++) {
          if ((filter[i][k] = 1e9*cos((double)((2*i+1)*(16-k)*PI64))) >= 0)
             modf(filter[i][k]+0.5, &filter[i][k]);
          else
             modf(filter[i][k]-0.5, &filter[i][k]);
          filter[i][k] *= 1e-9;
   }
}



/************************************************************************
*
* filter_subband()
*
* PURPOSE:  Calculates the analysis filter bank coefficients
*
* SEMANTICS:
*      The windowed samples #z# is filtered by the digital filter matrix #m#
* to produce the subband samples #s#. This done by first selectively
* picking out values from the windowed samples, and then multiplying
* them by the filter matrix, producing 32 subband samples.
*
************************************************************************/

int	fInit_windowFilterSubband;

/*____ windowFilterSubband() ____________________________________________*/

void  oldwindowFilterSubband( short ** buffer, int k, double s[SBLIMIT] )
{
 typedef double MM[SBLIMIT][64];

 double y[64];
 int    i,j;

  double  t;
  double *  rpMM;

  static int off[2];

  int	offcache;

  typedef double XX[2][512];

  static MM m;
  static XX x;
	double *dp;
	short	 *sp;

  if (!fInit_windowFilterSubband) 
  {
		off[0] = 0;
		off[1] = 0;
    for (i=0;i<2;i++)
      for (j=0;j<512;j++)
        x[i][j] = 0;

    create_ana_filter(m);
    fInit_windowFilterSubband = 1;
  }

  dp = &x[k][0];
  sp = *buffer;
	offcache = off[k];

  /* replace 32 oldest samples with 32 new samples */
  for ( i=0 ; i<32 ; i++ )
		dp[31-i+offcache] = (double) sp[i]/SCALE;
	*buffer += 32;
  
  for( i = 0 ; i<64 ; i++ )
  {
    t =  dp[(i+64*0+offcache)&(512-1)] * enwindow[i+64*0];
    t += dp[(i+64*1+offcache)&(512-1)] * enwindow[i+64*1];
    t += dp[(i+64*2+offcache)&(512-1)] * enwindow[i+64*2];
    t += dp[(i+64*3+offcache)&(512-1)] * enwindow[i+64*3];
    t += dp[(i+64*4+offcache)&(512-1)] * enwindow[i+64*4];
    t += dp[(i+64*5+offcache)&(512-1)] * enwindow[i+64*5];
    t += dp[(i+64*6+offcache)&(512-1)] * enwindow[i+64*6];
    t += dp[(i+64*7+offcache)&(512-1)] * enwindow[i+64*7];
    y[i] = t;
  }

  off[k] += 480;              /*offset is modulo (HAN_SIZE-1)*/
  off[k] &= 512-1;



  rpMM = (double *) (m);

  for ( i=0 ; i<SBLIMIT ; i++ )
  {
		t = 0;
		for( j = 0 ; j < 64 ; j++ )
			t += rpMM[j] * y[j];
		rpMM += j;
		s[i] = t;
	}	 
	 
}

/*____ new windowFilterSubband() ____________________________________________*/

void  windowFilterSubband( short ** buffer, int k, double s[SBLIMIT] )
{
	typedef double MM[SBLIMIT][64];

 	double y[64];
	int    i,j;
	int		a;

  double  t;    /* TJ */
  double *  rpMM;    /* TJ */

  static int off[2];
	static int half[2];

  typedef double XX[2][512];

  static MM m;
  static XX x;
	double *dp, *dp2;
	short	 *sp;
	double * pEnw;
	
  if (!fInit_windowFilterSubband) 
  {
		off[0] = 0;
		off[1] = 0;
		half[0] = 0;
		half[1] = 0;
    for (i=0;i<2;i++)
      for (j=0;j<512;j++)
        x[i][j] = 0;

    create_ana_filter(m);
    fInit_windowFilterSubband = 1;
  }

  dp = x[k]+off[k]+half[k];
  sp = *buffer;

  /* replace 32 oldest samples with 32 new samples */
  for ( i=0 ; i<32 ; i++ )
		dp[(31-i)*8] = (double) sp[i]/SCALE;
	*buffer += 32;


	for( j = 0 ; j < 32+1 ; j+=32 )
	{

		if( j == 0 )
		{
			dp = x[k] +half[k];
			a = off[k];
		}
		else
		{
			if( half[k] == 0 )
			{
				dp = x[k]+256;
				a = off[k];
			}
			else
			{
				dp = x[k];
				a = (off[k]+1) & 7;
			}
		}
		
		dp2 = dp;
		pEnw = enwindow + j;

		switch( a )
		{
			case 0:
			  for( i = 0 ; i<32 ; i++ )
	  		{
	    		    t =  *dp2++ * pEnw[64*0];
			    t += *dp2++ * pEnw[64*1];
			    t += *dp2++ * pEnw[64*2];
			    t += *dp2++ * pEnw[64*3];
			    t += *dp2++ * pEnw[64*4];
			    t += *dp2++ * pEnw[64*5];
			    t += *dp2++ * pEnw[64*6];
			    t += *dp2++ * pEnw[64*7];
			    pEnw++;
			    y[i+j] = t;
			  }			
				break;
			case 1:
			  for( i = 0 ; i<32 ; i++ )
	  		{
			    t =  *dp2++ * pEnw[64*7];
		    	    t += *dp2++ * pEnw[64*0];
			    t += *dp2++ * pEnw[64*1];
			    t += *dp2++ * pEnw[64*2];
			    t += *dp2++ * pEnw[64*3];
			    t += *dp2++ * pEnw[64*4];
			    t += *dp2++ * pEnw[64*5];
			    t += *dp2++ * pEnw[64*6];
			    pEnw++;
			    y[i+j] = t;
			  }			
				break;
			case 2:
			  for( i = 0 ; i<32 ; i++ )
	  		{
			    t =  *dp2++ * pEnw[64*6];
			    t += *dp2++ * pEnw[64*7];
	    		    t += *dp2++ * pEnw[64*0];
			    t += *dp2++ * pEnw[64*1];
			    t += *dp2++ * pEnw[64*2];
			    t += *dp2++ * pEnw[64*3];
			    t += *dp2++ * pEnw[64*4];
			    t += *dp2++ * pEnw[64*5];
			    pEnw++;
			    y[i+j] = t;
			  }			
				break;
			case 3:
			  for( i = 0 ; i<32 ; i++ )
	  		{
			    t =  *dp2++ * pEnw[64*5];
			    t += *dp2++ * pEnw[64*6];
			    t += *dp2++ * pEnw[64*7];
	    		    t += *dp2++ * pEnw[64*0];
			    t += *dp2++ * pEnw[64*1];
			    t += *dp2++ * pEnw[64*2];
			    t += *dp2++ * pEnw[64*3];
			    t += *dp2++ * pEnw[64*4];
			    pEnw++;
			    y[i+j] = t;
			  }			
				break;
			case 4:
			  for( i = 0 ; i<32 ; i++ )
	  		{
			    t =  *dp2++ * pEnw[64*4];
			    t += *dp2++ * pEnw[64*5];
			    t += *dp2++ * pEnw[64*6];
			    t += *dp2++ * pEnw[64*7];
	    		    t += *dp2++ * pEnw[64*0];
			    t += *dp2++ * pEnw[64*1];
			    t += *dp2++ * pEnw[64*2];
			    t += *dp2++ * pEnw[64*3];
			    pEnw++;
			    y[i+j] = t;
			  }			
				break;
			case 5:
			  for( i = 0 ; i<32 ; i++ )
	  		{
	    		    t =  *dp2++ * pEnw[64*3];
			    t += *dp2++ * pEnw[64*4];
			    t += *dp2++ * pEnw[64*5];
			    t += *dp2++ * pEnw[64*6];
			    t += *dp2++ * pEnw[64*7];
			    t += *dp2++ * pEnw[64*0];
			    t += *dp2++ * pEnw[64*1];
			    t += *dp2++ * pEnw[64*2];
			    pEnw++;
			    y[i+j] = t;
			  }			
				break;
			case 6:
			  for( i = 0 ; i<32 ; i++ )
	  		{
	    		    t =  *dp2++ * pEnw[64*2];
			    t += *dp2++ * pEnw[64*3];
			    t += *dp2++ * pEnw[64*4];
			    t += *dp2++ * pEnw[64*5];
			    t += *dp2++ * pEnw[64*6];
			    t += *dp2++ * pEnw[64*7];
			    t += *dp2++ * pEnw[64*0];
			    t += *dp2++ * pEnw[64*1];
			    pEnw++;
			    y[i+j] = t;
			  }			
				break;
			case 7:
			  for( i = 0 ; i<32 ; i++ )
	  		{
	    		    t =  *dp2++ * pEnw[64*1];
			    t += *dp2++ * pEnw[64*2];
			    t += *dp2++ * pEnw[64*3];
			    t += *dp2++ * pEnw[64*4];
			    t += *dp2++ * pEnw[64*5];
			    t += *dp2++ * pEnw[64*6];
			    t += *dp2++ * pEnw[64*7];
			    t += *dp2++ * pEnw[64*0];
			    pEnw++;
			    y[i+j] = t;
			  } /* for(i) */			
				break;
		} /* switch(a) */
	} /* for(j) */

	half[k] = 256-half[k];
	if( half[k] )
	  off[k] = (off[k]+7) & 7;              /*offset is modulo (HAN_SIZE-1)*/

	/* ======================================= */

  rpMM = (double *) (m);

  for ( i=0 ; i<SBLIMIT ; i++ )
  {
		t = 0;
		for( j = 0 ; j < 64 ; j++ )
			t += rpMM[j] * y[j];
		rpMM += j;
		s[i] = t;
	}	 
}
