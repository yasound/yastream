//
//  Mp3Frame.cpp
//  MP3Parser
//
//  Created by matthieu campion on 11/23/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#include "Mp3Frame.h"


Mp3Frame::Mp3Frame()
{
  mPosition = 0;
  mTime = 0;
}

Mp3Frame::Mp3Frame(nglIStream& rStream, int bytePosition, TimeMs time)
: mPosition(bytePosition),
  mTime(time),
  mHeader(rStream, bytePosition)
{
}

Mp3Frame::~Mp3Frame()
{
  
}

const int Mp3Frame::GetBytePosition() const
{
  return mPosition;
}

const int Mp3Frame::GetByteLength() const
{
  int length = mHeader.GetFrameByteLength();
  return length;
}

const int Mp3Frame::GetEndBytePosition() const
{
  int end = GetBytePosition() + GetByteLength();
  return end;
}

const TimeMs Mp3Frame::GetTime() const
{
  return mTime;
}

const TimeMs Mp3Frame::GetDuration() const
{
  TimeMs duration = mHeader.GetFrameDuration();
  return duration;
}

const TimeMs Mp3Frame::GetEndTime() const
{
  TimeMs end = GetTime() + GetDuration();
  return end;
}

const Mp3Header& Mp3Frame::GetHeader() const
{
  return mHeader;
}

const bool Mp3Frame::IsValid() const
{
  bool valid = mHeader.IsValid();
  return valid;
}

bool Mp3Frame::operator==(const Mp3Frame& rFrame)
{
  bool same = true;
  same &= (mPosition == rFrame.mPosition);
  same &= (mTime == rFrame.mTime);
  same &= (mHeader == rFrame.mHeader);

  return same;
}

bool Mp3Frame::operator!=(const Mp3Frame& rFrame)
{
  bool different = !(*this == rFrame);
  return different;
}

const std::string Mp3Frame::ToString() const
{
  char temp[1024];
  
  // position
  sprintf(temp, "position: '%d'  ", mPosition);
  std::string positionStr(temp);
  
  // time
  sprintf(temp, "time: '%lu'  ", mTime);
  std::string timeStr(temp);
  
  std::string s;
  s += positionStr;
  s += timeStr;
  s += "   ";
  s += mHeader.ToString();
  
  return s;
}

