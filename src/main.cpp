
#include "nui.h"
#include "nuiInit.h"
#include "nuiHTTPServer.h"
#include "nuiHTTP.h"
#include "HTTPHandler.h"
#include "Radio.h"
#include <signal.h>

#include "RedisClient.h"

nuiHTTPHandler* HandlerDelegate(nuiTCPClient* pClient);
nuiHTTPHandler* HandlerDelegate(nuiTCPClient* pClient)
{
  return new HTTPHandler(pClient);
}

void SigPipeSink(int signal)
{
  // Ignore...
  //printf("SigPipe!\n");
}

int main(int argc, const char** argv)
{
  nuiInit(NULL);
  NGL_OUT("yasound streamer\n");

#if defined _MINUI3_
  App->CatchSignal(SIGPIPE, SigPipeSink);
#endif

  int port = 8001;
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-p") == 0)
    {
      i++;
      if (i >= argc)
      {
        printf("ERROR: -p must be followed by a port number\n");
        exit(1);
      }

      port = atoi(argv[i]);
    }
    else if (strcmp(argv[i], "--test") == 0)
    {
      Radio* pRadio = new Radio("matid");
      nglPath current(ePathCurrent);
      nglPath p = current + nglPath("/data");
      pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));
      pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));
      pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));
      pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));
      
//      pRadio->AddTrack(p + nglPath("/Money Talks.mp3"));
//      pRadio->AddTrack(p + nglPath("/04 The Vagabound.mp3"));
      pRadio->AddTrack(p + nglPath("/Radian.mp3"));
      pRadio->Start();

      printf("test mode ENABLED\n");
    }
  }
  
  RedisClient redis;
  if (!redis.Connect(nuiNetworkHost("127.0.0.1", 6379, nuiNetworkHost::eTCP)))
  {
    printf("Error connecting to redis server.\n");
    exit(0);
  }

  redis.StartCommand("DEL");
  redis.AddArg("prout");
  redis.PrintSendCommand();
  
  //redis.StartCommand("GET");
  redis.StartCommand("LPUSH");
  redis.AddArg("prout");
  redis.AddArg("bleh");
  redis.PrintSendCommand();

  redis.StartCommand("LPUSH");
  redis.AddArg("prout");
  redis.AddArg("woah");
  redis.PrintSendCommand();

  redis.StartCommand("LRANGE");
  redis.AddArg("prout");
  redis.AddArg("0");
  redis.AddArg("-1");
  redis.PrintSendCommand();
  
  redis.StartCommand("OBJECT");
  redis.AddArg("ENCODING");
  redis.AddArg("prout");
  redis.PrintSendCommand();

  //  nuiHTTPRequest request("https://dev.yasound.com/admin/");
//  nuiHTTPResponse* pResponse = request.SendRequest();
//  printf("response: %d - %s\n", pResponse->GetStatusCode(), pResponse->GetStatusLine().GetChars());
//  printf("data:\n %s\n\n", pResponse->GetBodyStr().GetChars());


  nuiHTTPServer* pServer = new nuiHTTPServer();

  //Radio* pRadio = new Radio("fakeid");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/Money Talks.mp3");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/Thunderstruck.mp3");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/ebc_preview64.mp3");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/ebc.mp3");
  //pRadio->AddTrack("/space/new/medias/song/eca/c9f/ebc.mp3");

  pServer->SetHandlerDelegate(HandlerDelegate);


  if (pServer->Bind(0, port))
  {
    pServer->AcceptConnections();
  }
  else
  {
    NGL_OUT("Unable to bind port %d\n", port);
  }

  delete pServer;
  nuiUninit();

  return 0;
}

