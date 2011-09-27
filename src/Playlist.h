/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/

#pragma once

#include "nui.h"
#include "Song.h"


class PlayList
{
public:
  PlayList();
  virtual ~PlayList();
  
  void AddSong(const Song& rSong);
  const std::vector<Song>& GetPool() const;

  Song& GetNextSong();
  void SetSecondsBeforeReplay(double seconds);
private:
  std::vector<Song> mPool;
  double mCurrentTime;
  
  double mSecondBeforeReplay;
};
