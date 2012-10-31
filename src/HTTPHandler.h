
#pragma once

#include "nui.h"
#include "nuiHTTPServer.h"
#include "nuiStringTemplate.h"

class Radio;
class Mp3Chunk;

///////////////////////////////////////////////////
class HTTPHandler : public nuiHTTPHandler
{
public:
  HTTPHandler(nuiSocket::SocketType s);
  virtual ~HTTPHandler();
  bool OnMethod(const nglString& rValue);
  bool OnURL(const nglString& rValue);
  bool OnProtocol(const nglString& rValue, const nglString rVersion);
  bool OnHeader(const nglString& rKey, const nglString& rValue);
  bool OnBodyStart();
  bool OnBodyData(const std::vector<uint8>& rData);
  void OnBodyEnd();
  void AddChunk(Mp3Chunk* pChunk);
  void OnWriteClosed();

  void GoOffline();

  bool IsLive() const;

  enum ListenStatus
  {
    eStartListen = 0,
    eStopListen
  };
  void SendListenStatus(ListenStatus status);

  static void SetPool(nuiSocketPool* pPool);
private:
  nglCriticalSection mCS;
  std::deque<Mp3Chunk*> mChunks;
  static nuiSocketPool* gmpPool;

  Mp3Chunk* GetNextChunk();



  bool SendFromTemplate(const nglString& rString, nuiObject* pObject);

  bool mOnline;
  bool mLive;
  nglString mUsername;
  nglString mApiKey;
  nglString mRadioID;
  nglString mUserID;
  nglTime mStartTime;

  nuiStringTemplate* mpTemplate;

  Radio* mpRadio;
};

