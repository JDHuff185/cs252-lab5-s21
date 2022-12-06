#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <malloc.h>

char *doStuff( int fd );
void autherr(int fd);
void errnotfound(int fd);

int main( int argc, char ** argv )
{

  const char * usage = "Usage: ./myhttpd [ -f | -t | -p] [port]\n";
  int mode = 0;
  /*
    Mode is our concurrency mode; 0 means no concurrency, 1 means new process
    for each request, 2 means new thread for each request, 3 means pool of threads.
  */
  int port = 8008;
  if (argc == 2) {
    if (strncmp(argv[1], "-f", 3) == 0) {
      //new process for each request
      mode = 1;
      //printf("-f\n");
    } else if (strncmp(argv[1], "-t", 3) == 0) {
      //new thread for each request
      mode = 2;
      //printf("-t\n");
    } else if (strncmp(argv[1], "-p", 3) == 0) {
      //pool of threads
      mode = 3;
      //printf("-p\n");
    } else if (atoi(argv[1]) > 1024 && atoi(argv[1]) < 65536) {
      port = atoi(argv[1]);
      //printf("port: %d\n", port);
    } else {
      printf("%s", usage);
      exit(1);
    }
  } else if (argc == 3) {
    if (strncmp(argv[1], "-f", 3) == 0) {
      mode = 1;
      //printf("-f\n");
    } else if (strncmp(argv[1], "-t", 3) == 0) {
      mode = 2;
      //printf("-t\n");
    } else if (strncmp(argv[1], "-p", 3) == 0) {
      mode = 3;
      //printf("-p\n");
    } else {
      printf("%s", usage);
      exit(1);
    }

    if (atoi(argv[2]) <= 1024 || atoi(argv[2]) >= 65536) {
      printf("%s", usage);
      exit(1);
    } else {
      port = atoi(argv[2]);
      //printf("port: %d\n", port);
    }
  } else if (argc > 3) {
    printf("%s", usage);
    exit(1);
  }
  clock_t start, end;
  double cpu_time_used;
  printf("port: %d mode: %d\n", port, mode);
  fflush(stdout);

  struct sockaddr_in serverIPAddress;
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);

  int masterSocket = socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit(-1);
  }

  int optval = 1;
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR,
      (char *) &optval, sizeof(int));

  int error = bind( masterSocket, (struct sockaddr *)&serverIPAddress,
      sizeof(serverIPAddress));
  if (error) {
    perror("bind");
    exit(-1);
  }

  error = listen( masterSocket, 5);
  if(error) {
    perror("listen");
    exit(-1);
  }
  struct sockaddr_in clientIPAddress;
  int alen;
  int slaveSocket;
  //printf("reached line 100\n");
  if (mode == 0) {
    while (1) {
      FILE *logfile = fopen("logs", "a");
      start = clock();
      alen = sizeof(clientIPAddress);
      slaveSocket = accept( masterSocket, (struct sockaddr *) &clientIPAddress,
          (socklen_t *) &alen);

      if (slaveSocket < 0) {
        perror("accept");
        exit(-1);
      }

    //do stuff
      char * res_req = doStuff(slaveSocket);

      close(slaveSocket);
      end = clock();
      cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
      fprintf(logfile, "Request: %s, source: %u \n", res_req, clientIPAddress.sin_addr.s_addr);
      free(res_req);
      fclose(logfile);
    }
  } else if (mode == 1) {
    //new process for each request
    while (1) {
      alen = sizeof(clientIPAddress);
      slaveSocket = accept( masterSocket, (struct sockaddr *) &clientIPAddress,
          (socklen_t *) &alen);
      if (slaveSocket < 0) {
        perror("accept");
        exit(-1);
      }
      printf("accepted request\n");
      int forkret = fork();
      if (forkret == 0) {
        printf("child process created\n");
        doStuff(slaveSocket);
        printf("stuff done\n");
        printf("child process terminated\n");
        fflush(stdout);
        exit(0);
      } else {
        //printf("PID of child: %d\n", forkret);
        waitpid(forkret, NULL, 0);
        close(slaveSocket);
      }
    }
  } else if (mode == 2) {
    //new thread for each request
  } else if (mode == 3) {
    //pool of threads
  }
  return 1;
}

char *doStuff(int fd) {
  const int MaxBuff = 4096;
  char buf[MaxBuff + 1];
  int buflen = 0;
  int n;
  char finpath[1024];
  printf("start doing stuff fd: %d\n", fd);
  //fcntl(fd, F_SETFL, O_NONBLOCK);
  unsigned char newChar;
  while (buflen < MaxBuff &&
    ( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {
    //printf("%d\n", n);
    buf[buflen] = newChar;
    buflen++;

    if (buflen > 4 &&  buf[buflen - 4]=='\r' && buf[buflen - 3] == '\n' &&
    buf[buflen - 2] == '\r' && buf[buflen - 1] == '\n') {
      break;
    }
    //printf("%c", newChar);

  }
  //printf("1\n");
  buf[buflen] = 0;
  printf("req=%s\n", buf);
  const char * dirstr = "http-root-dir/htdocs";
  char filname[1024];
  char * c = &buf[4];
  int i = 0;
  while (*c != ' ' && i < 1024) {
    filname[i] = *c;
    i++;
    c++;
  }
  filname[i] = '\0';
  printf("filname requested: %s\n", filname);
  const char * index = "/index.html";
  if (strcmp(filname, "/") == 0) {
    strncpy(filname, index, 1024);
  }
  if (strncmp(filname, "/cgi-bin/", 9) == 0) {
    char query_string[1024];
    query_string[0] = 0;
    c = strchr(filname, '?');
    if (c != NULL) {
      *c = 0;
      c++;
      printf("new filname: %s\n", filname);

      i = 0;
      while (*c != ' ') {
        query_string[i] = *c;
        c++;
        i++;
      }
      printf("query string: %s\n", query_string);
      return NULL;

    }
    char *script = &(filname[9]);
    char *argv[128];
    argv[0] = script;
    argv[1] = NULL;
    printf("script: %s\n", script);
    char cgipath[1024];
    const char *cpath = "./http-root-dir";
    strncpy(cgipath, cpath, 1024);
    strcat(cgipath, filname);
    printf("cgipath: %s\n", cgipath);
    int chpid;
    if ((chpid = fork()) == 0) {
      setenv("REQUEST_METHOD", "GET", 1);
      setenv("QUERY_STRING", "", 1);
      dup2(fd, 1);
      printf("HTTP/1.1 200 Document follows \r\n");
      printf("Server: CS252 lab 5 \r\n");
      execvp(cgipath, argv);
    } else {
      char *toreturn = (char *) malloc(1024);
      strncpy(toreturn, cgipath, 1024);
      return toreturn;
    }
  }
  char * extension = strchr(filname, '.');
  if (extension == NULL) {
      if (false) {

      } else {
        errnotfound(fd);
        exit(1);
      }
  }

  strncpy(finpath, dirstr, 1024);
  strcat(finpath, filname);
  //printf("finpath: %s\n", finpath);
  char lastChar;
  const char * auth = "Authorization: Basic amh1ZmY5OnNtZWdtYQ==";
  bool authfound = false;
  while(*c != '\0') {
    if(strncmp(c, auth, 41) == 0) {
      authfound = true;
      break;
    }
    c++;
  }
  if (!authfound) {
    autherr(fd);
    return NULL;
  }

  const char * hdr1 = "HTTP/1.1 200 Document follows \r\n"
                      "Server: CS252 lab 5 \r\n";
  const char * hdr2 = "Content-type: ";
  char contentType[25];
  const char * hdr3 = "\r\n\r\n";
  const char * document = "<html>\n"
                          "<body\n"
                          "<H1>Welcome to the cs252 web server</H1>\n"
                          "</body>\n"
                          "</html>\n";

  if (strncmp(extension, ".html", 5) == 0) {
    strncpy(contentType, "text/html", 10);
  } else if (strncmp(extension, ".gif", 4) == 0) {
    strncpy(contentType, "image/gif", 10);
  } else if (strncmp(extension, ".txt", 4) == 0) {
    strncpy(contentType, "text/plain", 11);
  } else if (strncmp(extension, ".png", 4) == 0) {
    strncpy(contentType, "image/png", 10);
  } else if (strncmp(extension, ".jpg", 4) == 0) {
    strncpy(contentType, "image/jpg", 10);
  } else if (strncmp(extension, ".svg", 4) == 0) {
    strncpy(contentType, "image/svg", 10);
  } else {
    //unsupported file type
    errnotfound(fd);
    return NULL;
  }

  write(fd, hdr1, strlen(hdr1));
  write(fd, hdr2, strlen(hdr2));
  write(fd, contentType, strlen(contentType));
  write(fd, hdr3, strlen(hdr3));

  i = 0;

  FILE * fp = fopen(finpath, "r");

  if (fp == NULL) {
    errnotfound(fd);
    return NULL;
  }

  int readreturn = 5;

  char outbuf[1025];

  while (1) {

    readreturn = fread(outbuf, 1, 1024, fp);



    outbuf[readreturn] = '\0';
    write(fd, outbuf, readreturn);

    if (readreturn < 1024) {
      break;
    }
  }
  fsync(fd);
  char *toreturn = (char *) malloc(1024);
  strncpy(toreturn, finpath, 1024);
  return toreturn;

}

void autherr(int fd) {
  const char * h1 = "HTTP/1.1 401 Unauthorized\r\n";
  const char * h2 = "WWW-Authenticate: Basic realm=\"myhttpd-cs252\"\r\n\r\n";
  write(fd, h1, strlen(h1));
  write(fd, h2, strlen(h2));
}

void errnotfound(int fd) {
  const char * h1 = "HTTP/1.1 404 File Not Found\r\n"
                    "Server: CS 252 lab 5 \r\n"
                    "Content-type text/plain\r\n\r\n";
  write(fd, h1, strlen(h1));
}
