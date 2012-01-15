//
//  Mp3Parser.cpp
//  MP3Parser
//
//  Created by matthieu campion on 11/22/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#include "Mp3Parser.h"
#include <iostream>


Mp3Parser::Mp3Parser(nglIStream& rStream)
: mrStream(rStream)
{
  //printf("new parser with stream\n");
  mDataLength = mrStream.Available();

  mCurrentFrame = FindFirstFrame();
  //printf("done with first frame? %s\n", YESNO(mCurrentFrame.IsValid()));
  mDuration = 0;
  mId = 0;
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

TimeMs Mp3Parser::GetDuration()
{
  if (mDuration == 0)
  {
    // store current frame
    Mp3Frame frame = mCurrentFrame;

    ParseAll();
    mDuration = mCurrentFrame.GetEndTime();

    // back to stored state
    Reset();
    bool ok = true;
    while (ok && mCurrentFrame != frame)
    {
      ok = GoToNextFrame();
    }
  }

  return mDuration;
}


void Mp3Parser::ParseAll()
{
  Reset();

  bool ok = true;
  while (ok)
  {
    printf("---FRAME--- %s\n", mCurrentFrame.ToString().c_str());
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
  Mp3Frame next(mrStream, nextOffset, nextTime);
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
  //printf("ComputeNextFrame (%d, %d)\n", byteOffset, mDataLength);
  int b = byteOffset;
  while (b < mDataLength - 4)
  {
    Mp3Frame temp(mrStream, b, time);
    if (temp.IsValid())
    {
      // found !
      //printf("ComputeNextFrame ok\n");
      return temp;
    }

    b++;
  }

  //printf("ComputeNextFrame not found\n");
  return Mp3Frame();
}

Mp3Chunk* Mp3Parser::GetChunk()
{
  Mp3Chunk* pChunk = new Mp3Chunk();
  pChunk->SetId(mId++);
  pChunk->SetDuration(0.001 * (double)mCurrentFrame.GetDuration());
  if (ReadFrameBytes(pChunk->GetData()))
    return pChunk;

  delete pChunk;
  return NULL;
}

bool Mp3Parser::ReadFrameBytes(std::vector<uint8>& rData)
{
  mrStream.SetPos(mCurrentFrame.GetBytePosition());
  int32 len = mCurrentFrame.GetByteLength();
  rData.resize(len);
  int32 res = mrStream.Read(&rData[0], len, 1);
  if (len != res)
  {
    rData.resize(res);
    return false;
  }
  return true;
}

