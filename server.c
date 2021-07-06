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

struct client {
  int fd;
  short data;
  struct sockaddr_in addr;
};

int main(int argc, char** argv) {
  const int LISTEN_OFFSET = 2;
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

  int bufsize = 1 * 2;
  short buf[bufsize / 2];

  int nclient = nevents - 1;
  struct client clist[nclient];
  for (int i = 0; i < nclient; i++) {
    clist[i].fd = -1;
    clist[i].data = 0;
  }
  int cnum = 0;

  int sockmatrix[nclient][nclient];
  for (int i = 0; i < nclient; i++) {
    for (int j = 0; j < nclient; j++) {
      sockmatrix[i][j] = -1;
    }
  }

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
          if (clist[j].fd == -1) {
            client_addr.sin_port = htons(atoi(argv[1]) + LISTEN_OFFSET);
            clist[j].addr.sin_family = client_addr.sin_family;
            clist[j].addr.sin_addr.s_addr = client_addr.sin_addr.s_addr;
            clist[j].addr.sin_port = htons(atoi(argv[1]) + LISTEN_OFFSET);
            printf("port check: %d\n", htons(clist[j].addr.sin_port));

            for (int k = 0; k < nclient; k++) {
              if (clist[k].fd != -1) {
                sockmatrix[k][j] = socket(PF_INET, SOCK_STREAM,
                                          0);  // sock for sending to newsock
                if (sockmatrix[k][j] == -1) die("socket k j");

                int ret = connect(sockmatrix[k][j],
                                  (struct sockaddr*)&(clist[j].addr),
                                  sizeof(clist[j].addr));
                if (ret == -1) die("connect k j");

                sockmatrix[j][k] = socket(PF_INET, SOCK_STREAM,
                                          0);  // sock for sending to old sock
                if (sockmatrix[j][k] == -1) die("socket j k");

                ret = connect(sockmatrix[j][k],
                              (struct sockaddr*)&(clist[k].addr),
                              sizeof(clist[k].addr));
                if (ret == -1) die("connect j k");
              }
            }
            clist[j].fd = newsock;
            break;
          }
        }

        EV_SET(&kev, newsock, EVFILT_READ, EV_ADD, 0, 0, NULL);
        ret = kevent(kq, &kev, 1, NULL, 0, NULL);
        if (ret == -1) die("kevent accept");

        printf("socket matrix:\n");
        for (int x = 0; x < nclient; x++) {
          for (int y = 0; y < nclient; y++) {
            printf("%d ", sockmatrix[x][y]);
          }
          printf("\n");
        }
      } else {
        int n = recv(fd, buf, bufsize, 0);
        if (n == -1) die("recv");
        if (n == 0) {
          printf("disconnected from %d\n", fd);
          close(fd);

          int receiver = 0;
          cnum -= 1;
          for (int j = 0; j < nclient; j++) {
            if (clist[j].fd == fd) {
              clist[j].fd = -1;
              receiver = j;
              break;
            }
          }
          for (int j = 0; j < nclient; j++) {
            if (sockmatrix[receiver][j] != -1) {
              close(sockmatrix[receiver][j]);
              sockmatrix[receiver][j] = -1;
            }
            if (sockmatrix[j][receiver] != -1) {
              close(sockmatrix[j][receiver]);
              sockmatrix[j][receiver] = -1;
            }
          }
          printf("socket matrix:\n");
          for (int x = 0; x < nclient; x++) {
            for (int y = 0; y < nclient; y++) {
              printf("%d ", sockmatrix[x][y]);
            }
            printf("\n");
          }
        }
        if (n > 0) {
          if (!quiet) printf("data received from %d\n", fd);
          if (!quiet) write(1, buf, n);

          int receiver = 0;
          for (int j = 0; j < nclient; j++) {
            if (clist[j].fd == fd) {
              receiver = j;
              break;
            }
          }
          for (int j = 0; j < nclient; j++) {
            if (sockmatrix[receiver][j] != -1) {
              n = send(sockmatrix[receiver][j], buf, n, 0);
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