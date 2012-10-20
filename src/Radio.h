
#pragma once

#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "nglReaderWriterLock.h"
#include "nuiTCPClient.h"
#include "nuiNetworkHost.h"
#include "RedisThread.h"

class HTTPHandler;


class Radio
{
public:
  Radio(const nglString& rID, const nglString& rHost = nglString::Null);
  virtual ~Radio();

  void Start();

  void UnregisterClient(HTTPHandler* pClient);
  void OnStart();
  void OnStartProxy();

  static Radio* GetRadio(const nglString& rURL, HTTPHandler* pClient, bool HQ);
  static void SetParams(const nglString& appurl, const nglString& hostname, int port, const nglPath& rDataPath);

  void AddTrack(const nglPath& rPath);

  bool IsLive() const;
  bool IsOnline() const;

  static void AddRadioSource(const nglString& rID, const nglString& rURL);
  static void DelRadioSource(const nglString& rID, const nglString& rURL);

  static const nglString& GetHostName()
  {
    return mHostname;
  }

  static const nglString& GetAppUrl()
  {
    return mAppUrl;
  }

  void SetNetworkSource(nuiTCPClient* pHQSource, nuiTCPClient* pLQSource);

  static void HandleRedisMessage(const RedisReply& rReply);
private:
  void RegisterClient(HTTPHandler* pClient, bool highQuality);
  bool SetTrack(const nglPath& rPath);
  bool LoadNextTrack();
  double ReadSet(int64& chunk_count_preview, int64& chunk_count);
  double ReadSetProxy(int64& chunk_count_preview, int64& chunk_count);
  void KillClients();

  Mp3Chunk* GetChunk(nuiTCPClient* pClient);

  bool mLive;
  bool mOnline;
  bool mGoOffline;
  nglCriticalSection mCS;
  nglCriticalSection mClientListCS;
  nglString mID;

  typedef std::list<HTTPHandler*> ClientList;

  nglIStream* mpStream;
  Mp3Parser* mpParser;
  std::deque<Mp3Chunk*> mChunks;
  double mBufferDuration;
  ClientList mClients;

  nglIStream* mpStreamPreview;
  Mp3Parser* mpParserPreview;
  std::deque<Mp3Chunk*> mChunksPreview;
  double mBufferDurationPreview;
  ClientList mClientsPreview;

  nuiTCPClient* mpPreviewSource;
  nuiTCPClient* mpSource;

  void AddChunk(Mp3Chunk* pChunk, bool previewMode);
  nglPath GetPreviewPath(const nglPath& rOriginalPath);

  std::deque<nglPath> mTracks;

  nglThreadDelegate* mpThread;

//  uint64 mTime;

  static nglCriticalSection gCS;
  typedef std::map<nglString, Radio*> RadioMap;
  static RadioMap gRadios;
  static nglString mHostname;
  static nglString mAppUrl;
  static int mPort;
  static nglPath mDataPath;


  static Radio* CreateRadio(const nglString& rURL, const nglString& rHost);
  static void RegisterRadio(const nglString& rURL, Radio* pRadio);
  static void UnregisterRadio(const nglString& rURL);


  static RedisThread* mpRedisThreadIn;
  static RedisThread* mpRedisThreadOut;

  static void StartRedis();
  static void StopRedis();

};

