/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/


#pragma once
#include "nui.h"
#include "nuiAudioEngine.h"
#include "nuiSoundManager.h"
#include "nuiFileSound.h"
#include "nuiFileVoice.h"

#include <lame/lame.h>
#include <shout/shout.h>

#include "Playlist.h"

//class Song:
class Player : public nglThread
{
public:
  Player(const nglString& rStreamName, const nglPath& rPlayListPath, const nglString& rServerIP, int ServerPort, const nglString& rServerPassword, int BitRate);
  virtual ~Player();

  void OnStart();
  
private:
  PlayList mPlayList;
  bool mShutDown;
  
  shout_t* mpShout;
  lame_global_flags* mLameFlags;

  nuiFileSound* mpCurrentSong;
  nuiFileVoice* mpCurrentVoice;
};

