/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/


#include "nui.h"
#include "Song.h"

Song::Song(const nglPath& rPath)
  : mPath(rPath),
    mDuration(0),
    mLastBroadcastTime(0)
{
}


