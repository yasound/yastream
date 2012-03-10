//
//  Mp3Parser.cpp
//  MP3Parser
//
//  Created by matthieu campion on 11/22/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#include "Mp3Parser.h"
#include "Mp3Chunk.h"
#include <iostream>


Mp3Parser::Mp3Parser(nglIStream& rStream, bool logging, bool SkipPadding)
: mrStream(rStream), mLog(logging), mCurrentFrame(logging), mSkipPadding(SkipPadding), mFirstFrameFound(false)
{
  if (mLog)
    printf("new parser with stream\n");
  mDataLength = mrStream.Available();

  mCurrentFrame = FindFirstFrame();
  mFirstFrameFound = mCurrentFrame.IsValid();
  if (mLog)
    printf("done with first frame? %s\n", YESNO(mCurrentFrame.IsValid()));
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
    printf("---FRAME---(%dms)\n%s \n", (int)mCurrentFrame.GetDuration(), mCurrentFrame.ToString().c_str());
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
  Mp3Frame next(mrStream, nextOffset, nextTime, mLog, !mFirstFrameFound);

  if (next.IsValid())
    if (!mSkipPadding || !next.GetHeader().mIsXing)
      return next;

  next = ComputeNextFrame(previous);
  while (mSkipPadding && next.GetHeader().mIsXing)
  {
    if (!next.IsValid())
      return next;
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
  if (mLog)
    printf("ComputeNextFrame (%d, %d)\n", byteOffset, mDataLength);
  int b = byteOffset;
  while (b < mDataLength - 4)
  {
    if (mLog)
      printf("trying %d\n", b);
    
    Mp3Frame temp(mrStream, b, time, mLog, !mFirstFrameFound);
    if (temp.IsValid())
    {
      // found !
      if (!mSkipPadding || !temp.GetHeader().mIsXing)
      {
        if (mLog)
          printf("ComputeNextFrame ok\n");
        return temp;
      }
    }

    // Not valid:
    // Try to skip ID3 Frames:
    mrStream.SetPos(b);
    unsigned char data[3];
    int64 r = mrStream.Read(data, 3, 1);
    if (!r)
      return Mp3Frame(mLog);

    if (data[0] == 'I' && data[1] == 'D' && data[2] == '3')
    {
      printf("ID3 frame (%c %c %c)\n", data[0], data[1], data[2]);
      
      uint8 version[2];
      uint8 flags;
      uint8 sizes[4];
      r = mrStream.Read(version, 2, 1);
      if (!r)
        return Mp3Frame(mLog);
      
      r = mrStream.Read(&flags, 1, 1);
      if (!r)
        return Mp3Frame(mLog);
      r = mrStream.Read(sizes, 4, 1);
      if (!r)
        return Mp3Frame(mLog);
      
      int32 size = 0;
      for (int32 i = 0; i < 4; i++)
        size = (size << 7) + sizes[i];
      
      if (mLog)
        printf("id3 frame size: %d (0x%x) + 10 bytes of header = %d (0x%x)\n", size, size, size + 10, size + 10);
      
      b += size + 10;

    }
    else if (data[0] == 'T' && data[1] == 'A' && data[2] == 'G')
    {
      // This is an ID3v1.x tag
      b += 128;
    }
    else 
    {
      if (mSkipPadding && temp.GetHeader().mIsXing)
        b = temp.GetEndBytePosition();
      else // Try next byte...
        b++;
    }
  }

  if (mLog)
    printf("ComputeNextFrame not found\n");
  return Mp3Frame(mLog);
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

void Mp3Parser::SetLog(bool l)
{
  mLog = l;
}

nglAtomic Mp3Chunk::mCount = 0;
