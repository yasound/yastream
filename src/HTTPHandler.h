
#pragma once

#include "nui.h"
#include "nuiHTTPServer.h"
#include "nuiStringTemplate.h"
#include "RadioUser.h"

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

  const RadioUser& GetUser() const;

  static void SetPool(nuiSocketPool* pPool);
  
  static void DumpTimeProfile(nglString& rDump);
private:
  nglCriticalSection mCS;
  std::deque<Mp3Chunk*> mChunks;
  static nuiSocketPool* gmpPool;

  Mp3Chunk* GetNextChunk();

  RadioUser mUser;


  bool SendFromTemplate(const nglString& rString, nuiObject* pObject);

  bool mOnline;
  bool mLive;
  nglString mUsername;
  nglString mApiKey;
  nglString mRadioID;
  nglTime mStartTime;

  nuiStringTemplate* mpTemplate;

  Radio* mpRadio;
  
  static std::map<nglString, double> gAddChunkTimeProfile;
};

