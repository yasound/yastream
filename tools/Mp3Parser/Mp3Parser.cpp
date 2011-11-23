//
//  Mp3Parser.cpp
//  MP3Parser
//
//  Created by matthieu campion on 11/22/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#include "Mp3Parser.h"
#include <iostream>


Mp3Parser::Mp3Parser(unsigned char* data, int nbBytes)
{
  mpData = data;
  mDataLength = nbBytes;
  
  mCurrentFrame = FindFirstFrame();
}

Mp3Parser::~Mp3Parser()
{
}

void Mp3Parser::Reset()
{
  mCurrentFrame = FindFirstFrame();
}

const Mp3Frame& Mp3Parser::GetCurrentFrame()
{
  return mCurrentFrame;
}

bool Mp3Parser::GoToNextFrame()
{
  Mp3Frame f = FindNextFrame(mCurrentFrame);
  if (!f.IsValid())
    return false;
  
  mCurrentFrame = f;
  return true;
}


void Mp3Parser::ParseAll()
{
  Reset();
  
  bool ok = true;
  while (ok)
  {
//    printf("---FRAME--- %s\n", mCurrentFrame.ToString().c_str());
    ok = GoToNextFrame();
  }
}


Mp3Frame Mp3Parser::FindFirstFrame()
{  
  Mp3Frame header = ComputeFirstFrame();
  return header;
}

Mp3Frame Mp3Parser::FindNextFrame(Mp3Frame previous)
{    
  int nextOffset = previous.GetEndBytePosition();
  TimeMs nextTime = previous.GetEndTime();
  Mp3Frame next(mpData + nextOffset, nextOffset, nextTime);
  if (!next.IsValid())
  {
    next = ComputeNextFrame(previous);
  }
  
  return next;
}


Mp3Frame Mp3Parser::ComputeNextFrame(Mp3Frame previous)
{
  TimeMs time = previous.GetEndTime();
  int b = previous.GetEndBytePosition();
  
  Mp3Frame frame = ComputeNextFrame(b, time);
  return frame;
}

Mp3Frame Mp3Parser::ComputeFirstFrame()
{
  Mp3Frame frame = ComputeNextFrame(0, 0);
  return frame;
}

Mp3Frame Mp3Parser::ComputeNextFrame(int byteOffset, TimeMs time)
{
  Mp3Frame frame;
  int b = byteOffset;
  while (b < mDataLength - 4 && !frame.IsValid()) 
  {
    Mp3Frame temp(mpData + b, b, time);
    if (temp.IsValid())
    {
      // found !
      frame = temp;
    }
    
    b++;
  }
  
  return frame;
}

