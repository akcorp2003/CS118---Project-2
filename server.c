#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>
#include <signal.h>

static int maxPacketSize = 1000;
static int headerLen = 16;
int timeout = 0;

/*
header format:
src port 2 bytes
dest port 2 bytes
seq num 4 bytes
ack num 4 bytes
data len 2 bytes
checksum 2 bytes
16 bytes total
*/

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int isHit (int prob)
{
    time_t t;
    srand((unsigned) time(&t));
    if (rand() % 101 <= prob)
        return 1;
    return 0;
}

void setTimeout (int signum)
{
  timeout = 1;
}

int main(int argc, char *argv[])
{
     double probLoss, probCorruption;
     int sockfd, newsockfd, portno, pid, CWnd;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;

     if (argc < 5) {
         fprintf(stderr,"ERROR, missing argument\n");
         exit(1);
     }

     CWnd = atoi(argv[2]);
     probLoss = atoi(argv[3]);
     probCorruption = atoi(argv[4]);

     sockfd = socket(AF_INET, SOCK_DGRAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     memset((char *) &serv_addr, 0, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
              error("ERROR on binding");
     
     int n, loss = 0, corruption = 0, base = 0, nextSeqnum = 0, seq = 0,
	 ack = 0, numUnacked = 0;
     char buffer[256];
     char packetbuffer[20][maxPacketSize];
     int window[CWnd][2];

     while (1)
     {
   	 memset(buffer, 0, 256);
	 	
   	 n = recvfrom(sockfd, (void*) buffer, sizeof(buffer), 0, 
                     (struct sockaddr *) &cli_addr, &sizeof(cli_addr));

	 if (n == EINTR)
           if (timeout == 1)
           {
               timeout = 0;
               // TODO: resend all unacked
	       signal (SIGALRM, setTimeout);
               alarm (5);
	       continue; 
           }
           else
               error("ERROR reading from socket");
	 else if (n < 0) error("ERROR reading from socket");
   	 
         loss = isHit(probLoss); 
	 corruption = isHit(probCorruption);
	 if (loss != 1 && corruption != 1)
         {
             if (n < 0) error("ERROR writing to socket");
	     // TODO: if file request do this if ack do that
         }
	 else
         {
             loss = 0;
	     corruption = 0;
         }
     }
     close(sockfd);
         
     return 0; 
}


