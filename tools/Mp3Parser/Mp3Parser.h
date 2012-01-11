//
//  Mp3Parser.h
//  MP3Parser
//
//  Created by matthieu campion on 11/22/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#pragma once
#include "Mp3Frame.h"

class Mp3Parser 
{  
public:
  Mp3Parser(unsigned char* data, int nbBytes);
  virtual ~Mp3Parser();
  
  const Mp3Frame& GetCurrentFrame();
  bool GoToNextFrame();
  
  TimeMs GetDuration();
  
  // for test purpose
  void ParseAll();
  
private:
  unsigned char* mpData;
  int mDataLength;
  
  Mp3Frame mCurrentFrame;
  TimeMs mDuration;
  
  void Reset();
  
  Mp3Frame FindFirstFrame();
  Mp3Frame FindNextFrame(Mp3Frame previous);
  
  Mp3Frame ComputeFirstFrame();
  Mp3Frame ComputeNextFrame(Mp3Frame previous);
  Mp3Frame ComputeNextFrame(int byteOffset, TimeMs time);
};
