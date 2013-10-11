#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define PORT "9034"   // port we're listening on
#define TRUE 1
#define FALSE 0
#define DEBUG 0

int currentpendingfd;

typedef int bool;
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* Compare's entered key with secret key and returns TRUE if same */
bool key_auth(char* entered_key)
{
    printf("Authorising user with key: %s\n", entered_key);
    FILE *fp;
    char key[256];
    fp = fopen("bin/key.txt","r");
    if (fp == NULL) 
    {
        printf ("File not created okay, errno = %d\n", errno);
        return FALSE;
    } else
    {
        printf("Scanning file\n");
        fscanf(fp, "%s", key);      // Would normally check for error when reading from file
        printf("key: %s\n", key);
        int keydiff;
        if (keydiff = strcmp(entered_key, key) != 0)
        {
            printf("Key was incorrect by: %i\n", keydiff);
            return FALSE;
        } else {
            printf("Key was correct\n");
            return TRUE;
        }
    }
    fclose (fp);
}


void getKey()
{
    FILE *fp;
    char key[256];
    fp = fopen ("bin/key.txt","r");
    fscanf(fp, "%s", key);
    send(currentpendingfd, key, sizeof key, 0);
    printf("key: %s\n", key);
}

void receive_key(int *nbytes, int local_fd, char* buf)
{
    char buffer[128];
    char first_buf[512];
    *nbytes = recv(local_fd, first_buf, sizeof first_buf, 0);
    strcpy(buf, first_buf);
    strcpy(buffer, first_buf);
}

int main(int argc, char* argv[])
{
    char buf[256];    // buffer for client data
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    fd_set pending_fds; // Pre-authenticated connections
    fd_set readpending_fds;
    int fdmax;        // maximum file descriptor number. Old closed connections are reused but fdmax stays as the max number ever connected
    int pendingmax;
    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;
    struct timeval timeout;
    

    
    int nbytes;
    int bytecount;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    FD_ZERO(&pending_fds);
    FD_ZERO(&readpending_fds);


    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
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
        fprintf(stderr, "selectserver: failed to bind\n");
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
    pending_fds = master;
    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one
    pendingmax = listener;

    // main loop
    for(;;) {
        timeout.tv_sec=0;
        timeout.tv_usec=500;
        read_fds = master; // copy it
        readpending_fds = pending_fds;
        /* Modifies &read_fds to only have connections that are still 
        connected and drops those that are no longer connected */
        if (select(fdmax+1, &read_fds, NULL, NULL, &timeout) == -1) {
            perror("select");
            exit(4);
        }
        if (select(fdmax+1, &readpending_fds, NULL, NULL, &timeout) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &readpending_fds)) {
                if (i != listener){
                    currentpendingfd = i;
                    receive_key(&nbytes, i,buf);
                    if (nbytes <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &pending_fds); // remove from master set
                    } else {
                        buf[nbytes-1] = '\0';
                        if (key_auth(buf) == 1){
                            char authstring[] = "Thank you for joining\n";
                            FD_SET(i, &master);
                            if (i > fdmax)
                            {
                                fdmax = i;
                            }
                            FD_CLR(i, &pending_fds);
                            send(i, authstring, sizeof authstring, 0);
                        } else {
                            char noauthstring[] = "Wrong password sorry\n";
                            send(i, noauthstring, sizeof noauthstring, 0);
                            FD_CLR(i, &pending_fds);
                            close(i);
                        }
                    }
                }
            }
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &pending_fds);
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        char passprompt[] = "Please enter the password\n";
                        send(newfd, passprompt, sizeof passprompt, 0 );
                        printf("selectserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }

                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client
                        for(j = 0; j <= fdmax; j++) {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) {
                                // except the listener and ourselves
                                if (j != listener && j != i) {
                                    if (send(j, buf, nbytes, 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
                    }
                } // END handle data from client   
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    return 0;
}