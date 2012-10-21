
#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "Radio.h"
#include "HTTPHandler.h"
#include "nuiHTTP.h"
#include "nuiJson.h"

#define IDEAL_BUFFER_SIZE 3.0
#define MAX_BUFFER_SIZE 4.0

#define ENABLE_REDIS_THREADS 1

///////////////////////////////////////////////////
//class Radio
Radio::Radio(const nglString& rID, const nglString& rHost)
: mID(rID),
  mLive(false),
  mOnline(false),
  mGoOffline(false),
  mpParser(NULL),
  mpStream(NULL),
  mBufferDuration(0),
  mpParserPreview(NULL),
  mpStreamPreview(NULL),
  mBufferDurationPreview(0),
  mpSource(NULL),
  mpPreviewSource(NULL),
  mpThread(NULL)
{
  if (!rHost.IsNull())
  {
    // Proxy mode
    NGL_LOG("radio", NGL_LOG_INFO, "Radio::Radio Proxy for radio '%s' (source url: http://%s:%d/%s )\n", mID.GetChars(), rHost.GetChars(), mPort, mID.GetChars());
    mpSource = new nuiTCPClient(); // Don't connect the non preview source for now
    mpSource->Connect(nuiNetworkHost(rHost, mPort, nuiNetworkHost::eTCP));
    mpPreviewSource = new nuiTCPClient();
    mpPreviewSource->Connect(nuiNetworkHost(rHost, mPort, nuiNetworkHost::eTCP));
  }

  RegisterRadio(mID, this);
  mOnline = true;
}

Radio::~Radio()
{
  NGL_LOG("radio", NGL_LOG_INFO, "Radio::~Radio() [%s]", mID.GetChars());
  delete mpSource;
  delete mpPreviewSource;

  delete mpParser;
  delete mpStream;
  delete mpParserPreview;
  delete mpStreamPreview;

  UnregisterRadio(mID);
  NGL_LOG("radio", NGL_LOG_INFO, "Radio::~Radio() OK");
}

void Radio::SetNetworkSource(nuiTCPClient* pHQSource, nuiTCPClient* pLQSource)
{
  nglCriticalSectionGuard guard(mCS);
  NGL_LOG("radio", NGL_LOG_INFO, "Radio::Radio Set Network Source for radio '%s'\n", mID.GetChars());

  if (mpSource)
    mpSource->Close();
  if (mpPreviewSource)
    mpPreviewSource->Close();

  if (!mLive)
  {
    delete mpSource;
    delete mpPreviewSource;
  }

  mpSource = pHQSource;
  mpPreviewSource = pLQSource;

  bool old = mLive;
  mLive = (mpSource || mpPreviewSource);

  if (old && !mLive)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "Stopping live");
  }
  else if (!old && mLive)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "Starting live");
  }
}

void Radio::Start()
{
  size_t stacksize = 1024 * 1024 * 4;

  if (mpPreviewSource)
  {
    mOnline = mpPreviewSource->IsReadConnected();

    if (mOnline)
      mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStartProxy), nglThread::Normal, stacksize);
  }
  else
  {
    //mOnline = LoadNextTrack();
    LoadNextTrack();

    if (mOnline)
      mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStart), nglThread::Normal, stacksize);
    else
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Error while creating radio: unable to find a track to play\n");
    }
  }

  if (mpThread)
  {
    mpThread->SetAutoDelete(true);
    mpThread->Start();
    size_t size = mpThread->GetStackSize();

    //NGL_LOG("radio", NGL_LOG_INFO, "New thread stack size: %ld (requested %ld)\n", size, stacksize);
  }
}

void Radio::RegisterClient(HTTPHandler* pClient, bool highQuality)
{
  pClient->SetAutoPool(NULL);

  //NGL_LOG("radio", NGL_LOG_INFO, "RegisterClient(%p)", pClient);
  ClientList& rClients            = highQuality ? mClients : mClientsPreview;
  std::deque<Mp3Chunk*>& rChunks  = highQuality ? mChunks : mChunksPreview;

  if (pClient)
  {
    nglCriticalSectionGuard guard(mClientListCS);
    //NGL_LOG("radio", NGL_LOG_INFO, "RegisterClient(%p) CS OK", pClient);


    mOnline = true;
    mGoOffline = false;
    rClients.push_back(pClient);

    pClient->Log(200);

    NGL_LOG("radio", NGL_LOG_INFO, "HTTP Method: %s", pClient->GetMethod().GetChars());

    if (pClient->IsLive())
    {
      NGL_OUT("radio", NGL_LOG_INFO, "Starting to get data from client for radio %s", pClient->GetURL().GetChars());
      SetNetworkSource(NULL, pClient);
    }

    const RadioUser& rUser = pClient->GetUser();
    nglString sessionid;
    sessionid.Add(pClient);
    mpRedisThreadOut->RegisterListener(mID, sessionid, rUser.uuid);

    // Reply + Headers:
    pClient->ReplyLine("HTTP/1.0 200 OK");
    pClient->ReplyHeader("Cache-Control", "no-cache");
    pClient->ReplyHeader("Server", "Yastream 1.0.0");

    if (!pClient->IsLive())
    {
      pClient->ReplyHeader("Content-Type", "audio/mpeg");
      pClient->ReplyHeader("icy-name", "no name");
      pClient->ReplyHeader("icy-pub", "1");
    }
    else
    {
      pClient->ReplyHeader("Content-Type", "text/plain");
    }

    pClient->ReplyLine("");

    // Do the streaming:
    NGL_LOG("radio", NGL_LOG_ERROR, "Do the streaming\n");

  }

  //NGL_LOG("radio", NGL_LOG_INFO, "Prepare the new client:\n");
  // Fill the buffer:
  {
    nglCriticalSectionGuard guard(mClientListCS);
    for (std::deque<Mp3Chunk*>::const_iterator it = rChunks.begin(); it != rChunks.end(); ++it)
    {
      Mp3Chunk* pChunk = *it;
      pClient->AddChunk(pChunk);
      if (!pClient->IsWriteConnected() || !pClient->IsReadConnected())
      {
        return;
      }
      //NGL_LOG("radio", NGL_LOG_INFO, "Chunk %f\n", pChunk->GetTime());
    }
  }
  //NGL_LOG("radio", NGL_LOG_INFO, "RegisterClient(%p) DONE", pClient);
}

void Radio::UnregisterClient(HTTPHandler* pClient)
{
  NGL_LOG("radio", NGL_LOG_INFO, "client is gone for radio %p %s\n", this, mID.GetChars());
  nglCriticalSectionGuard guard(mClientListCS);
  mClients.remove(pClient);
  mClientsPreview.remove(pClient);
  NGL_LOG("radio", NGL_LOG_INFO, "    %d clients left in radio %s\n", mClientsPreview.size(), mID.GetChars());

  if (mClients.empty() && mClientsPreview.empty())
  {
    //  Shutdown radio
    NGL_LOG("radio", NGL_LOG_INFO, "Last client is gone: Shutting down radio %s\n", mID.GetChars());
    //mOnline = false;
    mGoOffline = true;
  }

  const RadioUser& rUser = pClient->GetUser();
  nglString sessionid;
  sessionid.Add(pClient);
  mpRedisThreadOut->RegisterListener(mID, sessionid, rUser.uuid);
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
  base += "_lq";
  base += ".";
  base += ext;
  nglPath previewPath(base);
  if (!previewPath.Exists())
  {
    // Try old file:
    base = rOriginalPath.GetRemovedExtension();
    base += "_preview64";
    base += ".";
    base += ext;
  }
  //NGL_LOG("radio", NGL_LOG_INFO, "preview path %s\n", previewPath.GetPathName().GetChars());
  return previewPath;
}

void Radio::AddChunk(Mp3Chunk* pChunk, bool previewMode)
{
  pChunk->Acquire();

  ClientList& rClients            = previewMode ? mClientsPreview : mClients;
  std::deque<Mp3Chunk*>& rChunks  = previewMode ? mChunksPreview : mChunks;
  double& rBufferDuration         = previewMode ? mBufferDurationPreview : mBufferDuration;
  std::vector<HTTPHandler*> ClientsToKill;

  rChunks.push_back(pChunk);
  rBufferDuration += pChunk->GetDuration();

  //NGL_LOG("radio", NGL_LOG_INFO, "AddChunk %p -> %f\n", pChunk, rBufferDuration);
  if (previewMode)
  {
    //NGL_LOG("radio", NGL_LOG_INFO, "AddChunk %p to %d clients\n", pChunk, rClients.size());
  }

  // Push the new chunk to the current connections:
  {
    nglCriticalSectionGuard guard(mClientListCS);
    for (ClientList::const_iterator it = rClients.begin(); it != rClients.end(); ++it)
    {
      HTTPHandler* pClient = *it;
      if (pClient->IsReadConnected() && pClient->IsWriteConnected())
        pClient->AddChunk(pChunk);
      else
        ClientsToKill.push_back(pClient);
    }
  }

  for (int i = 0; i < ClientsToKill.size(); i++)
  {
    HTTPHandler* pClient = ClientsToKill[i];
    NGL_LOG("radio", NGL_LOG_ERROR, "Radio::AddChunk Kill client %p\n", pClient);
    delete pClient;
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

int32 offset = 0;

Mp3Chunk* Radio::GetChunk(nuiTCPClient* pClient)
{
  if (!pClient)
    return NULL;
  Mp3Chunk* pChunk = new Mp3Chunk();
  std::vector<uint8>& data(pChunk->GetData());
  data.resize(4);
  if (!pClient->Receive(data))
  {
    NGL_LOG("radio", NGL_LOG_ERROR, "Radio::GetChunk Unable to receive mp3 header from client\n");
    delete pChunk;
    return NULL;
  }

  Mp3Header hdr(&data[0], false);
  if (!hdr.IsValid())
  {
    NGL_LOG("radio", NGL_LOG_ERROR, "Radio::GetChunk Mp3Header invalid [0x%02x%02x%02x%02x]\n", data[0], data[1], data[2], data[3]);
    pClient->Close();
    delete pChunk;
    return NULL;
  }

  pChunk->SetDuration(hdr.GetFrameDuration());
  int32 len = hdr.GetFrameByteLength();
  int32 left = len - 4;
  int32 done = 4;
  int32 todo = left;
  offset += 4;
  data.resize(len);

  while (done != len && pClient->IsReadConnected())
  {
    int32 res = pClient->Receive(&data[len - todo], todo);

    if (res < 0)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Radio::GetChunk error getting %d bytes from the stream\n", left);
      delete pChunk;
      pClient->Close();
      return NULL;
    }

    todo -= res;
    done += res;
    offset += res;
  }


  //NGL_ASSERT(res == left);

  //NGL_LOG("radio", NGL_LOG_INFO, "Radio::GetChunk read %d bytes from the stream [offset 0x%x 0x%02x%02x%02x%02x]\n", done, offset, data[0], data[1], data[2], data[3]);
  return pChunk;
}


double Radio::ReadSet(int64& chunk_count_preview, int64& chunk_count)
{
  if (!mpParser)
  {
    if (!LoadNextTrack())
      return 0;
  }

  double duration = 0;
  bool nextFrameOK = true;
  bool nextFramePreviewOK = true;

  Mp3Chunk* pChunk = mpParser->GetChunk();
  Mp3Chunk* pChunkPreview = mpParserPreview->GetChunk();

  if (pChunk)
  {
    chunk_count++;
    //if (!(chunk_count % 100))
      //NGL_LOG("radio", NGL_LOG_INFO, "%ld chunks\n", chunk_count);

    // Store this chunk locally for incomming connections and push it to current clients:
    AddChunk(pChunk, false);
  }

  if (pChunkPreview)
  {
    chunk_count_preview++;
    //if (!(chunk_count_preview % 100))
    //NGL_LOG("radio", NGL_LOG_INFO, "%ld chunks preview\n", chunk_count_preview);

    // Store this chunk locally for incomming connections and push it to current clients:
    AddChunk(pChunkPreview, true);
    duration += pChunkPreview->GetDuration();
  }

  nextFrameOK = mpParser->GoToNextFrame();
  nextFramePreviewOK = mpParserPreview->GoToNextFrame();

  if (!pChunk || !nextFramePreviewOK || !nextFrameOK)
  {
    //NGL_LOG("radio", NGL_LOG_INFO, "[skip: %c][pChunk: %p][nextFrameOK: %c / %c]\n", skip?'y':'n', pChunk, nextFramePreviewOK?'y':'n', nextFrameOK?'y':'n');
    LoadNextTrack();
    //mOnline = LoadNextTrack();

    if (!mOnline)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Error while getting next song for radio '%s'. Shutting down...\n", mID.GetChars());
      return 0;
    }
  }

  //NGL_LOG("radio", NGL_LOG_INFO, "mBufferDurationPreview: %f / mBufferDuration: %f\n", mBufferDurationPreview, mBufferDuration);
  return duration;
}

double Radio::ReadSetProxy(int64& chunk_count_preview, int64& chunk_count)
{
  //NGL_LOG("radio", NGL_LOG_INFO, "ReadSetProxy(int64& chunk_count_preview, int64& chunk_count)");

  double duration = 0;
  for (int32 i = 0; i < 13 && mOnline; i++)
  {
    bool nextFrameOK = true;
    bool nextFramePreviewOK = true;
    bool skip = i == 12;
    Mp3Chunk* pChunk = NULL;
    if (!skip && mpSource && mpSource->IsReadConnected())
    {
      pChunk = GetChunk(mpSource);
    }

    if (pChunk)
    {
      chunk_count++;
      //if (!(chunk_count % 100))
      //NGL_LOG("radio", NGL_LOG_INFO, "%ld chunks\n", chunk_count);

      // Store this chunk locally for incomming connections and push it to current clients:
      AddChunk(pChunk, false);
    }


    Mp3Chunk* pChunkPreview = GetChunk(mpPreviewSource);

if (0)
{
    double dur = 0;
    while (pChunkPreview && dur == 0)
    {
      chunk_count_preview++;
      //if (!(chunk_count_preview % 100))
      //NGL_LOG("radio", NGL_LOG_INFO, "%ld chunks preview\n", chunk_count_preview);

      // Store this chunk locally for incomming connections and push it to current clients:
      AddChunk(pChunkPreview, true);
      dur = pChunkPreview->GetDuration();
      duration += dur;

      if (dur == 0)
      {
        //NGL_LOG("radio", NGL_LOG_INFO, "Skipping 0 length chunk");
        pChunkPreview = GetChunk(mpPreviewSource);
      }
      else
      {
        //NGL_LOG("radio", NGL_LOG_INFO, "length chunk ok");
      }
    }
}

    if (pChunkPreview)
    {
      chunk_count_preview++;
      //if (!(chunk_count_preview % 100))
      //NGL_LOG("radio", NGL_LOG_INFO, "%ld chunks preview\n", chunk_count_preview);

      // Store this chunk locally for incomming connections and push it to current clients:
      AddChunk(pChunkPreview, true);
      duration += pChunkPreview->GetDuration();
    }

//    if (!skip)
//      nextFrameOK = mpParser->GoToNextFrame();
//    nextFramePreviewOK = mpParserPreview->GoToNextFrame();

    //if ((!skip && !pChunk) || !nextFramePreviewOK || !mpPreviewSource->IsReadConnected() || !mpSource->IsReadConnected())
    if (!nextFramePreviewOK || !pChunkPreview) // #FIXME Handle high quality stream (see commented line above)
    {
      //NGL_LOG("radio", NGL_LOG_INFO, "PROXY [skip: %c][pChunk: %p][nextFrameOK: %c / %c]\n", skip?'y':'n', pChunk, nextFramePreviewOK?'y':'n', nextFrameOK?'y':'n');
      NGL_LOG("radio", NGL_LOG_ERROR, "Error while getting next song for proxy radio '%s'. Shutting down...\n", mID.GetChars());
  //NGL_LOG("radio", NGL_LOG_INFO, "ReadSetProxy UNLOCK ABORT");
      return 0;
    }
  }

  //NGL_LOG("radio", NGL_LOG_INFO, "mBufferDurationPreview: %f / mBufferDuration: %f\n", mBufferDurationPreview, mBufferDuration);
  return duration;
}


void Radio::OnStart()
{
  int64 chunk_count = 0;
  int64 chunk_count_preview = 0;

  // the preview is 64k while the hq is 192k. For some reasons, the chunks are not of the same duration in between preview and HQ: 12 HQ chunks = 13 Preview chunks. So let's skip one HQ every 13 LQ:
  int counter = 0;

  // Pre buffering:
  while ((mBufferDurationPreview < IDEAL_BUFFER_SIZE) && mOnline)
  {
    ReadSet(chunk_count_preview, chunk_count);
    //NGL_LOG("radio", NGL_LOG_INFO, "Preload buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
  }
  // Do the actual regular streaming:
  double nexttime = nglTime();
  while (mOnline)
  {
    {
      if (mLive)
      {
        double over = nglTime() - nexttime;
        while (mOnline && mLive && ((mBufferDurationPreview < IDEAL_BUFFER_SIZE) || over >= 0))
        {
          nglCriticalSectionGuard guard(mCS);
          double duration = ReadSetProxy(chunk_count_preview, chunk_count);
          nexttime += duration;
          if (duration == 0)
          {
            if (mLive)
            {
              SetNetworkSource(NULL, NULL);
              nexttime = nglTime();

              //NGL_LOG("radio", NGL_LOG_INFO, "Radio source broken");
              if (!mOnline)
              {
                NGL_LOG("radio", NGL_LOG_INFO, "Radio offline");
              }
            }
            else
            {
              NGL_LOG("radio", NGL_LOG_INFO, "Radio broken AND offline");
              mOnline = false; //#FIXME Handle HQ Stream: && mpSource->IsReadConnected();
            }
          }
          //NGL_LOG("radio", NGL_LOG_INFO, "buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
        }
      }
      else
      {
        double now = nglTime();
        while (mOnline && ((mBufferDurationPreview < IDEAL_BUFFER_SIZE) || now >= nexttime))
        {
          nglCriticalSectionGuard guard(mCS);
          nexttime += ReadSet(chunk_count_preview, chunk_count);
          //NGL_LOG("radio", NGL_LOG_INFO, "buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
        }
      }
    }
    nglThread::MsSleep(10);
  }

  // tell clients to stop:
  KillClients();

  NGL_LOG("radio", NGL_LOG_INFO, "radio '%s' is now offline\n", mID.GetChars());

  delete this;
}

void Radio::OnStartProxy()
{
  NGL_LOG("radio", NGL_LOG_INFO, "Start Proxy for radio '%s'\n", mID.GetChars());
  // We need to finish the HTTP connection to the original server:
  nglString request;
  request.Add("GET ").Add(mID).Add(" HTTP/1.1\r\n\r\n");
  mpPreviewSource->Send(request);

  std::vector<uint8> data;
  data.resize(1);
  bool cont = true;
  nglChar c[4];
  c[0] = c[1] = c[2] = c[3] = 0;
  nglString headers;

  while (cont && mpPreviewSource->Receive(data))
  {
    headers.Add((nglChar)data[0]);
    c[0] = c[1];
    c[1] = c[2];
    c[2] = c[3];
    c[3] = data[0];

    //NGL_LOG("radio", NGL_LOG_INFO, "%c", c[3]);
    if (c[0] == '\r' && c[1] == '\n' && c[2] == '\r' && c[3] == '\n')
      cont = false;
  }

  //NGL_LOG("radio", NGL_LOG_INFO, "Headers:\n%s", headers.GetChars());

  int64 chunk_count = 0;
  int64 chunk_count_preview = 0;

  // the preview is 64k while the hq is 192k. For some reasons, the chunks are not of the same duration in between preview and HQ: 12 HQ chunks = 13 Preview chunks. So let's skip one HQ every 13 LQ:
  int counter = 0;

  // Pre buffering:
  while ((mBufferDurationPreview < IDEAL_BUFFER_SIZE && mOnline))
  {
    ReadSetProxy(chunk_count_preview, chunk_count);
    //NGL_LOG("radio", NGL_LOG_INFO, "Preload buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
  }

  // Do the actual regular streaming:
  double nexttime = nglTime();
  while (mOnline)
  {
    while (mOnline && ((mBufferDurationPreview < IDEAL_BUFFER_SIZE) || nglTime() >= nexttime))
    {
      double duration = ReadSetProxy(chunk_count_preview, chunk_count);
      nexttime += duration;
      if (duration == 0)
      {
        mOnline = false;
      }
      //NGL_LOG("radio", NGL_LOG_INFO, "buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
    }

    nglThread::MsSleep(100);
  }

  // tell clients to stop:
  KillClients();

  NGL_LOG("radio", NGL_LOG_INFO, "radio '%s' is now offline\n", mID.GetChars());

  delete this;
}

void Radio::KillClients()
{
  NGL_LOG("radio", NGL_LOG_INFO, "Force '%d' clients to stop relaying our data\n", mClientsPreview.size() + mClients.size());
  nglCriticalSectionGuard guard(mClientListCS);
  ClientList l = mClientsPreview;

  for (ClientList::const_iterator it = l.begin(); it != l.end(); ++it)
  {
    HTTPHandler* pClient = *it;
    pClient->GoOffline();
  }

  l = mClients;
  for (ClientList::const_iterator it = l.begin(); it != l.end(); ++it)
  {
    HTTPHandler* pClient = *it;
    pClient->GoOffline();
  }
}

bool Radio::LoadNextTrack()
{
  if (mGoOffline) // We were asked to kill this radio once the current song was finished.
    return false;

  // Otherwise load a track from mTracks
  if (!mTracks.empty())
  {
    Track track = mTracks.front();
    mTracks.pop_front();

    nglString p = track.mFileID;
    //p.Insert("_preview64", 9);
    p.Insert('/', 6);
    p.Insert('/', 3);

    //nglPath path = "/space/new/medias/song";
    nglPath path = mDataPath;//"/data/glusterfs-storage/replica2all/song/";
    path += p;

    while (!SetTrack(p) && mOnline && !mTracks.empty())
    {
      NGL_LOG("radio", NGL_LOG_INFO, "Skipping unreadable static file '%s'\n", p.GetChars());
      track = mTracks.front();
      mTracks.pop_front();

      p = track.mFileID;
      //p.Insert("_preview64", 9);
      p.Insert('/', 6);
      p.Insert('/', 3);

      //nglPath path = "/space/new/medias/song";
      path = mDataPath;//"/data/glusterfs-storage/replica2all/song/";
      path += p;
    }
    //NGL_LOG("radio", NGL_LOG_INFO, "Started '%s' from static track list\n", p.GetChars());
    return true;
  }

  NGL_LOG("radio", NGL_LOG_INFO, "No more track in the list. Bailout...\n");
  return false;
}

nglCriticalSection Radio::gCS;
std::map<nglString, Radio*> Radio::gRadios;
nglString Radio::mHostname = "0.0.0.0";
nglString Radio::mAppUrl = "https://api.yasound.com";
int Radio::mPort = 8000;
nglPath Radio::mDataPath = "/space/new/medias/song";

void Radio::SetParams(const nglString& appurl, const nglString& hostname, int port, const nglPath& rDataPath)
{
  mHostname = hostname;
  mAppUrl = appurl;
  mPort = port;
  mDataPath = rDataPath;
}

bool Radio::GetUser(const nglString& rToken, RadioUser& rUser)
{
  nglString id("auth_");
  id.Add(rToken);
  nglSyncEvent* pEvent = AddEvent(id);
  mpRedisThreadOut->UserAuthentication(rToken);
  if (!pEvent->Wait(YASCHEDULER_WAIT_TIME))
  {
    // Timeout... no reply from the scheduler
    NGL_LOG("radio", NGL_LOG_ERROR, "Time out from the scheduler for user auth token %s\n", rToken.GetChars());

    DelEvent(id);
    return false;
  }
  DelEvent(id);

  {
    nglCriticalSectionGuard guard(gCS);
    std::map<nglString, RadioUser>::iterator it = gUsers.find(id);
    if (it != gUsers.end())
    {
      rUser = it->second;
      gUsers.erase(it);
    }
  }

  return true;
}

bool Radio::GetUser(const nglString& rUsername, const nglString& rApiKey, RadioUser& rUser)
{
  nglString id("auth_");
  id.Add(rUsername).Add(rApiKey);
  nglSyncEvent* pEvent = AddEvent(id);
  mpRedisThreadOut->UserAuthentication(rUsername, rApiKey);
  if (!pEvent->Wait(YASCHEDULER_WAIT_TIME))
  {
    // Timeout... no reply from the scheduler
    NGL_LOG("radio", NGL_LOG_ERROR, "Time out from the scheduler for username/apikey %s / %s\n", rUsername.GetChars(), rApiKey.GetChars());

    DelEvent(id);
    return false;
  }
  DelEvent(id);

  {
    nglCriticalSectionGuard guard(gCS);
    std::map<nglString, RadioUser>::iterator it = gUsers.find(id);
    if (it != gUsers.end())
    {
      rUser = it->second;
      gUsers.erase(it);
    }
  }

  return true;
}


Radio* Radio::GetRadio(const nglString& rURL, HTTPHandler* pClient, bool HQ)
{
  //NGL_LOG("radio", NGL_LOG_INFO, "Getting radio %s\n", rURL.GetChars());
  nglCriticalSectionGuard guard(gCS);

  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it == gRadios.end())
  {
    nglSyncEvent* pEvent = AddEvent(rURL);
    mpRedisThreadOut->PlayRadio(rURL);
    if (!pEvent->Wait(YASCHEDULER_WAIT_TIME))
    {
      // Timeout... no reply from the scheduler
      NGL_LOG("radio", NGL_LOG_ERROR, "Time out from the scheduler for radio %s\n", rURL.GetChars());

      DelEvent(rURL);
      return NULL;
    }
    DelEvent(rURL);

    it = gRadios.find(rURL);
    if (it == gRadios.end())
    {
      // No proxy radio has been created... so it should be ok
      // Create the radio!
      //NGL_LOG("radio", NGL_LOG_INFO, "Trying to create the radio '%s'\n", rURL.GetChars());
      Radio* pRadio = CreateRadio(rURL, nglString::Null);
      if (pRadio)
        pRadio->RegisterClient(pClient, HQ);
      return pRadio;
    }

  }
  //NGL_LOG("radio", NGL_LOG_INFO, "Getting existing radio %s\n", rURL.GetChars());
  Radio* pRadio = it->second;
  if (pRadio)
    pRadio->RegisterClient(pClient, HQ);
  return pRadio;
}

void Radio::RegisterRadio(const nglString& rURL, Radio* pRadio)
{
  nglCriticalSectionGuard guard(gCS);
  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it != gRadios.end())
    NGL_LOG("radio", NGL_LOG_ERROR, "the radio '%s' is already registered\n", rURL.GetChars());

  gRadios[rURL] = pRadio;
}

void Radio::UnregisterRadio(const nglString& rURL)
{
  nglCriticalSectionGuard guard(gCS);
  NGL_LOG("radio", NGL_LOG_INFO, "Unregistering radio '%s'\n", rURL.GetChars());

  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it == gRadios.end())
  {
    //NGL_LOG("radio", NGL_LOG_ERROR, "Error, radio '%s' was never registered\n", rURL.GetChars());
  }
  gRadios.erase(rURL);

  mpRedisThreadOut->StopRadio(rURL);
}

Radio* Radio::CreateRadio(const nglString& rURL, const nglString& rHost)
{
  Radio* pRadio = new Radio(rURL, rHost);
  pRadio->Start();
  if (pRadio->IsOnline())
    return pRadio;

  NGL_LOG("radio", NGL_LOG_ERROR, "Radio::CreateRadio unable to create radio\n");
  delete pRadio;
  return NULL;
}

bool Radio::IsLive() const
{
  nglCriticalSectionGuard guard(gCS);
  return mLive;
}

bool Radio::IsOnline() const
{
  nglCriticalSectionGuard guard(gCS);
  return mOnline;
}

void Radio::HandleRedisMessage(const RedisReply& rReply)
{
  nglString str = rReply.GetReply(3);
  nuiJson::Reader reader;
  nuiJson::Value msg;

//  for (int i = 0; i < rReply.GetCount(); i++)
//  {
//    NGL_LOG("radio", NGL_LOG_INFO, "   Redis arg[%d] = '%s'", i, rReply.GetReply(i).GetChars());
//  }

  bool res = reader.parse(str.GetStdString(), msg);

  if (!res)
  {
    NGL_LOG("radio", NGL_LOG_ERROR, "unable to parse json message from scheduler:\n'%s'\n", str.GetChars());
    return;
  }

  nglString type = msg.get("type", nuiJson::Value()).asString();

  //NGL_LOG("radio", NGL_LOG_INFO, "Got messages from yascheduler: %s", type.GetChars());
  if (type == "radio_started")
  {
    nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
    NGL_LOG("radio", NGL_LOG_INFO, "Redis: radio_started %s\n", uuid.GetChars());

    SignallEvent(uuid);
  }
  else if (type == "radio_exists")
  {
    nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
    nglString master_streamer = msg.get("master_streamer", nuiJson::Value()).asString();
    NGL_LOG("radio", NGL_LOG_INFO, "Redis: radio_exists %s %s\n", uuid.GetChars(), master_streamer.GetChars());

    // #FIXME, disabled proxy radio creation for now, the scheduler doesn't handle the dying streamer properly:
    //CreateRadio(uuid, master_streamer);
    SignallEvent(uuid);
  }
  else if (type == "radio_stopped")
  {
    nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
    NGL_LOG("radio", NGL_LOG_INFO, "Redis: radio_stopped %s\n", uuid.GetChars());
  }
  else if (type == "play")
  {
    nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
    nglString filename = msg.get("filename", nuiJson::Value()).asString();
    double delay = msg.get("delay", nuiJson::Value()).asDouble();
    double offset = msg.get("offset", nuiJson::Value()).asDouble();
    double crossfade = msg.get("crossfade_duration", nuiJson::Value()).asDouble();

    NGL_LOG("radio", NGL_LOG_INFO, "Redis: play %s %s d:%f o:%f f:%f\n", uuid.GetChars(), filename.GetChars(), delay, offset, crossfade);
    {
      nglCriticalSectionGuard g(gCS);
      RadioMap::const_iterator it = gRadios.find(uuid);

      if (it != gRadios.end())
      {
        Radio* pRadio = it->second;
        NGL_ASSERT(pRadio);
        nglString id(uuid);
        id.Add(pRadio);
        SignallEvent(id);

        pRadio->PlayTrack(filename, delay, offset, crossfade);
      }
      else
      {
        NGL_LOG("radio", NGL_LOG_INFO, "Redis: unkown radio %s\n", uuid.GetChars());
      }
    }
  }
  else if (type == "user_authentication")
  {
    nglString uuid = msg.get("user_id", nuiJson::Value()).asString();
    bool hd = msg.get("hd", nuiJson::Value()).asBool();
    nglString auth_token = msg.get("auth_token", nuiJson::Value()).asString();
    nglString username = msg.get("username", nuiJson::Value()).asString();
    nglString api_key = msg.get("api_key", nuiJson::Value()).asString();

    NGL_LOG("radio", NGL_LOG_INFO, "Redis: user_authentication '%s' (%s) (%s) (%s) (%s)\n", uuid.GetChars(), hd?"HD":"SD", auth_token.GetChars(), username.GetChars(), api_key.GetChars());

    nglString id("auth_");
    id.Add(auth_token).Add(username).Add(api_key);

    {
      nglCriticalSectionGuard g(gCS);
      RadioUser u;
      u.hd = hd;
      u.uuid = uuid;
      gUsers[id] = u;
    }
    SignallEvent(id);
  }
  else if (type == "ping")
  {
    mpRedisThreadOut->Pong();
    NGL_LOG("radio", NGL_LOG_INFO, "Redis: ping\n");
  }
  else if (type == "test")
  {
    nglString info = msg.get("info", nuiJson::Value()).asString();
    NGL_LOG("radio", NGL_LOG_INFO, "Redis: test '%s'\n", info.GetChars());
  }
  else
  {
    NGL_LOG("radio", NGL_LOG_INFO, "Redis: unknown message '%s'\n", type.GetChars());
  }

}


RedisThread* Radio::mpRedisThreadIn = NULL;
RedisThread* Radio::mpRedisThreadOut = NULL;

void Radio::StartRedis()
{
#if ENABLE_REDIS_THREADS
  mpRedisThreadIn = new RedisThread(nuiNetworkHost("127.0.0.1", 6379, nuiNetworkHost::eTCP), RedisThread::MessagePump, mHostname);
  mpRedisThreadIn->SetMessageHandler(Radio::HandleRedisMessage);
  mpRedisThreadIn->Start();

  mpRedisThreadOut = new RedisThread(nuiNetworkHost("127.0.0.1", 6379, nuiNetworkHost::eTCP), RedisThread::Broadcaster, mHostname);
  mpRedisThreadOut->Start();

  mpRedisThreadOut->RegisterStreamer(mHostname);
  mpRedisThreadOut->Test("POUET!");
#endif
}

void Radio::StopRedis()
{
#if ENABLE_REDIS_THREADS
  mpRedisThreadOut->UnregisterStreamer(mHostname);
#endif

  delete mpRedisThreadIn;
  delete mpRedisThreadOut;
  mpRedisThreadIn = NULL;
  mpRedisThreadOut = NULL;
}

Radio::EventMap Radio::gEvents;
std::map<nglString, RadioUser> Radio::gUsers;
nglCriticalSection Radio::gEventCS;

nglSyncEvent* Radio::AddEvent(const nglString& rName)
{
  nglCriticalSectionGuard g(gEventCS);

  NGL_ASSERT(gEvents.find(rName) == gEvents.end());

  nglSyncEvent* pEvent = new nglSyncEvent();
  //pEvent->Reset();
  gEvents[rName] = pEvent;
  NGL_LOG("radio", NGL_LOG_INFO, "add event: %s - %p", rName.GetChars(), pEvent);
  return pEvent;
}

void Radio::DelEvent(const nglString& rName)
{
  nglCriticalSectionGuard g(gEventCS);

  EventMap::iterator it = gEvents.find(rName);
  NGL_ASSERT(it != gEvents.end());

  nglSyncEvent* pEvent = gEvents[rName];
  NGL_LOG("radio", NGL_LOG_INFO, "del event: %s - %p", rName.GetChars(), pEvent);
  delete pEvent;
  gEvents.erase(it);
}

void Radio::SignallEvent(const nglString& rName)
{
  nglCriticalSectionGuard g(gEventCS);

  EventMap::iterator it = gEvents.find(rName);
  NGL_ASSERT(it != gEvents.end());

  nglSyncEvent* pEvent = gEvents[rName];
  NGL_LOG("radio", NGL_LOG_INFO, "signal event: %s - %p", rName.GetChars(), pEvent);
  pEvent->Set();
}

void Radio::PlayTrack(const nglString& rFilename, double delay, double offet, double fade)
{
  // #TODO
  nglCriticalSectionGuard g(mCS);
  Track track(rFilename, delay, offset, fade);
  mTracks.push_back(track);
}

