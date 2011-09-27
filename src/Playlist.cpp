/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/


#include "nui.h"
#include "Playlist.h"


PlayList::PlayList()
{
  mCurrentTime = 0;
  mSecondBeforeReplay = 60 * 60; // at least 60 minutes before replaying the same song
}

PlayList::~PlayList()
{
  
}

void PlayList::AddSong(const Song& rSong)
{
  mPool.push_back(rSong);
}

const std::vector<Song>& PlayList::GetPool() const
{
  return mPool;
}

Song& PlayList::GetNextSong()
{
  Song* pSong = NULL;
  
  nglTime now;
  double timelimit = now - mSecondBeforeReplay;
  
  int32 i = rand() % mPool.size();
  const int32 first = i;
  while (mPool[i].mLastBroadcastTime > timelimit)
  {
    i++;
    i %= mPool.size();
    
    if (i == first) // Escape! There is no way we can find a correct title in this playlist in a reasonable time.
      return mPool[i];
  }

  return mPool[i];
}

