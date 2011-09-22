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
#include <shout/shout.h>

void OnLameError(const char *format, va_list ap)
{
//  nglString err;
//  err.CFormat(format, ap);
//  NGL_OUT("Lame error: %s\n", err.GetChars()); 
  vprintf("Lame error: %s\n", ap); 
}

void OnLameDebug(const char *format, va_list ap)
{
//  nglString err;
//  err.CFormat(format, ap);
//  NGL_OUT("Lame debug: %s\n", err.GetChars()); 
  vprintf("Lame debug: %s\n", ap); 
}

void OnLameMsg(const char *format, va_list ap)
{
//  nglString err;
//  err.CFormat(format, ap);
//  NGL_OUT("Lame message: %s\n", err.GetChars()); 
  vprintf("Lame message: %s\n", ap); 
}


int main(int argc, char** argv)
{
  nuiInit(NULL);
  
  NGL_OUT("=================================\nnuiAudioDecoder tutorial\n");
  
  if (argc < 2)
  {
    NGL_OUT("usage:\n%s <path to mp3 file>\n", argv[0]);
    return -1;
  }  

  nglPath p(argv[1]);
  nglIStream* pStream = p.OpenRead();
  
  if (!pStream)
  {
    NGL_OUT("Unable to open file '%s'\n", p.GetChars());
    return -2;
  }
 

  // Init shout cast lib:
	shout_init();
  
	int major, minor, patch;
	const char* shoutname = shout_version(&major, &minor, &patch);
  
	NGL_OUT("libshout version %d.%d.%d\n", major, minor, patch);
  
	shout_t* pShout = shout_new();
  
	shout_set_host(pShout, "192.168.1.109");
	shout_set_port(pShout, 8000);
	shout_set_protocol(pShout, SHOUT_PROTOCOL_HTTP);
	shout_set_format(pShout, SHOUT_FORMAT_MP3);
	shout_set_mount(pShout, "test.mp3");
	shout_set_public(pShout, 1);
	shout_set_user(pShout, "source");
	shout_set_password(pShout, "hackme");
	NGL_OUT("about to open conection\n");
  
	shout_open(pShout);
  
	NGL_OUT("connection open\n");
	
  nuiAudioDecoder decoder(*pStream);
  
  nuiSampleInfo info;
  
  nglTime start;
  decoder.GetInfo(info);
  nglTime stop;
  
  NGL_OUT("nuiAudioDecoder::GetInfo() took %f seconds\n", (double)stop - (double)start);
  
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

  NGL_OUT("Sample rate     : %f\n", sr);
  NGL_OUT("Channels        : %d\n", channels);
  NGL_OUT("bits per sample : %d\n", bps);
  NGL_OUT("Sample frames   : %lld\n", sampleframes);
  NGL_OUT("Start frame     : %lld\n", startframe);
  NGL_OUT("Stop frame      : %lld\n", stopframe);
  NGL_OUT("Tempo           : %f\n", tempo);
  NGL_OUT("Tag             : %d\n", tag);
  NGL_OUT("Time sig num    : %d\n", tsn);
  NGL_OUT("Time sig denum  : %d\n", tsd);
  NGL_OUT("Beats           : %f\n", beats);
  
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
  lame_set_quality(lame_flags, 0);
  //lame_set_mode(lame_flags, 0);
  lame_set_errorf(lame_flags, OnLameError);
  lame_set_debugf(lame_flags, OnLameDebug);
  lame_set_msgf(lame_flags, OnLameMsg);
  lame_set_brate(lame_flags, 128); // 128Kbits
  //lame_set_asm_optimizations(lame_flags, 1);
  lame_init_params(lame_flags);
  lame_print_config(lame_flags);
  
  
  NGL_OUT("\n\nDecoding:\n\n");
 
  const uint32 frames = 4096 * 16;
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
      
      
      // Send to icecast2:
      shout_send(pShout, outbuffer, res);
//      check_error;
      shout_sync(pShout);
//      check_error;
      
      //NGL_OUT("in %d samples -> %d bytes\n", r, res);
      //NGL_OUT(".");
      done += r;
    } 
    while (done < sampleframes && r == frames);

    lame_encode_flush(lame_flags, outbuffer, outbuffersize);
    
    double realtime = (double)done / 44100.0;
    
    nglTime stop;
    
    double t = (double)stop - (double)start;
    double ratio = realtime / t;
    NGL_OUT("\nDone in %f seconds (%d samples read, %f seconds) %f X realtime \n", t, done, realtime, ratio);
  }

  // Close shout cast
  shout_close(pShout);
  shout_free(pShout);
  shout_shutdown();

  delete pStream;

  nuiUninit();
  
  return 0;
}
