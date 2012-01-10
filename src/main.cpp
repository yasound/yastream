
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
    while (mpClient->ReceiveAvailable(data))
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
    return true;
  }
  
  bool OnBodyData(const std::vector<uint8>& rData)
  {
    return true;
  }
  
  void OnBodyEnd()
  {
  }
  
private:
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
    while ((pClient = Accept()))
    {
      OnNewClient(pClient);
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

private:
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
    printf("POUEEEEEEEEET\n");
    return true;
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
  
  if (pServer->Bind(0, 8001))
  {
    pServer->Listen();
    pServer->AcceptConnections();
  }
  else
  {
    NGL_OUT("Unable to bind port 8001\n");
  }

  delete pServer;
  nuiUninit();
  
  return 0;
}

