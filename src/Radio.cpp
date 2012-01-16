
#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "Radio.h"
#include "HTTPHandler.h"
#include "nuiHTTP.h"

#define IDEAL_BUFFER_SIZE 8

///////////////////////////////////////////////////
//class Radio
Radio::Radio(const nglString& rID)
: mID(rID), mLive(true), mpParser(NULL), mpStream(NULL), mBufferDuration(0)
{
  nglString URL;
  URL.Add("/" + mID);
  RegisterRadio(URL, this);
  size_t stacksize = 1024 * 512 * 1;
  mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStart), nglThread::Normal, stacksize);
  mpThread->Start();
  size_t size = mpThread->GetStackSize();

  printf("New thread stack size: %ld (requested %ld)\n", size, stacksize);
}

Radio::~Radio()
{
  nglString URL;
  URL.Add("/" + mID);
  UnregisterRadio(URL);
}

void Radio::RegisterClient(HTTPHandler* pClient)
{
  nglCriticalSectionGuard guard(mCS);
  mLive = true;
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

  if (mClients.empty())
  {
    //  Shutdown radio
    mLive = false;
  }
}

bool Radio::SetTrack(const nglPath& rPath)
{
  nglIStream* pStream = rPath.OpenRead();
  if (!pStream)
    return false;

  Mp3Parser* pParser = new Mp3Parser(*pStream);
  bool valid = pParser->GetCurrentFrame().IsValid();
  if (!valid)
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
      Mp3Chunk* pChunk = pChunk = mpParser->GetChunk();
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

bool Radio::LoadNextTrack()
{
  // Try to get the new track from the app server:
  nglString url;
  url.Format("https://dev.yasound.com/api/v1/radio/%s/get_next_song/", mID.GetChars());
  nuiHTTPRequest request(url);
  nuiHTTPResponse* pResponse = request.SendRequest();
  //printf("response: %d - %s\n", pResponse->GetStatusCode(), pResponse->GetStatusLine().GetChars());
  //printf("data:\n %s\n\n", pResponse->GetBodyStr().GetChars());

  if (pResponse->GetStatusCode() == 200)
  {
    nglString p = pResponse->GetBodyStr();
    p.Insert('/', 6);
    p.Insert('/', 3);

    nglPath path = "/space/new/medias/song";
    path += p;

    NGL_OUT("new song from server: %s\n", path.GetChars());
    if (SetTrack(path))
      return true;
  }

  // Otherwise load a track from
  if (!mTracks.empty())
  {
    nglPath p = mTracks.front();
    mTracks.pop_front();
    while (!SetTrack(p) && mLive)
    {
      NGL_OUT("Skipping unreadable '%s'\n", p.GetChars());
      p = mTracks.front();
      mTracks.pop_front();

      if (mTracks.empty())
        return false;
    }
    NGL_OUT("Started '%s' from static track list\n", p.GetChars());
    return true;
  }

  return false;
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


