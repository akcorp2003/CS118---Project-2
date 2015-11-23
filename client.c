#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>

static int MAX_PACKETSIZE = 1000;
static int HEADER_LEN = 16;
static int HEADER_FIELDS = 6;
int timeout = 0;

/*
 * header format:
 * src port 2 bytes
 * dest port 2 bytes
 * seq num 4 bytes
 * ack num 4 bytes
 * data len 2 bytes
 * checksum 2 bytes
 * 16 bytes total
*/

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    struct hostent* server;
    struct sockaddr_in serv_addr;
    double prob_loss = 0.0;
    double prob_corrupt = 0.0;
    char* filename;
    int expected_packet_number = 0;

    //check for incorrect input
    if(argc != 6)
    {
        fprintf(stderr, "usage: %s <hostname> <server port> <filename> <probability loss> <probability corrupt>\n", argv[0]);
        exit(0);
    }

    char buffer[256];
    char packetbuffer[20][MAX_PACKETSIZE];
    char packet[HEADER_FIELDS][HEADER_LEN];

    portno = atoi(argv[2]);
    filename = argv[3];
    prob_loss = atof(argv[4]) * 100.0;
    prob_corrupt = atof(argv[5]) * 100.0;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0)
        error("ERROR opening socket");

    //set up the socket
    server = gethostbyname(argv[1]);

    if(server == NULL)
    {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) //establish a connection to the server
        error("ERROR connecting");

    int n;
    //send the request over to the server
    n = write(sockfd,,strlen());
    if (n < 0)
        error("ERROR writing to socket");

    printf("Requested the file: %s\n", filename);

    char* buffer_from_server;
    while(1)
    {
        //run until we receive the FIN packet
        if(read(sockfd,,strlen() < 0)
            error("ERROR reading from socket.");

        //packet loss
        if(rand() % 100 < prob_loss)
        {
            //don't do anything special
            printf("A data packet was lost!! The sequence number was: ");
        }

        //packet corrpution
        else if(rand() % 100 < prob_corrupt)
        {
            printf("A data packet is corrupted!! The sequence number was: ");
            //send out an ACK that we expected
            //prep up the packet
            sprintf(packet[0], "%d", portno); //src port
	   //dst port?
	    sprintf(packet[2], "%d", expected_ack_number - 1); //seq field
	    if(write(sockfd, packet, strlen(packet)< 0)
	    {
		error("Unable to write to sockfd");
	    }

        }

        //nothing bad happened
        else
        {

        }



    }//end while










}
