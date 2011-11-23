//
//  Mp3Parser.h
//  MP3Parser
//
//  Created by matthieu campion on 11/22/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#pragma once

#include <string>

enum MpegAudioVersion
{
  eMpegVersion2_5 = 0,
  eMpegVersionReserved = 1,
  eMpegVersion2 = 2,
  eMpegVersion1 = 3,
  eMpegVersionUndefined = 4
};

enum MpegLayer
{
  eMpegLayerReserved = 0,
  eMpegLayer3 = 1,
  eMpegLayer2 = 2,
  eMpegLayer1 = 3,
  eMpegLayerUndefined = 4
};

enum MpegChannelMode
{
  eMpegStereo = 0,
  eMpegJointStereo = 1,
  eMpegDualMono = 2,
  eMpegMono = 3,
  eMpegChannelModeUndefined = 4
};

enum MpegEmphasis
{
  eMpegEmphasisNone = 0,
  eMpegEmphasis50_15ms = 1,
  eMpegEmphasisReserved = 2,
  eMpegEmphasisCCIT_J_17 = 3,
  eMpegEmphasisUndefined = 4
};

typedef unsigned long TimeMs;


class Mp3Header 
{
public:
  Mp3Header();
  Mp3Header(unsigned char* data);
  virtual ~Mp3Header();
  
  static float GetBitrate(unsigned int bitrate_index, MpegAudioVersion version, MpegLayer layer);
  static float GetSampleRate(unsigned int samplerate_index, MpegAudioVersion version);
  
  static float sBitrates[5][16];
  static float sSamplerates[3][3];
  static int sSamplesPerFrame[2][3];
  
  bool IsValid();
  const std::string ToString() const;
  
  int GetFrameByteLength();
  int GetFrameHeaderByteLength();
  int GetFrameDataByteLength();
  
  TimeMs GetFrameDuration();
  
  MpegAudioVersion mVersion;
  MpegLayer mLayer;
  MpegChannelMode mChannelMode;
  MpegEmphasis mEmphasis;
  float mBitrate;
  float mSamplerate;
  bool mUseCRC;
  bool mUsePadding;
  bool mIsCopyrighted;
  bool mIsOriginal;
  
  private:
  void Reset();
  void ParseHeaderData(unsigned char* data);
  int GetSamplesPerFrame();
};