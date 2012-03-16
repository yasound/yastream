
#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "Radio.h"
#include "HTTPHandler.h"
#include "nuiHTTP.h"

#define IDEAL_BUFFER_SIZE 3.0
#define MAX_BUFFER_SIZE 4.0

///////////////////////////////////////////////////
//class Radio
Radio::Radio(const nglString& rID)
: mID(rID),
  mLive(false),
  mpParser(NULL),
  mpStream(NULL),
  mBufferDuration(0),
  mpParserPreview(NULL),
  mpStreamPreview(NULL),
  mBufferDurationPreview(0)
{
  RegisterRadio(mID, this);
  mLive = true;
}

Radio::~Radio()
{
  UnregisterRadio(mID);
}

void Radio::Start()
{
  size_t stacksize = 1024 * 1024 * 4;

  mLive = LoadNextTrack();

  mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStart), nglThread::Normal, stacksize);
  mpThread->SetAutoDelete(true);
  mpThread->Start();
  size_t size = mpThread->GetStackSize();

  //printf("New thread stack size: %ld (requested %ld)\n", size, stacksize);
}

void Radio::RegisterClient(HTTPHandler* pClient, bool highQuality)
{
  nglCriticalSectionGuard guard(mCS);

  ClientList& rClients            = highQuality ? mClients : mClientsPreview;
  std::deque<Mp3Chunk*>& rChunks  = highQuality ? mChunks : mChunksPreview;

  mLive = true;
  rClients.push_back(pClient);

  //printf("Prepare the new client:\n");
  // Fill the buffer:
  for (std::deque<Mp3Chunk*>::const_iterator it = rChunks.begin(); it != rChunks.end(); ++it)
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
  mClientsPreview.remove(pClient);

  if (mClients.empty() && mClientsPreview.empty())
  {
    //  Shutdown radio
    //printf("Shutting down radio %s\n", mID.GetChars());
    //mLive = false;
  }
}

bool Radio::SetTrack(const nglPath& rPath)
{
  nglIStream* pStream = rPath.OpenRead();
  if (!pStream)
  {
    return false;
  }

  nglPath previewPath = GetPreviewPath(rPath);
  nglIStream* pStreamPreview = previewPath.OpenRead();
  if (!pStreamPreview)
  {
    delete pStream;
    return false;
  }

  Mp3Parser* pParser = new Mp3Parser(*pStream, false, false);
  bool valid = pParser->GetCurrentFrame().IsValid();
  if (!valid)
  {
    delete pParser;
    delete pStream;
    delete pStreamPreview;
    return false;
  }

  Mp3Parser* pParserPreview = new Mp3Parser(*pStreamPreview, false, false);
  valid = pParserPreview->GetCurrentFrame().IsValid();
  if (!valid)
  {
    delete pParserPreview;
    delete pStreamPreview;
    delete pParser;
    delete pStream;
    return false;
  }

  delete mpParser;
  delete mpStream;
  delete mpParserPreview;
  delete mpStreamPreview;
  mpParser = pParser;
  mpStream = pStream;
  mpParserPreview = pParserPreview;
  mpStreamPreview = pStreamPreview;

  return true;
}

nglPath Radio::GetPreviewPath(const nglPath& rOriginalPath)
{
  nglString ext = rOriginalPath.GetExtension();
  nglString base = rOriginalPath.GetRemovedExtension();
  base += "_preview64";
  base += ".";
  base += ext;
  nglPath previewPath(base);
  //printf("preview path %s\n", previewPath.GetPathName().GetChars());
  return previewPath;
}

void Radio::AddChunk(Mp3Chunk* pChunk, bool previewMode)
{
  nglCriticalSectionGuard guard(mCS);
  pChunk->Acquire();

  ClientList& rClients            = previewMode ? mClientsPreview : mClients;
  std::deque<Mp3Chunk*>& rChunks  = previewMode ? mChunksPreview : mChunks;
  double& rBufferDuration         = previewMode ? mBufferDurationPreview : mBufferDuration;

  rChunks.push_back(pChunk);
  rBufferDuration += pChunk->GetDuration();

  //printf("AddChunk %p -> %f\n", pChunk, rBufferDuration);

  // Push the new chunk to the current connections:
  for (ClientList::const_iterator it = rClients.begin(); it != rClients.end(); ++it)
  {
    HTTPHandler* pClient = *it;
    pClient->AddChunk(pChunk);
  }

  while (rBufferDuration > MAX_BUFFER_SIZE)
  {
    // remove old chunks:
    pChunk = rChunks.front();
    rChunks.pop_front();
    rBufferDuration -= pChunk->GetDuration();

    pChunk->Release();
  }
}

double Radio::ReadSet(int64& chunk_count_preview, int64& chunk_count)
{
  double duration = 0;
  for (int32 i = 0; i < 13 && mLive; i++)
  {
    bool nextFrameOK = true;
    bool nextFramePreviewOK = true;
    bool skip = i == 12;
    Mp3Chunk* pChunk = NULL;
    if (!skip)
      pChunk = mpParser->GetChunk();

    Mp3Chunk* pChunkPreview = mpParserPreview->GetChunk();

    if (pChunk)
    {
      chunk_count++;
      //if (!(chunk_count % 100))
        //printf("%ld chunks\n", chunk_count);

      // Store this chunk locally for incomming connections and push it to current clients:
      AddChunk(pChunk, false);
    }

    if (pChunkPreview)
    {
      chunk_count_preview++;
      //if (!(chunk_count_preview % 100))
      //printf("%ld chunks preview\n", chunk_count_preview);

      // Store this chunk locally for incomming connections and push it to current clients:
      AddChunk(pChunkPreview, true);
      duration += pChunkPreview->GetDuration();
    }

    if (!skip)
      nextFrameOK = mpParser->GoToNextFrame();
    nextFramePreviewOK = mpParserPreview->GoToNextFrame();

    if ((!skip && !pChunk) || !nextFramePreviewOK || !nextFrameOK)
    {
      printf("[skip: %c][pChunk: %p][nextFrameOK: %c / %c]\n", skip?'y':'n', pChunk, nextFramePreviewOK?'y':'n', nextFrameOK?'y':'n');
      mLive = LoadNextTrack();

      if (!mLive)
      {
        printf("Error while getting next song for radio '%s'. Shutting down...\n", mID.GetChars());
      }
    }
  }

  //printf("mBufferDurationPreview: %f / mBufferDuration: %f\n", mBufferDurationPreview, mBufferDuration);
  return duration;
}

void Radio::OnStart()
{
  int64 chunk_count = 0;
  int64 chunk_count_preview = 0;

  // the preview if 64k while the hq is 192k. For some reasons, the chunks are not of the same duration in between preview and HQ: 12 HQ chunks = 13 Preview chunks. So let's skip one HQ every 13 LQ:
  int counter = 0;

  // Pre buffering:
  while ((mBufferDurationPreview < IDEAL_BUFFER_SIZE && mLive))
  {
    ReadSet(chunk_count_preview, chunk_count);
    //printf("Preload buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
  }

  // Do the actual regular streaming:
  double nexttime = nglTime();
  while (mLive)
  {
    while (mLive && ((mBufferDurationPreview < IDEAL_BUFFER_SIZE) || nglTime() >= nexttime))
    {
      nexttime += ReadSet(chunk_count_preview, chunk_count);
      //printf("buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
    }

    nglThread::MsSleep(100);
  }

  printf("radio '%s' is now offline\n", mID.GetChars());

  delete this;
}

bool Radio::LoadNextTrack()
{
  // Try to get the new track from the app server:
  nglString url;
  url.Format("https://dev.yasound.com/api/v1/radio/%s/get_next_song/", mID.GetChars());
  nuiHTTPRequest request(url);
  nuiHTTPResponse* pResponse = request.SendRequest();
//  printf("response: %d - %s\n", pResponse->GetStatusCode(), pResponse->GetStatusLine().GetChars());
//  printf("new trackid: %s\n", pResponse->GetBodyStr().GetChars());

  if (pResponse->GetStatusCode() == 200)
  {
    nglString p = pResponse->GetBodyStr();
    //p.Insert("_preview64", 9);
    p.Insert('/', 6);
    p.Insert('/', 3);

    nglPath path = "/space/new/medias/song";
    path += p;

    //printf("new song from server: %s\n", path.GetChars());
    if (SetTrack(path))
    {
      delete pResponse;
      return true;
    }
  }

  delete pResponse;

  // Otherwise load a track from mTracks
  if (!mTracks.empty())
  {
    nglPath p = mTracks.front();
    mTracks.pop_front();
    while (!SetTrack(p) && mLive)
    {
//      printf("Skipping unreadable '%s'\n", p.GetChars());
      p = mTracks.front();
      mTracks.pop_front();

      if (mTracks.empty())
      {
        //printf("No more track in the list. Bailout...\n");
        return false;
      }
    }
//    printf("Started '%s' from static track list\n", p.GetChars());
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
  pRadio->Start();
  return pRadio;
}

bool Radio::IsLive() const
{
  nglCriticalSectionGuard guard(gCS);
  return mLive;
}

