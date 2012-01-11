
#pragma once

#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "nglReaderWriterLock.h"

class HTTPHandler;


class Radio
{
public:
  Radio(const nglString& rURL);
  virtual ~Radio();
  
  void RegisterClient(HTTPHandler* pClient);
  void UnregisterClient(HTTPHandler* pClient);
  void OnStart();
  
  static Radio* GetRadio(const nglString& rURL);
  
  void AddTrack(const nglPath& rPath);
private:
  bool SetTrack(const nglPath& rPath);
  void LoadNextTrack();

  bool mLive;
  nglCriticalSection mCS;
  nglString mURL;
  typedef std::list<HTTPHandler*> ClientList;
  ClientList mClients;
  nglIStream* mpStream;
  Mp3Parser* mpParser;
  std::deque<Mp3Chunk*> mChunks;
  void AddChunk(Mp3Chunk* pChunk);
  
  std::deque<nglPath> mTracks;
  
  nglThreadDelegate* mpThread;
  
  uint64 mTime;
  double mBufferDuration;
  
  static nglCriticalSection gCS;
  typedef std::map<nglString, Radio*> RadioMap;
  static RadioMap gRadios;

  static Radio* CreateRadio(const nglString& rURL);
  static void RegisterRadio(const nglString& rURL, Radio* pRadio);
  static void UnregisterRadio(const nglString& rURL);
};

