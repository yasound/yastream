
#pragma once

#include "nui.h"
#include "nuiHTTPServer.h"

class Radio;
class Mp3Chunk;

///////////////////////////////////////////////////
class HTTPHandler : public nuiHTTPHandler
{
public:
  HTTPHandler(nuiTCPClient* pClient);
  virtual ~HTTPHandler();
  bool OnMethod(const nglString& rValue);
  bool OnURL(const nglString& rValue);
  bool OnProtocol(const nglString& rValue, const nglString rVersion);
  bool OnHeader(const nglString& rKey, const nglString& rValue);
  bool OnBodyStart();
  bool OnBodyData(const std::vector<uint8>& rData);
  void OnBodyEnd();
  void AddChunk(Mp3Chunk* pChunk);
  
private:
  nglCriticalSection mCS;
  std::deque<Mp3Chunk*> mChunks;
  
  Mp3Chunk* GetNextChunk();
  
  enum ListenStatus
  {
    eStartListen = 0,
    eStopListen
  };
  
  void SendListenStatus(ListenStatus status);
  
  bool mLive;
  nglString mUsername;
  nglString mApiKey;
  nglString mRadioID;
};

