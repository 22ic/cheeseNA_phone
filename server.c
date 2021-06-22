#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void die(char* s) {
  perror(s);
  exit(1);
}

int main(int argc, char** argv) {
  if (argc < 2) die("./server [option] <port>");
  int quiet = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-q") == 0) {
      quiet = 1;
    }
  }
  if (quiet == 1) printf("quiet mode\n");

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

  int kq = kqueue();
  if (kq == -1) die("kqueue");

  struct kevent kev;
  EV_SET(&kev, ss, EVFILT_READ, EV_ADD, 0, 0, NULL);
  ret = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (ret == -1) die("kevent");

  int nevents = 10;
  struct kevent kevlist[nevents];

  int bufsize = 1024 * 2;
  unsigned char buf[bufsize];

  int nclient = nevents - 1;
  int client[nclient];
  for (int i = 0; i < nclient; i++) {
    client[i] = -1;
  }
  int cnum = 0;

  while (1) {
    int kn = kevent(kq, NULL, 0, kevlist, nevents, NULL);
    for (int i = 0; i < kn; i++) {
      int fd = kevlist[i].ident;
      if (fd == ss) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(struct sockaddr_in);
        int newsock = accept(ss, (struct sockaddr*)&client_addr, &len);
        if (newsock == -1) die("newsock");
        printf("new sock created: %d\n", newsock);

        if (cnum == nclient) die("too many client");
        cnum += 1;
        for (int j = 0; j < nclient; j++) {
          if (client[j] == -1) {
            client[j] = newsock;
            break;
          }
        }

        EV_SET(&kev, newsock, EVFILT_READ, EV_ADD, 0, 0, NULL);
        ret = kevent(kq, &kev, 1, NULL, 0, NULL);
        if (ret == -1) die("kevent accept");
      } else {
        int n = recv(fd, buf, bufsize, 0);
        if (n == -1) die("recv");
        if (n == 0) {
          printf("disconnected from %d\n", fd);
          close(fd);

          cnum -= 1;
          for (int j = 0; j < nclient; j++) {
            if (client[j] == fd) {
              client[j] = -1;
              break;
            }
          }
        }
        if (n > 0) {
          if (!quiet) printf("data received from %d\n", fd);
          if (!quiet) write(1, buf, n);

          for (int j = 0; j < nclient; j++) {
            if (client[j] != -1 && client[j] != fd) {
              n = send(client[j], buf, n, 0);
              if (n == -1) die("send");
            }
          }
        }
      }
    }
  }

  close(ss);

  return 0;
}