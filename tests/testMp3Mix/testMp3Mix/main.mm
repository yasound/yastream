//
//  main.m
//  testMp3Mix
//
//  Created by matthieu campion on 11/23/11.
//

#import <Foundation/Foundation.h>
#include "Mp3Parser.h"

#include <lame/lame.h>
#include </opt/local/include/mpg123.h>

#include <vector>
#include <math.h>

#include "Utils.h"

void OnLameError(const char *format, va_list ap);
void OnLameDebug(const char *format, va_list ap);
void OnLameMsg(const char *format, va_list ap);

NSData* decode(NSString* srcPath, int nbInFrames, int nbSkipInFrames, bool skipDecoderDelay=true);
void writeToFile(NSString* destFileName, NSData* data);
bool decodeAndWriteToFile(NSString* srcPath, int nbInFrames, int nbSkipInFrames, NSString* destFileName, bool skipDecoderDelay=true);

void generateTestSignal(int nbSampleFrames, int nbChannels, std::vector<float>& rBuffer);

NSData* encode(const std::vector<float>& buffer, int nbInChannels);
void encodeAndWriteToFile(const std::vector<float>& buffer, int nbInChannels, NSString* destFileName);

NSData* encode(const std::vector<float>& buffer, int nbInChannels, int nbFrames, int nbSkipFrames);
void encodeAndWriteToFile(const std::vector<float>& buffer, int nbInChannels, NSString* destFileName, int nbFrames, int nbSkipFrames);

NSData* copyFrames(NSString* srcPath, int nbFrames, int nbSkipFrames);
void copyFramesAndWriteToFile(NSString* srcPath, int nbFrames, int nbSkipFrames, NSString* destFileName);

Mp3Parser getParser(NSData* data);

int gNbSampleFramesPerMPEGFrame = 1152;

int main (int argc, const char * argv[])
{
  @autoreleasepool 
  { 
      
      NSString* src = @"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/test.mp3";
      int channels = 2;
      int sampleFramesPerFrame = gNbSampleFramesPerMPEGFrame;
      int samplesPerFrame = sampleFramesPerFrame * channels;
   
      int nbFrames1 = 3;
      int framesToCopy = nbFrames1 - 1;
      NSData* data1 = copyFrames(src, framesToCopy, 0);
      writeToFile(@"data1.mp3", data1);
      
      NSData* decoded = decode(src, 3/*enough to get 1 frame*/, framesToCopy);
      
      std::vector<float> wav2;
      int nbFramesGenerated = 10;
      generateTestSignal(gNbSampleFramesPerMPEGFrame * nbFramesGenerated, channels, wav2);

      std::vector<float> wav;
      wav.resize((1 + nbFramesGenerated) * samplesPerFrame);
      memcpy(&wav[0], decoded.bytes, 1 * samplesPerFrame * sizeof(float));

      memcpy(&wav[1 * samplesPerFrame], &wav2[0], nbFramesGenerated * samplesPerFrame * sizeof(float));
      
      NSData* wavData = [NSData dataWithBytes:&wav[0] length:(1 + nbFramesGenerated) * samplesPerFrame * sizeof(float)];
      writeToFile(@"wav.raw", wavData);
      
      NSData* data2 = encode(wav, channels, 1 + nbFramesGenerated, 0/*skip*/);
      writeToFile(@"data2.mp3", data2);
      
      NSMutableData* outData = [NSMutableData data];
      [outData appendData:data1];
      [outData appendData:data2];

      // result
      writeToFile(@"mix.mp3", outData);
  }
  return 0;
}

void generateTestSignal(int nbSampleFrames, int nbChannels, std::vector<float>& rBuffer)
{
    int period = 192;
    float square[period];
    float triangle[period];
    float sine[period];
    float square2[period];
    float triangle2[period];
    float sine2[period];
    float coeff = 0.7;
    int nbStepsInCycle = 6;
    int nbSampleFramesInCycle = nbStepsInCycle * period;
    
    for (int i = 0; i < period; i++)
    {
        float val = (i < period / 2) ? 1 : -1;
        square[i] = val;
        square2[i] = coeff * val;
        
        if (i <= (float)period / 4.f)
        {
            val = (float)i / ((float)period / 4.f);
        }
        else if (i <= 3.f * ((float)period / 4.f))
        {
            val = 1.f - 2.f * (((float)i - (float)period/4.f) / ((float)period/2.f));
        }
        else 
        {
            val = -1.f + ((float)i - 3.f * ((float)period/4.f)) / ((float)period/4.f);
        }
        triangle[i] = val;
        triangle2[i] = coeff * val;
        
        val = sinf(2 * M_PI * i / period);
        sine[i] = val;
        sine2[i] = coeff * val;
    }
    
    rBuffer.resize(nbSampleFrames * nbChannels);
    
    for (int f = 0; f < nbSampleFrames; f++)
    {
        int cycleOffset = f % nbSampleFramesInCycle;
        int stepIndex = floorf((float)cycleOffset / (float)period);
        int stepOffset = cycleOffset % period;
        
        float val;
        if (stepIndex == 0)
            val = square[stepOffset];
        else if (stepIndex == 1)
            val = triangle[stepOffset];
        else if (stepIndex == 2)
            val = sine[stepOffset];
        else if (stepIndex == 3)
            val = square2[stepOffset];
        else if (stepIndex == 4)
            val = triangle2[stepOffset];
        else if (stepIndex == 5)
            val = sine2[stepOffset];
        
        for (int c = 0; c < nbChannels; c++)
            rBuffer[f * nbChannels + c] = val;
    }
}

NSData* encode(const std::vector<float>& buffer, int nbInChannels)
{
    float samplerate = 44100;
    float bitrate = 192;
    
    // LAME
    lame_global_flags* lameFlags;
    lameFlags = lame_init();
    lame_set_in_samplerate(lameFlags, samplerate);
    lame_set_num_channels(lameFlags, nbInChannels);
    lame_set_scale(lameFlags, 1.0f);
    lame_set_out_samplerate(lameFlags, samplerate);
    lame_set_bWriteVbrTag(lameFlags, 0);
    lame_set_quality(lameFlags, 0);
    lame_set_errorf(lameFlags, OnLameError);
    lame_set_debugf(lameFlags, OnLameDebug);
    lame_set_msgf(lameFlags, OnLameMsg);
    lame_set_brate(lameFlags, bitrate);
    lame_init_params(lameFlags);
    lame_print_config(lameFlags);
    
    int nogap = lame_get_nogap_total(lameFlags);
    lame_set_nogap_total(lameFlags, 1);
    nogap = lame_get_nogap_total(lameFlags);
    
    int disable_reservoir = lame_get_disable_reservoir(lameFlags);
    lame_set_disable_reservoir(lameFlags, 1);
    disable_reservoir = lame_get_disable_reservoir(lameFlags);

    NSMutableData* outData = [NSMutableData data];
    
    int inNbSampleFrames = buffer.size() / nbInChannels;
    
    int toFeed = inNbSampleFrames;
    int inputOffset = 0;
    int res = 1;
    while (toFeed > 0 || res > 0) 
    {
        int nbInputSampleFrames = MIN(1152, toFeed);
        const float* input = &buffer[inputOffset * nbInChannels];
        
        int mp3buf_size = 1.25 * 1152 + 7200;
        unsigned char mp3buf[mp3buf_size];
        res = lame_encode_buffer_interleaved_ieee_float(lameFlags, input, nbInputSampleFrames, mp3buf, mp3buf_size);
        if (res >= 0)
        {
            NSLog(@"%d bytes encoded", res);
            [outData appendBytes:mp3buf length:res];
        }
        else
            NSLog(@"encode error = %d", res);
        
        inputOffset += nbInputSampleFrames;
        toFeed -= nbInputSampleFrames;
    }
    
    
    
    return outData;
}


NSData* encode(const std::vector<float>& buffer, int nbInChannels, int nbFrames, int nbSkipFrames)
{
    NSData* data = encode(buffer, nbInChannels);
    int nbBytes = data.length;
    const char* bytes = (const char*)data.bytes;
    
    nglIMemory stream(bytes, nbBytes);
    Mp3Parser parser(stream, false, true);
    
    NSMutableData* outData = [NSMutableData data];
    
    int f = 0;
    int skip = nbSkipFrames;
    bool ok = true;
    while (f < nbFrames && ok) 
    {
        Mp3Frame frame = parser.GetCurrentFrame();
        if (frame.IsValid() && !frame.GetHeader().mIsXing)
        {
            if (skip == 0)
            {
                int b = frame.GetByteLength();
                std::vector<uint8> buffer;
                buffer.resize(b);
                parser.ReadFrameBytes(buffer);
                [outData appendBytes:&buffer[0] length:b];
                f++;
            }
            else
            {
                skip--;
            }
        }
        ok = parser.GoToNextFrame();
    }
    
    return outData;
}

void encodeAndWriteToFile(const std::vector<float>& buffer, int nbInChannels, NSString* destFileName)
{
    NSData* data = encode(buffer, nbInChannels);
    writeToFile(destFileName, data);
}

void encodeAndWriteToFile(const std::vector<float>& buffer, int nbInChannels, NSString* destFileName, int nbFrames, int nbSkipFrames)
{
    NSData* data = encode(buffer, nbInChannels, nbFrames, nbSkipFrames);
    writeToFile(destFileName, data);
}

NSData* copyFrames(NSString* srcPath, int nbFrames, int nbSkipFrames)
{
    NSData* inData = [NSData dataWithContentsOfFile:srcPath];
    Mp3Parser parser = getParser(inData);
    
    NSMutableData* outData = [NSMutableData data];
    
    
    int f = 0;
    int skip = nbSkipFrames;
    bool ok = true;
    while (f < nbFrames && ok) 
    {
        Mp3Frame frame = parser.GetCurrentFrame();
        if (frame.IsValid() && !frame.GetHeader().mIsXing)
        {
            if (skip == 0)
            {
                f++;
                int bytes = frame.GetByteLength();
                std::vector<uint8> data;
                data.resize(bytes);
                parser.ReadFrameBytes(data);
                [outData appendBytes:&data[0] length:bytes];
            }
            else 
            {
                skip--;
            }
        }
        ok = parser.GoToNextFrame();
    }
    
    return outData;
}

void copyFramesAndWriteToFile(NSString* srcPath, int nbFrames, int nbSkipFrames, NSString* destFileName)
{
    NSData* data = copyFrames(srcPath, nbFrames, nbSkipFrames);
    writeToFile(destFileName, data);
}



NSData* decode(NSString* srcPath, int nbInFrames, int nbSkipInFrames, bool skipDecoderDelay)
{
    double samplerate = 44100;
    int err = mpg123_init();
    mpg123_handle* handle = NULL;
    handle = mpg123_new(NULL, &err);    
    err = mpg123_open_feed(handle);
    err = mpg123_param(handle, MPG123_REMOVE_FLAGS, MPG123_GAPLESS, 0);
    mpg123_format_none(handle);
    err = mpg123_format(handle, samplerate, MPG123_STEREO, MPG123_ENC_FLOAT_32);
    
    
    NSData* inData = [NSData dataWithContentsOfFile:srcPath];
    Mp3Parser parser = getParser(inData);
    
    int f = 0;
    bool readOK = true;
    bool nextFrameOK = true;
    do {
        Mp3Frame frame = parser.GetCurrentFrame();
        bool isValid = frame.IsValid();
        bool isXing = frame.GetHeader().mIsXing;
        if (isValid && !isXing)
        {
            if (nbSkipInFrames > 0)
            {
                nbSkipInFrames--;
            }
            else 
            {
                int byteLength = frame.GetByteLength();
                std::vector<uint8> frameData;
                frameData.resize(byteLength);
                readOK = parser.ReadFrameBytes(frameData);
                err = mpg123_feed(handle, &frameData[0], byteLength);
                NSLog(@"feed (%d bytes)  res = %s", byteLength, mpg123_plain_strerror(err));
                f++;
            }
            
        }
        nextFrameOK = parser.GoToNextFrame();
    } while (f < nbInFrames && nextFrameOK && readOK);
    
    NSMutableData* outData = [NSMutableData data];
    int channels = 2;
    size_t outSize = 1152 * channels * sizeof(float);
    unsigned char outBuffer[outSize];
    size_t outDone = 0;
    do {
        err = mpg123_read(handle, outBuffer, outSize, &outDone);
        [outData appendBytes:outBuffer length:outDone];
        NSLog(@"read (ask = %d  done = %d)  res = %s", (int)outSize, (int)outDone, mpg123_plain_strerror(err));
    } while (err == MPG123_OK || err == MPG123_NEW_FORMAT);
    
    
    if (skipDecoderDelay)
    {
        int skip = 1104 * channels * sizeof(float);
        const char* bytes = (const char*)outData.bytes;
        int nbBytes = outData.length;
        nbBytes -= skip;
        bytes = bytes + skip;
        if (nbBytes < 0)
            nbBytes = 0;
        outData = [NSMutableData dataWithBytes:bytes length:nbBytes];
    }


    return outData;
}

void writeToFile(NSString* destFileName, NSData* data)
{
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, NO);
    NSString* fullPathToDesktop = [paths objectAtIndex:0];
    NSString* outpath = [[fullPathToDesktop stringByAppendingPathComponent:destFileName] stringByExpandingTildeInPath];
    [data writeToFile:outpath atomically:YES];
}

bool decodeAndWriteToFile(NSString* srcPath, int nbInFrames, int nbSkipInFrames, NSString* destFileName, bool skipDecoderDelay)
{
    NSData* data = decode(srcPath, nbInFrames, nbSkipInFrames, skipDecoderDelay);
    writeToFile(destFileName, data);
    return true;
}

Mp3Parser getParser(NSData* data)
{
    NSUInteger inByteCount = data.length;
    const char* inBytes = (const char*)data.bytes;
    
    nglIMemory* pInStream = new nglIMemory(inBytes, inByteCount); // memory leak !!!!!! (just for testing)
    Mp3Parser parser(*pInStream, false, true);
    return parser;
}





void OnLameError(const char *format, va_list ap)
{
  //  nglString err;
  //  err.CFormat(format, ap);
  //  NGL_LOG("Player", NGL_LOG_INFO, "Lame error: %s\n", err.GetChars()); 
  vprintf(format, ap); 
}

void OnLameDebug(const char *format, va_list ap)
{
  //  nglString err;
  //  err.CFormat(format, ap);
  //  NGL_LOG("Player", NGL_LOG_INFO, "Lame debug: %s\n", err.GetChars()); 
  vprintf(format, ap); 
}

void OnLameMsg(const char *format, va_list ap)
{
  //  nglString err;
  //  err.CFormat(format, ap);
  //  NGL_LOG("Player", NGL_LOG_INFO, "Lame message: %s\n", err.GetChars()); 
  vprintf(format, ap); 
}

