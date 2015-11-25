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

struct Header
{
	short srcPort;
	short destPort;
	int seq;
	int ack;
	short dataLen;
	short checkSum;
};

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void getAck(char * buffer, struct Header * packet)
{
	memcpy(packet, buffer, HEADER_LEN);
}

void printPacket(char * packet)
{
	struct Header * pktHeader;
	memcpy(pktHeader, packet, HEADER_LEN);

	printf("srcPort: %d\ndestPort: %d\nseq: %d\n", pktHeader->srcPort,
		pktHeader->destPort, pktHeader->seq);
	printf("ack: %d\ndataLen: %d\ncheckSum: %d\n", pktHeader->ack,
		pktHeader->dataLen, pktHeader->checkSum);
	printf("%s", packet + HEADER_LEN);
}

int get_SequenceNumber(char* packet)
{
	struct Header* pktHeader;
	memcpy(pktHeader, packet, HEADER_LEN);

	return pktHeader->seq;
}

int get_datalen(char* packet)
{
	struct Header* pktHeader;
	memcpy(pktHeader, packet, HEADER_LEN);
	
	return (int)(pktHeader->dataLen);
}

int main(int argc, char *argv[])
{
	FILE* file_to_build;

    struct hostent* server;
    struct sockaddr_in serv_addr;
    double prob_loss = 0.0;
    double prob_corrupt = 0.0;
    char* filename;
    int expected_ACK_number = 0;
	int portno;
	int sockfd;
	struct Header header_data;

    //check for incorrect input
    if(argc != 6)
    {
        fprintf(stderr, "usage: %s <hostname> <server port> <filename> <probability loss> <probability corrupt>\n", argv[0]);
        exit(0);
    }

    char buffer[MAX_PACKETSIZE];
    char packetbuffer[20][MAX_PACKETSIZE];
    char packet[MAX_PACKETSIZE];

    portno = atoi(argv[2]);
    filename = argv[3];
    prob_loss = atof(argv[4]) * 100.0;
    prob_corrupt = atof(argv[5]) * 100.0;

	//for now...
	prob_loss = 0.0;
	prob_corrupt = 0.0;

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
    n = write(sockfd, filename,strlen(filename));
    if (n < 0)
        error("ERROR writing to socket");

    printf("Requested the file: %s\n", filename);

	file_to_build = fopen(filename, "wb");

    char* buffer_from_server;
    while(1)
    {
        //run until we receive the FIN packet
        if(read(sockfd,buffer_from_server,sizeof(buffer_from_server)) < 0)
            error("ERROR reading from socket.");

        //packet loss
        if(rand() % 100 <= prob_loss)
        {
            //don't do anything special
            printf("A data packet was lost!! The sequence number was: ");
        }

        //packet corrpution
        else if(rand() % 100 <= prob_corrupt)
        {
            printf("A data packet is corrupted!! The sequence number was: ");
            //send out an ACK that we expected
            //prep up the packet
            sprintf(packet[0], "%d", portno); //src port
		    //dst port?
			sprintf(packet[2], "%d", expected_ACK_number - 1); //seq field
			if(write(sockfd, packet, sizeof(packet)) < 0)
			{
				error("Unable to write to sockfd");
			}

	    }

        //nothing bad happened
        else
        {
			int seq_no = get_SequenceNumber(buffer_from_server);
			printf("Received packet of seq no.: %d\n", seq_no);
			fflush(file_to_build);
			

			//copy all data from the buffer
			int i;
			int j = 0;
			int datalen = get_datalen(buffer_from_server);
			for (i = HEADER_LEN; i < datalen; i++)
			{
				buffer[j] = buffer_from_server[i];
			}

			fprintf(file_to_build, "%s", buffer);
			fflush(file_to_build);

			if (seq_no == expected_ACK_number)
			{
				//missing srcport for now...
				header_data.destPort = (short)serv_addr.sin_port;
				header_data.ack = seq_no;
				header_data.dataLen = sizeof(short)*2 + sizeof(int);
				bzero(packet, MAX_PACKETSIZE);
				memcpy(packet, (void*)&header_data, HEADER_LEN);
				n = write(sockfd, packet, sizeof(packet));
				if (n < 0)
					error("Something went wrong when sending the packet under normal conditions...");

				expected_ACK_number += 1;
			}


        }



    }//end while

	
	fclose(file_to_build);
	close(sockfd);

	return 0;


}
