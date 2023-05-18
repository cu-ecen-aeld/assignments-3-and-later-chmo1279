#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>
#include "queue.h"

#define PORT "9000"   // port we're listening on
#define DATAOUTPUTFILE "/var/tmp/aesdsocketdata" // file to save received data

// memory related
char *text;
FILE *fptr;

//mutex related
pthread_mutex_t lock;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void sig_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM){
        // complete open connection operations
        // close any open sockets
        fclose(fptr);
        remove(DATAOUTPUTFILE); // delete the file /var/tmp/aesdsocketdata
        syslog(LOG_SYSLOG, "Caught signal, exiting");
    }
}

int _daemon(int nochdir, int noclose) 
{
    pid_t pid;
    pid = fork(); // Fork off the parent process
    if (pid < 0) 
        exit(EXIT_FAILURE);
    if (pid > 0) 
        exit(EXIT_SUCCESS);
    return 0;
}
//timer related definitions
void expired();
pid_t gettid(void);

//Timer Thread
//based code madified from https://opensource.com/article/21/10/linux-timers
void *timer(){
    int res = 0;
    timer_t timerId = 0;

    /*  sigevent specifies behaviour on expiration  */
    struct sigevent sev = { 0 };

    /* specify start delay and interval
     * it_value and it_interval must not be zero */

    struct itimerspec its = {   .it_value.tv_sec  = 10,
                                .it_value.tv_nsec = 0,
                                .it_interval.tv_sec  = 10,
                                .it_interval.tv_nsec = 0
                            };

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = &expired;


    /* create timer */
    res = timer_create(CLOCK_REALTIME, &sev, &timerId);
    

    if (res != 0){
        fprintf(stderr, "Error timer_create: %s\n", strerror(errno));
        exit(-1);
    }

    /* start timer */
    res = timer_settime(timerId, 0, &its, NULL);

    if (res != 0){
        fprintf(stderr, "Error timer_settime: %s\n", strerror(errno));
        exit(-1);
    }
    return NULL;
}

// When timer expires (modified code from https://www.tutorialspoint.com/c_standard_library/c_function_strftime.htm)
void expired(){
    time_t rawtime;
    struct tm *info;
    char buffer[80];
 
    time( &rawtime );
 
    info = localtime( &rawtime );
 
    strftime(buffer,80,"timestamp:%c\n", info);
    pthread_mutex_lock(&lock);
    fptr = fopen(DATAOUTPUTFILE, "a+");
    fputs(buffer ,fptr); 
    fclose(fptr);
    pthread_mutex_unlock(&lock);
}

//this is a dirty struct... no one should do this... (ie mark for refactor/cleanup***
typedef struct connection_params {
    int i; //this is connection instance number
    int listener;
    socklen_t addrlen;
    struct sockaddr_storage remoteaddr;
    int newfd;
    fd_set master;
    int fdmax;
    char remoteIP[INET6_ADDRSTRLEN];
} connection_instance;

void *connection_data_thread(connection_instance *connection_data_thread_pointer)
{
    //connection_instance connection_data_thread_pointer = connection_instance_p;
    int         i                           = connection_data_thread_pointer->i;
    int         listener                    = connection_data_thread_pointer->listener;
    int         nbytes;
    int         newfd                       = connection_data_thread_pointer->newfd;
    int         fdmax                       = connection_data_thread_pointer->fdmax;
    char        remoteIP[INET6_ADDRSTRLEN]; //  = connection_data_thread_pointer.remoteIP;
    char        buf[200];                // = connection_data_thread_pointer.buf;    // buffer for client data
    socklen_t   addrlen                     = connection_data_thread_pointer->addrlen;
    fd_set      master                      = connection_data_thread_pointer->master;
    int j;
    struct      sockaddr_storage remoteaddr;

    strncpy(remoteIP, connection_data_thread_pointer->remoteIP, sizeof(connection_data_thread_pointer->remoteIP));
    
    if (i == listener) {
        // handle new connections
        addrlen = sizeof remoteaddr;
        printf("listener before accept: %d\n",listener);
        printf("i before accept: %d\n",i);
        newfd = accept(listener,
            (struct sockaddr *)&remoteaddr,
            &addrlen);
        printf("listener after accept: %d\n",listener);
        if (newfd == -1) {
            perror("accept");
        } else {
            FD_SET(newfd, &master); // add to master set
            if (newfd > fdmax) {    // keep track of the max
                fdmax = newfd;
            }
            
            syslog(LOG_SYSLOG, "Accepted connection from %s", 
                    inet_ntop(remoteaddr.ss_family,
                    get_in_addr((struct sockaddr*)&remoteaddr),
                    remoteIP, INET6_ADDRSTRLEN));
    
            printf("aesdsocket: new connection from %s on "
                "socket %d\n",
                inet_ntop(remoteaddr.ss_family,
                    get_in_addr((struct sockaddr*)&remoteaddr),
                    remoteIP, INET6_ADDRSTRLEN),
                newfd);
        }
    } else {
        printf("America's most blunted\n");
        // handle data from a client
        memset(buf, 0, sizeof buf); 
        if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
            // got error or connection closed by client
            if (nbytes == 0) {
                // connection closed
                printf("aesdsocket: socket %d hung up\n", i);
                syslog(LOG_SYSLOG, "Closed connection from %s", 
                    inet_ntop(remoteaddr.ss_family,
                    get_in_addr((struct sockaddr*)&remoteaddr),
                    remoteIP, INET6_ADDRSTRLEN));
    
            } else {
                perror("recv");
            }
            close(i); // bye!
            FD_CLR(i, &master); // remove from master set
        } else {
            // we got some data from a client
            // write to file
            pthread_mutex_lock(&lock);
            fptr = fopen(DATAOUTPUTFILE, "a+");
            fputs(buf, fptr);
            fclose(fptr);
            pthread_mutex_unlock(&lock);
            fptr = fopen(DATAOUTPUTFILE, "a+");
            fseek(fptr, 0L, SEEK_END);
            long numbytes = ftell(fptr);
            fseek(fptr,0L,SEEK_SET);
            text = malloc(numbytes);
            fread(text, sizeof(char),numbytes, fptr);
            for(j = 0; j <= fdmax; j++) {
                // send to everyone!
                if (FD_ISSET(j, &master)) {
                    // except the listener and ourselves
                    if (j != listener) {
                        
                        if (send(j, text, numbytes , 0) == -1) {
                            perror("send");
                        }
                    }
                }
            }
            free(text);
            fclose(fptr);
        }
    } // END handle data from client
}

    

int main(int argc, char *argv[])
{

    //command line arg processing
    if (argc == 1) {
        printf("Entering interactive mode...\n");
    } else if (argc == 2) {
        if (!(strcmp(argv[1], "-d"))){ // not sure why strcmp is *not*, but this is for option "-d"
            printf("Entering daemon mode...\n");
            _daemon(0,0); // start as daemon (fork as child and terminate parent)
        }
        else {
            printf("Unknown argument %s.\n", argv[1]);
            return -1;
        }
    } else if( argc > 2 ) {
        printf("Too many arguments supplied.\n");
        return -1;
    } 

    //start timer thread
    pthread_t timerThread;
    pthread_t connThread[64];
    pthread_create(&timerThread,NULL, timer, NULL);
    pthread_join(timerThread, NULL);

    connection_instance connection_data_thread_pointer[64];

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accepted socket descriptor
    socklen_t addrlen;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    //handle signals
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");
    if (signal(SIGTERM, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGTERM");

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "aesdsocket: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "aesdsocket: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
/* start a thread for new connection*/ 
               printf("listener before thread spinup: %d\n",listener);           
               connection_data_thread_pointer[i].i           = i;                           
               connection_data_thread_pointer[i].listener    = listener;                     
               connection_data_thread_pointer[i].newfd       = newfd;                        
               connection_data_thread_pointer[i].fdmax       = fdmax;                        
               connection_data_thread_pointer[i].addrlen     = addrlen;
               connection_data_thread_pointer[i].master      = master;                       
               strncpy(connection_data_thread_pointer[i].remoteIP, remoteIP, sizeof(remoteIP));
               pthread_create(&connThread[i], NULL, connection_data_thread, (void *)&connection_data_thread_pointer[i]);
               pthread_join((pthread_t)connThread[i], NULL);
/*end thread*/
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    return 0;
}


