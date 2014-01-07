/* mjpeg HTTP stream decoder */

#include <sys/types.h>

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mjpegrx.h"
#include "mjpeg_sck.h"
#include "mjpeg_mutex.h"

/* Returns the larger of a or b */
#define mjpeg_max(a, b) ((a > b) ? a : b)

char * strtok_r_n(char *str, char *sep, char **last, char *used);

#ifdef _WIN32
void
_sck_wsainit(){
    WORD vs;
    WSADATA wsadata;

    vs = MAKEWORD(2, 2);
    WSAStartup(vs, &wsadata);

    return;
}
#endif

/* Read a byte from sd into (*buf)+*bufpos . If afterwards,
   *bufpos == (*bufsize)-1, reallocate the buffer as 1024
   bytes larger, updating *bufsize . This function blocks
   until either a byte is received, or cancelfd becomes
   ready for reading. */
int
mjpeg_rxbyte(char **buf, int *bufpos, int *bufsize, int sd, int cancelfd){
    int bytesread;

    bytesread = mjpeg_sck_recv(sd, (*buf)+*bufpos, 1, cancelfd);
    if(bytesread == -1) return -1;
    if(bytesread != 1) return 1;
    (*bufpos)++;

    if(*bufpos == (*bufsize)-1){
        *bufsize += 1024;
        *buf = realloc(*buf, *bufsize);
    }

    return 0;
}

/* Read data up until the character sequence "\r\n\r\n" is
   received. */
int
mjpeg_rxheaders(char **buf_out, int *bufsize_out, int sd, int cancelfd){
    int allocsize;
    int bufpos;
    char *buf;

    bufpos = 0;
    allocsize = 1024;
    buf = malloc(allocsize);

    while(1){
        if(mjpeg_rxbyte(&buf, &bufpos, &allocsize, sd, cancelfd) != 0){
            free(buf);
            return -1;
        }
        if(bufpos >= 4 && buf[bufpos-4] == '\r' && buf[bufpos-3] == '\n' &&
            buf[bufpos-2] == '\r' && buf[bufpos-1] == '\n'){
            break;
        }
    }
    buf[bufpos] = '\0';

    *buf_out = buf;
    *bufsize_out = bufpos;

    return 0;
}

/* mjpeg_sck_recv() blocks until either len bytes of data have
   been read into buf, or cancelfd becomes ready for reading.
   If either len bytes are read, or cancelfd becomes ready for
   reading, the number of bytes received is returned. On error,
   -1 is returned, and errno is set appropriately. */
int
mjpeg_sck_recv(int sockfd, void *buf, size_t len, int cancelfd)
{
    int error;
    int nread;
    fd_set readfds;
    fd_set exceptfds;

    nread = 0;
    while(nread < len) {
        FD_ZERO(&readfds);
        FD_ZERO(&exceptfds);

        /* Set the sockets into the fd_set s */
        FD_SET(sockfd, &readfds);
        FD_SET(sockfd, &exceptfds);
        if(cancelfd) {
            FD_SET(cancelfd, &readfds);
            FD_SET(cancelfd, &exceptfds);
        }

        error = select(mjpeg_max(sockfd, cancelfd)+1, &readfds, NULL, &exceptfds, NULL);
        if(error == -1) {
            return -1;
        }

        /* If an exception occurred with either one, return error. */
        if((cancelfd && FD_ISSET(cancelfd, &exceptfds)) ||
            FD_ISSET(sockfd, &exceptfds)) {
            return -1;
        }

        /* If cancelfd is ready for reading, return now with what
           we have read so far. */
        if(cancelfd && FD_ISSET(cancelfd, &readfds)) {
            return nread;
        }

        /* Otherwise, read some more. */
        error = recv(sockfd, buf+nread, len-nread, 0);
        if(error < 1) {
            return -1;
        }
        nread += error;
    }

    return nread;
}

/* mjpeg_sck_connect() attempts to connect to the specified
   remote host on the specified port. The function blocks
   until either cancelfd becomes ready for reading, or the
   connection succeeds or times out.
   If the connection succeeds, the new socket descriptor
   is returned. On error, -1 isreturned, and errno is
   set appropriately. */
int
mjpeg_sck_connect(char *host, int port, int cancelfd)
{
    int sd;
    int error;
    int error_code;
    int error_code_len;

    struct hostent *hp;
    struct sockaddr_in pin;
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;

    #ifdef _WIN32
    _sck_wsainit();
    #endif

    /* Create a new socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if(mjpeg_sck_valid(sd) == 0) return -1;

    /* Set the non-blocking flag */
    error = mjpeg_sck_setnonblocking(sd, 1);
    if ( error != 0){
        return error;
    }

    /* Resolve the specified hostname to an IPv4
       address. */
    hp = gethostbyname(host);
    if(hp == NULL) {
        mjpeg_sck_close(sd);
        return -1;
    }

    /* Set up the sockaddr_in structure. */
    memset(&pin, 0, sizeof(struct sockaddr_in));
    pin.sin_family = AF_INET;
    pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    pin.sin_port = htons(port);

    /* Try to connect */
    error = connect(
        sd,
        (struct sockaddr *)&pin,
        sizeof(struct sockaddr_in));

#ifdef _WIN32
    if(error != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
        mjpeg_sck_close(sd);
        return -1;
    }
#else
    if(error != 0 && errno != EINPROGRESS) {
        mjpeg_sck_close(sd);
        return -1;
    }
#endif

    /* select(2) for reading and exceptions on the socket and
       cancelfd. */
    FD_ZERO(&readfds);
    FD_SET(cancelfd, &readfds);

    FD_ZERO(&writefds);
    FD_SET(sd, &writefds);

    FD_ZERO(&exceptfds);
    FD_SET(sd, &exceptfds);
    FD_SET(cancelfd, &exceptfds);
    error = select(mjpeg_max(sd, cancelfd)+1, &readfds, &writefds, &exceptfds, NULL);
    if(error == -1) {
        mjpeg_sck_close(sd);
        return -1;
    }

    /* We were interrupted by data at cancelfd before we could
       finish connecting. */
    if(FD_ISSET(cancelfd, &readfds)) {
        mjpeg_sck_close(sd);
#ifdef _WIN32
        WSASetLastError(WSAETIMEDOUT);
#else
        errno = ETIMEDOUT;
#endif
        return -1;
    }

    /* Something bad happened. Probably one of the sockets
       selected for an exception. */
    if(!FD_ISSET(sd, &writefds)) {
        mjpeg_sck_close(sd);
#ifdef _WIN32
        WSASetLastError(WSAEBADF);
#else
        errno = EBADF;
#endif
        return -1;
    }

    /* Check that connecting was successful. */
    error_code_len = sizeof(error_code);
    error = getsockopt(sd, SOL_SOCKET, SO_ERROR, (void *) &error_code, (socklen_t *) &error_code_len);
    if(error == -1) {
      mjpeg_sck_close(sd);
      return -1;
    }
    if(error_code != 0) {
    /* Note: Setting errno on systems which either do not support
       it, or whose socket error codes are not consistent with its
       system error codes is a bad idea. */
#if _WIN32
        WSASetLastError(error);
#else
        errno = error;
#endif
        return -1;
    }

    /* We're connected */
    return sd;
}

char *
strchrs(char *s, char *c)
{
    char *t;
    char *ct;

    for(t = s; *t != '\0'; t++){
        for(ct = c; *ct != '\0'; ct++){
            if(*t == *ct) return t;
        }
    }

    return NULL;
}

/* A slightly modified version of strtok_r. When the function encounters a
   character in the separator list, that character is set into *used before
   the token is returned. This allows the caller to determine which separator
   character preceded the returned token. */
char *
strtok_r_n(char *str, char *sep, char **last, char *used)
{
    char *strsep;
    char *ret;

    if(str != NULL) *last = str;

    strsep = strchrs(*last, sep);
    if(strsep == NULL) return NULL;
    if(used != NULL) *used = *strsep;
    *strsep = '\0';

    ret = *last;
    *last = strsep+1;

    return ret;
}

/* Processes the HTTP response headers, separating them into key-value
   pairs. These are then stored in a linked list. The "header" argument
   should point to a block of HTTP response headers in the standard ':'
   and '\n' separated format. The key is the text on a line before the
   ':', the value is the text after the ':', but before the '\n'. Any
   line without a ':' is ignored. a pointer to the first element in a
   linked list is returned. */
struct keyvalue_t *
mjpeg_process_header(char *header)
{
    char *strtoksave;
    struct keyvalue_t *list = NULL;
    struct keyvalue_t *start = NULL;
    char *key;
    char *value;
    char used;

    header = strdup(header);
    if(header == NULL) return NULL;

    key = strtok_r_n(header, ":\n", &strtoksave, &used);
    if(key == NULL){
        return NULL;
    }

    while(1){ /* we break out inside */
        /* if no ':' exists on the line, ignore it */
        if(used == '\n'){
            key = strtok_r_n(
                NULL,
                ":\n",
                &strtoksave,
                &used);
            if(key == NULL) break;
            continue;
        }

        /* create a linked list element */
        if(list == NULL){
            list = malloc(sizeof(struct keyvalue_t));
            start = list;
        }
        else{
            list->next = malloc(sizeof(struct keyvalue_t));
            list = list->next;
        }
        list->next = NULL;

        /* save off the key */
        list->key = strdup(key);

        /* get the value */
        value = strtok_r_n(NULL, "\n", &strtoksave, NULL);
        if(value == NULL){
            list->value = strdup("");
            break;
        }
        value++;
        if(value[strlen(value)-1] == '\r'){
            value[strlen(value)-1] = '\0';
        }
        list->value = strdup(value);

        /* get the key for next loop */
        key = strtok_r_n(NULL, ":\n", &strtoksave, &used);
        if(key == NULL) break;
    }

    free(header);

    return start;
}

/* mjpeg_freelist() frees a key/value pair list generated by
   mjpeg_process_header() . */
void
mjpeg_freelist(struct keyvalue_t *list){
    struct keyvalue_t *tmp = NULL;
    struct keyvalue_t *c;

    for(c = list; c != NULL; c = c->next){
        if(tmp != NULL) free(tmp);
        tmp = c;
        free(c->key);
        free(c->value);
    }
    if(tmp != NULL) free(tmp);

    return;
}

/* Return the data in the specified list that corresponds
   to the specified key. */
char *
mjpeg_getvalue(struct keyvalue_t *list, char *key)
{
    struct keyvalue_t *c;
    for(c = list; c != NULL; c = c->next){
        if(strcmp(c->key, key) == 0) return c->value;
    }

    return NULL;
}

/* A platform independent wrapper function which acts like
   the call socketpair(AF_INET, SOCK_STREAM, 0, sv) . */
int
mjpeg_pipe(int sv[2])
{
#ifdef _WIN32
    return dumb_socketpair((SOCKET *) sv, 0);
#else
    /* return pipe(sv); */
    return socketpair(AF_LOCAL, SOCK_STREAM, 0, sv);
#endif
}

/* Create a new mjpegrx instance, and launch it's thread. This
   begins streaming of the specified MJPEG stream.
   On success, a pointer to an mjpeg_inst_t structure is returned.
   On error, NULL is returned.*/
struct mjpeg_inst_t *
mjpeg_launchthread(
        char *host,
        int port,
        char *reqpath,
        struct mjpeg_callbacks_t *callbacks
        )
{
    int error;
    int pipefd[2];
    struct mjpeg_inst_t *inst;

    /* Allocate an instance object for this instance. */
    inst = malloc(sizeof(struct mjpeg_inst_t));

    /* Fill the object's fields. */
    inst->host = strdup(host);
    inst->port = port;
    inst->reqpath = strdup(reqpath);

    /* Create a pipe that, when written to, causes any operation
       in the mjpegrx thread currently blocking to be cancelled. */
    error = mjpeg_pipe(pipefd);
    if(error != 0) {
      return NULL;
    }
    inst->cancelfdr = pipefd[0];
    inst->cancelfdw = pipefd[1];

    /* Copy the mjpeg_callbacks_t structure into it's corresponding
       field in the mjpeg_inst_t structure. */
    memcpy(
        &inst->callbacks,
        callbacks,
        sizeof(struct mjpeg_callbacks_t)
    );

    mjpeg_mutex_init(&inst->mutex);

    /* Mark the thread as running. */
    mjpeg_mutex_lock(&inst->mutex);
    inst->threadrunning = 1;
    mjpeg_mutex_unlock(&inst->mutex);

    /* Spawn the thread. */
    error = mjpeg_thread_create(&inst->thread, mjpeg_threadmain, inst);
    if(error != 0) {
        free(inst->host);
        free(inst->reqpath);
        free(inst);
        return NULL;
    }

    return inst;
}

/* Stop the specified mjpegrx instance. */
void
mjpeg_stopthread(struct mjpeg_inst_t *inst)
{
    /* Signal the thread to exit. */
    inst->threadrunning = 0;

    /* Cancel any currently blocking operations. */
    /* write(inst->cancelfdw, "U", 1); */
    send(inst->cancelfdw, "U", 1, 0);

    /* Wait for the thread to exit. */
    mjpeg_thread_join(&inst->thread, NULL);

    /* Free the mjpeg_inst_t and associated memory. */
    free(inst->host);
    free(inst->reqpath);
    mjpeg_mutex_destroy(&inst->mutex);
    free(inst);

    return;
}

/* The thread's main function. */
void *
mjpeg_threadmain(void *optarg)
{
    struct mjpeg_inst_t* inst = optarg;

    int sd;

    char *asciisize;
    int datasize;
    char *buf;
    char tmp[256];

    char *headerbuf;
    int headerbufsize;
    struct keyvalue_t *headerlist;

    int bytesread;

    /* Connect to the remote host. */
    sd = mjpeg_sck_connect(inst->host, inst->port, inst->cancelfdr);
    if(sd == -1){
    	fprintf(stderr, "mjpegrx: Connection failed\n");
        /* call the thread finished callback */
        if(inst->callbacks.donecallback != NULL){
            inst->callbacks.donecallback(
                inst->callbacks.optarg);
        }

        mjpeg_thread_exit(NULL);
        return NULL;
    }
    inst->sd = sd;

    /* Send the HTTP request. */
    snprintf(tmp, 255, "GET %s HTTP/1.0\r\n\r\n", inst->reqpath);
    send(sd, tmp, strlen(tmp), 0);
    printf("%s", tmp);

    int threadrunning = 1;
    while(threadrunning > 0){
        /* Read and parse incoming HTTP response headers. */
        if(mjpeg_rxheaders(&headerbuf, &headerbufsize, sd, inst->cancelfdr) == -1) {
            fprintf(stderr, "mjpegrx: recv(2) failed\n");
            break;
        }
        headerlist = mjpeg_process_header(headerbuf);
        free(headerbuf);
        if(headerlist == NULL) break;

        /* Read the Content-Length header to determine the
           length of data to read. */
        asciisize = mjpeg_getvalue(
            headerlist,
            "Content-Length");

        if(asciisize == NULL){
            mjpeg_freelist(headerlist);
            continue;
        }

        datasize = atoi(asciisize);
        mjpeg_freelist(headerlist);

        /* Read the JPEG image data. */
        buf = malloc(datasize);
        bytesread = mjpeg_sck_recv(sd, buf, datasize, inst->cancelfdr);
        if(bytesread != datasize){
            free(buf);
            fprintf(stderr, "mjpegrx: recv(2) failed\n");
            break;
        }

        if(inst->callbacks.readcallback != NULL){
            inst->callbacks.readcallback(
                buf,
                datasize,
                inst->callbacks.optarg);
        }
        free(buf);

        mjpeg_mutex_lock(&inst->mutex);
        threadrunning = inst->threadrunning;
        mjpeg_mutex_unlock(&inst->mutex);
    }

    /* The loop has exited. We should now clean up and exit the thread. */
    mjpeg_sck_close(sd);

    /* Call the user's donecallback() function. */
    if(inst->callbacks.donecallback != NULL){
        inst->callbacks.donecallback(inst->callbacks.optarg);
    }

    mjpeg_thread_exit(NULL);
    return NULL;
}

