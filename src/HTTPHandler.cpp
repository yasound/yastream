
#include "nui.h"
#include "HTTPHandler.h"
#include "Radio.h"
#include "nuiHTTP.h"
#include "nuiNetworkHost.h"
#include "nuiJson.h"
#include "nglThreadChecker.h"

#define SUBSCRIPTION_NONE "none"
#define SUBSCRIPTION_PREMIUM "premium"

#define TEMPLATE_TEST 0

void HTTPHandler::SetPool(nuiSocketPool* pPool)
{
  gmpPool = pPool;
}

nuiSocketPool* HTTPHandler::gmpPool = NULL;

///////////////////////////////////////////////////
//class HTTPHandler : public nuiHTTPHandler
HTTPHandler::HTTPHandler(nuiSocket::SocketType s)
: nuiHTTPHandler(s), mOnline(true), mLive(false), mpRadio(NULL), mCS(nglString("HTTPHandler(").Add(s).Add(")"))
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

  if (mpRadio)
  {
//   if (mLive)
//     pRadio->SetNetworkSource(NULL, NULL);

    NGL_LOG("radio", NGL_LOG_INFO, "Client (%p) disconnecting from %s\n", this, mRadioID.GetChars());
    mpRadio->UnregisterClient(this);
  }
}

bool HTTPHandler::OnMethod(const nglString& rValue)
{
  return true;
}

bool HTTPHandler::OnURL(const nglString& rValue)
{
  SetName(nglString("OnURL ") + mURL);
  NGL_LOG("radio", NGL_LOG_INFO, " %p HTTPHandler::OnURL(%s)", this, rValue.GetChars());
  if (mURL == "/favicon.ico")
  {
    nglString str;
    str.Format("Unable to find %s on this server", mURL.GetChars());
    return ReplyError(404, str); // We don't have a favicon right now...
  }
  else if (mURL == "/ping")
  {
    ReplyLine("HTTP/1.1 200 OK");
    ReplyHeader("Content-Type", "text/plain");
    ReplyLine("");

    nglString str;

    nglString report;
    nuiSocket::GetStatusReport(report);

    str.CFormat("All systems nominal\n\n");
    str.Add(report.GetChars());
    str.AddNewLine();

    Radio::GetCache().DumpStats(str);

    ReplyLine(str);

    return ReplyAndClose();
  }
  else if (mURL == "/threads")
  {
    ReplyLine("HTTP/1.1 200 OK");
    ReplyHeader("Content-Type", "text/plain");
    ReplyLine("");

    nglString report = nglThreadChecker::Dump();

    ReplyLine(report);

    return ReplyAndClose();
  }
  else if (mURL == "/radios")
  {
    ReplyLine("HTTP/1.1 200 OK");
    ReplyHeader("Content-Type", "text/plain");
    ReplyLine("");

    nglString report;
    Radio::DumpAll(report);

    ReplyLine(report);

    return ReplyAndClose();
  }
  else if (mURL == "/cache")
  {
    ReplyLine("HTTP/1.1 200 OK");
    ReplyHeader("Content-Type", "text/plain");
    ReplyLine("");

    nglString str;

    nglString report;
    nuiSocket::GetStatusReport(report);

    Radio::GetCache().Dump(str);
    ReplyLine(str);

    return ReplyAndClose();
  }
  else if ((mURL.GetLeft(4) == "http") || (mURL.GetLeft(5) == "/http"))
  {
    ReplyError(404, "Not found");
    SetAutoDelete(true);
    return ReplyAndClose();
  }

  SetName(nglString("OnURLOK ") + mURL);
  return true;
}

bool HTTPHandler::OnProtocol(const nglString& rValue, const nglString rVersion)
{
  return true;
}

bool HTTPHandler::OnHeader(const nglString& rKey, const nglString& rValue)
{
  SetName(nglString("OnHeader ") + mURL);
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
  return BufferedSend(rString);
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
  SetName(nglString("OnBodyStart ") + mURL);

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

    return ReplyAndClose();
  }
#endif

  if (mURL.GetLength() > 100)
  {
    nglString str;
    str.Format("Unable to find %s on this server", mURL.GetChars());
    ReplyError(404, str);
    return ReplyAndClose();
  }

  std::vector<nglString> tokens;
  mURL.Tokenize(tokens, "/");

  if (tokens.size() < 1)
  {
    SetName(nglString("OnBodyStart TokenError ") + mURL);
    return false;
  }

  mRadioID = tokens[0];

  // Parse parameters:
  std::map<nglString, nglString> params;
  if (tokens.size() >= 2)
  {
    nglString args = tokens[1];

    NGL_LOG("radio", NGL_LOG_INFO, "url = %s, args: %s\n", mURL.GetChars(), args.GetChars());

    if (args[0] == '?')
    {
      // Parse arguments:
      args.DeleteLeft(1); //  remove '?'

      std::vector<nglString> arguments;
      args.Tokenize(arguments, '&');
      for (int i = 0; i < arguments.size(); i++)
      {
        nglString arg = arguments[i];

        int next = arg.Find('=');
        if (next > 0)
        {
          nglString name = arg.Extract(0, next);
          next++;

          nglString value = arg.Extract(next);

          name.ToLower();
          params[name] = value;

          NGL_LOG("radio", NGL_LOG_INFO, "\turl param: %s = %s\n", name.GetChars(), value.GetChars());
        }
      }
    }
  }

  bool hq = false;

  if (tokens.size() > 1)
  {
    std::map<nglString, nglString>::const_iterator it = params.find("token");

    if (it != params.end())
    {
      nglString token = it->second;
      // check if the user is allowed to play high quality stream
      Radio::GetUser(token, mUser);
    }
    else
    {
      // check with old method if the user is allowed to play high quality stream
      Radio::GetUser(mUsername, mApiKey, mUser);
    }

    it = params.find("hd");

    if (it != params.end() && it->second == "1")
    {
      // Check with new method (temp token):

      hq = mUser.hd;
    }
  }
//   if (!hq)
//     NGL_LOG("radio", NGL_LOG_WARNING, "Requesting low quality stream\n");


  if (GetMethod() == "POST")
  {
    NGL_OUT("radio", NGL_LOG_INFO, "This is a live stream %s", GetURL().GetChars());
    mLive = true;
  }

// Find the Radio:
  mpRadio = Radio::GetRadio(mRadioID, this, hq);
  if (!mpRadio || !mpRadio->IsOnline())
  {
    NGL_LOG("radio", NGL_LOG_ERROR, " %p HTTPHandler::Start unable to create radio %s\n", this, mRadioID.GetChars());
    nglString str;
    str.Format("Unable to find %s on this server", mURL.GetChars());
    ReplyError(404, str);
    mpRadio = NULL;
    SetName(nglString("OnBodyStart RadioError ") + mURL);
    return ReplyAndClose();
  }



  //SetName(nglString("OnBodyStart OK ") + mURL);
  
  if (!IsReadConnected() || !IsWriteConnected())
  {
    NGL_LOG("radio", NGL_LOG_INFO, "%p HTTPHandler::OnBodyStart Died in init", this);
    return ReplyAndClose();
  }
  

  return true;
}

bool HTTPHandler::IsLive() const
{
  return mLive;
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
  nglTime t0;
  nglCriticalSectionGuard guard(mCS);
  nglTime t1;
  //NGL_LOG("radio", NGL_LOG_INFO, "handle id = %d\n", pChunk->GetId());
  //pChunk->Acquire();
  BufferedSend(&pChunk->GetData()[0], pChunk->GetData().size(), false);
  nglTime t2;
  if (mOut.GetSize() > 3600 * pChunk->GetData().size())
  {
    // more than 30 frames? Kill!!!
    NGL_LOG("radio", NGL_LOG_ERROR, "Killing client %p (%d bytes stalled)", this, pChunk->GetData().size());
    Close();
  }
  nglTime t3;
  //mChunks.push_back(pChunk);
  
  double t = t3 - t0;
  NGL_LOG("radio", NGL_LOG_ERROR, "%p HTTPHandler::AddChunk: TOTAL  : %lf seconds\n", this, t);
  t = t1 - t0;
  NGL_LOG("radio", NGL_LOG_ERROR, "%p HTTPHandler::AddChunk: step 0 : %lf seconds\n", this, t);
  t = t2 - t1;
  NGL_LOG("radio", NGL_LOG_ERROR, "%p HTTPHandler::AddChunk: step 1 : %lf seconds\n", this, t);
  t = t3 - t2;
  NGL_LOG("radio", NGL_LOG_ERROR, "%p HTTPHandler::AddChunk: step 2 : %lf seconds\n", this, t);
//  t = t4 - t3;
//  NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio::AddChunk (preview = %d): step 3 : %lf seconds\n", this, mID.GetChars(), previewMode, t);
//  t = t5 - t4;
//  NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio::AddChunk (preview = %d): step 4 : %lf seconds\n", this, mID.GetChars(), previewMode, t);
//  t = t6 - t5;
//  NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio::AddChunk (preview = %d): step 5 : %lf seconds\n", this, mID.GetChars(), previewMode, t);
//  t = t7 - t6;
//  NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio::AddChunk (preview = %d): step 6 : %lf seconds\n", this, mID.GetChars(), previewMode, t);
//  t = t8 - t7;
//  NGL_LOG("radio", NGL_LOG_ERROR, "[%p - %s] Radio::AddChunk (preview = %d): step 7 : %lf seconds\n", this, mID.GetChars(), previewMode, t);
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

void HTTPHandler::GoOffline()
{
  mCS.Lock();
  
  mOnline = false;
  Close();

  mCS.Unlock();
  delete this;
}

void HTTPHandler::OnWriteClosed()
{
  mCS.Lock();
  
  nuiTCPClient::OnWriteClosed();
  
  mCS.Unlock();
  delete this;
}

const RadioUser& HTTPHandler::GetUser() const
{
  return mUser;
}

