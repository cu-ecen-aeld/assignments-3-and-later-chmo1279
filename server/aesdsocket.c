#include <fcntl.h>
#include <stdbool.h>
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
#include <linux/unistd.h>
#include <sys/queue.h>

#define PORT "9000"   // port we're listening on
#define DATAOUTPUTFILE "/var/tmp/aesdsocketdata" // file to save received data

// memory related
char *text;
FILE *fptr;
fd_set master;

//mutex related
pthread_mutex_t lock;

//thread related

typedef struct threads{
    pthread_t ids;
    int new_fd;
    TAILQ_ENTRY(threads) nodes;
    bool finished;
} node_t;

typedef TAILQ_HEAD(head_s,node) head_t;
head_t head;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//pre termination house keeping
void *expurgation()
{
    for(;;){
        node_t *tmp = NULL;
        TAILQ_FOREACH(tmp, &head, nodes){
            if(tmp->finished == 1){
                pthread_join(tmp->ids, NULL);
            }
            TAILQ_REMOVE(&head, tmp, nodes);
            free(tmp);
            break;
        }
    }
    return NULL;
}

void sig_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM){
        // complete open connection operations
        // close any open sockets
        fptr = fopen(DATAOUTPUTFILE, "a+");
        fclose(fptr);
        remove(DATAOUTPUTFILE); // delete the file /var/tmp/aesdsocketdata
        syslog(LOG_SYSLOG, "Caught signal, exiting");
        expurgation();
        exit(1);
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

node_t *_fill_queue(head_t *head, const pthread_t threadid, const int new_fd)
{
    struct threads *e = malloc(sizeof(struct threads));
    if (e == NULL)
    {
        fprintf(stderr, "malloc failed");
        exit(EXIT_FAILURE);
    }
    e->ids = threadid;
    e->new_fd = new_fd;
    e->finished = 0;
    TAILQ_INSERT_TAIL(head, e, nodes);
    e = NULL;
    return TAILQ_LAST(head, head_s);
}

// Removes all of the elements from the queue before free()ing them.
void _free_queue(head_t * head)
{
    struct threads *e = NULL;
    while (!TAILQ_EMPTY(head))
    {
        e = TAILQ_FIRST(head);
        TAILQ_REMOVE(head, e, nodes);
        free(e);
        e = NULL;
    }
}

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
void expired()
{
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

void *connThread(void *conn_thread_data_ptr)
{
    char   buf[200];
    int    nbytes, numbytes;
    struct threads *conn_thread_data = conn_thread_data_ptr;
    struct stat file;
     
    memset(buf, 0, sizeof buf); 
    for(;;){
        // get data from a client
        nbytes = recv(conn_thread_data->new_fd, buf, sizeof buf, 0);
        
        // handle data recv'd
        if (nbytes == 0) {
            // connection closed
            printf("aesdsocket: socket %d hung up\n", conn_thread_data->new_fd);
            conn_thread_data->finished = 1;
            break;
        } 
        if (nbytes < 0) perror("recv");
        if (nbytes > 0) { // data recv'd
            // wite to file  
            pthread_mutex_lock(&lock);
            if (fptr = open(DATAOUTPUTFILE, O_RDWR | O_CREAT | O_APPEND, 0660) == -1) perror("open");
            if (write(fptr, buf, nbytes) == -1) perror("write");
            if (close(fptr) == -1) perror("close");
            pthread_mutex_unlock(&lock);
            
            //find proper position, read from file and return to sender
            if (fptr = open(DATAOUTPUTFILE, O_RDWR | O_CREAT | O_APPEND, 0660) == -1) perror("open");
            numbytes = lseek(fptr, 0, SEEK_END);            
            lseek(fptr, 0, SEEK_SET);
            printf("file size %d\n", numbytes);
            text = malloc(numbytes);
            read(fptr, text, numbytes);
            
            //re read output file and send to client
            if (send(conn_thread_data->new_fd, text, numbytes, 0) == -1) perror("send");
            close(fptr);
            free(text);
        }
    }
    return NULL;
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
    pthread_create(&timerThread,NULL, timer, NULL);
    pthread_join(timerThread, NULL);

    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accepted socket descriptor
    socklen_t addrlen;

    char remoteIP[INET6_ADDRSTRLEN];

    int tbool=1;        // for setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;
    struct sockaddr_storage remoteaddr;

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
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &tbool, sizeof(int));

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

    TAILQ_INIT(&head);

    // main loop
    for(;;) {
       // handle new connections
       addrlen = sizeof remoteaddr;
       newfd = accept(listener,
           (struct sockaddr *)&remoteaddr,
           &addrlen);
       if (newfd == -1) {
           perror("accept");
       } 
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
           
           node_t *connection_node = NULL;
           connection_node = _fill_queue(&head, 0, newfd);
           pthread_create(&(connection_node->ids), NULL, connThread,(void *)connection_node);
    }
    return 0;
}
