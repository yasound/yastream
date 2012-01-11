
#include "nui.h"
#include "nuiInit.h"
#include "nuiHTTPServer.h"
#include "HTTPHandler.h"
#include "Radio.h"

nuiHTTPHandler* HandlerDelegate(nuiTCPClient* pClient);
nuiHTTPHandler* HandlerDelegate(nuiTCPClient* pClient)
{
  return new HTTPHandler(pClient);
}

int main(int argc, const char** argv)
{
  nuiInit(NULL);
  NGL_OUT("yasound streamer\n");
  nuiHTTPServer* pServer = new nuiHTTPServer();

  Radio* pRadio = new Radio("/test.mp3");
  pRadio->AddTrack("/Users/meeloo/work/yastream/data/Money Talks.mp3");
  pRadio->AddTrack("/Users/meeloo/work/yastream/data/Thunderstruck.mp3");
  //pRadio->Start();
  
  pServer->SetHandlerDelegate(HandlerDelegate);
  int port = 8000;
  signal(SIGPIPE, SIG_IGN);
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

