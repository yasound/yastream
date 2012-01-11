
#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "Radio.h"
#include "HTTPHandler.h"

#define IDEAL_BUFFER_SIZE 8

///////////////////////////////////////////////////
//class Radio
Radio::Radio(const nglString& rURL)
: mURL(rURL), mLive(true), mpParser(NULL), mpStream(NULL), mBufferDuration(0)
{
  RegisterRadio(rURL, this);
  mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStart));
  mpThread->Start();
}

Radio::~Radio()
{
  UnregisterRadio(mURL);
}

void Radio::RegisterClient(HTTPHandler* pClient)
{
  nglCriticalSectionGuard guard(mCS);
  mClients.push_back(pClient);

  printf("Prepare the new client:\n");
  // Fill the buffer:
  for (std::deque<Mp3Chunk*>::const_iterator it = mChunks.begin(); it != mChunks.end(); ++it)
  {
    Mp3Chunk* pChunk = *it;
    pClient->AddChunk(pChunk);
    //printf("Chunk %f\n", pChunk->GetTime());
  }
}

void Radio::UnregisterClient(HTTPHandler* pClient)
{
  nglCriticalSectionGuard guard(mCS);
  mClients.remove(pClient);
}

bool Radio::SetTrack(const nglPath& rPath)
{
  nglIStream* pStream = rPath.OpenRead();
  if (!pStream)
    return false;
  
  Mp3Parser* pParser = new Mp3Parser(*pStream);
  if (!pParser->GetCurrentFrame().IsValid())
  {
    delete pParser;
    delete pStream;
    return false;
  }
  
  delete mpParser;
  delete mpStream;
  mpParser = pParser;
  mpStream = pStream;
  
  return true;
}

void Radio::AddChunk(Mp3Chunk* pChunk)
{
  nglCriticalSectionGuard guard(mCS);
  pChunk->Acquire();
  mChunks.push_back(pChunk);
  mBufferDuration += pChunk->GetDuration();

  //printf("AddChunk %p -> %f\n", pChunk, mBufferDuration);
  
  // Push the new chunk to the current connections:
  for (ClientList::const_iterator it = mClients.begin(); it != mClients.end(); ++it)
  {
    HTTPHandler* pClient = *it;
    pClient->AddChunk(pChunk);
  }

  pChunk = mChunks.front();
  if (mBufferDuration - pChunk->GetDuration() > IDEAL_BUFFER_SIZE)
  {
    // remove old chunks:
    mChunks.pop_front();
    mBufferDuration -= pChunk->GetDuration();
    
    pChunk->Release();
  }
}

void Radio::OnStart()
{
  LoadNextTrack();

  double nexttime = nglTime();
  while (mLive)
  {
    bool cont = true;
    while ((mBufferDuration < IDEAL_BUFFER_SIZE && cont) || nglTime() >= nexttime)
    {
      Mp3Chunk* pChunk = new Mp3Chunk();
      pChunk = mpParser->GetChunk();
      if (pChunk)
      {
        // Store this chunk locally for incomming connections and push it to current clients:
        nexttime += pChunk->GetDuration();
        AddChunk(pChunk);
      }
      
      if (!pChunk || !mpParser->GoToNextFrame())
      {
        LoadNextTrack();
        cont = true;
      }
    }
    
    nglThread::MsSleep(10);
  }
}

void Radio::LoadNextTrack()
{
  nglPath p = mTracks.front();
  mTracks.pop_front();
  while (!SetTrack(p) && mLive)
  {
    p = mTracks.front();
    mTracks.pop_front();
    
    if (mTracks.empty())
      nglThread::MsSleep(10);
  }
}

void Radio::AddTrack(const nglPath& rPath)
{
  mTracks.push_back(rPath);
}


nglCriticalSection Radio::gCS;
std::map<nglString, Radio*> Radio::gRadios;

Radio* Radio::GetRadio(const nglString& rURL)
{
  nglCriticalSectionGuard guard(gCS);
  
  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it == gRadios.end())
  {
    // Create the radio!
    //return CreateRadio(rURL);
    return NULL;
  }
  return it->second;
}

void Radio::RegisterRadio(const nglString& rURL, Radio* pRadio)
{
  nglCriticalSectionGuard guard(gCS);
  
  gRadios[rURL] = pRadio;
}

void Radio::UnregisterRadio(const nglString& rURL)
{
  nglCriticalSectionGuard guard(gCS);
  
  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it == gRadios.end())
  {
    Radio* pRadio = it->second;
    delete pRadio;
  }
}

Radio* Radio::CreateRadio(const nglString& rURL)
{
  Radio* pRadio = new Radio(rURL);
  return pRadio;
}


