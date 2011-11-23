/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/


#include "nui.h"

#pragma once

//class Song:
class Song
{
public:
  Song(const nglPath& rPath);
  
  nglString mTitle;
  nglString mArtist;
  nglString mAlbum;
  nglPath mPath;
  
  double mDuration;
  double mLastBroadcastTime;
};

