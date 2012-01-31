
#pragma once

#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "nglReaderWriterLock.h"

class HTTPHandler;


class Radio
{
public:
  Radio(const nglString& rID);
  virtual ~Radio();

  void Start();

  void RegisterClient(HTTPHandler* pClient, bool highQuality = false);
  void UnregisterClient(HTTPHandler* pClient);
  void OnStart();

  static Radio* GetRadio(const nglString& rURL);

  void AddTrack(const nglPath& rPath);

  bool IsLive() const;

private:
  bool SetTrack(const nglPath& rPath);
  bool LoadNextTrack();
  double ReadSet(int64& chunk_count_preview, int64& chunk_count);

  bool mLive;
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

  void AddChunk(Mp3Chunk* pChunk, bool previewMode);
  nglPath GetPreviewPath(const nglPath& rOriginalPath);

  std::deque<nglPath> mTracks;

  nglThreadDelegate* mpThread;

//  uint64 mTime;

  static nglCriticalSection gCS;
  typedef std::map<nglString, Radio*> RadioMap;
  static RadioMap gRadios;

  static Radio* CreateRadio(const nglString& rURL);
  static void RegisterRadio(const nglString& rURL, Radio* pRadio);
  static void UnregisterRadio(const nglString& rURL);
};

