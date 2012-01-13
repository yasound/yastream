
#include "nui.h"
#include "nuiInit.h"
#include "nuiHTTPServer.h"
#include "nuiHTTP.h"
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

  int port = 8000;
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
  }
  
//  nuiHTTPRequest request("https://dev.yasound.com/admin/");
//  nuiHTTPResponse* pResponse = request.SendRequest();
//  printf("response: %d - %s\n", pResponse->GetStatusCode(), pResponse->GetStatusLine().GetChars());
//  printf("data:\n %s\n\n", pResponse->GetBodyStr().GetChars());

  
  nuiHTTPServer* pServer = new nuiHTTPServer();

  Radio* pRadio = new Radio("fakeid");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/Money Talks.mp3");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/ebc_preview64.mp3");
  pRadio->AddTrack("/Users/meeloo/work/yastream/data/ebc.mp3");
  pRadio->AddTrack("/Users/meeloo/work/yastream/data/Thunderstruck.mp3");
  //pRadio->Start();
  
  pServer->SetHandlerDelegate(HandlerDelegate);
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

