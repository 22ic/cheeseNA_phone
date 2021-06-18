#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void die(char* s) {
  perror(s);
  exit(1);
}

int main(int argc, char** argv) {
  if (argc != 2) die("./server <port>");
  int ss = socket(PF_INET, SOCK_STREAM, 0);
  if (ss == -1) die("socket");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(atoi(argv[1]));

  int ret = bind(ss, (struct sockaddr*)&addr, sizeof(addr));
  if (ret == -1) die("bind");

  ret = listen(ss, 10);
  if (ret == -1) die("listen");

  struct sockaddr_in client_addr;
  socklen_t len = sizeof(struct sockaddr_in);
  int s = accept(ss, (struct sockaddr*)&client_addr, &len);
  if (s == -1) die("socket2");

  close(ss);

  int bufsize = 1024 * 2;
  unsigned char buf[bufsize];
  int n;

  while (1) {
    n = recv(s, buf, bufsize, 0);
    // printf("n: %d\n", n);
    if (n == -1) die("recv");
    if (n == 0) break;
    n = send(s, buf, n, 0);
    if (n == -1) die("send");
  }

  close(s);

  return 0;
}