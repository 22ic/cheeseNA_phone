#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void die(char* s) {
  perror(s);
  exit(1);
}

void com(void* s);

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

  pthread_t thread;

  ret = listen(ss, 10);
  if (ret == -1) die("listen");

  struct sockaddr_in client_addr;
  socklen_t len = sizeof(struct sockaddr_in);
  int s;
  while (1) {
    s = accept(ss, (struct sockaddr*)&client_addr, &len);
    if (s == -1) die("socket2");

    ret = pthread_create(&thread, NULL, (void*)com, (void*)&s);
    if (ret != 0) die("pthread_create");
    ret = pthread_detach(thread);
    if (ret != 0) die("pthread_detach");
  }
  // close(ss);
  return 0;
}

void com(void* s) {
  int sock = *((int*)s);
  printf("%d\n", sock);
  int bufsize = 1024 * 2;
  unsigned char buf[bufsize];
  int n;

  while (1) {
    n = recv(sock, buf, bufsize, 0);
    // printf("n: %d\n", n);
    if (n == -1) die("recv");
    if (n == 0) break;
    n = send(sock, buf, n, 0);
    if (n == -1) die("send");
  }

  close(sock);
  return;
}