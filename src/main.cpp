/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/


#include "nui.h"
#include "nuiInit.h"

#include "nglConsole.h"
#include "nuiAudioDecoder.h"


int main(int argc, const char** argv)
{
  nuiInit(NULL);
  

  nuiUninit();
  
  return 0;
}
