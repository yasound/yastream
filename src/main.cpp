
#include "nui.h"
#include "nuiInit.h"
#include "nuiHTTPServer.h"
#include "nuiHTTP.h"
#include "HTTPHandler.h"
#include "Radio.h"
#include <signal.h>

#include "RedisClient.h"

#include <syslog.h>

nuiHTTPHandler* HandlerDelegate(nuiTCPClient* pClient);
nuiHTTPHandler* HandlerDelegate(nuiTCPClient* pClient)
{
  return new HTTPHandler(pClient);
}

void SigPipeSink(int signal)
{
  // Ignore...
  //NGL_LOG("radio", NGL_LOG_INFO, "SigPipe!\n");
}

class SyslogConsole : public nglConsole
{
public:
  SyslogConsole (bool IsVisible = false)
  {
    openlog("yastream", LOG_PID, LOG_DAEMON);
  }

  virtual ~SyslogConsole()
  {
  }


  virtual void OnOutput (const nglString& rText)
  {
    syslog(LOG_ALERT, rText.GetChars());
  }

  virtual void OnInput  (const nglString& rLine)
  {
    // No input handled...
  }
};


int main(int argc, const char** argv)
{
  nuiInit(NULL);

  //nglOStream* pLogOutput = nglPath("/home/customer/yastreamlog.txt").OpenWrite(false);
  App->GetLog().SetLevel("yastream", 1000);
  App->GetLog().SetLevel("kernel", 1000);
  //App->GetLog().AddOutput(pLogOutput);
  App->GetLog().Dump();
  NGL_LOG("yastream", NGL_LOG_INFO, "yasound streamer\n");

#if defined _MINUI3_
  App->CatchSignal(SIGPIPE, SigPipeSink);
#endif


  int port = 8000;
  nglString hostname = "0.0.0.0";
  bool daemon = false;
  nglPath datapath = "/data/glusterfs-storage/replica2all/song/";
  nglString redishost = "127.0.0.1";
  int redisport = 6379;
  int redisdb = 1;
  nglString bindhost = "0.0.0.0";
  bool flushall = false;

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
    else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
    {
      printf("%s [-p port] [-host hostname]\n", argv[0]);
      printf("\t-port\tset the server port.\n");
      printf("\t-host\tset the server host name or ip address.\n");
      printf("\t-redisport\tset the redis server port.\n");
      printf("\t-redishost\tset the redis server host name or ip address.\n");
      printf("\t-redisdb\tset the redis db index (must be an integer, default = 0).\n");
      printf("\t-datapath\tset the path to the song folder (the one that contains the mp3 hashes).\n");
      printf("\t-daemon\tlaunch in daemon mode (will fork!).\n");
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
      nglPath current(ePathCurrent);
      nglPath p = current + nglPath("/data");
      pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));
      pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));
      pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));
      pRadio->AddTrack(p + nglPath("/Thunderstruck.mp3"));

//      pRadio->AddTrack(p + nglPath("/Money Talks.mp3"));
//      pRadio->AddTrack(p + nglPath("/04 The Vagabound.mp3"));
      pRadio->AddTrack(p + nglPath("/Radian.mp3"));
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

  Radio::SetParams(hostname, port, datapath, redishost, redisport, redisdb);
  Radio::FlushRedis(flushall);

  NGL_OUT("Starting http streaming server %s:%d\n", bindhost.GetChars(), port);
  nuiHTTPServer* pServer = new nuiHTTPServer();
  //pServer->SetClientStackSize(1024 * 1024 * 4);
  pServer->SetHandlerDelegate(HandlerDelegate);

  if (pServer->Bind(bindhost, port))
  {
    pServer->AcceptConnections();
  }
  else
  {
    NGL_OUT("Unable to bind %s:%d\n", bindhost.GetChars(), port);
  }

  delete pServer;
  nuiUninit();

  return 0;
}

