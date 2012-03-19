
#pragma once

#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "nglReaderWriterLock.h"
#include "RedisClient.h"

class HTTPHandler;


class Radio
{
public:
  Radio(const nglString& rID, const nglString& rHost = nglString::Null);
  virtual ~Radio();

  void Start();

  void RegisterClient(HTTPHandler* pClient, bool highQuality = false);
  void UnregisterClient(HTTPHandler* pClient);
  void OnStart();
  void OnStartProxy();

  static Radio* GetRadio(const nglString& rURL);
  static void SetParams(const nglString& hostname, int port, const nglPath& rDataPath, const nglString& rRedisHost, int RedisPort);

  void AddTrack(const nglPath& rPath);

  bool IsLive() const;

  static void AddRadioSource(const nglString& rID, const nglString& rURL);
  static void DelRadioSource(const nglString& rID, const nglString& rURL);
  
  static void FlushRedis();

private:
  bool SetTrack(const nglPath& rPath);
  bool LoadNextTrack();
  double ReadSet(int64& chunk_count_preview, int64& chunk_count);
  double ReadSetProxy(int64& chunk_count_preview, int64& chunk_count);

  Mp3Chunk* GetChunk(nuiTCPClient* pClient);
  
  bool mLive;
  bool mGoOffline;
  nglCriticalSection mCS;
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
  static RedisClient gRedis;
  static nglString mHostname;
  static int mPort;
  static nglPath mDataPath;
  static nglString mRedisHost;
  static int mRedisPort;

  static void InitRedis();


  static Radio* CreateRadio(const nglString& rURL, const nglString& rHost);
  static void RegisterRadio(const nglString& rURL, Radio* pRadio);
  static void UnregisterRadio(const nglString& rURL);
};

