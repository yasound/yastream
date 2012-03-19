
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
  //printf("SigPipe!\n");
}

int main(int argc, const char** argv)
{
  nuiInit(NULL);
  
  nglOStream* pLogOutput = nglPath("~/yastreamlog.txt").OpenWrite(false);
  App->GetLog().SetLevel("yastream", 0);
  App->GetLog().AddOutput(pLogOutput);
  App->GetLog().Dump();
  NGL_LOG("yastream", NGL_LOG_INFO, "yasound streamer\n");

#if defined _MINUI3_
  App->CatchSignal(SIGPIPE, SigPipeSink);
#endif

  int port = 8001;
  nglString hostname = "0.0.0.0";
  bool daemon = false;
  nglPath datapath = "/data/glusterfs-storage/replica2all/song/";
  nglString redishost = "127.0.0.1";
  int redisport = 6379;
  
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-port") == 0)
    {
      i++;
      if (i >= argc)
      {
        printf("ERROR: -port must be followed by a port number\n");
        exit(1);
      }
      
      port = atoi(argv[i]);
    }
    else if (strcmp(argv[i], "-redisport") == 0)
    {
      i++;
      if (i >= argc)
      {
        printf("ERROR: -redisport must be followed by a port number\n");
        exit(1);
      }
      
      redisport = atoi(argv[i]);
    }
    else if (strcmp(argv[i], "-host") == 0)
    {
      i++;
      if (i >= argc)
      {
        printf("ERROR: -host must be followed by a hostname or an ip address\n");
        exit(1);
      }
      
      hostname = argv[i];
    }
    else if (strcmp(argv[i], "-redishost") == 0)
    {
      i++;
      if (i >= argc)
      {
        printf("ERROR: -redishost must be followed by a hostname or an ip address\n");
        exit(1);
      }
      
      redishost = argv[i];
    }
    else if (strcmp(argv[i], "-datapath") == 0)
    {
      i++;
      if (i >= argc)
      {
        printf("ERROR: -datapath must be followed by a valid path\n");
        exit(1);
      }
      
      datapath = argv[i];
    }
    else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
    {
      printf("yastrm [-p port] [-host hostname]\n");
      printf("\t-port\tset the server port.\n");
      printf("\t-host\tset the server host name or ip address.\n");
      printf("\t-redisport\tset the redis server port.\n");
      printf("\t-redishost\tset the redis server host name or ip address.\n");
      printf("\t-datapath\tset the path to the song folder (the one that contains the mp3 hashes).\n");
      printf("\t-daemon launch in daemon mode (will fork!).\n");
    }
    else if (strcmp(argv[i], "-daemon") == 0)
    {
      daemon = true;
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

      printf("test mode ENABLED\n");
    }
  }

  if (daemon)
  {
    openlog("yastream", LOG_PID, LOG_DAEMON);
    syslog(LOG_ALERT, "starting streaming server");
    
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
  
#if 0
  RedisClient redis;
  if (!redis.Connect(nuiNetworkHost("127.0.0.1", 6379, nuiNetworkHost::eTCP)))
  {
    printf("Error connecting to redis server.\n");
    exit(0);
  }

  redis.StartCommand("DEL");
  redis.AddArg("prout");
  redis.PrintSendCommand();
  
  //redis.StartCommand("GET");
  redis.StartCommand("LPUSH");
  redis.AddArg("prout");
  redis.AddArg("bleh");
  redis.PrintSendCommand();

  redis.StartCommand("LPUSH");
  redis.AddArg("prout");
  redis.AddArg("woah");
  redis.PrintSendCommand();

  redis.StartCommand("LRANGE");
  redis.AddArg("prout");
  redis.AddArg("0");
  redis.AddArg("-1");
  redis.PrintSendCommand();
  
  redis.StartCommand("OBJECT");
  redis.AddArg("ENCODING");
  redis.AddArg("prout");
  redis.PrintSendCommand();
  
  redis.StartCommand("INFO");
  redis.PrintSendCommand();
#endif
  
  //  nuiHTTPRequest request("https://dev.yasound.com/admin/");
//  nuiHTTPResponse* pResponse = request.SendRequest();
//  printf("response: %d - %s\n", pResponse->GetStatusCode(), pResponse->GetStatusLine().GetChars());
//  printf("data:\n %s\n\n", pResponse->GetBodyStr().GetChars());

  Radio::SetParams(hostname, port, datapath, redishost, redisport);

  nuiHTTPServer* pServer = new nuiHTTPServer();

  //Radio* pRadio = new Radio("fakeid");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/Money Talks.mp3");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/Thunderstruck.mp3");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/ebc_preview64.mp3");
  //pRadio->AddTrack("/Users/meeloo/work/yastream/data/ebc.mp3");
  //pRadio->AddTrack("/space/new/medias/song/eca/c9f/ebc.mp3");

  pServer->SetHandlerDelegate(HandlerDelegate);


  if (pServer->Bind(hostname, port))
  {
    pServer->AcceptConnections();
  }
  else
  {
    NGL_OUT("Unable to bind port %d\n", port);
  }

  delete pServer;
  nuiUninit();

  return 0;
}

