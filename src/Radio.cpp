
#include "nui.h"
#include "Mp3Parser/Mp3Parser.h"
#include "Radio.h"
#include "HTTPHandler.h"
#include "nuiHTTP.h"
#include "nuiJson.h"

#define IDEAL_BUFFER_SIZE 3.0
#define MAX_BUFFER_SIZE 4.0

#define ENABLE_REDIS_THREADS 1

int Radio::gRedisDB = 3;
nglString Radio::gRedisHost = "127.0.0.1";

std::map<nglString, double> Radio::gReadSetTimeProfile;

///////////////////////////////////////////////////
//class Radio
Radio::Radio(const nglString& rID, const nglString& rHost)
: mID(rID),
  mLive(false),
  mOnline(false),
  mGoOffline(false),
  mCheckClients(false),
  mpParser(NULL),
  mpStream(NULL),
  mBufferDuration(0),
  mpParserPreview(NULL),
  mpStreamPreview(NULL),
  mBufferDurationPreview(0),
  mpSource(NULL),
  mpPreviewSource(NULL),
  mpThread(NULL),
  mLastUpdateTime(0),
  mCS(nglString("RadioCS (").Add(rID).Add(")")),
  mClientListCS(nglString("RadioClientList (").Add(rID).Add(")"))
{
  mSentDataDuration = 0;
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
  SetOnline(true);
  
  NGL_LOG("radio", NGL_LOG_INFO, "Radio::Radio() [%p - %s]", this, mID.GetChars());
}

Radio::~Radio()
{
  NGL_LOG("radio", NGL_LOG_INFO, "Radio::~Radio() [%p - %s]", this, mID.GetChars());
  // tell clients to stop:
  UnregisterRadio(this);
  KillClients();

  delete mpSource;
  delete mpPreviewSource;

  delete mpParser;
  delete mpStream;
  delete mpParserPreview;
  delete mpStreamPreview;

  NGL_LOG("radio", NGL_LOG_INFO, "Radio::~Radio() OK [%p - %s]", this, mID.GetChars());
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
  nglString name;
  name.Add("Radio[").Add(mID).Add("]");

  if (mpPreviewSource)
  {
    SetOnline(mpPreviewSource->IsReadConnected());

    if (mOnline)
    {
      name.Add("Proxy");
      mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStartProxy), name, nglThread::Normal, stacksize);
    }
  }
  else
  {
    SetOnline(true);
    mpThread = new nglThreadDelegate(nuiMakeDelegate(this, &Radio::OnStart), name, nglThread::Normal, stacksize);
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

  NGL_LOG("radio", NGL_LOG_INFO, "[%p - %s] RegisterClient(%p)", this, mID.GetChars(), pClient);
  ClientList& rClients            = highQuality ? mClients : mClientsPreview;
  std::deque<Mp3Chunk*>& rChunks  = highQuality ? mChunks : mChunksPreview;

  {
    nglCriticalSectionGuard guard(mClientListCS);

    SetOnline(true);
    mGoOffline = false;
    pClient->SetName(nglString("RegisterClient ") + pClient->GetURL() + nglString("  "));
    rClients.push_back(pClient);

    pClient->Log(200);

    if (pClient->IsLive())
    {
      NGL_LOG("radio", NGL_LOG_INFO, "Starting to get data from client for radio %s", pClient->GetURL().GetChars());
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
      NGL_LOG("radio", NGL_LOG_INFO, "client %p: set content type audio/mpeg for radio %s", pClient, pClient->GetURL().GetChars());
      pClient->ReplyHeader("Content-Type", "audio/mpeg");
      pClient->ReplyHeader("icy-name", "no name");
      pClient->ReplyHeader("icy-pub", "1");
    }
    else
    {
      NGL_LOG("radio", NGL_LOG_INFO, "client %p: set content type text/plain for radio %s", pClient, pClient->GetURL().GetChars());
      pClient->ReplyHeader("Content-Type", "text/plain");
    }

    pClient->ReplyLine("");

    // Do the streaming:
    NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Do the streaming\n", this, mID.GetChars());
  }

  //NGL_LOG("radio", NGL_LOG_INFO, "Prepare the new client:\n");
  // Fill the buffer:
  {
    nglCriticalSectionGuard guard(mCS);
    for (std::deque<Mp3Chunk*>::const_iterator it = rChunks.begin();
         it != rChunks.end()
         && pClient->IsWriteConnected()
         && pClient->IsReadConnected();
         ++it)
    {
      Mp3Chunk* pChunk = *it;
      pClient->AddChunk(pChunk);
      //NGL_LOG("radio", NGL_LOG_INFO, "Chunk %f\n", pChunk->GetTime());
    }
  }
  
}

void Radio::UnregisterClient(HTTPHandler* pClient)
{
  
  pClient->SetName(nglString("UNRegisterClient ") + pClient->GetURL() + nglString("  "));

  NGL_LOG("radio", NGL_LOG_INFO, "client (%p) is gone for radio [%p - %s]\n", pClient, this, mID.GetChars());
  
  {
    nglCriticalSectionGuard guard(mClientListCS);
    mClients.remove(pClient);
    mClientsPreview.remove(pClient);
    NGL_LOG("radio", NGL_LOG_INFO, "    %d clients left in radio [%p - %s]\n", mClientsPreview.size(), this, mID.GetChars());
    
  }
  
  mCheckClients = true;

  const RadioUser& rUser = pClient->GetUser();
  nglString sessionid;
  sessionid.Add(pClient);
  mpRedisThreadOut->UnregisterListener(mID, sessionid, rUser.uuid);
  
  
}

bool Radio::SetTrack(const Track& rTrack)
{
  nglString p = rTrack.mFileID;
  //nglPath path = "/space/new/medias/song";
  nglPath path = p;

  NGL_LOG("radio", NGL_LOG_INFO, "[%p - %s] SetTrack %s\n", this, mID.GetChars(), path.GetChars());

  nglPath previewPath = GetPreviewPath(path);

//  nglIStream* pStream = path.OpenRead();
//  nglIStream* pStreamPreview = previewPath.OpenRead();
  nglIStream* pStream = gpCache->GetStream(path);
  nglIStream* pStreamPreview = gpCache->GetStream(previewPath);

  if (!pStream || !pStreamPreview)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "SetTrack error (%p, %p)\n", pStream, pStreamPreview);
    delete pStream;
    return false;
  }

  Mp3Parser* pParser = new Mp3Parser(*pStream, false, false);
  Mp3Parser* pParserPreview = new Mp3Parser(*pStreamPreview, false, false);
  bool valid = pParser->GetCurrentFrame().IsValid() && pParserPreview->GetCurrentFrame().IsValid();
  if (!valid)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "SetTrack error 4\n");
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

  if (rTrack.mOffset > 0)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "An offset of %f seconds was requested.", rTrack.mOffset); 
    mpParser->Seek(rTrack.mOffset);
    mpParserPreview->Seek(rTrack.mOffset);
  }

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
  NGL_LOG("radio", NGL_LOG_INFO, "preview path %s\n", previewPath.GetPathName().GetChars());
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
  double duration = pChunk->GetDuration();
  rBufferDuration += duration;
  

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
    NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio::AddChunk Kill client %p\n", this, mID.GetChars(), pClient);
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


Mp3Chunk* Radio::GetChunk(nuiTCPClient* pClient)
{
  int32 offset = 0;
  
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
  nglTime t0;
  if (!mpParser)
  {
    return 0;
  }

  double duration = 0;
  bool nextFrameOK = true;
  bool nextFramePreviewOK = true;
  
  Mp3Chunk* pChunk = mpParser->GetChunk();
  Mp3Chunk* pChunkPreview = mpParserPreview->GetChunk();
  
  nglTime t1;

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
  
  nglTime t2;

  nextFrameOK = mpParser->GoToNextFrame();
  nextFramePreviewOK = mpParserPreview->GoToNextFrame();
  
  nglTime t3;

  if (!pChunk || !nextFramePreviewOK || !nextFrameOK)
  {
    delete mpParser;
    delete mpStream;
    delete mpParserPreview;
    delete mpStreamPreview;
    mpParser = NULL;
    mpStream = NULL;
    mpParserPreview = NULL;
    mpStreamPreview = NULL;

    if (!mOnline)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "Error while getting next song for radio '%s'. Shutting down...\n", mID.GetChars());
      return 0;
    }
  }
  
  nglTime t4;
  {
    double total_duration = t4 - t0;
    double getChunk_duration = t1 - t0;
    double addChunk_duration = t2 - t1;
    double goToNextFrame_duration = t3 - t1;
    double cleanup_duration = t4 - t3;
    
    if (gReadSetTimeProfile.empty())
    {
      gReadSetTimeProfile["count"] = 1;
      gReadSetTimeProfile["total_duration"] = total_duration;
      gReadSetTimeProfile["get_chunk_duration"] = getChunk_duration;
      gReadSetTimeProfile["add_chunk_duration"] = addChunk_duration;
      gReadSetTimeProfile["go_to_next_frame_duration"] = goToNextFrame_duration;
      gReadSetTimeProfile["cleanup_duration"] = cleanup_duration;
    }
    else
    {
      gReadSetTimeProfile["count"]++;
      gReadSetTimeProfile["total_duration"] += total_duration;
      gReadSetTimeProfile["get_chunk_duration"] += getChunk_duration;
      gReadSetTimeProfile["add_chunk_duration"] += addChunk_duration;
      gReadSetTimeProfile["go_to_next_frame_duration"] += goToNextFrame_duration;
      gReadSetTimeProfile["cleanup_duration"] += cleanup_duration;
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

  // Do the actual regular streaming:
  double nexttime = nglTime();
  while (mOnline)
  {
    if (mLive)
    {
      double over = nglTime() - nexttime;
      while (mOnline && mLive && ((mBufferDurationPreview < IDEAL_BUFFER_SIZE) || over >= 0))
      {
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
                NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio offline", this, mID.GetChars());
              }
            }
            else
            {
              NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio broken AND offline", this, mID.GetChars());
              SetOnline(false); //#FIXME Handle HQ Stream: && mpSource->IsReadConnected();
            }
          }
          //NGL_LOG("radio", NGL_LOG_INFO, "buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
        }
        
        if (mCheckClients)
          CheckClients();
      }
    }
    else
    {
      double now = nglTime();
      while (mOnline && ((mBufferDurationPreview < IDEAL_BUFFER_SIZE) || now >= nexttime))
      {
        {
          nglCriticalSectionGuard guard(mCS);
          UpdateRadio();
          double duration = ReadSet(chunk_count_preview, chunk_count);
          nexttime += duration;
          //NGL_LOG("radio", NGL_LOG_INFO, "buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
          
          // #MATDEBUG
          if (mSentDataDuration == 0)
            mStreamingStart = nglTime();
          mSentDataDuration += duration;
          double elapsed = nglTime() - mStreamingStart;
          NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio::OnStart:   elapsed = %lf s  /  sent = %lf s\n", this, mID.GetChars(), elapsed, mSentDataDuration);
          
          if (!mpParser) // we're waiting for the scheduler to send us something to do so let's not tax the CPU for nothing
          {
            NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio::OnStart wait for the scheduler to have a parser\n", this, mID.GetChars());
            nglThread::MsSleep(1);
          }

        }
        
        if (mCheckClients)
          CheckClients();
        
        now = nglTime();
      }
      
      NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] buffer duration = %lf / next time = %lf (now = %lf)\n", this, mID.GetChars(), mBufferDurationPreview, nexttime, (double)now);
    }
    nglThread::MsSleep(10);
  }

  // tell clients to stop:
  KillClients();

  NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] radio is now offline\n", this, mID.GetChars());
  
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
        SetOnline(false);
      }
      //NGL_LOG("radio", NGL_LOG_INFO, "buffer duration: %f / %f\n", mBufferDurationPreview, IDEAL_BUFFER_SIZE);
      
      if (mCheckClients)
        CheckClients();
    }

    nglThread::MsSleep(100);
  }

  NGL_LOG("radio", NGL_LOG_INFO, "radio '%s' is now offline\n", mID.GetChars());  
  delete this;
}
            
void Radio::SetOnline(bool set)
{
  if (mOnline == set)
    return;

  mOnline = set;
  
  if (!mOnline)
    UnregisterRadio(this);
}

bool Radio::CheckClients()
{
  if (!mCheckClients)
    return true;
  
  bool res = true;
  nglCriticalSectionGuard guard(gRadioAndUserListsCS);
  
  if (mClients.empty() && mClientsPreview.empty())
  {
    //  Shutdown radio
    NGL_LOG("radio", NGL_LOG_INFO, "Last client is gone: Shutting down radio [%p - %s]\n", this, mID.GetChars());
    // Now that the scheduler handles the programming of the radio, the stream just can drop the radio when no one listens anymore
    SetOnline(false);
    mGoOffline = true;
  }

  mCheckClients = false;
  return res;
}

void Radio::KillClients()
{
  NGL_LOG("radio", NGL_LOG_INFO, "[%p - %s] Force '%d' clients to stop relaying our data\n", this, mID.GetChars(), mClientsPreview.size() + mClients.size());
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

void Radio::UpdateRadio()
{
  //NGL_LOG("radio", NGL_LOG_ALWAYS, "UpdateRadio()");
  double now = nglTime();
  double t = now - mLastUpdateTime;
  if (mLastUpdateTime == 0)
  {
    // We're starting up, let's pretend...
    //NGL_LOG("radio", NGL_LOG_INFO, "first time in update radio");
    if (!mTracks.empty())
      t = mTracks.front().mDelay;
    else
      t = 0;
  }
  mLastUpdateTime = now;
  //NGL_LOG("radio", NGL_LOG_ALWAYS, "UpdateRadio for %f seconds (%d tracks)", t, mTracks.size());

  std::list<Track>::iterator it = mTracks.begin();
  std::list<Track>::iterator end = mTracks.end();

  for ( ; it != end; ++it)
  {
    Track& rTrack(*it);
    rTrack.mDelay -= t;
  }

  while (!mTracks.empty() && mTracks.front().mDelay <= 0)
  {
    Track& track(mTracks.front());
    NGL_LOG("radio", NGL_LOG_INFO, "Start new track: %s", track.mFileID.GetChars());
    SetTrack(track);
    mTracks.pop_front();
    //NGL_LOG("radio", NGL_LOG_INFO, "Started '%s' from static track list\n", p.GetChars());
  }
}

nglCriticalSection Radio::gRadioAndUserListsCS("Radio::gRadioAndUserListsCS");
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
    nglCriticalSectionGuard guard(gRadioAndUserListsCS);
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
    nglCriticalSectionGuard guard(gRadioAndUserListsCS);
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
  NGL_LOG("radio", NGL_LOG_INFO, "Getting radio %s (client %p)\n", rURL.GetChars(), pClient);
  Radio* pRadio = NULL;
  
  nglCriticalSectionGuard guard(gRadioAndUserListsCS);
  
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
      
      NGL_LOG("radio", NGL_LOG_INFO, "Get radio: FAILED [%s] (client %p)\n", rURL.GetChars(), pClient);
      return NULL;
    }
    DelEvent(rURL);
    
    it = gRadios.find(rURL);
    if (it == gRadios.end())
    {
      // No proxy radio has been created... so it should be ok
      // Create the radio!
      pRadio = CreateRadio(rURL, nglString::Null);
      NGL_LOG("radio", NGL_LOG_INFO, "Get radio: radio created [%p - %s] (client %p)\n", pRadio, rURL.GetChars(), pClient);
    }
    else
    {
      pRadio = it->second;
    }
  }
  else
  {
    pRadio = it->second;
  }
  
  if (pRadio)
  {
    pRadio->RegisterClient(pClient, HQ);
  }
  
  return pRadio;
}

void Radio::RegisterRadio(const nglString& rURL, Radio* pRadio)
{
  nglCriticalSectionGuard guard(gRadioAndUserListsCS);
  RadioMap::const_iterator it = gRadios.find(rURL);
  if (it != gRadios.end())
    NGL_LOG("radio", NGL_LOG_ERROR, "the radio '%s' is already registered\n", rURL.GetChars());

  gRadios[rURL] = pRadio;
}

void Radio::UnregisterRadio(Radio* pRadio)
{
  nglString url = pRadio->mID;
  NGL_LOG("radio", NGL_LOG_INFO, "Unregistering radio [%p - %s]\n", pRadio, url.GetChars());
  
  nglCriticalSectionGuard guard(gRadioAndUserListsCS);
  
  RadioMap::const_iterator it = gRadios.find(url);
  if (it == gRadios.end())
  {
    NGL_LOG("radio", NGL_LOG_INFO, "Unregistering radio [%p - %s] radio was never registered\n", pRadio, url.GetChars());
    return;
  }
  if (it->second != pRadio)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "Cancel radio unregistering '%s' (%p requested, %p in map)\n", url.GetChars(), pRadio, it->second);
    return;
  }
  
  gRadios.erase(url);
  mpRedisThreadOut->StopRadio(url);
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
  return mLive;
}

bool Radio::IsOnline() const
{
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
  else if (type == "radio_unkown")
  {
    nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
    NGL_LOG("radio", NGL_LOG_INFO, "Redis: radio_unkown %s\n", uuid.GetChars());

    //SignallEvent(uuid);
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
      nglCriticalSectionGuard g(gRadioAndUserListsCS);
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
    int uuid = msg.get("user_id", nuiJson::Value()).asUInt();
    bool hd = msg.get("hd", nuiJson::Value()).asBool();
    nglString auth_token = msg.get("auth_token", nuiJson::Value()).asString();
    nglString username = msg.get("username", nuiJson::Value()).asString();
    nglString api_key = msg.get("api_key", nuiJson::Value()).asString();

    NGL_LOG("radio", NGL_LOG_INFO, "Redis: user_authentication '%d' (%s) (%s) (%s) (%s)\n", uuid, hd?"HD":"SD", auth_token.GetChars(), username.GetChars(), api_key.GetChars());

    nglString id("auth_");
    id.Add(auth_token).Add(username).Add(api_key);

    {
      nglCriticalSectionGuard g(gRadioAndUserListsCS);
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
  mpRedisThreadIn = new RedisThread(nuiNetworkHost(gRedisHost, 6379, nuiNetworkHost::eTCP), RedisThread::MessagePump, mHostname, gRedisDB);
  mpRedisThreadIn->SetMessageHandler(Radio::HandleRedisMessage);
  mpRedisThreadIn->Start();

  mpRedisThreadOut = new RedisThread(nuiNetworkHost(gRedisHost, 6379, nuiNetworkHost::eTCP), RedisThread::Broadcaster, mHostname, gRedisDB);
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
nglCriticalSection Radio::gEventCS("Radio::gEvents");

nglSyncEvent* Radio::AddEvent(const nglString& rName)
{
  nglCriticalSectionGuard g(gEventCS);

  EventMap::iterator it = gEvents.find(rName);
  if (it != gEvents.end())
  {
    EventPair& rEvent(it->second);
    rEvent.second++;
    return rEvent.first;
  }

  nglSyncEvent* pEvent = new nglSyncEvent();
  EventPair Event(pEvent, 1);
  gEvents[rName] = Event;
  //NGL_LOG("radio", NGL_LOG_INFO, "add event: %s - %p", rName.GetChars(), pEvent);
  return pEvent;
}

void Radio::DelEvent(const nglString& rName)
{
  nglCriticalSectionGuard g(gEventCS);

  EventMap::iterator it = gEvents.find(rName);
  NGL_ASSERT(it != gEvents.end());

  EventPair& rEvent(it->second);
  rEvent.second--;
  //NGL_LOG("radio", NGL_LOG_INFO, "del event: %s - %p", rName.GetChars(), rEvent.first);

  if (!rEvent.second)
  {
    //NGL_LOG("radio", NGL_LOG_INFO, "kill event: %s - %p", rName.GetChars(), rEvent.first);
    nglSyncEvent* pEvent = rEvent.first;
    delete pEvent;
    gEvents.erase(it);
  }
}

void Radio::SignallEvent(const nglString& rName)
{
  nglCriticalSectionGuard g(gEventCS);

  EventMap::iterator it = gEvents.find(rName);
  if (it != gEvents.end())
  {
    EventPair& rEvent(it->second);
    nglSyncEvent* pEvent = rEvent.first;
    //NGL_LOG("radio", NGL_LOG_INFO, "signal event: %s - %p", rName.GetChars(), pEvent);
    pEvent->Set();
  }
}

bool compare_track(const Track& rLeft, const Track& rRight);

bool compare_track(const Track& rLeft, const Track& rRight)
{
  return rLeft.mDelay < rRight.mDelay;
}

void Radio::PlayTrack(const nglString& rFilename, double delay, double offset, double fade)
{
  nglCriticalSectionGuard g(mCS);
  NGL_LOG("radio", NGL_LOG_INFO, "Add track %s to radio %s", rFilename.GetChars(), mID.GetChars());
  Track track(rFilename, delay, offset, fade);
  mTracks.push_back(track);
  mTracks.sort(compare_track);
}

void Radio::SetRedisDB(const nglString& rHost, int db)
{
  gRedisDB = db;
  gRedisHost = rHost;
}

FileCache* Radio::gpCache = NULL;

void Radio::InitCache(int64 MaxSizeBytes, const nglPath& rSource, const nglPath& rDestination)
{
  gpCache = new FileCache(MaxSizeBytes, rSource, rDestination);
}

void Radio::ReleaseCache()
{
  delete gpCache;
}

const FileCache& Radio::GetCache()
{
  return *gpCache;
}

void Radio::Dump(nglString& rDump)
{
  nglCriticalSectionGuard guard(mCS);

  rDump.Add(mID);
  rDump.Add(" [ ").Add(mBufferDuration).Add(" / ").Add(mBufferDurationPreview).Add("] ");

  if (mLive)
    rDump.Add("Live ");

  if (mOnline)
    rDump.Add("Online ");

  if (mGoOffline)
    rDump.Add("GoOffline ");

  if (mpParser)
    rDump.Add("ParserOK ");

  if (mpParserPreview)
    rDump.Add("ParserPreviewOK ");

  rDump.AddNewLine();
  {
    nglCriticalSectionGuard guard(mClientListCS);

    {
      ClientList::iterator it = mClients.begin();
      ClientList::iterator end = mClients.end();

      while (it != end)
      {
        rDump.Add("\tHQ").Add((*it)->GetDesc()).AddNewLine();
        ++it;
      }
    }

    {
      ClientList::iterator it = mClientsPreview.begin();
      ClientList::iterator end = mClientsPreview.end();

      while (it != end)
      {
        rDump.Add("\tlq").Add((*it)->GetDesc()).AddNewLine();
        ++it;
      }
    }
  }

  rDump.AddNewLine();
  rDump.AddNewLine();
}

void Radio::DumpAll(nglString& rDump)
{
  nglCriticalSectionGuard guard(gRadioAndUserListsCS);

  rDump.Add((int64)gRadios.size()).Add(" Radios").AddNewLine();
  rDump.AddNewLine();

  RadioMap::const_iterator it = gRadios.begin();
  RadioMap::const_iterator end = gRadios.end();

  while (it != end)
  {
    it->second->Dump(rDump);
    rDump.AddNewLine();

    ++it;
  }

}

void Radio::DumpTimeProfile(nglString& rDump)
{
  rDump.Add("TIME PROFILE:").AddNewLine().AddNewLine();
  
  
  
  rDump.Add("Radio::ReadSet").AddNewLine();
  
  if (gReadSetTimeProfile.empty())
  {
    rDump.Add("???");
  }
  else
  {
    double count = gReadSetTimeProfile["count"];
    double total = gReadSetTimeProfile["total_duration"] / count;
    double getChunk = gReadSetTimeProfile["get_chunk_duration"] / count;
    double addChunk = gReadSetTimeProfile["add_chunk_duration"] / count;
    double gotToNextFrame = gReadSetTimeProfile["go_to_next_frame_duration"] / count;
    double cleanup = gReadSetTimeProfile["cleanup_duration"] / count;
    rDump.Add("\ttotal            : %lf seconds", total).AddNewLine();
    rDump.Add("\tget chunk        : %lf seconds", getChunk).AddNewLine();
    rDump.Add("\tadd chunk        : %lf seconds", addChunk).AddNewLine();
    rDump.Add("\tgo to next frame : %lf seconds", gotToNextFrame).AddNewLine();
    rDump.Add("\tcleanup          : %lf seconds", cleanup).AddNewLine();
  }
  
  
}


