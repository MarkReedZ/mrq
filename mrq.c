#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <stdarg.h>
#include "aemain.h"
#include <netinet/in.h>
#include "ae/ae.h"
#include "ae/anet.h"
#include <assert.h>
#include <unistd.h>

#define DBG if(0) 
#define DBG_EV if(0) 

#define BUFFER_SIZE 1024*1024

struct settings settings;

typedef struct {
  char *start;
  char *write;
  char *read;
  unsigned long read_sz[2];
  char rhalf;
  char whalf;
  int sz;
  unsigned long last_wrap;
} slot_t;

slot_t slots[256][256] = {0};

typedef struct
{
  char *buf;
  int max_sz;
  int cur_sz;
  int needs;
} my_conn_t;

struct sockaddr_in addr;

static int total_clients = 0;  // Total number of connected clients
static int total_mem = 0;
static int num_writes = 0;
static double start_time = 0;


static void print_buffer( char* b, int len ) {
  for ( int z = 0; z < len; z++ ) {
    printf( "%02x ",(int)b[z]);
  }
  printf("\n");
}

static void conn_init( my_conn_t *c ) {
  c->buf = malloc( BUFFER_SIZE*2 );
  c->cur_sz = 0;
  c->max_sz = BUFFER_SIZE;
  c->needs = 0;
}

static void conn_append( my_conn_t* c, char *data, int len ) {
  DBG printf(" append cur %d \n", c->cur_sz);
  if ( (c->cur_sz + len) > c->max_sz ) {
    while ( (c->cur_sz + len) > c->max_sz ) c->max_sz <<= 1;
    c->buf = realloc( c->buf, c->max_sz );
  }
  memcpy( c->buf + c->cur_sz, data, len ); 
  c->cur_sz += len;
  DBG printf(" append cur now %d \n", c->cur_sz);
  //DBG print_buffer( c->buf, c->cur_sz );
}


void on_data(my_conn_t *conn, ssize_t nread, char *buf) 
{

  if (start_time == 0) start_time = clock(); //TODO DELME
  //printf( " %d writes time taken %f \n ", num_writes, ((double)(clock()-start_time))/CLOCKS_PER_SEC );
  DBG printf("on_data\n");
  //DBG print_buffer( buf, 32 );

  if (nread <= 0) { //TODO
    printf("nread <= 0\n");
    exit(1);
    //if (nread != UV_EOF) fprintf(stderr, "Read error %s\n", uv_err_name(nread));
    //uv_close((uv_handle_t*) client, on_close);
    //free(buf->base);
    return;
  }

  int data_left = nread;
  char *p;

  //printf("YAY %d\n", conn->cur_sz);
  // If we have partial data for this connection
  if ( conn->cur_sz ) {
    DBG printf("Received %d more bytes need %d\n",(int)nread, conn->needs);
    //conn_print( conn );
    conn_append( conn, buf, nread );
    DBG print_buffer( conn->buf, 32 );
    DBG printf("cur is now %d\n",conn->cur_sz);
    //conn_print( conn );
    if ( conn->cur_sz >= conn->needs ) {
      p = conn->buf;
      data_left = conn->cur_sz;
      DBG printf("Got enough %d num wirtes %d\n", data_left, num_writes);
      conn->cur_sz = 0;
    } else {
      return;
    }
  } else {
    p = buf;
  }

  while ( data_left > 0 ) {

    //if ( num_writes > 20197772 ) DBG printf( " nread %d, data left %d\n", (int)nread, data_left );
    //if ( data_left < 100 ) {
      //DBG print_buffer( p, data_left );
    //}
    if ( data_left < 4 ) {
      conn_append( conn, p, data_left );
      conn->needs = 4;
      DBG printf("Received partial data %d more bytes need %d\n",data_left, conn->needs);
      return;
    }

  int cmd   = (unsigned char)p[1];
  int topic = (unsigned char)p[2];
  int slot  = (unsigned char)p[3];

  if ( cmd == 9 ) {
    printf( " %d writes time taken %f \n ", num_writes, ((double)(clock()-start_time))/CLOCKS_PER_SEC );
    start_time = clock();
    data_left -= 4;
    p += 4;
  }
  else if ( cmd == PUSH ) { 

    if ( data_left < 8 ) {
      conn_append( conn, p, data_left );
      conn->needs = 4;
      DBG print_buffer( p, data_left);
      DBG printf("Received partial data %d more bytes need %d\n",data_left, conn->needs);
      return;
    }
    int data_sz   = *((int*)(p)+1);

    if ( data_left < 8+data_sz ) {
      DBG print_buffer( p, data_left );
      conn_append( conn, p, data_left );
      conn->needs = 8+data_sz;
      DBG print_buffer( p, data_left);
      DBG printf("Received partial data %d more bytes need %d\n",data_left, conn->needs);
      break; 
    }
    //if ( data_left < 100 ) {
      //DBG print_buffer( p, 8 + data_sz );
    //}
    //if ( num_writes > 20197772 ) printf( " DELME data sz %d\n", data_sz );
    

    num_writes += 1;
    //DBG printf( " %d writes nread %d left %d\n",num_writes,(int)nread,data_left);

    slot_t *s = &slots[topic][slot];

    if ( s->start == NULL ) {
      s->start = malloc( 4*1024*1024 + 1024 ); //TODO
      s->sz = 4*1024*1024;
      total_mem += s->sz;
      s->write = s->read = s->start;
      s->rhalf = 0; s->whalf = 0;
    }

    // Wrap
    if ( (s->start+s->sz)-(s->write+data_sz+4) < 0 ) { 
    //if ( (s->start+s->sz)-(s->write+data_sz+4) < 0 ) { 
      int expand = 0;
      if ( data_sz > (s->sz>>3) ) expand = 1;
      if ( time(NULL) - s->last_wrap < 4 ) expand = 1; // Resize if we wrap too fast

      // Either we grow or wrap back to start
      if ( expand ) {
        int write = s->write-s->start;
        int orig = s->sz;
        s->sz <<= 1;
        DBG printf("expand data sz %d slot sz %d\n", data_sz, s->sz);
        while ( data_sz > (s->sz>>3) ) {
          s->sz <<= 1;
          if ( total_mem + s->sz > settings.max_memory ) {
            fprintf( stderr, "Out of memory\n" ); // TODO Dump memory stats
            exit(1);
          }
        }

        if ( (total_mem+(s->sz-orig)) > settings.max_memory ) {
          s->sz >>= 1;
          if ( data_sz > orig ) {
            fprintf( stderr, "Out of memory\n" ); // TODO Dump memory stats
            exit(1);
          } 
          s->last_wrap = time(NULL);
          s->whalf ^= 1;
          s->write = s->start;
        } else {  
          s->start = realloc( s->start, s->sz );
          s->write = s->start + write;
          printf("Expanded to %d\n", s->sz);
        }

      } else {
        s->last_wrap = time(NULL);
        s->whalf ^= 1;
        s->write = s->start;
      }
    }

    // Halt if we bump into the read ptr          
/*
    if ( s->rhalf != s->whalf ) {
      if ( s->write+data_sz >= s->read ) {
        //TODO drop the write
        //printf("DELME hit read ptr\n");
      } 
    }
*/

    //if ( num_writes > 20197772 ) printf( " A\n" );
    s->read_sz[s->whalf] += data_sz + 4;
    memcpy( s->write, p+4, data_sz+4 );
    s->write += data_sz+4;
    p += 8 + data_sz;
    data_left -= 8 + data_sz;
    //if ( num_writes > 20197772 ) printf( " B\n" );

  } 
  else if ( cmd == PULL ) { // pull

    data_left -= 4;
    p += 4;

    slot_t *s = &slots[topic][slot];
    // read s->read_sz[s->rhalf] bytes from the read ptr 
    //printf(" s->read_sz %d", s->read_sz[s->rhalf]);
    //write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
    //req->buf = uv_buf_init(s->read, s->read_sz[s->rhalf]);
    //printf(" pull  %.*s\n", req->buf.len, req->buf.base );
    if ( s->read_sz[s->rhalf^1] != 0 ) {
      s->read_sz[s->rhalf] = 0;
      s->rhalf ^= 1;
    }
    //TODO uv_write((uv_write_t*) req, client, &req->buf, 1, on_write_done);

  } 
  else {
    printf( " bad cmd: %d topic %d slot %d\n", cmd, topic, slot );
    fprintf(stderr, "Received bad cmd %02x\n", cmd);
    exit(1);
  }
  }
     
}

char recv_buffer[BUFFER_SIZE];

void readFromClient(aeEventLoop *loop, int fd, void *clientdata, int mask)
{
    int size;
    size = read(fd, recv_buffer, BUFFER_SIZE);

    my_conn_t* conn = (my_conn_t*)clientdata;
    on_data( conn, size, recv_buffer );
}

void acceptTcpHandler(aeEventLoop *loop, int fd, void *clientdata, int mask)
{
  int client_port, client_fd;
  char client_ip[128];
  // create client socket
  client_fd = anetTcpAccept(NULL, fd, client_ip, 128, &client_port);
  printf("Accepted %s:%d\n", client_ip, client_port);

  // set client socket non-block
  anetNonBlock(NULL, client_fd);

  my_conn_t *conn = (my_conn_t*) malloc(sizeof(my_conn_t));
  conn_init(conn);

  // regist on message callback
  int ret;
  ret = aeCreateFileEvent(loop, client_fd, AE_READABLE, readFromClient, (void*)conn);
  assert(ret != AE_ERR);
}


static void sig_handler(const int sig) {
  printf( " %d writes time taken %f \n ", num_writes, ((double)(clock()-start_time))/CLOCKS_PER_SEC );
  //printf( " %d writes time taken %f \n ", num_writes, time(NULL)-start_time ); 
  printf("Signal handled: %s.\n", strsignal(sig));
  exit(EXIT_SUCCESS);
}

static void usage(void) {
  //printf(PACKAGE " " VERSION "\n");
  printf( "Mrq Version 0.1\n"
          "    -p, --port=<num>          TCP port to listen on (default: 7000)\n"
          "    -m, --max-socket=<megabytes>  Maximum amount of memory in mb (default: 256)\n"
          "\n"
        );
}
int main (int argc, char **argv) {

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  char *shortopts =
    "h"
    "m:"
    "p:"
    ;
  const struct option longopts[] = {
    {"help",             no_argument, 0, 'h'},
    {"max-memory", required_argument, 0, 'm'},
    {"port",       required_argument, 0, 'p'},
    {0, 0, 0, 0}
  };

  settings.port = 7000;
  settings.max_memory = 256;

  int optindex, c;
  while (-1 != (c = getopt_long(argc, argv, shortopts, longopts, &optindex))) {
    switch (c) {
    case 'p':
      settings.port = atoi(optarg);
      break;
    case 'm':
      settings.max_memory = atoi(optarg);
      break;
    case 'h':
      usage();
      return(2);
      break;
    default:
      usage();
      return(2);
    }
  }

  settings.max_memory *= 1024*1024;

  int ipfd;
  ipfd = anetTcpServer(NULL, settings.port, "0.0.0.0", 0);
  assert(ipfd != ANET_ERR);

  aeEventLoop *loop;
  loop = aeCreateEventLoop(1024);

  int ret;
  ret = aeCreateFileEvent(loop, ipfd, AE_READABLE, acceptTcpHandler, NULL);
  assert(ret != AE_ERR);

  aeMain(loop);

  aeDeleteEventLoop(loop);

  //int enable = 1;
  //setsockopt(sd, SOL_SOCKET, SO_OOBINLINE, &enable, sizeof(enable));
  
  return 0;
}
