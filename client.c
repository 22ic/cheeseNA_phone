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
  if (argc != 3) die("./client <ip> <port>");

  int s = socket(PF_INET, SOCK_STREAM, 0);
  if (s == -1) die("socket");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(argv[1]);
  addr.sin_port = htons(atoi(argv[2]));

  int ret = connect(s, (struct sockaddr*)&addr, sizeof(addr));
  if (ret == -1) die("connect");

  FILE* readf = popen("rec -t raw -b 16 -c 1 -e s -r 44100 - ", "r");
  if (readf == NULL) die("popen readf");
  int fdread = fileno(readf);
  if (fdread == -1) die("fileno");

  FILE* writef = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ", "w");
  if (writef == NULL) die("popen writef");
  int fdwrite = fileno(writef);
  if (fdwrite == -1) die("fileno");

  int kq = kqueue();
  if (kq == -1) die("kqueue");

  struct kevent kev;
  EV_SET(&kev, s, EVFILT_READ, EV_ADD, 0, 0, NULL);
  ret = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (ret == -1) die("kevent s");

  EV_SET(&kev, fdread, EVFILT_READ, EV_ADD, 0, 0, NULL);
  ret = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (ret == -1) die("kevent fdread");

  int nevents = 2;
  struct kevent kevlist[nevents];

  int bufsize = 1024 * 2;
  unsigned char buf[bufsize];

  while (1) {
    int kn = kevent(kq, NULL, 0, kevlist, nevents, NULL);
    for (int i = 0; i < kn; i++) {
      int fd = kevlist[i].ident;
      if (fd == s) {
        int n = recv(s, buf, bufsize, 0);
        if (n == -1) die("recv");
        if (n == 0) {
          close(s);
          pclose(readf);
          pclose(writef);
          return 0;
        }

        n = write(fdwrite, buf, n);
        if (n == -1) die("write");
      } else if (fd == fdread) {
        int n = read(fdread, buf, bufsize);
        if (n == -1) die("fread");
        if (n == 0) {
          close(s);
          pclose(readf);
          pclose(writef);
          return 0;
        }

        n = send(s, buf, n, 0);
        if (n == -1) die("send");
      }
    }
  }

  close(s);
  pclose(readf);

  return 0;
}