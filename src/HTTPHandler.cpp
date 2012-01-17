
#include "nui.h"
#include "HTTPHandler.h"
#include "Radio.h"
#include "nuiNetworkHost.h"

///////////////////////////////////////////////////
//class HTTPHandler : public nuiHTTPHandler
HTTPHandler::HTTPHandler(nuiTCPClient* pClient)
: nuiHTTPHandler(pClient), mLive(true)
{
}

HTTPHandler::~HTTPHandler()
{
}

bool HTTPHandler::OnMethod(const nglString& rValue)
{
  return true;
}

bool HTTPHandler::OnURL(const nglString& rValue)
{
  return true;
}

bool HTTPHandler::OnProtocol(const nglString& rValue, const nglString rVersion)
{
  return true;
}

bool HTTPHandler::OnHeader(const nglString& rKey, const nglString& rValue)
{
  return true;
}

bool HTTPHandler::OnBodyStart()
{
  nuiNetworkHost client(0, 0, nuiNetworkHost::eTCP);
  bool res = mpClient->GetDistantHost(client);
  if (!res)
    return false;

  uint32 ip = client.GetIP();
  uint8* pIp = (uint8*)&ip;
  nglString t = nglTime().GetLocalTimeStr("%a, %d %b %Y %H:%M:%S %z");
  NGL_OUT("%d.%d.%d.%d %s \"%s\" %s", pIp[0], pIp[1], pIp[2], pIp[3], mMethod.GetChars(), mURL.GetChars(), t.GetChars());

  if (mURL == "/favicon.ico")
    return false; // We don't have a favicon right now...

  // Find the Radio:
  Radio* pRadio = Radio::GetRadio(mURL);
  if (!pRadio)
  {
    ReplyLine("HTTP/1.1 404 Radio not found");
    ReplyHeader("Cache-Control", "no-cache");
    ReplyHeader("Content-Type", "text/plain");
    ReplyHeader("Server", "Yastream 1.0.0");
    ReplyLine("");
    nglString str;
    str.Format("Unable to find %s on this server", mURL.GetChars());
    ReplyLine(str);

    return false;
  }

  pRadio->RegisterClient(this);

  // Reply + Headers:
  ReplyLine("HTTP/1.1 200 OK");
  ReplyHeader("Cache-Control", "no-cache");
  ReplyHeader("Content-Type", "audio/mpeg");
  ReplyHeader("Server", "Yastream 1.0.0");
  ReplyHeader("icy-name", "no name");
  ReplyHeader("icy-pub", "1");
  ReplyLine("");

  // Do the streaming:

  while (mLive && mpClient->IsConnected())
  {
    // GetNext chunk:
    Mp3Chunk* pChunk = NULL;

    pChunk = GetNextChunk();
    int cnt = 0;
    while (!pChunk)
    {
      cnt++;
      nglThread::MsSleep(10);
      pChunk = GetNextChunk();
    }
//    if (cnt)
//      printf("%d", cnt);
    //printf("^");
    //printf("%d", cnt);
    mpClient->Send(&pChunk->GetData()[0], pChunk->GetData().size());

    pChunk->Release();
  }

  pRadio->UnregisterClient(this);

  return false;
}

bool HTTPHandler::OnBodyData(const std::vector<uint8>& rData)
{
  return true;
}

void HTTPHandler::OnBodyEnd()
{
}

void HTTPHandler::AddChunk(Mp3Chunk* pChunk)
{
  nglCriticalSectionGuard guard(mCS);
  //printf("handle id = %d\n", pChunk->GetId());
  pChunk->Acquire();
  mChunks.push_back(pChunk);
}

Mp3Chunk* HTTPHandler::GetNextChunk()
{
  nglCriticalSectionGuard guard(mCS);

  if (mChunks.empty())
    return NULL;

  Mp3Chunk* pChunk = mChunks.front();
  mChunks.pop_front();

  return pChunk;
}
