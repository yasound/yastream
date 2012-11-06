
#pragma once

#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "nglReaderWriterLock.h"
#include "nuiTCPClient.h"
#include "nuiNetworkHost.h"
#include "RedisThread.h"
#include "RadioUser.h"
#include "Cache.h"

class HTTPHandler;

class Track
{
public:
  Track(const nglString& fileid, double delay, double offset, double fade)
  : mFileID(fileid), mDelay(delay), mOffset(offset), mFade(fade)
  {
  }

  nglString mFileID;
  double mDelay;
  double mOffset;
  double mFade;
};

#define YASCHEDULER_WAIT_TIME 1000

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


  void PlayTrack(const nglString& rFilename, double delay, double offet, double fade);

  static void StartRedis();
  static void StopRedis();

  static bool GetUser(const nglString& rToken, RadioUser& rUser);
  static bool GetUser(const nglString& rUsername, const nglString& rApiKey, RadioUser& rUser);

  static void SetRedisDB(const nglString& rHost, int db);

  static void InitCache(int64 MaxSizeBytes, const nglPath& rSource, const nglPath& rDestination);
  static void ReleaseCache();
private:
  static void HandleRedisMessage(const RedisReply& rReply);
  void RegisterClient(HTTPHandler* pClient, bool highQuality);
  bool SetTrack(const Track& rTrack);
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

  std::list<Track> mTracks;

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

  typedef std::pair<nglSyncEvent*, int> EventPair;
  typedef std::map<nglString, EventPair > EventMap;
  static EventMap gEvents;
  static std::map<nglString, RadioUser> gUsers;
  static nglCriticalSection gEventCS;

  static nglSyncEvent* AddEvent(const nglString& rName);
  static void DelEvent(const nglString& rName);
  static void SignallEvent(const nglString& rName);


  void UpdateRadio();
  double mLastUpdateTime;

  static nglString gRedisHost;
  static int gRedisDB;

  static FileCache* gpCache;
};

