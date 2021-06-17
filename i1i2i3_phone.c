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
  if (argc == 1 || argc >= 4) die("argc");
  int server = 0;
  if (argc == 2) {
    server = 1;
  }

  int s;

  if (server == 1) {  // server
    // printf("\n-----SERVER-----\n\n");
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
    s = accept(ss, (struct sockaddr*)&client_addr, &len);
    if (s == -1) die("socket2");

    close(ss);
  } else {  // client
    // printf("\n-----CLIENT-----\n\n");
    s = socket(PF_INET, SOCK_STREAM, 0);
    if (s == -1) die("socket");

    // printf("sock\nsock %d\n\n", s);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_port = htons(atoi(argv[2]));

    int ret = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) die("connect");

    // printf("connect\nconnect %d\n\n", ret);
  }

  // FILE* writef = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ", "w");
  // if (writef == NULL) die("popen writef");

  FILE* readf = popen("rec -t raw -b 16 -c 1 -e s -r 44100 - ", "r");
  // FILE* readf = popen("ls -la", "r");

  if (readf == NULL) die("popen readf");

  int bufsize = 8192;
  unsigned char buf[bufsize];
  unsigned char buf2[bufsize];
  int n, m;

  while (1) {
    n = fread(buf, 1, bufsize, readf);  // sending
    if (n == -1) die("fread");
    if (n == 0) break;
    n = send(s, buf, n, 0);
    if (n == -1) die("send");

    m = recv(s, buf2, bufsize, 0);  // receiving
    if (m == -1) die("recv");
    if (m == 0) break;
    // m = fwrite(buf, 1, m, writef);
    m = write(1, buf2, m);
    if (m == -1) die("fwrite");
  }

  close(s);
  // pclose(writef);
  pclose(readf);

  return 0;
}