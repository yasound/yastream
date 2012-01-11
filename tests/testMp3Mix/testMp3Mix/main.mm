//
//  main.m
//  testMp3Mix
//
//  Created by matthieu campion on 11/23/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "Mp3Parser.h"

#include <lame/lame.h>
#include </opt/local/include/mpg123.h>

#include <vector>

void OnLameError(const char *format, va_list ap);
void OnLameDebug(const char *format, va_list ap);
void OnLameMsg(const char *format, va_list ap);

int main (int argc, const char * argv[])
{
  @autoreleasepool 
  {    
    float samplerate = 44100;
    float bitrate = 192;
    
    // LAME
    lame_global_flags* lameFlags;
    lameFlags = lame_init();
    lame_set_in_samplerate(lameFlags, samplerate);
    lame_set_num_channels(lameFlags, 2);
    lame_set_scale(lameFlags, 1.0f);
    lame_set_out_samplerate(lameFlags, samplerate);
    lame_set_bWriteVbrTag(lameFlags, 0);
    lame_set_quality(lameFlags, 0);
    //lame_set_mode(lameFlags, 0);
    lame_set_errorf(lameFlags, OnLameError);
    lame_set_debugf(lameFlags, OnLameDebug);
    lame_set_msgf(lameFlags, OnLameMsg);
    lame_set_brate(lameFlags, bitrate);
    //lame_set_asm_optimizations(lameFlags, 1);
    lame_init_params(lameFlags);
    lame_print_config(lameFlags);
    //
    
    int pcmBlockSampleFrames = 1152;
    int pcmBlocks = 5;
    int pcmSampleFrames = pcmBlocks * pcmBlockSampleFrames;
    int channels = 2;
    std::vector<short*> pcm;
    for (int c = 0; c < channels; c++)
    {
      short* channelpcm = new short[pcmSampleFrames];
      pcm.push_back(channelpcm);
    }
    
    std::vector<short*> pcm_zero;
    for (int c = 0; c < channels; c++)
    {
      short* channelpcm = new short[pcmSampleFrames];
      memset(channelpcm, 0, pcmBlockSampleFrames * sizeof(short));
      pcm_zero.push_back(channelpcm);
    }
    
    int semiperiod = pcmBlockSampleFrames / 2;
    int count = 0;
    float sign = 1;
    for (int i = 0; i < pcmSampleFrames; i++)
    {
      short val;
      if (i / pcmBlockSampleFrames == 0)
        val = (short)(32768.f * 0.2 * sign);
      else if (i / pcmBlockSampleFrames == 1)
        val = (short)(32768.f * 0.4 * sign);
      else if (i / pcmBlockSampleFrames == 2)
        val = (short)(32768.f * 0.6 * sign);
      else if (i / pcmBlockSampleFrames == 3)
        val = (short)(32768.f * 0.8 * sign);
      else if (i / pcmBlockSampleFrames == 4)
        val = (short)(32768.f * 0.99 * sign);
      
      for (int c = 0; c < pcm.size(); c++)
        pcm[c][i] = val;
      
      count++;
      if (count >= semiperiod)
      {
        count = 0;
        sign *= -1.f;
      }
    }
    
    
    NSMutableData* data = [[NSMutableData alloc] init];
    
    int nogap = lame_get_nogap_total(lameFlags);
    lame_set_nogap_total(lameFlags, 1);
    nogap = lame_get_nogap_total(lameFlags);
    
    int disable_reservoir = lame_get_disable_reservoir(lameFlags);
    lame_set_disable_reservoir(lameFlags, 1);
    disable_reservoir = lame_get_disable_reservoir(lameFlags);
    
    
    int mp3buf_size = 1.25 * pcmBlockSampleFrames + 7200;
    unsigned char* mp3buf = new unsigned char[mp3buf_size];
    int towrite = 0;
    int written = 0;
    
    int toskip = 626;
//    int toskip = 0;
    
//    int bytes = 3762;
    int bytes = 20000;
    
    short* pcm0;
    short* pcm1;
    
    // half-frame
    pcm0 = pcm[0] + 2 * pcmBlockSampleFrames + 576;
    pcm1 = pcm[1] + 2 * pcmBlockSampleFrames + 576;
    towrite = lame_encode_buffer(lameFlags, pcm0, pcm1, 576, mp3buf, mp3buf_size);
    if (towrite > 0)
    {
      int skip = MIN(toskip, towrite);
      int write = towrite - skip;
      write = MIN(write, bytes);
      [data appendBytes:(mp3buf + skip) length:write];
      
      written += write;
      toskip -= skip;
    }
    
    pcm0 = pcm[0] + 1 * pcmBlockSampleFrames;
    pcm1 = pcm[1] + 1 * pcmBlockSampleFrames;
    towrite = lame_encode_buffer(lameFlags, pcm0, pcm1, 1152, mp3buf, mp3buf_size);
    if (towrite > 0)
    {
      int skip = MIN(toskip, towrite);
      int write = towrite - skip;
      write = MIN(write, bytes);
      [data appendBytes:(mp3buf + skip) length:write];
      
      written += write;
      toskip -= skip;
    }
    

    
    // 0.2
    pcm0 = pcm[0];
    pcm1 = pcm[1];
    towrite = lame_encode_buffer(lameFlags, pcm0, pcm1, pcmBlockSampleFrames, mp3buf, mp3buf_size);
    if (towrite > 0)
    {
      int skip = MIN(toskip, towrite);
      int write = towrite - skip;
      write = MIN(write, bytes);
      [data appendBytes:(mp3buf + skip) length:write];
      bytes -= write;
      written += write;
      toskip -= skip;
    }
    
    // 0.4
    pcm0 = pcm[0] + pcmBlockSampleFrames;
    pcm1 = pcm[1] + pcmBlockSampleFrames;
    towrite = lame_encode_buffer(lameFlags, pcm0, pcm1, pcmBlockSampleFrames, mp3buf, mp3buf_size);
    if (towrite > 0)
    {
      int skip = MIN(toskip, towrite);
      int write = towrite - skip;
      write = MIN(write, bytes);
      [data appendBytes:(mp3buf + skip) length:write];
      bytes -= write;
      written += write;
      toskip -= skip;
    }
    
    // 0.6
    pcm0 = pcm[0] + 2 * pcmBlockSampleFrames;
    pcm1 = pcm[1] + 2 * pcmBlockSampleFrames;
    towrite = lame_encode_buffer(lameFlags, pcm0, pcm1, pcmBlockSampleFrames, mp3buf, mp3buf_size);
    if (towrite > 0)
    {
      int skip = MIN(toskip, towrite);
      int write = towrite - skip;
      write = MIN(write, bytes);
      [data appendBytes:(mp3buf + skip) length:write];
      bytes -= write;
      written += write;
      toskip -= skip;
    }
    
    
    // flush
    towrite = lame_encode_flush(lameFlags, mp3buf, mp3buf_size);
    if (towrite > 0)
    {
      int skip = MIN(toskip, towrite);
      int write = towrite - skip;
      write = MIN(write, bytes);
      [data appendBytes:(mp3buf + skip) length:write];
      bytes -= write;
      written += write;
      toskip -= skip;
    }
    
    
    NSMutableData* out_data = [[NSMutableData alloc] init];
    
    
    NSData* src_data = [NSData dataWithContentsOfFile:@"/Users/mat/Desktop/src.mp3"];
    
    unsigned char* src = ((unsigned char*)src_data.bytes) + 627;
    [out_data appendBytes:src_data.bytes length:1254];
    
    
    [out_data appendData:data];
    
    src = ((unsigned char*)src_data.bytes) + 627;
    [out_data appendBytes:src length:627];
    
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, NO);
    NSString* fullPathToDesktop = [paths objectAtIndex:0];
    NSString* outpath = [[fullPathToDesktop stringByAppendingPathComponent:@"mix.mp3"] stringByExpandingTildeInPath];
    
//    [data writeToFile:outpath atomically:YES];    
    [out_data writeToFile:outpath atomically:YES];    
    
    
    int nbFrames = 0;
    Mp3Parser parser((unsigned char*)data.bytes, (int)data.length);
    do
    {
      if (parser.GetCurrentFrame().IsValid())
      {
        nbFrames++;
        NSLog(@"%s", parser.GetCurrentFrame().ToString().c_str());
      }
    }
    while (parser.GoToNextFrame());
    
    
    NSLog(@"%d bytes written to file", written);
    NSLog(@"%d frames", nbFrames);
  }
  return 0;
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

