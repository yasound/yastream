//
//  Utils.cpp
//  testMp3Mix
//
//  Created by matthieu campion on 7/10/12.
//  Copyright (c) 2012 MXP4. All rights reserved.
//

#include "Utils.h"
#include <iostream>


int Decoder::sSampleFramesPerMp3Frame = 1152;

Decoder::Decoder(NSString* src)
:   mSrcPath(src),
    mInData(NULL),
    mpInStream(NULL),
    mpParser(NULL),
    mpMpeg123Handle(NULL),
    mSampleRate(44100),
    mChannels(2),
    mNoMoreMp3Frame(false)
{
    Init();
}

Decoder::~Decoder()
{
    if (mpMpeg123Handle)
        mpg123_delete(mpMpeg123Handle);
    if (mpParser)
        delete mpParser;
    if (mpInStream)
        delete mpInStream;
}


void Decoder::Init()
{
    mSkipInFrames = 0;
    mSkipOutFrames = 0;
    mSkipInFramesCurrent = 0;
    mSkipOutSamplesCurrent = 0;
    
    mInFrames = 0;
    mOutFrames = 0;
    mInFramesFed = 0;
    mOutSamplesRead = 0;
    
    mLimitInput = false;
    mLimitOutput = false;
    
    int err = mpg123_init();
    mpMpeg123Handle = NULL;
    mpMpeg123Handle = mpg123_new(NULL, &err);    
    err = mpg123_open_feed(mpMpeg123Handle);
    err = mpg123_param(mpMpeg123Handle, MPG123_REMOVE_FLAGS, MPG123_GAPLESS, 0);
    mpg123_format_none(mpMpeg123Handle);
    err = mpg123_format(mpMpeg123Handle, mSampleRate, mChannels, MPG123_ENC_FLOAT_32);
    
    mInData = [NSData dataWithContentsOfFile:mSrcPath];
    mpInStream = new nglIMemory(mInData.bytes, mInData.length);

    mpParser = new Mp3Parser(*mpInStream, false, true);
}

void Decoder::SetInSkipFrames(int skip)
{
    mSkipInFrames = skip;
    mSkipInFramesCurrent = 0;
}

void Decoder::SetInFramesCount(int count)
{
    mInFrames = count;
    mInFramesFed = 0;
    mLimitInput = true;
}

void Decoder::SetOutSkipFrames(int skip)
{
    mSkipOutFrames = skip;
    mSkipOutSamplesCurrent = 0;
    mSkipDecoderDelayCurrent = 0;
}

void Decoder::SetOutFramesCount(int count)
{
    mOutFrames = count;
    mOutSamplesRead = 0;
    mLimitOutput = 0;
}

Decoder::FeedResult Decoder::Feed()
{
    bool done = false;
    while (!done && !mNoMoreMp3Frame && (!mLimitInput || mInFramesFed < mInFrames)) 
    {
        if (mSkipInFramesCurrent == mSkipInFrames)
        {
            Mp3Frame frame = mpParser->GetCurrentFrame();
            bool isValid = frame.IsValid();
            bool isXing = frame.GetHeader().mIsXing;
            if (isValid && !isXing)
            {
                int byteLength = frame.GetByteLength();
                std::vector<uint8> frameData;
                frameData.resize(byteLength);
                bool readOK = mpParser->ReadFrameBytes(frameData);
                int err = mpg123_feed(mpMpeg123Handle, &frameData[0], byteLength);

                done = true;
                mInFramesFed++;
            }
        }
        else 
        {
            mSkipInFramesCurrent++;
        }
        
        mNoMoreMp3Frame = !mpParser->GoToNextFrame();
    }
    
    if (mNoMoreMp3Frame)
        return eFeedNoData;
    if (mInFramesFed == mInFrames)
        return eFeedLimit;
    
    return eFeedOK;
}

Decoder::ReadResult Decoder::Read(NSMutableData* data)
{
    int todo = GetSamplesPerMp3Frame();
    float buffer[todo];
    
    int err = MPG123_OK;
    int done = 0;
    while (done < todo && (err == MPG123_OK || err == MPG123_NEW_FORMAT)) 
    {
        int toread = todo - done;
        float* dest = buffer + done;
        
        size_t outDone = 0;
        err = mpg123_read(mpMpeg123Handle, (unsigned char*)dest, toread * sizeof(float), &outDone);

        done += outDone / sizeof(float);
    }
    
    float* src = buffer;
    int samplesToCopy = done;
    
    int delaySkip = GetDecoderDelaySamples() - mSkipDecoderDelayCurrent;
    if (delaySkip > 0)
    {
        int skipNow = MIN(samplesToCopy, delaySkip);
        src = src + skipNow;
        samplesToCopy -= skipNow;
        mSkipDecoderDelayCurrent += skipNow;
    }
    int outSkip = mSkipOutFrames * GetSamplesPerMp3Frame() - mSkipOutSamplesCurrent;
    if (outSkip > 0)
    {
        int skipNow = MIN(samplesToCopy, outSkip);
        src = src + skipNow;
        samplesToCopy -= skipNow;
        mSkipOutSamplesCurrent += skipNow;
    }
    
    int totalToCopy = mOutFrames * GetSamplesPerMp3Frame();
    samplesToCopy = MIN(samplesToCopy, totalToCopy - mOutSamplesRead);
    
    [data appendBytes:src length:samplesToCopy * sizeof(float)];
    
    mOutSamplesRead += samplesToCopy;
    
    if (mOutSamplesRead == totalToCopy)
        return eReadLimit;
    
    if (done < todo)
        return eReadNoData;
    
    return eReadOK;
}

NSData* Decoder::Run()
{
    NSMutableData* out = [NSMutableData data];
    
    bool ok = true;
    while (ok)
    {
        FeedResult res1 = Feed();
        ReadResult res2 = Read(out);
        
        ok &= (res2 != eReadLimit);
        ok &= !( (res1 != eFeedOK) && (res2 == eReadNoData) ); // can't feed and can't read ?
    }
    return out;
}

int Decoder::GetDecoderDelaySamples()
{
    int sampleframes = 1104;
    return sampleframes * mChannels;
}


int Decoder::GetSamplesPerMp3Frame()
{
    return sSampleFramesPerMp3Frame * mChannels;
}