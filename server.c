#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

static int maxPacketSize = 1000;
static int headerLen = 16;
static int packetBufferSize = 20;
int timeout = 0;
int fileRequest = 1;

struct Header {
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
	exit(1);
}

int isHit(int prob)
{
	if (prob == 0)
		return 0;
	time_t t;
	srand((unsigned)time(&t));
	if (rand() % 100 < prob)
		return 1;
	return 0;
}

void setTimeout(int signum)
{
	timeout = 1;
}

void getAck(char * buffer, struct Header * packet)
{
	memcpy(packet, buffer, headerLen);
}

void printPacket(char * packet)
{
	struct Header * pktHeader;
	memcpy(pktHeader, packet, headerLen);
	printf("srcPort: %d\ndestPort: %d\nseq: %d\n", pktHeader->srcPort,
		pktHeader->destPort, pktHeader->seq);
	printf("ack: %d\ndataLen: %d\ncheckSum: %d\n", pktHeader->ack,
		pktHeader->dataLen, pktHeader->checkSum);
	printf("%s", packet + headerLen);
}

int main(int argc, char *argv[])
{
	double probLoss, probCorruption;
	int sockfd, newsockfd, portno, pid, CWnd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	struct Header packetHeader;

	if (argc < 5) {
		fprintf(stderr, "ERROR, missing argument\n");
		exit(1);
	}

	CWnd = atoi(argv[2]);
	probLoss = atoi(argv[3]);
	probCorruption = atoi(argv[4]);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd < 0)
		error("ERROR opening socket");

	memset(&serv_addr, 0, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");

	int n, loss = 0, corruption = 0, base = 0, nextSeqnum = 0, seq = 0,
		ack = 0, numUnacked = 0, i, j, k, len, cumAck, sizeCliAddr;

	char buffer[256];
	char packetBuffer[packetBufferSize][maxPacketSize];
	int nxtPtr = 0, nxtFree = 0, count = 0;
	char unacked[CWnd][maxPacketSize + 1];
	struct Header recvAck, data;
	char * filename;
	char * temp;
	FILE * fp;
	char packet[maxPacketSize + 1];
	char cur;

	//col 1: 0 unacked, 1 acked or unused
	//col 2: window seq num
	//col 3: expected ack
	int window[CWnd][3];
	packet[maxPacketSize] = '\0';
	for (i = 0; i < CWnd; i++)
	{
		window[i][0] = 1;
		window[i][1] = 0;
		window[i][2] = 0;
	}
	signal(SIGALRM, setTimeout);
	while (1)
	{
		memset(buffer, 0, 256);
		// read if timeout has not occured
		if (timeout != 1)
		{
			sizeCliAddr = sizeof(cli_addr);
			n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
				(struct sockaddr *) &cli_addr, &sizeCliAddr);
		}
		else n = EINTR;
		// Timeout action
		if (n == EINTR)
			if (timeout == 1)
			{
				timeout = 0;
				k = base + CWnd;
				for (i = base; i < k; i++)
					for (j = 0; i < CWnd; j++)
						if (window[j][0] == 0 && window[j][1] == i)
						{
							n = sendto(sockfd, unacked[j], sizeof(unacked[j]), 0,
								(struct sockaddr *) &cli_addr, sizeof(cli_addr));
							if (n < 0)
								error("ERROR reading from socket");
							printPacket(unacked[j]);
							break;
						}
				//signal (SIGALRM, setTimeout);
				alarm(5);
				continue;
			}
			else
				error("ERROR reading from socket");
		else if (n < 0) error("ERROR reading from socket");
		// if no loss or corruption
		loss = isHit(probLoss);
		corruption = isHit(probCorruption);
		if (loss != 1 && corruption != 1)
		{
			// if client asks for file
			if (fileRequest == 1)
			{
				printf("File Requested: %s\n", buffer);
				memcpy(filename, buffer, n);
				filename[n] = '\0';
				fp = fopen(filename, "r");
				cur = (char)fgetc(fp);
				len = 0;
				data.srcPort = (short)serv_addr.sin_port;
				data.destPort = (short)cli_addr.sin_port;
				data.ack = 0;
				for (i = headerLen; cur != EOF; i++)
				{
					if (len < maxPacketSize - headerLen)
					{
						packet[i] = cur;
						cur = (char)fgetc(fp);
						len++;
					}
					else
					{
						data.seq = seq;
						data.dataLen = (short)len;
						data.checkSum = (short)1;
						memcpy(packet, (void *)&data, headerLen);
						if (nextSeqnum < base + CWnd)
							for (i = 0; i < CWnd; i++)
								if (window[i][0] == 1)
								{
									window[i][0] = 0;
									window[i][1] = nextSeqnum;
									window[i][2] = ack + len;
									n = sendto(sockfd, packet, sizeof(packet), 0,
										(struct sockaddr *) &cli_addr, sizeof(cli_addr));
									if (n < 0)
										error("ERROR writing to socket");
									printPacket(packet);
									temp = &unacked[i][0];
									temp = packet;
									seq += len;
									i = headerLen - 1;
									len = 0;
									nextSeqnum++;
									break;
								}
								else
								{
									memcpy(packetBuffer[nxtFree], packet,
										maxPacketSize);
									nxtFree++;
									count++;
								}
					}
				}
				data.seq = seq;
				data.dataLen = (short)len;
				data.checkSum = (short)1;
				memcpy(packet, (void *)&data, headerLen);
				if (nextSeqnum < base + CWnd)
					for (i = 0; i < CWnd; i++)
						if (window[i][0] == 1)
						{
							window[i][0] = 0;
							window[i][1] = nextSeqnum;
							window[i][2] = ack + len;
							n = sendto(sockfd, packet, sizeof(packet), 0,
								(struct sockaddr *) &cli_addr, sizeof(cli_addr));
							if (n < 0)
								error("ERROR writing to socket");
							printPacket(packet);
							temp = &unacked[i][0];
							temp = packet;
							seq += len;
							i = headerLen - 1;
							len = 0;
							nextSeqnum++;
							break;
						}
						else
						{
							memcpy(packetBuffer[nxtFree], packet,
								maxPacketSize);
							nxtFree++;
							count++;
						}
				fclose(fp);
				fileRequest = 0;
			}
			// if client sends ACK
			else
			{
				getAck(buffer, (struct Header *) &recvAck);
				cumAck = recvAck.ack;
				j = 0;
				for (i = 0; i < CWnd; i++)
				{
					if (window[i][1] >= base && cumAck >= window[i][2] &&
						window[i][0] == 0)
					{
						j++;
						window[i][0] = 1;
						if (window[i][1] == base)
						{
							//signal (SIGALRM, setTimeout);
							alarm(0);
						}
						if (count > 0)
						{
							count--;
							n = sendto(sockfd, packetBuffer[nxtPtr],
								sizeof(packetBuffer[nxtPtr]), 0,
								(struct sockaddr *) &cli_addr, sizeof(cli_addr));
							if (n < 0)
								error("ERROR writing to socket");
							printPacket(packetBuffer[nxtPtr]);
							nxtPtr = (nxtPtr + 1) % packetBufferSize;
							nxtFree = (nxtFree + 1) % packetBufferSize;
							// TODO: add stuff regarding window
						}
					}
				}
				base += j;
				for (i = 0; i < CWnd; i++)
					if (window[i][1] == base && window[i][0] == 0)
					{
						alarm(5);
						break;
					}
			}
		}
		// if loss or corruption do nothing
		else
		{
			if (loss == 1)
				printf("Packet Loss\n");
			if (corruption == 1)
				printf("Packet Corrupted\n");
			loss = 0;
			corruption = 0;
			printPacket(buffer);
		}
	}
	close(sockfd);
	return 0;
}
