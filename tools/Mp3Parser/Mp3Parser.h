//
//  Mp3Parser.h
//  MP3Parser
//
//  Created by matthieu campion on 11/22/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#pragma once
#include "nui.h"
#include "nglIStream.h"
#include "Mp3Frame.h"
#include "Mp3Chunk.h"

class Mp3Parser
{
public:
  Mp3Parser(nglIStream& rStream, bool logging, bool SkipPadding);
  virtual ~Mp3Parser();

  const Mp3Frame& GetCurrentFrame();
  bool GoToNextFrame();

  double Seek(double seconds);

  TimeMs GetDuration();

  // for test purpose
  void ParseAll();

  Mp3Chunk* GetChunk();
  bool ReadFrameBytes(std::vector<uint8>& rData);

  void SetLog(bool l);
private:
  nglIStream& mrStream;
  int mDataLength;
  int32 mId;
  bool mSkipPadding;
  bool mFirstFrameFound;

  Mp3Frame mCurrentFrame;
  TimeMs mDuration;

  void Reset();

  Mp3Frame FindFirstFrame();
  Mp3Frame FindNextFrame(Mp3Frame previous);

  Mp3Frame ComputeFirstFrame();
  Mp3Frame ComputeNextFrame(Mp3Frame previous);
  Mp3Frame ComputeNextFrame(int byteOffset, TimeMs time);

  bool mLog;

};
