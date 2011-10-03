/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/


#include "nui.h"
#include "nuiInit.h"

#include "nglConsole.h"
#include "Player.h"


#define NEXT_ARG(arg) { if (argc == c) { printf("syntax error: expecting parameter after %s\n", argv[c-1]); return -1; } arg = argv[c++]; }

int main(int argc, const char** argv)
{
  nuiInit(NULL);
  
  App->GetLog().SetLevel("Player", 10000);

  nglString StreamName = "Stream.mp3";
  nglPath PlayListPath(ePathApp);
  PlayListPath+= "test.lst";
  
  nglString ServerIP = "94.100.167.5";
  int ServerPort = 8000;
  nglString ServerPassword = "hackme";
  int BitRate = 192;

  // Parse arguments:
  int c = 1;
  while (c < argc)
  {
    nglString arg = argv[c++];
    
    if (arg == "--ip")
    {
      NEXT_ARG(arg);
      ServerIP = arg;
    }
    else if (arg == "--port")
    {
      NEXT_ARG(arg);
      ServerPort = arg.GetInt();
    }
    else if (arg == "--password")
    {
      NEXT_ARG(arg);
      ServerPassword = arg;
    }
    else if (arg == "--playlist")
    {
      NEXT_ARG(arg);
      PlayListPath = arg;
    }
    else if (arg == "--name")
    {
      NEXT_ARG(arg);
      StreamName = arg;
    }
    else if (arg == "--bitrate")
    {
      NEXT_ARG(arg);
      BitRate = arg.GetInt();
    }
  }
  
  
  Player player(StreamName, PlayListPath, ServerIP, ServerPort, ServerPassword, BitRate);
  player.OnStart();

  nuiUninit();
  
  return 0;
}
