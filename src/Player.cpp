/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/


#include "nui.h"
#include "Player.h"

#include <lame/lame.h>
#include <shout/shout.h>

void OnLameError(const char *format, va_list ap);
void OnLameDebug(const char *format, va_list ap);
void OnLameMsg(const char *format, va_list ap);

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

#define check_error if (shout_get_errno(mpShout) != SHOUTERR_SUCCESS) { NGL_LOG("Player", NGL_LOG_ERROR, "libshout error: %d '%s'", shout_get_errno(mpShout), shout_get_error(mpShout)); }

Player::Player(const nglString& rStreamName, const nglPath& rPlayListPath, const nglString& rServerIP, int ServerPort, const nglString& rServerPassword)
{
  mShutDown = false;
  mpCurrentVoice = NULL;
  mpCurrentSong = NULL;
  
  // Init shout cast lib:
	shout_init();
  
	int major, minor, patch;
	const char* shoutname = shout_version(&major, &minor, &patch);
  
	NGL_LOG("Player", NGL_LOG_INFO, "libshout version %d.%d.%d\n", major, minor, patch);
  
  mpShout = shout_new();
  
	shout_set_host(mpShout, rServerIP.GetChars()); //"192.168.1.109");
	shout_set_port(mpShout, ServerPort); //8000);
	shout_set_protocol(mpShout, SHOUT_PROTOCOL_HTTP);
	shout_set_format(mpShout, SHOUT_FORMAT_MP3);
	shout_set_mount(mpShout, rStreamName.GetChars());
	shout_set_public(mpShout, 1);
	shout_set_user(mpShout, "source");
	shout_set_password(mpShout, "hackme");
	NGL_LOG("Player", NGL_LOG_INFO, "about to open conection\n");
  
  shout_open(mpShout);
  check_error;
  
  NGL_LOG("Player", NGL_LOG_INFO, "connection open\n");

  // Init Lame:
  NGL_LOG("Player", NGL_LOG_INFO, "Lame init\n");
  //lame_get_version();
  NGL_LOG("Player", NGL_LOG_INFO, "version %s\n", get_lame_version());
  mLameFlags = lame_init();
  lame_set_in_samplerate(mLameFlags, 44100);
  lame_set_num_channels(mLameFlags, 2);
  lame_set_scale(mLameFlags, 1.0f);
  lame_set_out_samplerate(mLameFlags, 44100);
  lame_set_bWriteVbrTag(mLameFlags, 0);
  lame_set_quality(mLameFlags, 5);
  //lame_set_mode(mLameFlags, 0);
  lame_set_errorf(mLameFlags, OnLameError);
  lame_set_debugf(mLameFlags, OnLameDebug);
  lame_set_msgf(mLameFlags, OnLameMsg);
  lame_set_brate(mLameFlags, 128); // 128Kbits
  //lame_set_asm_optimizations(mLameFlags, 1);
  lame_init_params(mLameFlags);
  lame_print_config(mLameFlags);
  
  // Load playlist:
  if (rPlayListPath.Exists())
  {
    if (!rPlayListPath.IsLeaf())
    {
      // This is a folder: load all mp3 in the play list:
      std::list<nglPath> children;
      rPlayListPath.GetChildrenTree(children);
      
      for (std::list<nglPath>::iterator it = children.begin(); it != children.end(); ++it)
      {
        nglPath p(*it);
        Song song(p);
        
        NGL_LOG("Player", NGL_LOG_INFO, "Added '%s' to playlist\n", p.GetChars());
        mPlayList.AddSong(song);
      }
    }
  }
  else
  {
    NGL_LOG("Player", NGL_LOG_ERROR, "Playlist path doesn't exist: %s\n", rPlayListPath.GetChars());
  }
  NGL_LOG("Player", NGL_LOG_INFO, "Player init done\n");
}

Player::~Player()
{
  if (mpCurrentSong)
    mpCurrentSong->Release();
  
  // Close shout cast
  shout_close(mpShout);
  shout_free(mpShout);
  shout_shutdown();
  
  lame_close(mLameFlags);

}

void Player::OnStart()
{
  NGL_LOG("Player", NGL_LOG_INFO, "\n\nStreaming:\n\n");
  
  nglOFile stream("/Users/meeloo/out.mp3", eOFileCreate);
  //nglOFile bin("/Users/meeloo/out.bin", eOFileCreate);
  
  const uint32 frames = 4096 * 16;
  const uint32 channels = 2;
  float bufferleft[frames * sizeof(float) * channels];
  float bufferright[frames * sizeof(float) * channels];
  std::vector<float*> buffers;
  buffers.push_back(bufferleft);
  buffers.push_back(bufferright);
  const uint32 outbuffersize = frames;
  uint8 outbuffer[outbuffersize];
  uint32 done = 0;
  uint32 r = 0;
  int32 res = 0;
  {
    nglTime start;
    while (!mShutDown)
    {
      // Clear the buffers:
      memset(bufferleft, 0, sizeof(float) * frames);
      memset(bufferright, 0, sizeof(float) * frames);
      
      // Process the current voice:
      if (mpCurrentVoice)
      {        
        mpCurrentVoice->Process(buffers, frames);
        //bin.Write(bufferleft, frames, sizeof(float));
        
        if (mpCurrentVoice->IsDone())
        {
          mpCurrentVoice->Release();
          mpCurrentSong->Release();
          mpCurrentVoice = NULL;
          mpCurrentSong = NULL;
        }
      }

      if (!mpCurrentVoice)
      {
        Song& rSong(mPlayList.GetNextSong());
        nglPath path = rSong.mPath;
        mpCurrentSong = dynamic_cast<nuiFileSound*>(nuiSoundManager::Instance.GetSound(path));
        mpCurrentSong->Acquire();
        mpCurrentVoice = dynamic_cast<nuiFileVoice*>(mpCurrentSong->GetVoice());
        NGL_ASSERT(mpCurrentVoice && mpCurrentVoice->IsValid());
        mpCurrentVoice->Acquire();
        NGL_LOG("Player", NGL_LOG_INFO, "New song: %s\n", path.GetChars());
      }

      // Lame needs samples in [-32768, 32768] instead of [-1, 1]
      for (uint32 i = 0; i < r; i++)
      {
        bufferleft[i] *= 32768;
        bufferright[i] *= 32768;
      }
      
      // Encode with lame:
      res = lame_encode_buffer_float(mLameFlags, bufferleft, bufferright, frames, outbuffer, outbuffersize);
      if (res > 0)
      {
        stream.Write(outbuffer, outbuffersize, 1);
        
        // Send to icecast2:
        shout_send(mpShout, outbuffer, res);
        check_error;
        shout_sync(mpShout);
        check_error;
      }
      else
      {
        NGL_LOG("Player", NGL_LOG_ERROR, "lame_encore_buffer_float returned %d", res);
      }
      
      
      //NGL_LOG("Player", NGL_LOG_INFO, "in %d samples -> %d bytes\n", r, res);
      //NGL_LOG("Player", NGL_LOG_INFO, ".");
      done += r;
    } 
    
    lame_encode_flush(mLameFlags, outbuffer, outbuffersize);
    // Send to icecast2:
    shout_send(mpShout, outbuffer, res);
    check_error;
    shout_sync(mpShout);
    check_error;
  }
}


