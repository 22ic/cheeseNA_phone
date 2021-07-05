#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void die(char* s) {
  perror(s);
  exit(1);
}

int main(int argc, char** argv) {
  const int LISTEN_OFFSET = 2;
  const int NUM = 10;

  if (argc != 3) die("./client <ip> <port>");

  int s = socket(PF_INET, SOCK_STREAM, 0);
  if (s == -1) die("socket");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(argv[1]);
  addr.sin_port = htons(atoi(argv[2]));

  int ret = connect(s, (struct sockaddr*)&addr, sizeof(addr));
  if (ret == -1) die("connect");

  FILE* readf = popen("rec  -t raw -b 16 -c 1 -e s -r 44100 - ", "r");
  if (readf == NULL) die("popen readf");
  int fdread = fileno(readf);
  if (fdread == -1) die("fileno");

  FILE* writeflist[NUM];
  int fdlist[NUM];
  int recvsocks[NUM];
  for (int i = 0; i < NUM; i++) {
    writeflist[i] = NULL;
    fdlist[i] = -1;
    recvsocks[i] = -1;
  }

  int ss = socket(PF_INET, SOCK_STREAM, 0);  // listen socket
  if (ss == -1) die("socket");
  struct sockaddr_in listen_addr;
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  listen_addr.sin_port = htons(atoi(argv[2]) + LISTEN_OFFSET);

  ret = bind(ss, (struct sockaddr*)&listen_addr, sizeof(listen_addr));
  if (ret == -1) die("bind");

  ret = listen(ss, 10);
  if (ret == -1) die("listen");

  int kq = kqueue();
  if (kq == -1) die("kqueue");

  struct kevent kev;
  EV_SET(&kev, ss, EVFILT_READ, EV_ADD, 0, 0, NULL);
  ret = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (ret == -1) die("kevent ss");

  EV_SET(&kev, fdread, EVFILT_READ, EV_ADD, 0, 0, NULL);
  ret = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (ret == -1) die("kevent fdread");

  int nevents = 10;
  struct kevent kevlist[nevents];

  int bufsize = 1000 * 2;
  unsigned char buf[bufsize];

  while (1) {
    int kn = kevent(kq, NULL, 0, kevlist, nevents, NULL);
    for (int i = 0; i < kn; i++) {
      int fd = kevlist[i].ident;
      if (fd == fdread) {
        int n = read(fdread, buf, bufsize);
        if (n == -1) die("fread");
        if (n == 0) {
          close(s);
          pclose(readf);
          for (int j = 0; j < NUM; j++) {
            if (writeflist[j] != NULL) {
              pclose(writeflist[j]);
              close(recvsocks[j]);
            }
          }
          return 0;
        }

        n = send(s, buf, n, 0);
        if (n == -1) die("send");
      } else if (fd == ss) {
        printf("new sock\n");
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(struct sockaddr_in);
        int newsock = accept(ss, (struct sockaddr*)&client_addr, &len);
        if (newsock == -1) die("newsock");
        EV_SET(&kev, newsock, EVFILT_READ, EV_ADD, 0, 0, NULL);
        ret = kevent(kq, &kev, 1, NULL, 0, NULL);
        if (ret == -1) die("kevent accept");

        for (int j = 0; j < NUM; j++) {
          if (writeflist[j] == NULL) {
            printf("sock number: %d\n", j);
            recvsocks[j] = newsock;
            writeflist[j] =
                popen("play -t raw -b 16 -c 1 -e s -r 44100 - ", "w");
            if (writeflist[j] == NULL) die("popen writef");
            fdlist[j] = fileno(writeflist[j]);
            if (fdlist[j] == -1) die("fileno");
            break;
          }
        }
      } else {
        for (int j = 0; j < NUM; j++) {
          if (fd == recvsocks[j]) {
            int n = recv(fd, buf, bufsize, 0);
            if (n == -1) die("recv");
            if (n == 0) {
              close(fd);
              pclose(writeflist[j]);
              recvsocks[j] = -1;
              writeflist[j] = NULL;
              fdlist[j] = -1;
            }
            if (n > 0) {
              n = write(fdlist[j], buf, n);
              if (n == -1) die("write");
            }
          }
        }
      }
    }
  }

  close(s);
  pclose(readf);

  return 0;
}