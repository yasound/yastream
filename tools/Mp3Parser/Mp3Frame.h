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
  
  int GetBytePosition();
  int GetByteLength();
  int GetEndBytePosition();
  
  TimeMs GetTime();
  TimeMs GetDuration();
  TimeMs GetEndTime();
  
  const Mp3Header& GetHeader();
  
  bool IsValid();
  const std::string ToString() const;
  
private:
  int mPosition;
  TimeMs mTime;
  Mp3Header mHeader;
};
