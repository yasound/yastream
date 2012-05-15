
#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "Radio.h"
#include "HTTPHandler.h"
#include "nuiHTTP.h"

#define IDEAL_BUFFER_SIZE 3.0
#define MAX_BUFFER_SIZE 4.0

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
    mOnline = mpPreviewSource->IsConnected();

    if (mOnline)
      mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStartProxy), nglThread::Normal, stacksize);
  }
  else
  {
    mOnline = LoadNextTrack();

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
  }

  //NGL_LOG("radio", NGL_LOG_INFO, "Prepare the new client:\n");
  // Fill the buffer:
  {
    nglCriticalSectionGuard guard(mClientListCS);
    for (std::deque<Mp3Chunk*>::const_iterator it = rChunks.begin(); it != rChunks.end(); ++it)
    {
      Mp3Chunk* pChunk = *it;
      pClient->AddChunk(pChunk);
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
    mGoOffline = true;
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
  //NGL_LOG("radio", NGL_LOG_INFO, "preview path %s\n", previewPath.GetPathName().GetChars());
  return previewPath;
}

void Radio::AddChunk(Mp3Chunk* pChunk, bool previewMode)
{
  pChunk->Acquire();

  ClientList& rClients            = previewMode ? mClientsPreview : mClients;
  std::deque<Mp3Chunk*>& rChunks  = previewMode ? mChunksPreview : mChunks;
  double& rBufferDuration         = previewMode ? mBufferDurationPreview : mBufferDuration;

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
      pClient->AddChunk(pChunk);
    }
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

  while (done != len && pClient->IsConnected())
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
  double duration = 0;
  for (int32 i = 0; i < 13 && mOnline; i++)
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

    if (!skip)
      nextFrameOK = mpParser->GoToNextFrame();
    nextFramePreviewOK = mpParserPreview->GoToNextFrame();

    if ((!skip && !pChunk) || !nextFramePreviewOK || !nextFrameOK)
    {
      NGL_LOG("radio", NGL_LOG_INFO, "[skip: %c][pChunk: %p][nextFrameOK: %c / %c]\n", skip?'y':'n', pChunk, nextFramePreviewOK?'y':'n', nextFrameOK?'y':'n');
      mOnline = LoadNextTrack();

      if (!mOnline)
      {
        NGL_LOG("radio", NGL_LOG_ERROR, "Error while getting next song for radio '%s'. Shutting down...\n", mID.GetChars());
        return 0;
      }
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
    if (!skip && mpSource && mpSource->IsConnected())
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

    //if ((!skip && !pChunk) || !nextFramePreviewOK || !mpPreviewSource->IsConnected() || !mpSource->IsConnected())
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
              mOnline = false; //#FIXME Handle HQ Stream: && mpSource->IsConnected();
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
    nglThread::MsSleep(100);
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
  NGL_LOG("radio", NGL_LOG_INFO, "Make '%d' clients to stop relaying our data\n", mClientsPreview.size() + mClients.size());
  nglCriticalSectionGuard guard(mClientListCS);
  for (ClientList::const_iterator it = mClientsPreview.begin(); it != mClientsPreview.end(); ++it)
  {
    HTTPHandler* pClient = *it;
    pClient->GoOffline();
  }

  for (ClientList::const_iterator it = mClients.begin(); it != mClients.end(); ++it)
  {
    HTTPHandler* pClient = *it;
    pClient->GoOffline();
  }
}

bool Radio::LoadNextTrack()
{
  if (mGoOffline) // We were asked to kill this radio once the current song was finished.
    return false;

  while (1)
  {
    // Try to get the new track from the app server:
    nglString url;
    url.Format("%s/api/v1/radio/%s/get_next_song/", mAppUrl.GetChars(), mID.GetChars());
    nuiHTTPRequest request(url);
    nuiHTTPResponse* pResponse = request.SendRequest();

    //NGL_LOG("radio", NGL_LOG_INFO, "get next song: %s\n", url.GetChars());
    //NGL_LOG("radio", NGL_LOG_INFO, "response: %d - %s\n", pResponse->GetStatusCode(), pResponse->GetStatusLine().GetChars());

    if (pResponse->GetStatusCode() == 200)
    {
      //NGL_LOG("radio", NGL_LOG_INFO, "new trackid: %s\n", pResponse->GetBodyStr().GetChars());

      nglString p = pResponse->GetBodyStr();
      //p.Insert("_preview64", 9);
      p.Insert('/', 6);
      p.Insert('/', 3);

      //nglPath path = "/space/new/medias/song";
      nglPath path = mDataPath;//"/data/glusterfs-storage/replica2all/song/";
      path += p;

      NGL_LOG("radio", NGL_LOG_INFO, "new song from server: %s\n", path.GetChars());
      if (SetTrack(path))
      {
        delete pResponse;
        return true;
      }
      else
      {
        NGL_LOG("radio", NGL_LOG_ERROR, "Unable to set track '%s'\n", path.GetChars());
        delete pResponse;
        pResponse = request.SendRequest();
        //return false;
      }
    }
    else
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Server error (%s): %d\n", url.GetChars(), pResponse->GetStatusCode());
      NGL_LOG("radio", NGL_LOG_ERROR, "response data:\n%s\n", pResponse->GetBodyStr().GetChars());
      delete pResponse;
      return false;
    }
  }

  // Otherwise load a track from mTracks
  if (!mTracks.empty())
  {
    nglPath p = mTracks.front();
    mTracks.pop_front();
    while (!SetTrack(p) && mOnline)
    {
      NGL_LOG("radio", NGL_LOG_INFO, "Skipping unreadable static file '%s'\n", p.GetChars());
      p = mTracks.front();
      mTracks.pop_front();

      if (mTracks.empty())
      {
        NGL_LOG("radio", NGL_LOG_INFO, "No more track in the static list. Bailout...\n");
        return false;
      }
    }
    NGL_LOG("radio", NGL_LOG_INFO, "Started '%s' from static track list\n", p.GetChars());
    return true;
  }

  NGL_LOG("radio", NGL_LOG_INFO, "No more track in the list. Bailout...\n");
  return false;
}

void Radio::AddTrack(const nglPath& rPath)
{
  mTracks.push_back(rPath);
}


nglCriticalSection Radio::gCS;
std::map<nglString, Radio*> Radio::gRadios;
nglString Radio::mHostname = "0.0.0.0";
nglString Radio::mAppUrl = "https://api.yasound.com";
int Radio::mPort = 8000;
nglPath Radio::mDataPath = "/space/new/medias/song";
nglString Radio::mRedisHost = "127.0.0.1";
int Radio::mRedisPort = 6379;
int Radio::mRedisDB = 1;
RedisClient Radio::gRedis;

void Radio::SetParams(const nglString& appurl, const nglString& hostname, int port, const nglPath& rDataPath, const nglString& rRedisHost, int RedisPort, int RedisDB)
{
  mHostname = hostname;
  mAppUrl = appurl;
  mPort = port;
  mDataPath = rDataPath;
  mRedisHost = rRedisHost;
  mRedisPort = RedisPort;
  mRedisDB = RedisDB;
}

void Radio::InitRedis()
{
  if (!gRedis.IsConnected())
  {
    gRedis.Disconnect();
    NGL_LOG("radio", NGL_LOG_INFO, "Init Redis connection to server %s:%d with db index = %d.\n", mRedisHost.GetChars(), mRedisPort, mRedisDB);
    bool res = gRedis.Connect(nuiNetworkHost(mRedisHost, mRedisPort, nuiNetworkHost::eTCP));
    if (!res)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Error connecting to redis server.\n");
      return;
    }

    RedisRequest req;
    req.SELECT(mRedisDB);
    RedisReplyType reply = gRedis.SendCommand(req);
    if (reply == eRedisError)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Redis error: %s\n", req.GetError().GetChars());
    }
  }
}

void Radio::FlushRedis(bool FlushAll)
{
  NGL_LOG("radio", NGL_LOG_INFO, "Flush Redis DB\n");
  InitRedis();
  NGL_LOG("radio", NGL_LOG_INFO, "  Connected to Redis DB\n");

  if (FlushAll)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "------- FLUSH ALL !!! -------\n");
    RedisRequest req;
    req.FLUSHDB();
    RedisReplyType reply = gRedis.SendCommand(req);

    if (reply == eRedisError)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while FLUSHDB: %s\n", req.GetError().GetChars());
    }

    return;
  }

  RedisRequest req;
  nglString server;
  server.Add("server:").Add(mHostname);
  req.SMEMBERS(server);
  RedisReplyType reply = gRedis.SendCommand(req);

  if (reply == eRedisError)
  {
    NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while SMEMBERS: %s\n", req.GetError().GetChars());
  }

  //NGL_LOG("radio", NGL_LOG_INFO, "Got %d items back\n", gRedis.GetCount());

  std::vector<nglString> radios;
  int64 count = req.GetCount();

  NGL_LOG("radio", NGL_LOG_INFO, "\t%d radios to flush\n", count);
  if (count > 0)
  {
    radios.reserve(count);
    for (int i = 0; i < count; i++)
    {
      radios.push_back(req.GetReply(i));
      NGL_LOG("radio", NGL_LOG_INFO, "\t\t%s\n", radios[i].GetChars());
    }

    RedisRequest req;
    req.DEL(radios);
    reply = gRedis.SendCommand(req);
    if (reply == eRedisError)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while Flush DEL: %s\n", req.GetError().GetChars());
    }
  }

  // Clean the master key:
  req.DEL(server);
  reply = gRedis.SendCommand(req);
  if (reply == eRedisError)
  {
    NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while Master Flush DEL: %s\n", req.GetError().GetChars());
  }

  // Clean the stats:
  {
    nglString id("listeners:");
    id += mHostname;
    RedisRequest req;
    req.DEL(id);
    Radio::SendRedisCommand(req);
  }

  {
    nglString id("anonymouslisteners:");
    id += mHostname;

    RedisRequest req;
    req.DEL(id);
    Radio::SendRedisCommand(req);
  }
}


Radio* Radio::GetRadio(const nglString& rURL, HTTPHandler* pClient, bool HQ)
{
  //NGL_LOG("radio", NGL_LOG_INFO, "Getting radio %s\n", rURL.GetChars());
  nglCriticalSectionGuard guard(gCS);

  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it == gRadios.end())
  {
    // Ask Redis if he knows a server that handles this radio:
    nglString r;
    r.Add("radio:").Add(rURL);

    InitRedis();
    if (gRedis.IsConnected())
    {
      RedisRequest req;
      req.GET(r);
      RedisReplyType reply = gRedis.SendCommand(req);
      if (reply == eRedisError)
      {
        NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while GET: %s\n", req.GetError().GetChars());
      }

      NGL_ASSERT(reply != eRedisError);
      nglString host = req.GetReply(0);
      if (!host.IsNull())
      {
        // Create this radio from the actual server!
        NGL_LOG("radio", NGL_LOG_INFO, "Radio found on %s\n", host.GetChars());

        Radio* pRadio = CreateRadio(rURL, host);
        if (pRadio)
          if (pRadio->IsOnline())
            pRadio->RegisterClient(pClient, HQ);
          else
            pRadio = NULL;
        return pRadio;
      }

      // The radio was not on the server. we need to create it:
      NGL_LOG("radio", NGL_LOG_INFO, "Tell Redis we are creating radio %s\n", rURL.GetChars());
      req.SET(r, mHostname);
      reply = gRedis.SendCommand(req);
      if (reply == eRedisError)
      {
        NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while SET: %s\n", req.GetError().GetChars());
      }

      nglString server;
      server.Add("server:").Add(mHostname);
      req.SADD(server, rURL);
      if (gRedis.SendCommand(req) == eRedisError)
      {
        NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while SADD: %s\n", req.GetError().GetChars());
      }
    }
    else
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "redis not connected");
    }

    // Create the radio!
    //NGL_LOG("radio", NGL_LOG_INFO, "Trying to create the radio '%s'\n", rURL.GetChars());
    Radio* pRadio = CreateRadio(rURL, nglString::Null);
    if (pRadio)
      if (pRadio->IsOnline())
        pRadio->RegisterClient(pClient, HQ);
      else
        pRadio = NULL;
    return pRadio;
    //return NULL;
  }
  //NGL_LOG("radio", NGL_LOG_INFO, "Getting existing radio %s\n", rURL.GetChars());
  Radio* pRadio = it->second;
  if (pRadio)
    if (pRadio->IsOnline())
      pRadio->RegisterClient(pClient, HQ);
    else
      pRadio = NULL;
  return pRadio;
}

void Radio::RegisterRadio(const nglString& rURL, Radio* pRadio)
{
  nglCriticalSectionGuard guard(gCS);
  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it != gRadios.end())
    NGL_LOG("radio", NGL_LOG_ERROR, "the radio '%s' is already registered\n", rURL.GetChars());

  NGL_LOG("radio", NGL_LOG_INFO, "Registering radio '%s'\n", rURL.GetChars());
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

  InitRedis();
  if (gRedis.IsConnected())
  {
    NGL_LOG("radio", NGL_LOG_INFO, "Telling redis that radio '%s' is gone\n", rURL.GetChars());
    nglString r;
    r.Add("radio:").Add(rURL);
    NGL_LOG("radio", NGL_LOG_INFO, "redis DEL '%s'\n", r.GetChars());
    RedisRequest req;
    req.DEL(r);
    RedisReplyType reply = gRedis.SendCommand(req);
    if (reply == eRedisError)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while DEL: %s\n", req.GetError().GetChars());
    }

    nglString server;
    server.Add("server:").Add(mHostname);
    req.SREM(server, rURL);
    if (gRedis.SendCommand(req) == eRedisError)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Redis error while SREM: %s\n", req.GetError().GetChars());
    }
  }

}

Radio* Radio::CreateRadio(const nglString& rURL, const nglString& rHost)
{
  Radio* pRadio = new Radio(rURL, rHost);
  pRadio->Start();
  if (pRadio->IsOnline())
    return pRadio;

  NGL_LOG("radio", NGL_LOG_ERROR, "Radio::CreateRadio unable to create radio\n");
  delete pRadio;
  NGL_LOG("radio", NGL_LOG_INFO, "Radio::CreateRadio return NULL\n");
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

