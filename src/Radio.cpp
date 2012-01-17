
#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "Radio.h"
#include "HTTPHandler.h"
#include "nuiHTTP.h"

#define IDEAL_BUFFER_SIZE 12

///////////////////////////////////////////////////
//class Radio
Radio::Radio(const nglString& rID)
: mID(rID), mLive(false), mpParser(NULL), mpStream(NULL), mBufferDuration(0)
{
  RegisterRadio(mID, this);
  size_t stacksize = 1024 * 1024 * 4;

  mLive = LoadNextTrack();

  mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStart), nglThread::Normal, stacksize);
  mpThread->SetAutoDelete(true);
  mpThread->Start();
  size_t size = mpThread->GetStackSize();

  //printf("New thread stack size: %ld (requested %ld)\n", size, stacksize);
}

Radio::~Radio()
{
  UnregisterRadio(mID);
}

void Radio::RegisterClient(HTTPHandler* pClient)
{
  nglCriticalSectionGuard guard(mCS);
  mLive = true;
  mClients.push_back(pClient);

  //printf("Prepare the new client:\n");
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
    //printf("Shutting down radio %s\n", mID.GetChars());
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
  int64 chunk_count = 0;
  double nexttime = nglTime();
  while (mLive)
  {
    while ((mBufferDuration < IDEAL_BUFFER_SIZE && mLive) || nglTime() >= nexttime)
    {
      Mp3Chunk* pChunk = mpParser->GetChunk();

      if (pChunk)
      {
        chunk_count++;
        //if (!(chunk_count % 100))
          //printf("%ld chunks\n", chunk_count);

        // Store this chunk locally for incomming connections and push it to current clients:
        nexttime += pChunk->GetDuration();
        AddChunk(pChunk);
      }

      if (!pChunk || !mpParser->GoToNextFrame())
      {
        mLive = LoadNextTrack();

        if (!mLive)
        {
          printf("Error while getting next song for radio '%s'. Shutting down...\n", mID.GetChars());
        }
      }
    }

    nglThread::MsSleep(10);
  }

  //printf("radio '%s' is now offline\n", mID.GetChars());

  delete this;
}

bool Radio::LoadNextTrack()
{
  // Try to get the new track from the app server:
  nglString url;
  url.Format("https://dev.yasound.com/api/v1/radio/%s/get_next_song/", mID.GetChars());
  nuiHTTPRequest request(url);
  nuiHTTPResponse* pResponse = request.SendRequest();
  //printf("response: %d - %s\n", pResponse->GetStatusCode(), pResponse->GetStatusLine().GetChars());
  //printf("new trackid: %s\n", pResponse->GetBodyStr().GetChars());

  if (pResponse->GetStatusCode() == 200)
  {
    nglString p = pResponse->GetBodyStr();
    p.Insert('/', 6);
    p.Insert('/', 3);

    nglPath path = "/space/new/medias/song";
    path += p;

    //printf("new song from server: %s\n", path.GetChars());
    if (SetTrack(path))
      return true;
  }

  // Otherwise load a track from mTracks
  if (!mTracks.empty())
  {
    nglPath p = mTracks.front();
    mTracks.pop_front();
    while (!SetTrack(p) && mLive)
    {
      //printf("Skipping unreadable '%s'\n", p.GetChars());
      p = mTracks.front();
      mTracks.pop_front();

      if (mTracks.empty())
      {
        //printf("No more track in the list. Bailout...\n");
        return false;
      }
    }
    //printf("Started '%s' from static track list\n", p.GetChars());
    return true;
  }

  //printf("No more track in the list. Bailout...\n");
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
  //printf("Getting radio %s\n", rURL.GetChars());
  nglCriticalSectionGuard guard(gCS);

  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it == gRadios.end())
  {
    // Create the radio!
    //printf("Trying to create the radio '%s'\n", rURL.GetChars());
    return CreateRadio(rURL);
    //return NULL;
  }
  //printf("Getting existing radio %s\n", rURL.GetChars());
  return it->second;
}

void Radio::RegisterRadio(const nglString& rURL, Radio* pRadio)
{
  nglCriticalSectionGuard guard(gCS);
  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it != gRadios.end())
    printf("ERROR: the radio '%s' is already registered\n", rURL.GetChars());

  //printf("Registering radio '%s'\n", rURL.GetChars());
  gRadios[rURL] = pRadio;
}

void Radio::UnregisterRadio(const nglString& rURL)
{
  nglCriticalSectionGuard guard(gCS);
  //printf("Unregistering radio '%s'\n", rURL.GetChars());

  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it == gRadios.end())
  {
    //printf("Error, radio '%s' was never registered\n", rURL.GetChars());
  }
  gRadios.erase(rURL);
}

Radio* Radio::CreateRadio(const nglString& rURL)
{
  Radio* pRadio = new Radio(rURL);
  return pRadio;
}

bool Radio::IsLive() const
{
  nglCriticalSectionGuard guard(gCS);
  return mLive;
}

