#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>
#include <string.h>

static int MAX_PACKETSIZE = 30;
static int HEADER_LEN = 20;
static int HEADER_FIELDS = 7;
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
    int flag;
};

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void printPacket(char * packet)
{
	struct Header pktHeader;
	memcpy((struct Header *) &pktHeader, packet, HEADER_LEN);

	printf("srcPort: %d\ndestPort: %d\nseq: %d\n", pktHeader.srcPort,
		pktHeader.destPort, pktHeader.seq);
	printf("ack: %d\ndataLen: %d\ncheckSum: %d\n", pktHeader.ack,
		pktHeader.dataLen, pktHeader.checkSum);
        printf("flag: %d\n", pktHeader.flag);
	printf("data: %s", packet + HEADER_LEN);
        printf("\n");
}

int get_SequenceNumber(char* packet)
{
	struct Header pktHeader;
	memcpy((struct Header *) &pktHeader, packet, HEADER_LEN);

	return pktHeader.seq;
}

int get_ACKNumber(char* packet)
{
	struct Header pktHeader;
	memcpy((struct Header*) &pktHeader, packet, HEADER_LEN);

	return pktHeader.ack;
}

int get_datalen(char* packet)
{
	struct Header pktHeader;
	memcpy((struct Header *) &pktHeader, packet, HEADER_LEN);
	
	return (int) (pktHeader.dataLen);
}

int get_flag(char * packet)
{
  struct Header pktHeader;
  memcpy((struct Header *) &pktHeader, packet, HEADER_LEN); 
  return pktHeader.flag;
}

int main(int argc, char *argv[])
{
	FILE* file_to_build;

    struct hostent* server;
    struct sockaddr_in serv_addr, cli_addr;
    double prob_loss = 0.0;
    double prob_corrupt = 0.0;
    char* filename;
    char * filename2;
    int expected_SEQ_number = 0;
    int recent_received_SEQ_number = 0;
    int portno;
    int sockfd;
    struct Header header_data;
    time_t t;

    //check for incorrect input
    if(argc != 6)
    {
        fprintf(stderr, "usage: %s <hostname> <server port> <filename> <probability loss> <probability corrupt>\n", argv[0]);
        exit(0);
    }

    char buffer[MAX_PACKETSIZE];
    char packetbuffer[20][MAX_PACKETSIZE];
    char packet[MAX_PACKETSIZE];

    //initialize random generator
    srand((unsigned) time(&t));

    portno = atoi(argv[2]);
    filename = argv[3];
    prob_loss = atof(argv[4]) * 100.0;
    prob_corrupt = atof(argv[5]) * 100.0;

	//for now...
	//prob_loss = 0.0;
	//prob_corrupt = 0.0;

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


    cli_addr.sin_family = AF_INET;
    cli_addr.sin_addr.s_addr = INADDR_ANY;
    cli_addr.sin_port = htons(portno * 2);

	if (bind(sockfd, (struct sockaddr *) &cli_addr, sizeof(cli_addr)) < 0)
	{
		error("ERROR on binding");
	}
		

	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		error("ERROR on connecting");
	}

    int n;
    //send the file request over to the server
    header_data.srcPort = ntohs(cli_addr.sin_port);
	header_data.destPort = ntohs(serv_addr.sin_port);
	header_data.seq = 0;
	header_data.ack = 0;
	header_data.dataLen = (short) strlen(filename);
	bzero(packet, MAX_PACKETSIZE);
    int i;
    int j = 0;
    for(i = HEADER_LEN; i < HEADER_LEN + strlen(filename); i++)
    {
        packet[i] = filename[j];
        j++; 
    }
    packet[HEADER_LEN + strlen(filename)] = '\0';
	memcpy(packet, (struct Header*) &header_data, HEADER_LEN);
	if(write(sockfd, packet, sizeof(packet)) < 0)
	{
		error("Unable to write to sockfd");
	}
    printf("Packet Sent: \n");
    printPacket(packet);
    /*
    n = write(sockfd, filename,strlen(filename));
    if (n < 0)
        error("ERROR writing to socket");
    */

    printf("Requested the file: %s\n", filename);
    printf("\n");

    filename2 = (char *) malloc((strlen(filename)+2) * sizeof(char));
    memcpy(filename2 + 1, filename, strlen(filename));
    filename2[0] = '2';
    filename2[strlen(filename) + 1] = '\0';

	file_to_build = fopen(filename2, "w+");

    char buffer_from_server[2048];
    header_data.flag = 0;
    int sizeServAddr = sizeof(serv_addr);

    while(1)
    {
        printf("\n");
        //run until we receive the FIN packet
        n = read(sockfd,buffer_from_server,sizeof(buffer_from_server));
        if (n < 0) error("ERROR reading from socket");

        int seq_no = get_SequenceNumber(buffer_from_server);

        //packet loss
        if(rand() % 100 < prob_loss)
        {
			//do nothing special
            printf("A data packet was lost!! The sequence number was: %d\n", get_SequenceNumber(buffer_from_server));
        }

        //packet corruption
        else if(rand() % 100 < prob_corrupt)
        {
            printf("A data packet is corrupted!! The sequence number was: %d\n", get_SequenceNumber(buffer_from_server));

            //send out an ACK that we expected
			header_data.srcPort = ntohs(cli_addr.sin_port);
			header_data.destPort = ntohs(serv_addr.sin_port);
			header_data.seq = 0;
			header_data.ack = recent_received_SEQ_number;
			header_data.dataLen = (short)0;
			bzero(packet, MAX_PACKETSIZE);
			memcpy(packet, (struct Header*) &header_data, HEADER_LEN);
			if(write(sockfd, packet, sizeof(packet)) < 0)
			{
				error("Unable to write to sockfd");
			}

			printf("Packet sent with ACK: %d\n", recent_received_SEQ_number);
            printPacket(packet);

	    }

        //nothing bad happened
        else
        {
			printf("\nReceived packet of seq no.: %d\n", seq_no);
            printf("Packet Received:\n");
            printPacket(buffer_from_server);

			//copy all data from the buffer
			int i = HEADER_LEN;
			int j = 0;
			int datalen = get_datalen(buffer_from_server);
            memset(buffer, 0, sizeof(buffer));
			for (j = 0; j < datalen; j++)
			{
				buffer[j] = buffer_from_server[i];
                                i++;
			}
            buffer[j] = '\0';

			fprintf(file_to_build, "%s", buffer);

			if (seq_no == expected_SEQ_number)
			{
				  header_data.srcPort = ntohs(cli_addr.sin_port);
			      header_data.destPort = ntohs(serv_addr.sin_port);
				  header_data.seq = 0;
				  header_data.ack = seq_no + datalen;
				  header_data.dataLen = (short) 0;
				  bzero(packet, MAX_PACKETSIZE);
				  memcpy(packet,(struct Header *) &header_data, HEADER_LEN);
				  n = write(sockfd, packet, sizeof(packet));
				  if (n < 0)
				  {
					  error("Something went wrong when sending the packet under normal conditions...");
				  }
		   	    
				  printf("\nACK sent:\n");
				  printPacket(packet);
		  		  expected_SEQ_number += datalen;
                               
				  if (get_flag(buffer_from_server) == 1) //1 represents the FIN packet
				  {
					  break;
				  }
                           
			}//end if
            
			else
			{
				printf("\nReceived a packet that had an unexpected SEQ number...\n");
                printf("Expecting seq no.: %d\n", expected_SEQ_number);
                printf("Received seq no. of: %d\n", seq_no);

				//send out an ACK that we expected
				header_data.srcPort = ntohs(cli_addr.sin_port);
				header_data.destPort = ntohs(serv_addr.sin_port);
				header_data.seq = 0;
				header_data.ack = expected_SEQ_number;
				header_data.dataLen = (short)0;
				bzero(packet, MAX_PACKETSIZE);
				memcpy(packet, (struct Header*) &header_data, HEADER_LEN);
				if (write(sockfd, packet, sizeof(packet)) < 0)
				{
					error("Unable to write to sockfd");
				}

				printf("Packet sent with ACK: %d\n", expected_SEQ_number);
                                printPacket(packet);
                /*
                if (get_flag(buffer_from_server) == 1) //1 represents the FIN packet
				{
				     break;
				}*/
			}
            

        }//end else
    }//end while

	fclose(file_to_build);
	close(sockfd);

	return 0;
}

