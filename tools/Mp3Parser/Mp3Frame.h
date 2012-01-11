//
//  Mp3Frame.h
//  MP3Parser
//
//  Created by matthieu campion on 11/23/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#pragma once

#include "Mp3Header.h"

class Mp3Frame 
{
public:
  Mp3Frame();
  Mp3Frame(unsigned char* data, int bytePosition, TimeMs time);
  virtual ~Mp3Frame();
  
  const int GetBytePosition() const;
  const int GetByteLength() const;
  const int GetEndBytePosition() const;
  
  const TimeMs GetTime() const;
  const TimeMs GetDuration() const;
  const TimeMs GetEndTime() const;
  
  const Mp3Header& GetHeader() const;
  
  const bool IsValid() const;
  const std::string ToString() const;
  
  bool operator==(const Mp3Frame& rFrame);
  bool operator!=(const Mp3Frame& rFrame);
  
private:
  int mPosition;
  TimeMs mTime;
  Mp3Header mHeader;
};
