
#include "nui.h"
#include "nuiTCPServer.h"
#include "nuiTCPClient.h"

class ServerThread : public nglThread
{
public:
  ServerThread(nuiTCPClient* pClient)
  : mpClient(pClient)
  {
  }
  
  virtual ~ServerThread()
  {
    delete mpClient;
  }
  
  void OnStart()
  {
    std::vector<uint8> data;
    nglChar cur = 0;
    State state = Request;
    while (mpClient->ReceiveAvailable(data))
    {
      int32 index = 0;
      while (index < data.size())
      {
        cur = data[index];
        if (state == Body)
        {
          mBody.insert(mBody.end(), data.begin() + index, data.end());
          index = 0;
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
                  OnMethod(mMethod);
                  
                  while (mCurrentLine[pos] == ' ')
                    pos++;
                  int pos2 = pos;
                  while (mCurrentLine[pos2] != ' ')
                    pos2++;
                  mURL = mCurrentLine.Extract(pos, pos2 - pos);
                  NGL_OUT("URL: %s\n", mURL.GetChars());
                  OnMethod(mURL);

                  pos = pos2;
                  while (mCurrentLine[pos] == ' ')
                    pos++;
                  pos2 = pos;
                  while (mCurrentLine[pos2] != '/')
                    pos2++;
                  
                  mProtocol = mCurrentLine.Extract(pos, pos2 - pos);
                  mVersion = mCurrentLine.Extract(pos2+1);
                  mVersion.Trim();
                  NGL_OUT("Protocol: %s\n", mProtocol.GetChars());
                  NGL_OUT("Version: %s\n", mVersion.GetChars());
                  OnProtocol(mProtocol, mVersion);
                  
                  
                  state = Header;
                  
                  mCurrentLine.Wipe();
                }
                break;

              case Header:
                {
                  if (mCurrentLine.IsEmpty())
                  {
                    NGL_OUT("Start body...\n");
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

                    OnHeader(key, value);
                    
                    state = Header;
                    mCurrentLine.Wipe();
                  }
                }
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
  }
  
  void OnMethod(const nglString& rValue)
  {
  }

  void OnURL(const nglString& rValue)
  {
  }

  void OnProtocol(const nglString& rValue, const nglString rVersion)
  {
  }
  
  void OnHeader(const nglString& rKey, const nglString& rValue)
  {
  }
  
  void OnBodyData(const std::vector<uint8>& rData)
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

int main(int argc, const char** argv)
{
  nuiInit(NULL);
  NGL_OUT("yasound streamer\n");
  nuiTCPServer* pServer = new nuiTCPServer();

  if (pServer->Bind(0, 8001))
  {
    pServer->Listen();
    nuiTCPClient* pClient = NULL;
    while ((pClient = pServer->Accept()))
    {
      NGL_OUT("Received new connection...\n");
      ServerThread* pThread = new ServerThread(pClient);
      pThread->Start();
    }
  }
  else
  {
    NGL_OUT("Unable to bind port 80\n");
  }

  delete pServer;
  nuiUninit();
  
  return 0;
}
