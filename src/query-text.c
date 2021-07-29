#include <ares.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

static void query_callback(void *arg, int status, int timeouts,
    unsigned char* buf, int len) {
  if(status != ARES_SUCCESS){
    printf("Failed to lookup %s\n", ares_strerror(status));
    return;
  }
  struct hostent* host;
  int r = ares_parse_a_reply(buf, len, &host, NULL, NULL);
  if (r != ARES_SUCCESS) {
    printf("Failed to parse a record %s\n", ares_strerror(r));
    return;
  }
  printf("query_callback Parsed host: %s\n", host->h_name);
}

static void wait_ares(ares_channel channel) {
  for(;;){
    struct timeval *tvp, tv;
    fd_set read_fds, write_fds;
    int nfds;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    nfds = ares_fds(channel, &read_fds, &write_fds);
    if(nfds == 0){
        break;
    }
    tvp = ares_timeout(channel, NULL, &tv);
    select(nfds, &read_fds, &write_fds, NULL, tvp);
    ares_process(channel, &read_fds, &write_fds);
  }
}

int main(void) {
  ares_channel channel;
  int status;
  struct ares_options options;
  int optmask = 0;

  status = ares_library_init(ARES_LIB_INIT_ALL);
  if (status != ARES_SUCCESS){
      printf("ares_library_init: %s\n", ares_strerror(status));
      return 1;
  }

  status = ares_init_options(&channel, &options, optmask);
  if(status != ARES_SUCCESS) {
    printf("ares_init_options: %s\n", ares_strerror(status));
    return 1;
  }

  unsigned char* query;
  int len;
  status = ares_create_query("google.com", C_IN, T_A, 0x1234, 0, &query, &len, 0);
  if(status != ARES_SUCCESS) {
    printf("ares_create_query: %s\n", ares_strerror(status));
    return 1;
  }
  ares_send(channel, query, len, query_callback, NULL);

  wait_ares(channel);
  ares_destroy(channel);
  ares_library_cleanup();
  ares_free_string(query);
  return 0;
}
