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
      int channels = 2;
      int sampleFramesPerFrame = 1152;
      int samplesPerFrame = sampleFramesPerFrame * channels;
      int bytesPerFrame = samplesPerFrame * sizeof(float);
      NSString* src = @"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/test.mp3";
   
      int nbFrames1 = 5;
      NSData* data1 = copyFrames(src, nbFrames1, 0);
      
      int nbFramesDecode2 = 2;
      int nbFramesFeed2 = nbFramesDecode2 + 4;
      int nbFramesSkip2 = 1;
      int nbFrames2 = nbFramesDecode2 - nbFramesSkip2;
      
      NSData* decoded_all = decode(src, nbFramesFeed2, nbFrames1);
      NSData* decoded = [NSData dataWithBytes:decoded_all.bytes length:nbFramesDecode2 * bytesPerFrame];
      
      Mp3Parser parser2 = getParser(decoded);
      std::vector<float> wav2;
      wav2.resize(nbFrames2 * samplesPerFrame);
      const char* buffer = (const char*)decoded.bytes + nbFramesSkip2 * bytesPerFrame;
      memcpy(&wav2[0], buffer, nbFrames2 * bytesPerFrame);
      
      int nbFrames3 = 6;
      std::vector<float> signal;
      generateTestSignal(gNbSampleFramesPerMPEGFrame * nbFrames3, channels, signal);
      
      
      std::vector<float> wav;
      wav.resize((nbFrames2 + nbFrames3) * samplesPerFrame);
      
      memcpy(&wav[0], &wav2[0], nbFrames2 * bytesPerFrame);
      memcpy(&wav[nbFrames2 * samplesPerFrame], &signal[0], nbFrames3 * bytesPerFrame);
      
      
      NSData* data3 = encode(wav, channels);
      
      NSMutableData* outData = [NSMutableData data];
      [outData appendData:data1];
      [outData appendData:data3];
      
      writeToFile(@"mix-overlap.mp3", outData);      
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



//int main (int argc, const char * argv[])
//{
//  @autoreleasepool 
//  {    
//    float samplerate = 44100;
//    float bitrate = 192;
//    
//    // LAME
//    lame_global_flags* lameFlags;
//    lameFlags = lame_init();
//    lame_set_in_samplerate(lameFlags, samplerate);
//    lame_set_num_channels(lameFlags, 2);
//    lame_set_scale(lameFlags, 1.0f);
//    lame_set_out_samplerate(lameFlags, samplerate);
//    lame_set_bWriteVbrTag(lameFlags, 0);
//    lame_set_quality(lameFlags, 0);
//    lame_set_errorf(lameFlags, OnLameError);
//    lame_set_debugf(lameFlags, OnLameDebug);
//    lame_set_msgf(lameFlags, OnLameMsg);
//    lame_set_brate(lameFlags, bitrate);
//    lame_init_params(lameFlags);
//    lame_print_config(lameFlags);
//    
//    int nogap = lame_get_nogap_total(lameFlags);
//    lame_set_nogap_total(lameFlags, 1);
//    nogap = lame_get_nogap_total(lameFlags);
//    
//    int disable_reservoir = lame_get_disable_reservoir(lameFlags);
//    lame_set_disable_reservoir(lameFlags, 1);
//    disable_reservoir = lame_get_disable_reservoir(lameFlags);
//
//
//    
//    
//    if (argc < 3)
//      return -1;
//    
//    NSString* file1 = [NSString stringWithCString:argv[1] encoding:NSUTF8StringEncoding];
//    NSData* data = [NSData dataWithContentsOfFile:file1];
//    
//    if (!data)
//      return -2;
//
//    // mpg123
//    int err = mpg123_init();
//    mpg123_handle* handle1 = NULL;
//    handle1 = mpg123_new(NULL, &err);    
//    err = mpg123_open_feed(handle1);
//    err = mpg123_param(handle1, MPG123_REMOVE_FLAGS, MPG123_GAPLESS, 0);
//    mpg123_format_none(handle1);
//    err = mpg123_format(handle1, samplerate, MPG123_STEREO, MPG123_ENC_FLOAT_32);
//
//    
//    
//    // source
//    unsigned char* src = (unsigned char*)data.bytes;
//    int src_bytes = (int)data.length;
//    
//    // out data
//    NSMutableData* out_data = [[NSMutableData alloc] init];
//    int written = 0;
//    
//    // Mp3Parser
//    Mp3Parser parser(src, src_bytes);
//    
//    Mp3Frame current = parser.GetCurrentFrame();
//    Mp3Frame last;
//    
//    // copy mp3 frames byte directly from source file to dest file
//    do 
//    {
//      current = parser.GetCurrentFrame();
//      if (!current.IsValid())
//        break;
//      
//      unsigned char* mp3buf = src + current.GetBytePosition();
//      int mp3buf_bytes = current.GetByteLength();
//      
//      NSLog(@"1 frame directly copied to file (%d bytes)", mp3buf_bytes);
//      [out_data appendBytes:mp3buf length:mp3buf_bytes];
//      written += mp3buf_bytes;
//      
//      last = current;
//    } while (parser.GoToNextFrame() && parser.GetCurrentFrame().GetTime() < (8 * 1000));
//    
//    
//    unsigned char* mp3buf_in = src + last.GetBytePosition();
//    int mp3buf_in_bytes = last.GetByteLength();
//    
//    mpg123_feed(handle1, mp3buf_in, mp3buf_in_bytes);
//      
//    int pcm_sampleframes;
//    int channels = 2;
//    float* pcm;
//    
//    int mp3buf_out_size = 1.25 * 1152 + 7200;
//    unsigned char* mp3buf_out = new unsigned char[mp3buf_out_size];
//
//    
//    
//    off_t frame_id;
//    unsigned char* audio;
//    size_t audio_bytes;
//    
//    
//    int error_mpg123 = MPG123_OK;
//    
//    error_mpg123 = mpg123_decode_frame(handle1, &frame_id, &audio, &audio_bytes);
//    while (error_mpg123 == MPG123_NEED_MORE) 
//    {
//      mp3buf_in = src + current.GetBytePosition();
//      mp3buf_in_bytes = current.GetByteLength();
//      
//      mpg123_feed(handle1, mp3buf_in, mp3buf_in_bytes);
//      
//      parser.GoToNextFrame();
//      current = parser.GetCurrentFrame();
//      
//      error_mpg123 = mpg123_decode_frame(handle1, &frame_id, &audio, &audio_bytes);
//    }
//    
//    if (error_mpg123 == MPG123_NEW_FORMAT)
//      error_mpg123 = mpg123_decode_frame(handle1, &frame_id, &audio, &audio_bytes);
//    
//    if (error_mpg123 != MPG123_OK)
//      NSLog(@"oops ERROR !!!");
//    
//    // the first frame has been decoded to 'audio'
//    pcm = ((float*)audio) + channels * 576;
//    pcm_sampleframes = 576;
////    pcm = ((float*)audio);
////    pcm_sampleframes = 1152;
//
//    lame_encode_buffer_interleaved_ieee_float(lameFlags, pcm, pcm_sampleframes, NULL, 0);
//    
//    int towrite = 0;
//    int toskip = 627;
////    int toskip = 0;
//    
//    while (current.GetTime() < 12 * 1000) 
//    {
//      error_mpg123 = mpg123_decode_frame(handle1, &frame_id, &audio, &audio_bytes);
//      if (error_mpg123 != MPG123_OK)
//        NSLog(@"oops ERROR 2 !!!");
//      
//      if (audio_bytes != (1152 * channels * sizeof(float)))
//        NSLog(@"oops ERROR 3 !!!");  
//          
//      pcm = (float*)audio;
//      pcm_sampleframes = 1152;
//      
//      towrite = lame_encode_buffer_interleaved_ieee_float(lameFlags, pcm, pcm_sampleframes, mp3buf_out, mp3buf_out_size);
//      
//      NSLog(@"encoded: %d bytes", towrite);
//      if (towrite > 0)
//      {
//        int skip = MIN(towrite, toskip);
//        int write = towrite - skip;
//        [out_data appendBytes:(mp3buf_out + skip) length:write];
//        written += write;
//        toskip -= skip;
//      }
//      
//      mp3buf_in = src + current.GetBytePosition();
//      mp3buf_in_bytes = current.GetByteLength();
//      
//      mpg123_feed(handle1, mp3buf_in, mp3buf_in_bytes);      
//      
//      parser.GoToNextFrame();
//      current = parser.GetCurrentFrame();
//    }
//    
//    do 
//    {
//      towrite = lame_encode_flush(lameFlags, mp3buf_out, mp3buf_out_size);
//      NSLog(@"flushed: %d bytes", towrite);
//      if (towrite > 0)
//      {
//        [out_data appendBytes:mp3buf_out length:towrite];
//        written += towrite;
//      }
//      
//    } while (towrite > 0);
//
//    
//    NSLog(@"junction frame %f", parser.GetCurrentFrame().GetTime() / 1000.f);
//    
//    
//    // copy mp3 frames byte directly from source file to dest file
//    do 
//    {
//      current = parser.GetCurrentFrame();
//      if (!current.IsValid())
//        break;
//      
//      unsigned char* mp3buf = src + current.GetBytePosition();
//      int mp3buf_bytes = current.GetByteLength();
//      
//      NSLog(@"1 frame directly copied to file (%d bytes)", mp3buf_bytes);
//      [out_data appendBytes:mp3buf length:mp3buf_bytes];
//      written += mp3buf_bytes;
//      
//      last = current;
//    } while (parser.GoToNextFrame() && parser.GetCurrentFrame().GetTime() < (20 * 1000));
//    
//    
//    
//
//    
//    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, NO);
//    NSString* fullPathToDesktop = [paths objectAtIndex:0];
//    NSString* outpath = [[fullPathToDesktop stringByAppendingPathComponent:@"mix.mp3"] stringByExpandingTildeInPath];
//    
//    [out_data writeToFile:outpath atomically:YES];    
//
//    
//    
//    
//    
//    
//    
//    int nbFrames = 0;
//    Mp3Parser parser_out((unsigned char*)out_data.bytes, (int)out_data.length);
//    do
//    {
//      if (parser_out.GetCurrentFrame().IsValid())
//      {
//        nbFrames++;
//        NSLog(@"%s", parser_out.GetCurrentFrame().ToString().c_str());
//      }
//    }
//    while (parser_out.GoToNextFrame());
//    
//    
//    NSLog(@"%d bytes written to file", written);
//    NSLog(@"%d frames (out)", nbFrames);
//
//  }
//  
//  return 0;
//}

//int main (int argc, const char * argv[])
//{
//  @autoreleasepool 
//  {    
//    float samplerate = 44100;
//    float bitrate = 192;
//    
//    // LAME
//    lame_global_flags* lameFlags;
//    lameFlags = lame_init();
//    lame_set_in_samplerate(lameFlags, samplerate);
//    lame_set_num_channels(lameFlags, 2);
//    lame_set_scale(lameFlags, 1.0f);
//    lame_set_out_samplerate(lameFlags, samplerate);
//    lame_set_bWriteVbrTag(lameFlags, 0);
//    lame_set_quality(lameFlags, 0);
//    //lame_set_mode(lameFlags, 0);
//    lame_set_errorf(lameFlags, OnLameError);
//    lame_set_debugf(lameFlags, OnLameDebug);
//    lame_set_msgf(lameFlags, OnLameMsg);
//    lame_set_brate(lameFlags, bitrate);
//    //lame_set_asm_optimizations(lameFlags, 1);
//    lame_init_params(lameFlags);
//    lame_print_config(lameFlags);
//    //
//
//    int pcmBlockSampleFrames = 1152;
//    int pcmBlocks = 50;
//    int pcmSampleFrames = (1 + pcmBlocks) * pcmBlockSampleFrames;
//    int channels = 2;
//    std::vector<short*> pcm;
//    for (int c = 0; c < channels; c++)
//    {
//      short* channelpcm = new short[pcmSampleFrames];
//      pcm.push_back(channelpcm);
//    }
//
//    int semiperiod = pcmBlockSampleFrames / 2;
//    int count = 0;
//    float sign = 1;
//    for (int i = 0; i < pcmSampleFrames; i++)
//    {
//      short val;
//      if (i / pcmBlockSampleFrames == 0)
//        val = (short)(32768.f * 0.2 * sign);
//      else if (i / pcmBlockSampleFrames < 5)
//        val = (short)(32768.f * 0.4 * sign);
//      else
//        val = (short)(32768.f * 0.7 * sign);
//        
//      for (int c = 0; c < pcm.size(); c++)
//        pcm[c][i] = val;
//      
//      count++;
//      if (count >= semiperiod)
//      {
//        count = 0;
//        sign *= -1.f;
//      }
//    }
//    
//    
//    NSMutableData* data = [[NSMutableData alloc] init];
//    
//    int nogap = lame_get_nogap_total(lameFlags);
//    lame_set_nogap_total(lameFlags, 1);
//    nogap = lame_get_nogap_total(lameFlags);
//    
//    int disable_reservoir = lame_get_disable_reservoir(lameFlags);
//    lame_set_disable_reservoir(lameFlags, 1);
//    disable_reservoir = lame_get_disable_reservoir(lameFlags);
//    
//
//    int mp3buf_size = 1.25 * pcmBlockSampleFrames + 7200;
//    unsigned char* mp3buf = new unsigned char[mp3buf_size];
//    int towrite = 0;
//    int written = 0;
//    
//    
//    short* pcm0 = pcm[0] + 576;
//    short* pcm1 = pcm[1] + 576;
//    towrite = lame_encode_buffer(lameFlags, pcm0, pcm1, 576, NULL, 0);
////    int toskip = 626;
//    int toskip = 0;
//    
//    for (int b = 0; b < pcmBlocks; b++)
//    {
//      short* pcm0 = pcm[0] + (b + 1) * pcmBlockSampleFrames;
//      short* pcm1 = pcm[1] + (b + 1) * pcmBlockSampleFrames;
//      towrite = lame_encode_buffer(lameFlags, pcm0, pcm1, pcmBlockSampleFrames, mp3buf, mp3buf_size);
//      
//      NSLog(@"encoded: %d bytes", towrite);
//      if (towrite > 0)
//      {
//        int skip = MIN(towrite, toskip);
//        int write = towrite - skip;
//        [data appendBytes:(mp3buf + skip) length:write];
//        written += write;
//        toskip -= skip;
//      }
//      
//    }
//      
//    do 
//    {
//      towrite = lame_encode_flush(lameFlags, mp3buf, mp3buf_size);
//      NSLog(@"flushed: %d bytes", towrite);
//      if (towrite > 0)
//      {
//        [data appendBytes:mp3buf length:towrite];
//        written += towrite;
//      }
//        
//    } while (towrite > 0);
//    
//    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, NO);
//    NSString* fullPathToDesktop = [paths objectAtIndex:0];
//    NSString* outpath = [[fullPathToDesktop stringByAppendingPathComponent:@"mix.mp3"] stringByExpandingTildeInPath];
//    
//    [data writeToFile:outpath atomically:YES];    
//    
//    
//    int nbFrames = 0;
//    Mp3Parser parser((unsigned char*)data.bytes, (int)data.length);
//    do
//    {
//      if (parser.GetCurrentFrame().IsValid())
//      {
//        nbFrames++;
//        NSLog(@"%s", parser.GetCurrentFrame().ToString().c_str());
//      }
//    }
//    while (parser.GoToNextFrame());
//    
//    
//    NSLog(@"%d bytes written to file", written);
//    NSLog(@"%d frames", nbFrames);
//  }
//  return 0;
//}


//int main (int argc, const char * argv[])
//{
//  @autoreleasepool 
//  {
//    if (argc < 3)
//      return -1;
//    
//    NSString* file1 = [NSString stringWithCString:argv[1] encoding:NSUTF8StringEncoding];
//    NSString* file2 = [NSString stringWithCString:argv[2] encoding:NSUTF8StringEncoding];
//    
//    NSData* data1 = [NSData dataWithContentsOfFile:file1];
//    NSData* data2 = [NSData dataWithContentsOfFile:file2];
//    
//    if (!data1 || !data2)
//      return -2;
//    
//    unsigned char* bytes1 = (unsigned char*)data1.bytes;
//    int nbBytes1 = (int)data1.length;
//    
//    unsigned char* bytes2 = (unsigned char*)data2.bytes;
//    int nbBytes2 = (int)data2.length;
//    
//    Mp3Parser parser1(bytes1, nbBytes1);
//    Mp3Parser parser2(bytes2, nbBytes2);
//    
//    
//    TimeMs duration1 = parser1.GetDuration();
//    TimeMs duration2 = parser2.GetDuration();
//    
//    int min1 = (int)(duration1 / 1000 / 60);
//    int sec1 = (int)(duration1 / 1000) - min1 * 60;
//    
//    int min2 = (int)(duration2 / 1000 / 60);
//    int sec2 = (int)(duration2 / 1000) - min2 * 60;
//    
//    NSLog(@"'%@' => %d:%02d", [file1 lastPathComponent], min1, sec1);
//    NSLog(@"'%@' => %d:%02d", [file2 lastPathComponent], min2, sec2);
//    
//    
//    float samplerate = parser1.GetCurrentFrame().GetHeader().mSamplerate;
//    float bitrate = parser1.GetCurrentFrame().GetHeader().mBitrate;
//    
//    // LAME
//    lame_global_flags* lameFlags;
//    lameFlags = lame_init();
//    lame_set_in_samplerate(lameFlags, samplerate);
//    lame_set_num_channels(lameFlags, 2);
//    lame_set_scale(lameFlags, 1.0f);
//    lame_set_out_samplerate(lameFlags, samplerate);
//    lame_set_bWriteVbrTag(lameFlags, 0);
//    lame_set_quality(lameFlags, 0);
//    //lame_set_mode(lameFlags, 0);
//    lame_set_errorf(lameFlags, OnLameError);
//    lame_set_debugf(lameFlags, OnLameDebug);
//    lame_set_msgf(lameFlags, OnLameMsg);
//    lame_set_brate(lameFlags, bitrate);
//    //lame_set_asm_optimizations(lameFlags, 1);
//    lame_init_params(lameFlags);
//    lame_print_config(lameFlags);
//    //
//    
//    // mpg123
//    int err = mpg123_init();
//    mpg123_handle* mpg123Handle1 = NULL;
//    mpg123_handle* mpg123Handle2 = NULL;
//    
//    mpg123Handle1 = mpg123_new(NULL, &err);    
//    err = mpg123_open_feed(mpg123Handle1);
//    
//    mpg123Handle2 = mpg123_new(NULL, &err);
//    err = mpg123_open_feed(mpg123Handle2);
//    
//    err = mpg123_param(mpg123Handle1, MPG123_REMOVE_FLAGS, MPG123_GAPLESS, 0);
//    err = mpg123_param(mpg123Handle2, MPG123_REMOVE_FLAGS, MPG123_GAPLESS, 0);
//    
//    
//    
//    mpg123_format_none(mpg123Handle1);
//    err = mpg123_format(mpg123Handle1, samplerate, MPG123_STEREO, MPG123_ENC_FLOAT_32);
//    
//    mpg123_format_none(mpg123Handle2);
//    err = mpg123_format(mpg123Handle2, samplerate, MPG123_STEREO, MPG123_ENC_FLOAT_32);
//    //
//    
//    
//    NSMutableData* data = [[NSMutableData alloc] init];
//    
//    
//    TimeMs crossfadeLength = 10 * 1000;
//    int crossfadeSamples = crossfadeLength / 1000 * samplerate;
//    TimeMs crossfadeStart = duration1 - crossfadeLength;
//    
//    do
//    {
//      Mp3Frame f = parser1.GetCurrentFrame();
//      int offset = f.GetBytePosition();
//      int length = f.GetByteLength();
//      
//      void* bytes = bytes1 + offset;
//      [data appendBytes:bytes length:length];
//    }
//    while (parser1.GoToNextFrame() && parser1.GetCurrentFrame().GetTime() < crossfadeStart);
//    
//    
//    // mix
//    int crossfadeDone = 0;
//    bool isOver1 = false;
//    
//    int nbChannels = 2;
//    int outBufferFrames = 1152;
//    float* outBuffer = new float[outBufferFrames * nbChannels];
//    
//    while (crossfadeDone < crossfadeSamples) 
//    {
//      Mp3Frame f1 = parser1.GetCurrentFrame();
//      Mp3Frame f2 = parser2.GetCurrentFrame();
//      
//      unsigned char* buffer1 = bytes1 + f1.GetBytePosition();
//      int bufferSize1 = f1.GetByteLength();
//      
//      unsigned char* buffer2 = bytes2 + f2.GetBytePosition();
//      int bufferSize2 = f2.GetByteLength();
//      
//      err = mpg123_feed(mpg123Handle1, buffer1, bufferSize1);
//      err = mpg123_feed(mpg123Handle2, buffer2, bufferSize2);
//      
//      off_t frameOffset1, frameOffset2;
//      unsigned char* audio1;
//      unsigned char* audio2;
//      size_t audioBytes1, audioBytes2;
//      float* out1;
//      float* out2;
//      
//      
//      
//      
//      int res1 = MPG123_OK;
//      int res2 = MPG123_OK;
//      while ((res1 == MPG123_OK || res1 == MPG123_NEW_FORMAT) && (res2 == MPG123_OK || res2 == MPG123_NEW_FORMAT)) 
//      {
//        res1 = mpg123_decode_frame(mpg123Handle1, &frameOffset1, &audio1, &audioBytes1);
//        res2 = mpg123_decode_frame(mpg123Handle2, &frameOffset2, &audio2, &audioBytes2);
//        
//        if (audioBytes1 != audioBytes2)
//          printf("error not the same size !!!\n");
//        
//        if (audio1 && audio2)
//        {
//          out1 = (float*)audio1;
//          out2 = (float*)audio2;
//          int nbSampleframes = (int)(audioBytes1 / nbChannels) / sizeof(float);
//          
//          if (outBufferFrames < nbSampleframes)
//          {
//            delete[] outBuffer;
//            outBufferFrames = nbSampleframes * nbChannels;
//            outBuffer = new float[outBufferFrames];
//          }
//          
//          float v1, v2;
//          for (int c = 0; c < nbChannels; c++)
//          {
//            for (int i = 0; i < nbSampleframes; i++)
//            {
//              v1 = out1[i * nbChannels + c] * (1.f - ((float)crossfadeDone + i) / (float)crossfadeSamples);
//              v2 = out2[i * nbChannels + c] * (((float)crossfadeDone + i) / (float)crossfadeSamples);
//              outBuffer[i] = v1 + v2;
//            }
//          }
//          crossfadeDone += nbSampleframes;
//          
//          // encode outBuffer
//          int mp3bufSize = f1.GetHeader().GetFrameByteLength();
//          unsigned char* mp3buf = new unsigned char[mp3bufSize * 10];
//          int lameRes = lame_encode_buffer_interleaved_ieee_float(lameFlags, outBuffer, nbSampleframes, mp3buf, mp3bufSize);
//          if (lameRes > 0)
//          {
//            printf("append %d bytes\n", lameRes);
//            [data appendBytes:mp3buf length:lameRes];
//          }
//          
//          delete[] mp3buf;
//        
//        }
//        else 
//        {
//          NSLog(@"prout");
//        }
//      }
//      
//      isOver1 = !parser1.GoToNextFrame();
//      parser2.GoToNextFrame();
//      
//    }
//    
//    
//    do
//    {
//      Mp3Frame f = parser2.GetCurrentFrame();
//      int offset = f.GetBytePosition();
//      int length = f.GetByteLength();
//      
//      void* bytes = bytes2 + offset;
//      [data appendBytes:bytes length:length];
//    }
//    while (parser2.GoToNextFrame());
//    
//    
//    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, NO);
//    NSString* fullPathToDesktop = [paths objectAtIndex:0];
//    NSString* outpath = [[fullPathToDesktop stringByAppendingPathComponent:@"mix.mp3"] stringByExpandingTildeInPath];
//    
//    [data writeToFile:outpath atomically:YES];
//  }
//    return 0;
//}




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

