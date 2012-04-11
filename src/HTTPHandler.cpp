
#include "nui.h"
#include "HTTPHandler.h"
#include "Radio.h"
#include "nuiHTTP.h"
#include "nuiNetworkHost.h"

#define SUBSCRIPTION_NONE "none"
#define SUBSCRIPTION_PREMIUM "premium"

///////////////////////////////////////////////////
//class HTTPHandler : public nuiHTTPHandler
HTTPHandler::HTTPHandler(nuiTCPClient* pClient)
: nuiHTTPHandler(pClient), mLive(true)
{
#if TEMPLATE_TEST
  mpTemplate = new nuiStringTemplate("<html><body><br>This template is a test<br>ClassName: {{Class}}<br>ObjectName: {{Name}}<br>{%for elem in array%}{{elem}}<br>{%end%}Is it ok?<br></body></html>");
#else
  mpTemplate = NULL;
#endif
}

HTTPHandler::~HTTPHandler()
{
  if (mpTemplate)
    delete mpTemplate;
}

bool HTTPHandler::OnMethod(const nglString& rValue)
{
  return true;
}

bool HTTPHandler::OnURL(const nglString& rValue)
{
  if (mURL == "/favicon.ico")
  {
    nglString str;
    str.Format("Unable to find %s on this server", mURL.GetChars());
    ReplyError(404, str);
    return false; // We don't have a favicon right now...
  }
  else if (mURL == "/ping")
  {
    ReplyLine("HTTP/1.1 200 OK\n");
    ReplyLine("All systems nominal");
    return false; // We don't have a favicon right now...
  }
  else if ((mURL.GetLeft(4) == "http") || (mURL.GetLeft(5) == "/http"))
  {
    ReplyError(404, "Not found");
    return false;
  }

  return true;
}

bool HTTPHandler::OnProtocol(const nglString& rValue, const nglString rVersion)
{
  return true;
}

bool HTTPHandler::OnHeader(const nglString& rKey, const nglString& rValue)
{
  if (rKey == "Cookie")
  {
    std::vector<nglString> cookies;
    rValue.Tokenize(cookies, "; ");
    for (uint32 i = 0; i < cookies.size(); i++)
    {
      nglString cookie = cookies[i];
      std::vector<nglString> tokens;
      cookie.Tokenize(tokens, "=");
      if (tokens.size() == 2)
      {
        if (tokens[0] == "username")
        {
          mUsername = tokens[1];
        }
        else if (tokens[0] == "api_key")
        {
          mApiKey = tokens[1];
        }
      }
    }

    if (mUsername.IsEmpty() || mApiKey.IsEmpty())
    {
      // username or api_key is not specified => anonymous user
      mUsername = nglString::Empty;
      mApiKey = nglString::Empty;
    }
  }
  return true;
}

bool HTTPHandler::SendFromTemplate(const nglString& rString, nuiObject* pObject)
{
  return mpClient->Send(rString);
}

uint32 FakeGetter(uint32 i)
{
  return i;
}

void FakeSetter(uint32 i, uint32 v)
{
}

uint32 FakeRange(uint32 i)
{
  return 10;
}

bool HTTPHandler::OnBodyStart()
{
#if TEMPLATE_TEST
  if (mURL == "/")
  {
    // Reply + Headers:
    ReplyLine("HTTP/1.1 200 OK");
    //ReplyHeader("Cache-Control", "no-cache");
    ReplyHeader("Content-Type", "text/html");
    ReplyHeader("Server", "Yastream 1.0.0");
    ReplyLine("");

    nuiObject* obj = new nuiObject();
    nuiAttributeBase* pAttrib = new nuiAttribute<uint32>("array", nuiUnitNone, FakeGetter, FakeSetter, FakeRange);
    obj->AddInstanceAttribute("array", pAttrib);
    mpTemplate->Generate(obj, nuiMakeDelegate(this, &HTTPHandler::SendFromTemplate));

    return false;
  }
#endif

  if (mURL.GetLength() > 40)
  {
    nglString str;
    str.Format("Unable to find %s on this server", mURL.GetChars());
    ReplyError(404, str);
    return false;
  }

  std::vector<nglString> tokens;
  mURL.Tokenize(tokens, "/");

  if (tokens.size() < 1)
    return false;

  mRadioID = tokens[0];
  bool hq = false;
  if (tokens.size() > 1)
  {
    if (tokens[1] == "hq")
    {
      // check if the user is allowed to play high quality stream
      nglString url;
      url.Format("https://api.yasound.com/api/v1/subscription/?username=%s&api_key=%s", mUsername.GetChars(), mApiKey.GetChars());
      nuiHTTPRequest request(url);
      nuiHTTPResponse* pResponse = request.SendRequest();

      if (pResponse->GetStatusCode() == 200)
      {
        nglString subscription = pResponse->GetBodyStr();
        hq = (subscription == SUBSCRIPTION_PREMIUM);

        if (!hq)
          NGL_LOG("radio", NGL_LOG_WARNING, "user '%s' requested high quality but did not subscribe!\n", mUsername.GetChars());
      }
    }
  }

  // Find the Radio:
  Radio* pRadio = Radio::GetRadio(mRadioID);
  if (!pRadio || !pRadio->IsLive())
  {
    NGL_LOG("radio", NGL_LOG_ERROR, "HTTPHandler::Start unable to create radio %s\n", mRadioID.GetChars());
    nglString str;
    str.Format("Unable to find %s on this server", mURL.GetChars());
    ReplyError(404, str);

    return false;
  }

  Log(200);


  pRadio->RegisterClient(this, hq);
  SendListenStatus(eStartListen);

  // Reply + Headers:
  ReplyLine("HTTP/1.1 200 OK");
  ReplyHeader("Cache-Control", "no-cache");
  ReplyHeader("Content-Type", "audio/mpeg");
  ReplyHeader("Server", "Yastream 1.0.0");
  ReplyHeader("icy-name", "no name");
  ReplyHeader("icy-pub", "1");
  ReplyLine("");

  // Do the streaming:

  while (mLive && mpClient->IsWriteConnected())
  {
    // GetNext chunk:
    Mp3Chunk* pChunk = NULL;

    pChunk = GetNextChunk();
    int cnt = 0;
    while (!pChunk && mLive && mpClient->IsWriteConnected())
    {
      cnt++;
      nglThread::MsSleep(100);
      pChunk = GetNextChunk();
    }
//    if (cnt)
//      NGL_LOG("radio", NGL_LOG_INFO, "%d", cnt);
    //NGL_LOG("radio", NGL_LOG_INFO, "^");
    //NGL_LOG("radio", NGL_LOG_INFO, "%d", cnt);
    if (pChunk)
    {
      mpClient->Send(&pChunk->GetData()[0], pChunk->GetData().size());

      pChunk->Release();
    }
  }

  SendListenStatus(eStopListen);

  NGL_LOG("radio", NGL_LOG_INFO, "Client disconnecting from %s\n", mRadioID.GetChars());
  pRadio->UnregisterClient(this);

  NGL_LOG("radio", NGL_LOG_INFO, "HTTPHandler::OnBodyStart DoneOK");
  return false;
}

bool HTTPHandler::OnBodyData(const std::vector<uint8>& rData)
{
  return true;
}

void HTTPHandler::OnBodyEnd()
{
}

void HTTPHandler::AddChunk(Mp3Chunk* pChunk)
{
  nglCriticalSectionGuard guard(mCS);
  //NGL_LOG("radio", NGL_LOG_INFO, "handle id = %d\n", pChunk->GetId());
  pChunk->Acquire();
  mChunks.push_back(pChunk);
}

Mp3Chunk* HTTPHandler::GetNextChunk()
{
  nglCriticalSectionGuard guard(mCS);

  if (mChunks.empty())
    return NULL;

  Mp3Chunk* pChunk = mChunks.front();
  mChunks.pop_front();

  return pChunk;
}

void HTTPHandler::SendListenStatus(ListenStatus status)
{
  NGL_LOG("radio", NGL_LOG_INFO, "SendListenStatus");
  nglString statusStr;
  if (status == eStartListen)
    statusStr = "start_listening";
  else if (status == eStopListen)
    statusStr = "stop_listening";

  nglString params;
  if (!mUsername.IsEmpty() && !mApiKey.IsEmpty())
    params.CFormat("?username=%s&api_key=%s", mUsername.GetChars(), mApiKey.GetChars());
  else
  {
    nglString address;
    nuiNetworkHost client(0, 0, nuiNetworkHost::eTCP);
    bool res = mpClient->GetDistantHost(client);
    if (res)
    {
      uint32 ip = client.GetIP();
      int port = client.GetPort();
      address.CFormat("%d:%d", ip, port);
    }
    params.CFormat("?address=%s", address.GetChars());
  }

  if (status == eStartListen)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "SendListenStatus Start Listen");
    mStartTime = nglTime();
  }
  else if (status == eStopListen)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "SendListenStatus Stop Listen");
    nglTime now;
    nglTime duration = now - mStartTime;
    int32 seconds = ToBelow(duration.GetValue());
    nglString durationParam;
    durationParam.CFormat("&listening_duration=%d", seconds);
    params += durationParam;
  }

  nglString url;
  url.CFormat("https://api.yasound.com/api/v1/radio/%s/%s/%s", mRadioID.GetChars(), statusStr.GetChars(), params.GetChars());
  nuiHTTPRequest request(url, "POST");
  
  NGL_LOG("radio", NGL_LOG_INFO, "SendListenStatus SendRequest");

  nuiHTTPResponse* pResponse = request.SendRequest();

/*
  nglString log;
  log = "\n(url:";
  log += url;
  log += ") response = ";
  log += pResponse->GetStatusCode();
  log += " - ";
  log += pResponse->GetBodyStr();
  log += " ###END###";

  NGL_LOG("radio", NGL_LOG_INFO, "SendListenStatus log length : %d\n", log.GetLength());

  nglPath logPath = nglPath(ePathCurrent);
  logPath += "/log/ListenStatus.log";
  nglIOFile* pFile = new nglIOFile(logPath, eOFileAppend);
  pFile->WriteText(log);
  delete pFile;
  */

  NGL_LOG("radio", NGL_LOG_INFO, "SendListenStatus DoneOK");
}

void HTTPHandler::GoOffline()
{
  NGL_LOG("radio", NGL_LOG_INFO, "HTTPHandler::GoOffline");
  mLive = false;
  mpClient->Close();
  NGL_LOG("radio", NGL_LOG_INFO, "HTTPHandler::GoOffline OK");
}

