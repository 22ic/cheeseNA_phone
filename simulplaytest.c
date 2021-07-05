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
  FILE* writef1 = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ", "w");
  if (writef1 == NULL) die("popen writef");
  int fdwrite1 = fileno(writef1);
  if (fdwrite1 == -1) die("fileno");

  FILE* writef2 = popen("play -t raw -b 16 -c 1 -e s -r 44100 - ", "w");
  if (writef2 == NULL) die("popen writef");
  int fdwrite2 = fileno(writef2);
  if (fdwrite2 == -1) die("fileno");

  int kq = kqueue();
  if (kq == -1) die("kqueue");

  int f1 = open("../day4/sound/doremi.raw", O_RDONLY);
  if (f1 == -1) die("open");

  int f2 = open("../day4/sound/dq1.raw", O_RDONLY);
  if (f2 == -1) die("open");

  struct kevent kev;
  EV_SET(&kev, f1, EVFILT_READ, EV_ADD, 0, 0, NULL);
  int ret = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (ret == -1) die("kevent f1");

  EV_SET(&kev, f2, EVFILT_READ, EV_ADD, 0, 0, NULL);
  ret = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (ret == -1) die("kevent f2");

  int nevents = 2;
  struct kevent kevlist[nevents];

  int bufsize = 1 * 2;
  unsigned char buf[bufsize];

  while (1) {
    int kn = kevent(kq, NULL, 0, kevlist, nevents, NULL);
    for (int i = 0; i < kn; i++) {
      int fd = kevlist[i].ident;
      if (fd == f1) {
        int n = read(f1, buf, bufsize);
        if (n == -1) die("read");
        if (n == 0) {
          close(f1);
          pclose(writef1);
        }
        n = write(fdwrite1, buf, n);
        if (n == -1) die("write");
      } else if (fd == f2) {
        int n = read(f2, buf, bufsize);
        if (n == -1) die("read");
        if (n == 0) {
          close(f2);
          pclose(writef2);
        }
        n = write(fdwrite2, buf, n);
        if (n == -1) die("write");
      }
    }
  }

  return 0;
}