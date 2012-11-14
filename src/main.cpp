
#include "nui.h"
#include "nuiInit.h"
#include "nuiHTTPServer.h"
#include "nuiHTTP.h"
#include "HTTPHandler.h"
#include "Radio.h"
#include <signal.h>

#include "RedisClient.h"

#include <syslog.h>

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <cxxabi.h>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>

#include "nuiJson.h"

// #undef NGL_OUT
// #define NGL_OUT printf

#define ENABLE_REDIS_THREADS 0


int port = 8000;
nglString hostname = "0.0.0.0";
nglString appurl = "https://api.yasound.com";
nglPath datapath = "/data/glusterfs-storage/replica2all/song/";
nglPath CachePath = "/data/cache/";
//int64 CacheSize = 1024 * 1024 * 1024 * 1; // 1 Gb
int64 CacheSize = 1024 * 1024 * 10; // 10Mb
nglString redishost = "127.0.0.1";
int redisport = 6379;
int redisdb = 1;
nglString bindhost = "0.0.0.0";
bool flushall = false;


nuiSocketPool* pMainPool;

int merde = 0;

nuiHTTPHandler* HandlerDelegate(nuiSocket::SocketType sock);
nuiHTTPHandler* HandlerDelegate(nuiSocket::SocketType sock)
{
  merde++;
  NGL_LOG("radio", NGL_LOG_INFO, "http accept count: %d   s: %d", merde, sock);
  HTTPHandler* pClient = new HTTPHandler(sock);
  nuiNetworkHost source(0, 0, nuiNetworkHost::eTCP);
  nuiNetworkHost dest(0, 0, nuiNetworkHost::eTCP);
  pClient->GetLocalHost(dest);
  pClient->GetDistantHost(source);
  uint32 S = source.GetIP();
  uint32 D = dest.GetIP();
  uint8* s = (uint8*)&S;
  uint8* d = (uint8*)&D;

  if (0x6f7f284e == S || 0x727f284e == S || (!S && !D))
  {
    // This is the load balancer!!!
    delete pClient;
    return NULL;
  }

  NGL_LOG("yastream", NGL_LOG_INFO, "connection from %d.%d.%d.%d:%d to %d.%d.%d.%d:%d (0x%08x -> 0x%08x)\n",
                                    s[0], s[1], s[2], s[3], source.GetPort(),
                                    d[0], d[1], d[2], d[3], dest.GetPort(), S, D);

  return pClient;
}

void SigPipeSink(int signal)
{
  // Ignore...
  //NGL_LOG("radio", NGL_LOG_INFO, "SigPipe!\n");
}

void DumpStackTrace()
{
  void * array[25];
  int nSize = backtrace(array, 25);
  char ** symbols = backtrace_symbols(array, nSize);

  for (int i = 0; i < nSize; i++)
  {
    int status;
    char *realname;
    std::string current = symbols[i];
    size_t start = current.find("(");
    size_t end = current.find("+");
    realname = NULL;
    if (start != std::string::npos && end != std::string::npos)
    {
      std::string symbol = current.substr(start+1, end-start-1);
      realname = abi::__cxa_demangle(symbol.c_str(), 0, 0, &status);
    }
    if (realname != NULL)
      syslog(LOG_ERR, "[%d] %s (%p)\n", i, realname, array[i]);
    else
      syslog(LOG_ERR, "[%d] %s (%p)\n", i, symbols[i], array[i]);
    free(realname);
  }

  free(symbols);
}

void sig_handler(int sig)
{
  syslog(LOG_ERR, "Crash\n");
  DumpStackTrace();

  //signal(sig, &sig_handler);
  exit(-1);
}




//    kill(0, SIGSEGV);

class SyslogConsole : public nglConsole
{
public:
  SyslogConsole (bool IsVisible = false)
  {
    DumpStackTrace();
  }

  virtual ~SyslogConsole()
  {
  }


  virtual void OnOutput (const nglString& rText)
  {
    syslog(LOG_ALERT, "%s", rText.GetChars());
    //printf("%s", rText.GetChars());
  }

  virtual void OnInput  (const nglString& rLine)
  {
    // No input handled...
  }
};

class HTTPServer : public nuiHTTPServer
{
public:
  HTTPServer()
  {
    SetName("HTTP Streaming server");
  }

  void OnCanRead()
  {
    nuiTCPClient* pClient = Accept();
    if (pClient)
    {
      pClient->SetNonBlocking(true);
      pClient->SetMaxIdleTime(10);
      //pMainPool->Add(pClient, nuiSocketPool::eStateChange);
      pClient->SetAutoPool(pMainPool);
    }
  }
};

class RedisThread : public nglThread
{
public:
  enum Mode
  {
    MessagePump,
    Broadcaster
  };

  RedisThread(const nuiNetworkHost& rHost, Mode mode)
  : mHost(rHost), mMode(mode)
  {
    mpClient = new RedisClient();
    mOnline = true;
  }

  virtual ~RedisThread()
  {
    delete mpClient;
  }

  void Stop()
  {
    mOnline = false;
    mpClient->Disconnect();
  }

  void OnStart()
  {
    switch (mMode)
    {
      case MessagePump:
        PumpMessages();
        break;
      case Broadcaster:
        Broadcast();
        break;
    }
  }

  void Broadcast()
  {
    while (mOnline)
    {
      nuiNotification* pNotif = mMessageQueue.Get(1);
      if (pNotif)
      {
        if (pNotif->GetName() == "RedisRequest")
        {
          nuiToken<RedisRequest>* pToken = dynamic_cast<nuiToken<RedisRequest>*>(pNotif->GetToken());
          if (pToken)
          {
            RedisRequest Request = pToken->Token;

            int t = 1;
            // Make sure we're connected to redis:
            while  (mOnline && !mpClient->IsConnected())
            {
              NGL_LOG("radio", NGL_LOG_INFO, "Redis message pump connection\n");
              if (mpClient->Connect(mHost) && mpClient->IsConnected())
              {
                NGL_LOG("radio", NGL_LOG_INFO, "Redis message broadcaster connected\n");
              }
              else
              {
                nglThread::MsSleep(t);
                t = MIN(10000, t * 2);
              }
            }

            // Send request:
            mpClient->SendCommand(Request);
          }
        }
        pNotif->Release();
      }
    }
  }

  void PumpMessages()
  {
    while (mOnline)
    {
      int t = 1;
      while  (mOnline && !mpClient->IsConnected())
      {
        NGL_LOG("radio", NGL_LOG_INFO, "Redis message pump connection\n");
        if (mpClient->Connect(mHost) && mpClient->IsConnected())
        {
          NGL_LOG("radio", NGL_LOG_INFO, "Redis message pump connected\n");
          RedisRequest request;
          request.SUBSCRIBE("yascheduler");
          RedisReplyType res = mpClient->SendCommand(request);
          if (res == eRedisError)
          {
            NGL_LOG("radio", NGL_LOG_INFO, "Redis message unable to subscribe to yascheduler channel\n");
            mpClient->Disconnect();
          }
          else
          {
            NGL_LOG("radio", NGL_LOG_INFO, "Redis message subscribed to yascheduler channel\n");
          }
        }
        else
        {
          nglThread::MsSleep(t);
          t = MIN(10000, t * 2);
        }
      }

      if (mOnline && mpClient->IsConnected())
      {
        RedisReply reply;
        RedisReplyType res = mpClient->GetReply(reply);

        if (res == eRedisBulk)
        {
//          nglString str;
//          for (int i = 0; i < reply.GetCount(); i++)
//            str.Add("'").Add(reply.GetReply(i)).Add("'  ");
//
//          NGL_LOG("radio", NGL_LOG_INFO, "Redis message: %s\n", str.GetChars());
          HandleMessage(reply);
        }
        else
        {
          NGL_LOG("radio", NGL_LOG_INFO, "Redis reply\n");
        }
      }
    }
    NGL_LOG("radio", NGL_LOG_INFO, "Redis message offline\n");
 }


  void HandleMessage(const RedisReply& rReply)
  {
    nglString str = rReply.GetReply(1);
    nuiJson::Reader reader;
    nuiJson::Value msg;

    bool res = reader.parse(str.GetStdString(), msg);

    if (!res)
    {
      NGL_LOG("radio", NGL_LOG_ERROR, "unable to parse json message from scheduler");
      return;
    }

    nglString type = msg.get("type", nuiJson::Value()).asString();
    if (type == "radio_started")
    {
      nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
    }
    else if (type == "radio_exists")
    {
      nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
      nglString master_streamer = msg.get("master_streamer", nuiJson::Value()).asString();
    }
    else if (type == "radio_stopped")
    {
      nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
    }
    else if (type == "play")
    {
      nglString uuid = msg.get("radio_uuid", nuiJson::Value()).asString();
      nglString filename = msg.get("filename", nuiJson::Value()).asString();
      double delay = msg.get("delay", nuiJson::Value()).asDouble();
      double offset = msg.get("offset", nuiJson::Value()).asDouble();
      double crossfade = msg.get("crossfade_duration", nuiJson::Value()).asDouble();
    }
    else if (type == "user_authentication")
    {
      nglString uuid = msg.get("user_id", nuiJson::Value()).asString();
      bool hd = msg.get("hd", nuiJson::Value()).asBool();
      nglString auth_token = msg.get("auth_token", nuiJson::Value()).asString();
      nglString username = msg.get("username", nuiJson::Value()).asString();
      nglString api_key = msg.get("api_key", nuiJson::Value()).asString();
    }
    else if (type == "ping")
    {

    }

  }

  bool Post(const RedisRequest& request)
  {
    nuiToken<RedisRequest>* pToken = new nuiToken<RedisRequest>(request);
    nuiNotification* notif = new nuiNotification("RedisRequest");
    notif->Acquire();
    notif->SetToken(pToken);
    return mMessageQueue.Post(notif);
  }

  bool Post(nuiNotification* notif)
  {
    return mMessageQueue.Post(notif);
  }

  void Post(const nuiJson::Value& val)
  {
    RedisRequest request;
    nuiJson::FastWriter writer;
    nglString json = writer.write(val);
    request.PUBLISH("yascheduler", json);

    Post(request);
  }

  // API
  void RegisterStreamer()
  {
    nuiJson::Value val;
    val["type"] = "register_streamer";
    Post(val);
  }

private:
  RedisClient* mpClient;
  bool mOnline;
  nuiNetworkHost mHost;
  Mode mMode;
  nuiMessageQueue mMessageQueue;
};

int main(int argc, const char** argv)
{
  openlog("yastream", LOG_PID, LOG_DAEMON);
  signal(SIGSEGV, &sig_handler);

  bool daemon = false;

  nuiInit(NULL);

  NGL_OUT("Socket Pool Creation");
  pMainPool = new nuiSocketPool();
  NGL_OUT("Socket Pool OK");

  App->CatchSignal(SIGPIPE, SigPipeSink);
  App->CatchSignal(SIGSEGV, sig_handler);

  //nglOStream* pLogOutput = nglPath("/home/customer/yastreamlog.txt").OpenWrite(false);
  App->GetLog().SetLevel("yastream", 1000);
  App->GetLog().SetLevel("kernel", 1000);
  App->GetLog().SetLevel("radio", 1000);
  //App->GetLog().AddOutput(pLogOutput);
  App->GetLog().Dump();
  NGL_LOG("yastream", NGL_LOG_INFO, "yasound streamer\n");

  for (int i = 1; i < argc; i++)
  {
    NGL_LOG("radio", NGL_LOG_INFO, "arg[%d] = %s\n", i, argv[i]);
    if (strcmp(argv[i], "-port") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -port must be followed by a port number\n");
        exit(1);
      }

      port = atoi(argv[i]);
    }
    else if (strcmp(argv[i], "-redisport") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -redisport must be followed by a port number\n");
        exit(1);
      }

      redisport = atoi(argv[i]);
    }
    else if (strcmp(argv[i], "-host") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -host must be followed by a hostname or an ip address\n");
        exit(1);
      }

      hostname = argv[i];
    }
    else if (strcmp(argv[i], "-appurl") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -appurl must be followed by a hostname or an ip address\n");
        exit(1);
      }

      appurl = argv[i];
    }
    else if (strcmp(argv[i], "-bindhost") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -bind must be followed by a hostname or an ip address\n");
        exit(1);
      }

      bindhost = argv[i];
    }
    else if (strcmp(argv[i], "-redishost") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -redishost must be followed by a hostname or an ip address\n");
        exit(1);
      }

      redishost = argv[i];
    }
    else if (strcmp(argv[i], "-redisdb") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -redisdb must be followed by a port number\n");
        exit(1);
      }

      redisdb = atoi(argv[i]);
    }
    else if (strcmp(argv[i], "-flushall") == 0)
    {
      NGL_LOG("radio", NGL_LOG_INFO, "BEWARE: FLUSH ALL REQUESTED!\n");
      flushall = true;
    }
    else if (strcmp(argv[i], "-datapath") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -datapath must be followed by a valid path\n");
        exit(1);
      }

      datapath = argv[i];
    }
    else if (strcmp(argv[i], "-cachepath") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -cachepath must be followed by a valid path\n");
        exit(1);
      }

      CachePath = argv[i];
    }
    else if (strcmp(argv[i], "-cachesize") == 0)
    {
      i++;
      if (i >= argc)
      {
        NGL_LOG("yastream", NGL_LOG_ERROR, "ERROR: -cachesize must be followed by a number of bytes\n");
        exit(1);
      }

      CacheSize = atoi(argv[i]);
    }
    else if (strcmp(argv[i], "-testsignal") == 0)
    {
      kill(0, SIGSEGV);
    }
    else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
    {
      printf("%s [-p port] [-host hostname]\n", argv[0]);
      printf("\t-port\tset the server port.\n");
      printf("\t-host\tset the server host name or ip address.\n");
      printf("\t-appurl\tset the app server url.\n");
      printf("\t-redisport\tset the redis server port.\n");
      printf("\t-redishost\tset the redis server host name or ip address.\n");
      printf("\t-redisdb\tset the redis db index (must be an integer, default = 0).\n");
      printf("\t-datapath\tset the path to the song folder (the one that contains the mp3 hashes).\n");
      printf("\t-cachepath\tset the path to the song folder local cache.\n");
      printf("\t-cachesize\tset the maximum size of the local song cache in bytes.\n");
      printf("\t-daemon\tlaunch in daemon mode (will fork!).\n");
      printf("\t-syslog\tSend logs to syslog.\n");
      printf("\t-flushall\tBEWARE this option completely DESTROYS all records in the DB before proceeding to the server launch.\n");

      exit(0);
    }
    else if (strcmp(argv[i], "-daemon") == 0)
    {
      daemon = true;
    }
    else if (strcmp(argv[i], "-syslog") == 0)
    {
      nglConsole* pConsole = new SyslogConsole();
      App->SetConsole(pConsole);
    }
    else if (strcmp(argv[i], "--test") == 0)
    {
      Radio* pRadio = new Radio("matid");
      //nglPath current(ePathCurrent);
      nglPath current("/Users/meeloo/work/yastream");
      nglPath p = current + nglPath("data");
      for (int i = 0; i < 100; i++)
        pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));

//      pRadio->AddTrack(p + nglPath("/Money Talks.mp3"));
//      pRadio->AddTrack(p + nglPath("/04 The Vagabound.mp3"));
//      pRadio->AddTrack(p + nglPath("/Radian.mp3"));
      pRadio->Start();

      NGL_LOG("yastream", NGL_LOG_INFO, "test mode ENABLED\n");
    }
  }

  if (daemon)
  {
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
     *            we can exit the parent process. */
    if (pid > 0) {
      exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
      /* Log the failure */
      exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
      /* Log the failure */
      exit(EXIT_FAILURE);
    }

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  NGL_OUT("Radio BEGIN");

  NGL_OUT("Starting http streaming server %s:%d\n", bindhost.GetChars(), port);
  nuiHTTPServer* pServer = new HTTPServer();
  //pServer->SetClientStackSize(1024 * 1024 * 4);
  pServer->SetHandlerDelegate(HandlerDelegate);
  pServer->SetNonBlocking(true);

  Radio::SetParams(appurl, hostname, port, datapath);
  Radio::InitCache(CacheSize, datapath, CachePath);
  NGL_OUT("Flush OK");

  HTTPHandler::SetPool(pMainPool);
  NGL_OUT("Pool Set OK");

#if ENABLE_REDIS_THREADS
  RedisThread* pRedisThreadIn = new RedisThread(nuiNetworkHost("127.0.0.1", 6379, nuiNetworkHost::eTCP), RedisThread::MessagePump);
  pRedisThreadIn->Start();
  //pRedisThread->PumpMessages();

  RedisThread* pRedisThreadOut = new RedisThread(nuiNetworkHost("127.0.0.1", 6379, nuiNetworkHost::eTCP), RedisThread::Broadcaster);
  pRedisThreadOut->Start();

  pRedisThreadOut->RegisterStreamer();
#endif

  NGL_OUT("Ready to bind");
  if (pServer->Bind(bindhost, port) && pServer->Listen())
  {
    NGL_OUT("Bind OK");
    pMainPool->Add(pServer, nuiSocketPool::eContinuous);
    while (pMainPool->DispatchEvents(10000) >= 0)
    {
      //NGL_LOG("radio", NGL_LOG_INFO, "beep");
    }
  }
  else
  {
    NGL_OUT("Unable to bind or listen %s:%d\n", bindhost.GetChars(), port);
  }

  NGL_OUT("DONE OK");
#if ENABLE_REDIS_THREADS
  delete pRedisThreadIn;
  delete pRedisThreadOut;
#endif

  delete pServer;
  delete pMainPool;
  nuiUninit();


  return 0;
}

