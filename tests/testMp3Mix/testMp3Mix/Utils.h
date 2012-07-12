
//
//  Utils.h
//  testMp3Mix
//
//  Created by matthieu campion on 7/10/12.
//  Copyright (c) 2012 MXP4. All rights reserved.
//

#pragma once
#include </opt/local/include/mpg123.h>
#include "Mp3Parser.h"


class Decoder
{
public:
    Decoder(NSString* src);
    virtual ~Decoder();
    
    NSData* Run();
    
    void SetInSkipFrames(int skip);
    void SetInFramesCount(int count);
    
    void SetOutSkipFrames(int skip);
    void SetOutFramesCount(int count);
    
private:
    void Init();
    
    int GetSamplesPerMp3Frame();
    int GetDecoderDelaySamples();
    
    typedef enum
    {
        eFeedOK = 0,
        eFeedNoData,
        eFeedLimit
    } FeedResult;
    
    typedef enum
    {
        eReadOK = 0,
        eReadNoData,
        eReadLimit
    } ReadResult;
    
    FeedResult Feed();
    ReadResult Read(NSMutableData* data);
    
    NSString* mSrcPath;
    NSData* mInData;
    nglIMemory* mpInStream;
    Mp3Parser* mpParser;
    mpg123_handle* mpMpeg123Handle;
    
    int mSkipInFrames;
    int mSkipOutFrames;
    
    int mSkipInFramesCurrent;
    int mSkipOutSamplesCurrent;
    int mSkipDecoderDelayCurrent;
    
    int mInFrames;
    int mOutFrames;
    
    int mInFramesFed;
    int mOutSamplesRead;
    
    bool mLimitInput;
    bool mLimitOutput;
    
    double mSampleRate;
    int mChannels;
    
    bool mNoMoreMp3Frame;
    
    static int sSampleFramesPerMp3Frame;
};