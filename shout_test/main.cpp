#include <stdlib.h>
#include <stdio.h>
#include <shout/shout.h>
#include <lame/lame.h>

#define check_error { int err = shout_get_errno(pShout); if (err) { printf("error %d: %s\n", err, shout_get_error(pShout)); exit(0); } }

int main(int argc, char** argv)
{
  printf("yastrm\n");
  shout_init();

  int major, minor, patch;
  const char* shoutname = shout_version(&major, &minor, &patch);

  printf("libshout version %d.%d.%d\n", major, minor, patch);

  shout_t* pShout = shout_new();

  shout_set_host(pShout, "192.168.1.114");
  shout_set_port(pShout, 8000);
  shout_set_protocol(pShout, SHOUT_PROTOCOL_HTTP);
  shout_set_format(pShout, SHOUT_FORMAT_MP3);
  shout_set_mount(pShout, "test.mp3");
  shout_set_public(pShout, 1);
  shout_set_user(pShout, "source");
  shout_set_password(pShout, "hackme");
  printf("about to open conection\n");
  check_error;

  shout_open(pShout);

  check_error;
  printf("connection open\n");

  FILE* f = fopen("test.mp3", "rb");
  if (!f)
  {
    printf("error opening file\n");
    exit(0);
  }

  unsigned char data[2048];
  int len = 2048;
  int r = 0;
  do
  {
    r = fread(data, 1, len, f);
    printf("read %d\n", r);
    shout_send(pShout, data, r);
    check_error;
    shout_sync(pShout);
    check_error;
  }
  while (r > 0);

  printf("done\n");
  fclose(f);

  shout_close(pShout);
  shout_free(pShout);

  check_error;
  shout_shutdown();
}

