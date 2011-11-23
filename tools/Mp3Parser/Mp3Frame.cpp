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

Mp3Frame::Mp3Frame(unsigned char* data, int bytePosition, TimeMs time)
: mPosition(bytePosition),
  mTime(time),
  mHeader(data)
{
}

Mp3Frame::~Mp3Frame()
{
  
}

int Mp3Frame::GetBytePosition()
{
  return mPosition;
}

int Mp3Frame::GetByteLength()
{
  int length = mHeader.GetFrameByteLength();
  return length;
}

int Mp3Frame::GetEndBytePosition()
{
  int end = GetBytePosition() + GetByteLength();
  return end;
}

TimeMs Mp3Frame::GetTime()
{
  return mTime;
}

TimeMs Mp3Frame::GetDuration()
{
  TimeMs duration = mHeader.GetFrameDuration();
  return duration;
}

TimeMs Mp3Frame::GetEndTime()
{
  TimeMs end = GetTime() + GetDuration();
  return end;
}

const Mp3Header& Mp3Frame::GetHeader()
{
  return mHeader;
}

bool Mp3Frame::IsValid()
{
  bool valid = mHeader.IsValid();
  return valid;
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
