//
//  Mp3Parser.h
//  MP3Parser
//
//  Created by matthieu campion on 11/22/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#pragma once
#include "nui.h"

class Mp3Chunk : public nuiRefCount
{
public:
  Mp3Chunk()
  {
    mTime = nglTime();
    mId = -1;
    
    ngl_atomic_inc(mCount);
    //uint64 c = ngl_atomic_read(mCount);
    //if (!(c % 10))
    //  printf("%lld mp3 chunks (+)\n", c);
  }
  
  ~Mp3Chunk()
  {
    ngl_atomic_dec(mCount);
    //uint64 c = ngl_atomic_read(mCount);
    //if (!(c % 10))
    //  printf("%lld mp3 chunks (-)\n", c);
  }
  
  std::vector<uint8>& GetData()
  {
    return mData;
  }
  
  const std::vector<uint8>& GetData() const
  {
    return mData;
  }
  
  double GetTime() const
  {
    return mTime;
  }
  
  double GetDuration() const
  {
    return mDuration;
  }
  
  void SetDuration(double dur)
  {
    mDuration = dur;
  }
  
  void SetId(int32 id)
  {
    mId = id;
  }
  
  int32 GetId() const
  {
    return mId;
  }
private:
  std::vector<uint8> mData;
  double mTime;
  double mDuration;
  long mId;
  static nglAtomic mCount;
};

