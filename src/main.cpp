
#include "nui.h"
#include "nuiInit.h"
#include "nuiTCPServer.h"
#include "nuiTCPClient.h"
#include "nuiHTTPServer.h"


#include "Mp3Parser/Mp3Parser.h"


///////////////////////////////////////////////////
class HTTPHandler : public nuiHTTPHandler
{
public:
  HTTPHandler(nuiTCPClient* pClient)
  : nuiHTTPHandler(pClient)
  {
  }
  
  virtual ~HTTPHandler()
  {
  }
  
  bool OnMethod(const nglString& rValue)
  {
    return true;
  }
  
  bool OnURL(const nglString& rValue)
  {
    return true;
  }
  
  bool OnProtocol(const nglString& rValue, const nglString rVersion)
  {
    return true;
  }
  
  bool OnHeader(const nglString& rKey, const nglString& rValue)
  {
    return true;
  }
  
  bool OnBodyStart()
  {
    // 

    if (1)
    {
      
      nglIStream* pStream = nglPath("/Users/meeloo/work/yastream/data/Money Talks.mp3").OpenRead();
      if (!pStream)
      {
        ReplyLine("HTTP/1.1 404 Bleh");
        ReplyHeader("Cache-Control", "no-cache");
        ReplyHeader("Content-Type", "text/plain");
        ReplyHeader("Server", "Yastream 1.0.0");
        ReplyLine("");
        ReplyLine("Unable to read file");
        return false;
      }

      ReplyLine("HTTP/1.1 200 OK");
      ReplyHeader("Cache-Control", "no-cache");
      ReplyHeader("Content-Type", "audio/mpeg");
      ReplyHeader("Server", "Yastream 1.0.0");
      ReplyHeader("icy-name", "no name");
      ReplyHeader("icy-pub", "1");
      ReplyLine("");
      
      int total = 0;
      while (pStream->Available() && mpClient->IsConnected())
      {
        uint8 buf[1024];
        int done = pStream->Read(buf, 1024, 1);
        total += done;
        printf("send %d\n", total);
        mpClient->Send(buf, done);
        
        if (total > 100 * 1024)
          nglThread::MsSleep(70);
      }
    }
    else
    {
      ReplyHeader("Cache-Control", "no-cache");
      ReplyHeader("Content-Type", "text/plain");
      ReplyHeader("Server", "Yastream 1.0.0");
      ReplyLine("");
      ReplyLine("Hello world!");
    }
    return false;
  }
  
  bool OnBodyData(const std::vector<uint8>& rData)
  {
    return true;
  }
  
  void OnBodyEnd()
  {
  }
  
private:
};

nuiHTTPHandler* HandlerDelegate(nuiTCPClient* pClient)
{
  return new HTTPHandler(pClient);
}

int main(int argc, const char** argv)
{
  nuiInit(NULL);
  NGL_OUT("yasound streamer\n");
  nuiHTTPServer* pServer = new nuiHTTPServer();

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

