/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/


#include "nui.h"
#include "nuiInit.h"

#include "nglConsole.h"
#include "nuiAudioDecoder.h"

#include <lame/lame.h>

void OnLameError(const char *format, va_list ap)
{
  nglString err;
  err.CFormat(format, ap);
  NGL_OUT("Lame error: %s\n", err.GetChars()); 
}

void OnLameDebug(const char *format, va_list ap)
{
  nglString err;
  err.CFormat(format, ap);
  NGL_OUT("Lame debug: %s\n", err.GetChars()); 
}

void OnLameMsg(const char *format, va_list ap)
{
  nglString err;
  err.CFormat(format, ap);
  NGL_OUT("Lame message: %s\n", err.GetChars()); 
}


int main(int argc, const char** argv)
{
  nuiInit(NULL);
  
  printf("=================================\nnuiAudioDecoder tutorial\n");
  
  if (argc < 2)
  {
    printf("usage:\n%s <path to mp3 file>\n", argv[0]);
    return -1;
  }  

  nglPath p(argv[1]);
  nglIStream* pStream = p.OpenRead();
  
  if (!pStream)
  {
    printf("Unable to open file '%s'\n", p.GetChars());
    return -2;
  }
 
  nuiAudioDecoder decoder(*pStream);
  
  nuiSampleInfo info;
  
  nglTime start;
  decoder.GetInfo(info);
  nglTime stop;
  
  printf("nuiAudioDecoder::GetInfo() took %f seconds\n", (double)stop - (double)start);
  
  //nuiAudioFileFormat GetFileFormat() const;  
  double sr = info.GetSampleRate();
  uint32 channels = info.GetChannels();
  uint32 bps = info.GetBitsPerSample();
  uint64 sampleframes = info.GetSampleFrames();
  uint64 startframe = info.GetStartFrame();
  uint64 stopframe = info.GetStopFrame();
  double tempo = info.GetTempo();
  uint8 tag = info.GetFormatTag();
  uint8 tsn = info.GetTimeSignNom();
  uint8 tsd = info.GetTimeSignDenom();
  double beats = info.GetBeats();

  printf("Sample rate     : %f\n", sr);
  printf("Channels        : %d\n", channels);
  printf("bits per sample : %d\n", bps);
  printf("Sample frames   : %lld\n", sampleframes);
  printf("Start frame     : %lld\n", startframe);
  printf("Stop frame      : %lld\n", stopframe);
  printf("Tempo           : %f\n", tempo);
  printf("Tag             : %d\n", tag);
  printf("Time sig num    : %d\n", tsn);
  printf("Time sig denum  : %d\n", tsd);
  printf("Beats           : %f\n", beats);
  
  // Init Lame:
  NGL_OUT("Lame init\n");
  //lame_get_version();
  NGL_OUT("version %s\n", get_lame_version());
  lame_global_flags* lame_flags = lame_init();
  lame_set_in_samplerate(lame_flags, 44100);
  lame_set_num_channels(lame_flags, 2);
  lame_set_scale(lame_flags, 1.0f);
  lame_set_out_samplerate(lame_flags, 44100);
  lame_set_bWriteVbrTag(lame_flags, 0);
  lame_set_quality(lame_flags, 7);
  //lame_set_mode(lame_flags, 0);
  lame_set_errorf(lame_flags, OnLameError);
  lame_set_debugf(lame_flags, OnLameDebug);
  lame_set_msgf(lame_flags, OnLameMsg);
  lame_set_brate(lame_flags, 128); // 128Kbits
  //lame_set_asm_optimizations(lame_flags, 1);
  lame_init_params(lame_flags);
  lame_print_config(lame_flags);
  
  
  printf("\n\nDecoding:\n\n");
 
  const uint32 frames = 4096 * 8;
  float bufferleft[frames * sizeof(float) * channels];
  float bufferright[frames * sizeof(float) * channels];
  std::vector<void*> buffers;
  buffers.push_back(bufferleft);
  buffers.push_back(bufferright);
  const uint32 outbuffersize = frames;
  uint8 outbuffer[outbuffersize];
  uint32 done = 0;
  uint32 r = 0;
  {
    nglTime start;
    do
    {
      r = decoder.ReadDE(buffers, frames, eSampleFloat32);
      
      
      // Lame needs samples in [-32768, 32768] instead of [-1, 1]
      for (uint32 i = 0; i < r; i++)
      {
        bufferleft[i] *= 32768;
        bufferright[i] *= 32768;
      }

      // Encode with lame:
      int res = lame_encode_buffer_float(lame_flags, bufferleft, bufferright, r, outbuffer, outbuffersize);
      
      //printf("in %d samples -> %d bytes\n", r, res);
      //printf(".");
      done += r;
    } 
    while (done < sampleframes && r == frames);

    lame_encode_flush(lame_flags, outbuffer, outbuffersize);
    
    double realtime = (double)done / 44100.0;
    
    nglTime stop;
    
    double t = (double)stop - (double)start;
    double ratio = realtime / t;
    printf("\nDone in %f seconds (%d samples read, %f seconds) %f X realtime \n", t, done, realtime, ratio);
  }


  delete pStream;

  nuiUninit();
  
  return 0;
}
