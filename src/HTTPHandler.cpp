
#include "nui.h"
#include "HTTPHandler.h"
#include "Radio.h"

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
  
  while (mLive)
  {
    // GetNext chunk:
    Mp3Chunk* pChunk = NULL;
    
    pChunk = GetNextChunk();
    while (!pChunk)
    {
      nglThread::MsSleep(10);
      pChunk = GetNextChunk();
    }
    printf("^");
    mpClient->Send(&pChunk->GetData()[0], pChunk->GetData().size());
    
    pChunk->Release();
  }
  
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
