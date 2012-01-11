
#include "nui.h"
#include "nuiTCPServer.h"
#include "nuiTCPClient.h"

class nuiHTTPHandler
{
public:
  nuiHTTPHandler(nuiTCPClient* pClient)
  : mpClient(pClient)
  {
  }
  
  virtual ~nuiHTTPHandler()
  {
    delete mpClient;
  }
  
  void Parse()
  {
    std::vector<uint8> data;
    nglChar cur = 0;
    State state = Request;
    data.resize(1024);
    while (mpClient->Receive(data))
    {
      size_t index = 0;
      while (index < data.size())
      {
        cur = data[index];
        if (state == Body)
        {
          std::vector<uint8> d(data.begin() + index, data.end());
          NGL_OUT("...Body data... (%d)\n", d.size());
          OnBodyData(d);
          index = data.size();
        }
        else
        {
          index++;
          
          if (cur == 10)
          {
            // skip...
          }
          else if (cur == 13)
          {
            // found a line:
            switch (state)
            {
              case Request:
              {
                mCurrentLine.Trim();
                int pos = mCurrentLine.Find(' ');
                if (pos < 0)
                {
                  // Error!
                  return;
                }
                
                mMethod = mCurrentLine.GetLeft(pos);
                NGL_OUT("Method: %s\n", mMethod.GetChars());
                if (!OnMethod(mMethod))
                  return;
                
                while (mCurrentLine[pos] == ' ')
                  pos++;
                int pos2 = pos;
                while (mCurrentLine[pos2] != ' ')
                  pos2++;
                mURL = mCurrentLine.Extract(pos, pos2 - pos);
                NGL_OUT("URL: %s\n", mURL.GetChars());
                if (!OnMethod(mURL))
                  return;
                
                pos = pos2;
                while (mCurrentLine[pos] == ' ')
                  pos++;
                pos2 = pos;
                while (mCurrentLine[pos2] != '/')
                  pos2++;
                
                mProtocol = mCurrentLine.Extract(pos, pos2 - pos);
                mVersion = mCurrentLine.Extract(pos2 + 1);
                mVersion.Trim();
                NGL_OUT("Protocol: %s\n", mProtocol.GetChars());
                NGL_OUT("Version: %s\n", mVersion.GetChars());
                if (!OnProtocol(mProtocol, mVersion))
                  return;
                
                state = Header;
                
                mCurrentLine.Wipe();
              }
                break;
                
              case Header:
              {
                if (mCurrentLine.IsEmpty())
                {
                  NGL_OUT("Start body...\n");
                  if (!OnBodyStart())
                    return;
                  state = Body;
                }
                else
                {
                  mCurrentLine.Trim();
                  int pos = mCurrentLine.Find(':');
                  if (pos < 0)
                  {
                    // Error!
                    return;
                  }
                  
                  nglString key = mCurrentLine.GetLeft(pos);
                  nglString value = mCurrentLine.Extract(pos + 1);
                  
                  key.Trim();
                  value.Trim();
                  
                  mHeaders[key] = value;
                  
                  NGL_OUT("[%s]: '%s'\n", key.GetChars(), value.GetChars());
                  
                  if (!OnHeader(key, value))
                    return;
                  
                  state = Header;
                  mCurrentLine.Wipe();
                }
              }
                break;
              default:
                NGL_ASSERT(0);
                break;
            }
          }
          else
          {
            mCurrentLine.Append(cur);
          }
        }
        
      }
    }
    NGL_OUT("End body\n");
    OnBodyEnd();
  }
  
  virtual bool OnMethod(const nglString& rValue)
  {
    return true;
  }
  
  virtual bool OnURL(const nglString& rValue)
  {
    return true;
  }
  
  virtual bool OnProtocol(const nglString& rValue, const nglString rVersion)
  {
    return true;
  }
  
  virtual bool OnHeader(const nglString& rKey, const nglString& rValue)
  {
    return true;
  }
  
  virtual bool OnBodyStart()
  {
    return true;
  }
  
  virtual bool OnBodyData(const std::vector<uint8>& rData)
  {
    return true;
  }
  
  virtual void OnBodyEnd()
  {
  }
  
  bool ReplyLine(const nglString& rString)
  {
    bool res = mpClient->Send(rString);
    res &= mpClient->Send("\r\n");
    return res;
  }

  bool ReplyHeader(const nglString& rKey, const nglString& rValue)
  {
    nglString str;
    str.Add(rKey).Add(": ").Add(rValue);
    return ReplyLine(str);
  }

protected:
  enum State
  {
    Request,
    Header,
    Body
  };
  nuiTCPClient* mpClient;
  nglString mCurrentLine;
  nglString mURL;
  nglString mMethod;
  nglString mProtocol;
  nglString mVersion;
  std::map<nglString, nglString> mHeaders;
  std::vector<uint8> mBody;
};

class nuiHTTPServerThread : public nglThread
{
public:
  nuiHTTPServerThread(nuiHTTPHandler* pHandler)
  : mpHandler(pHandler)
  {
    SetAutoDelete(true);
  }
  
  virtual ~nuiHTTPServerThread()
  {
    delete mpHandler;
  }
  
  void OnStart()
  {
    mpHandler->Parse();
  }
  
private:
  nuiHTTPHandler* mpHandler;
};



class nuiHTTPServer : public nuiTCPServer
{
public:
  nuiHTTPServer()
  {
    SetHandlerDelegate(nuiMakeDelegate(this, &nuiHTTPServer::DefaultHandler));
  }
  virtual ~nuiHTTPServer()
  {
  }
  
  void AcceptConnections()
  {
    nuiTCPClient* pClient = NULL;
    Listen();
    while ((pClient = Accept()))
    {
      OnNewClient(pClient);
      //Listen();
    }
    
  }
  
  void OnNewClient(nuiTCPClient* pClient)
  {
    NGL_OUT("Received new connection...\n");
    nuiHTTPServerThread* pThread = new nuiHTTPServerThread(mDelegate(pClient));
    pThread->Start();
  }
  
  void SetHandlerDelegate(const nuiFastDelegate1<nuiTCPClient*, nuiHTTPHandler*>& rDelegate)
  {
    mDelegate = rDelegate;
  }

protected:
  nuiFastDelegate1<nuiTCPClient*, nuiHTTPHandler*> mDelegate;
  
  nuiHTTPHandler* DefaultHandler(nuiTCPClient* pClient)
  {
    return new nuiHTTPHandler(pClient);
  }
};

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
  struct sigaction act;
  
  // ignore SIGPIPE
  act.sa_handler=SIG_IGN;
  sigemptyset(&act.sa_mask);
  act.sa_flags=0;
  sigaction(SIGPIPE, &act, NULL); 
  
  // set my handler for SIGTERM
//  act.sa_handler=catcher;
//  sigemptyset(&act.sa_mask);
//  act.sa_flags=0;
//  sigaction(SIGTERM, &act, NULL);

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

