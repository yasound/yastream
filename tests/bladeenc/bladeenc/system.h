/*
			(c) Copyright 1998, 1999 - Tord Jansson
			=======================================

		This file is part of the BladeEnc MP3 Encoder, based on
		ISO's reference code for MPEG Layer 3 compression.

		This file doesn't contain any of the ISO reference code and
		is copyright Tord Jansson (tord.jansson@swipnet.se).

	BladeEnc is free software; you can redistribute this file
	and/or modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

*/

/*==== High level defines =====================================================*/


#define         WIN32_INTEL             0
#define         WIN32_ALPHA             1
#define         LINUX_I386              2
#define         LINUX_PPC               3
#define      		LINUX_SPARC    					4
#define         SOLARIS                 5
#define         IBM_OS2                 6
#define					IRIX										7
#define					ATARI_TOS								8
#define					UNIXWARE7								9
#define					SCO5										10
#define					ULTRIX									11
#define					NETBSD									12
#define					OPENBSD									13
#define					DARWIN									14

#ifndef	SYSTEM
	#define       SYSTEM         LINUX_I386   /* Set current system here, select */
#endif																			/* from list above. */


#define					PRECISE_TIMER					/* Gives more accurate speed calculations, */
																			/* just for debug purposes. Disable in release version! */
 
/*==== Low level defines ======================================================*/
 
/*
  LIST OF DEFINES
  ===============

  BYTEORDER	[byteorder]     Should either be set to BIG_ENDIAN or LITTLE_ENDIAN depending on the processor.
  INLINE    [prefix]        Defines the prefix for inline functions, normally _inline.
                             Skip this define if your compiler doesn't support inline functions.
  DRAG_DROP                 Set if Drag-n-Drop operations are supported. If defined, the hint for drag and drop
                             is displayed in the help menu.
  PRIO                      Set if priority can be set with the -PRIO switch.
  MSWIN                     Set this for windows systems. Includes "windows.h" etc.
  WILDCARDS                 Set this if the program has to expand wildcards itself on your system.
  NO_ZERO_CALLOC            Set this to work around a bug when allocation 0 bytes memory with some compilers.
	DIRECTORY_SEPARATOR				Should either be '\\' or '/'.
	WAIT_KEY									Set this on systems where we as default want to wait for a keypress before quiting.
*/


/*  Most systems allready have these two defines, but some doesn't 
    so we have to put them here, before they are used. */

#ifndef BIG_ENDIAN
	#define			BIG_ENDIAN				4321
#endif

#ifndef LITTLE_ENDIAN
	#define			LITTLE_ENDIAN			1234
#endif



/*_____ Windows 95/98/NT Intel defines ________________________________________*/

#if     SYSTEM == WIN32_INTEL
				#define					BYTEORDER							LITTLE_ENDIAN
        #define         INLINE								_inline
        #define         DRAG_DROP
        #define         PRIO
        #define         MSWIN
        #define         WILDCARDS
				#define					DIRECTORY_SEPARATOR		'\\'
				#define					WAIT_KEY
#endif

/*_____ Windows NT DEC Alpha defines __________________________________________*/

#if SYSTEM == WIN32_ALPHA

        #define         BYTEORDER							LITTLE_ENDIAN
        #define         INLINE								_inline
        #define         DRAG_DROP
        #define         PRIO
        #define         MSWIN
        #define         WILDCARDS
				#define					DIRECTORY_SEPARATOR		'\\'
				#define					WAIT_KEY
#endif


/*____ Linux i386 defines ___________________________________________________*/

#if SYSTEM == LINUX_I386

        #define         BYTEORDER							LITTLE_ENDIAN
				#define					INLINE								inline
				#define					DIRECTORY_SEPARATOR		'/'
#endif

/*____ Linux Sparc defines __________________________________________________*/

#if SYSTEM == LINUX_SPARC

				#define        	BYTEORDER             BIG_ENDIAN
				#define					INLINE								inline
				#define         DIRECTORY_SEPARATOR   '/'
#endif

/*____ LinuxPPC defines _____________________________________________________*/

#if SYSTEM == LINUX_PPC
				
				#define         BYTEORDER               BIG_ENDIAN
				#define					INLINE									inline
				#define         DIRECTORY_SEPARATOR     '/'
#endif

/*____ Solaris defines ______________________________________________________*/

#if SYSTEM == SOLARIS

				#define					BYTEORDER							BIG_ENDIAN
				#define					DIRECTORY_SEPARATOR		'/'
#endif


/*____ OS/2 _________________________________________________________________*/

#if SYSTEM == IBM_OS2

				#define					BYTEORDER							LITTLE_ENDIAN
        #define         INLINE								inline
        #define         PRIO
        #define         WILDCARDS
				#define					DIRECTORY_SEPARATOR		'\\'
        #define         OS2
				#define					WAIT_KEY
#endif


/*____ IRIX defines _________________________________________________________*/

#if SYSTEM == IRIX

				#define					BYTEORDER							BIG_ENDIAN
				#define					DIRECTORY_SEPARATOR		'/'
#endif


/*____ TOS __________________________________________________________________*/

#if SYSTEM == ATARI_TOS

				#define					BYTEORDER							BIG_ENDIAN
        #define         INLINE								inline
				#define					DIRECTORY_SEPARATOR		'\\'
        #define         TOS
 #endif


/*____ UNIXWARE7 ____________________________________________________________*/

#if SYSTEM == UNIXWARE7

				#define					BYTEORDER							LITTLE_ENDIAN
				#define					DIRECTORY_SEPARATOR		'/'
#endif

/*____ SCO OpenServer 5.x ___________________________________________________*/

#if SYSTEM == SCO5

				#define					BYTEORDER							LITTLE_ENDIAN
				#define					DIRECTORY_SEPARATOR		'/'
#endif

/*____ Ultrix defines ________________________________________________________*/

#if SYSTEM == ULTRIX

				#define					BYTEORDER							LITTLE_ENDIAN
        #define         DIRECTORY_SEPARATOR   '/'
        #define         HAVE_USHORT
        #define         HAVE_UINT
        #include        <sys/types.h>                  /* for uint and ushort */
#endif

/*____ NetBSD defines ________________________________________________________*/

#if SYSTEM == NETBSD

        #include        <machine/endian.h>
        #define					BYTEORDER             BYTE_ORDER
        #define         DIRECTORY_SEPARATOR   '/'
        #define         HAVE_USHORT
        #define         HAVE_UINT
        #define         HAVE_ULONG
#endif


/*____ OpenBSD defines ________________________________________________________*/

#if SYSTEM == OPENBSD
				#include				<machine/endian.h>
				#define					BYTEORDER							BYTE_ORDER
				#define					DIRECTORY_SEPARATOR		'/'
				#define					HAVE_USHORT
				#define					HAVE_UINT
				#define					HAVE_ULONG
#endif

/*____ Darwin defines ________________________________________________________*/

#if SYSTEM == DARWIN
#include				<machine/endian.h>
#define					BYTEORDER							BYTE_ORDER
#define					DIRECTORY_SEPARATOR		'/'
//#define					HAVE_USHORT
//#define					HAVE_UINT
//#define					HAVE_ULONG
#endif



/*____ To make sure that certain necessary defines are set... */

#ifndef INLINE
        #define INLINE
#endif

/*==== Other Global Definitions, placed here for convenience ==================*/

#define         FALSE           0
#define         TRUE            1

typedef		unsigned	char 	uchar;

#ifndef				_SYS_TYPES_H
	#ifndef		HAVE_USHORT
		typedef         unsigned        short   ushort;
	#endif
	#ifndef		HAVE_ULONG
		typedef         unsigned        long    ulong;
	#endif
	#ifndef		HAVE_UINT
		typedef         unsigned        int     uint;
	#endif
#endif




