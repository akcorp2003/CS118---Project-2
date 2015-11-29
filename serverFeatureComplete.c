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

static int maxPacketSize = 30;
static int headerLen = 20;
static int packetBufferSize = 100;
int timeout = 0;
int fileRequest = 1;
int dur = 8;

struct Header {
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
	exit(1);
}

int isHit(int prob)
{
	if (prob == 0)
		return 0;

	int num = rand() % 100;
	printf("%d\n", num);
	if (num < prob)
		return 1;
	return 0;
}

void setTimeout(int signum)
{
	timeout = 1;
	printf("SIGNAL RECEIVED\n");
}

int getAck(char * buffer)
{
	struct Header packet;
	memcpy((struct Header *) &packet, buffer, headerLen);
	return packet.ack;
}

int getDataLen(char * buffer)
{
	struct Header packet;
	memcpy((struct Header *) &packet, buffer, headerLen);
	return (int)packet.dataLen;
}

void getFileName(char * buffer, char * filename)
{
	struct Header packet;
	memcpy((struct Header *) &packet, buffer, headerLen);
	memcpy(filename, buffer + headerLen, packet.dataLen);
}

int getFlag(char * buffer)
{
	struct Header packet;
	memcpy((struct Header *) &packet, buffer, headerLen);
	return packet.flag;
}


void printPacket(char * packet)
{
	struct Header pktHeader;
	memcpy((struct Header *) &pktHeader, packet, headerLen);

	printf("srcPort: %d\ndestPort: %d\nseq: %d\n", pktHeader.srcPort,
		pktHeader.destPort, pktHeader.seq);
	printf("ack: %d\ndataLen: %d\n", pktHeader.ack,
		pktHeader.dataLen);
	printf("flag: %d\n", pktHeader.flag);
	printf("data: %s", packet + headerLen);
	printf("\n");
}

int main(int argc, char *argv[])
{
	double probLoss, probCorruption;
	int sockfd, newsockfd, portno, pid, CWnd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	struct Header packetHeader;
	struct sigaction sa;

	time_t t;
	srand((unsigned)time(&t));

	if (argc < 5) {
		fprintf(stderr, stderr, "usage: %s <port number> <Control Window (CWnd)> <probability loss> <probability corrupt>\n", argv[0]);
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
	char buffer[1024];
	char packetBuffer[packetBufferSize][maxPacketSize + 1];
	int nxtPtr = 0, nxtFree = 0, count = 0;
	char unacked[CWnd][maxPacketSize + 1];
	struct Header recvAck, data;
	char filename[1024];
	FILE * fp;
	char packet[maxPacketSize + 1];
	int cur;
	//col 1: 0 unacked, 1 acked or unused
	//col 2: window seq num
	//col 3: expected ack
	int window[CWnd][3];

	memset(packet, 0, sizeof(packet));
	for (i = 0; i < CWnd; i++)
	{
		window[i][0] = 1;
		window[i][1] = 0;
		window[i][2] = 0;
	}

	memset((struct sigaction *) &sa, 0, sizeof(sa));
	sa.sa_handler = setTimeout;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGALRM, &sa, NULL);
	while (1)
	{
		memset(buffer, 0, 1024);

		// read if timeout has not occured
		if (timeout != 1)
		{
			errno = 0;
			sizeCliAddr = sizeof(cli_addr);
			n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
				(struct sockaddr *) &cli_addr, &sizeCliAddr);
		}
		else errno = EINTR;

		if (errno == EINTR)
		{
			if (timeout == 1)
			{
				timeout = 0;

				k = base + CWnd;
				for (i = base; i < k; i++)
					for (j = 0; j < CWnd; j++)
						if (window[j][0] == 0 && window[j][1] == i)
						{
							n = sendto(sockfd, unacked[j], sizeof(unacked[j]), 0,
								(struct sockaddr *) &cli_addr, sizeof(cli_addr));
							if (n < 0)
								error("ERROR reading from socket");

							printf("\nPacket Sent: \n");
							printPacket(unacked[j]);
							break;
						}

				alarm(dur);
				errno = 0;
				continue;
			}
			else
				error("ERROR reading from socket");
		}
		else if (n < 0) error("ERROR reading from socket");

		// if no loss or corruption
		loss = isHit(probLoss);
		corruption = isHit(probCorruption);
		if (loss != 1 && corruption != 1)
		{
			// if client asks for file
			if (getDataLen(buffer) > 0)
			{
				memset(filename, 0, 1024);
				getFileName(buffer, filename);
				printf("File Requested: %s\n", filename);
				printPacket(buffer);

				fp = fopen(filename, "r");
				if (fp == NULL)
					error("Failed to open file");

				cur = fgetc(fp);
				len = 0;
				data.srcPort = ntohs(serv_addr.sin_port);
				data.destPort = ntohs(cli_addr.sin_port);
				data.ack = 0;
				data.flag = 0;
				i = headerLen;
				while (1)
				{
					if (len < maxPacketSize - headerLen && cur != EOF)
					{
						packet[i] = (char)cur;
						cur = fgetc(fp);
						len++;
						i++;
					}
					if (len == maxPacketSize - headerLen || cur == EOF)
					{
						data.seq = seq;
						data.dataLen = (short)len;
						data.checkSum = (short)1;
						if (cur == EOF)
							data.flag = 1;
						memcpy(packet, (struct Header *) &data, headerLen);
						if (nextSeqnum < base + CWnd)
						{
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

									if (nextSeqnum == base)
										alarm(dur);

									printf("\nPacket sent:\n");
									printPacket(packet);
									memcpy(&unacked[i][0], &packet[0], sizeof(packet));

									seq += len;
									ack += len;
									i = headerLen;
									len = 0;
									nextSeqnum++;

									break;
								}
						}
						else
						{
							memcpy(packetBuffer[nxtFree], packet,
								maxPacketSize);
							nxtFree++;
							count++;
							len = 0;
							i = headerLen;
						}
						memset(packet, 0, sizeof(packet));
						if (cur == EOF)
						{
							data.flag = 0;
							break;
						}
					}
				}

				fclose(fp);
				fileRequest = 0;
			}
			// if client sends ACK
			else
			{
				if (getFlag(buffer) == 1)
					printf("\nFIN Received:\n");
				else printf("\nACK Received:\n");
				printPacket(buffer);
				cumAck = getAck(buffer);
				j = 0;
				for (i = 0; i < CWnd; i++)
				{
					if (window[i][1] >= base && cumAck >= window[i][2] &&
						window[i][0] == 0)
					{
						j++;
						window[i][0] = 1;

						if (window[i][1] == base)
							alarm(dur);

						if (count > 0)
						{
							count--;
							memcpy((struct Header *) &data, packetBuffer[nxtPtr],
								headerLen);
							data.seq = seq;
							memcpy(packetBuffer[nxtPtr], (struct Header *) &data,
								headerLen);
							n = sendto(sockfd, packetBuffer[nxtPtr],
								sizeof(packetBuffer[nxtPtr]), 0,
								(struct sockaddr *) &cli_addr, sizeof(cli_addr));

							if (n < 0)
								error("ERROR writing to socket");
							printf("\nPacket sent:\n");
							printPacket(packetBuffer[nxtPtr]);

							for (i = 0; i < CWnd; i++)
								if (window[i][0] == 1)
								{
									window[i][0] = 0;
									window[i][1] = nextSeqnum;
									window[i][2] = ack +
										getDataLen(packetBuffer[nxtPtr]);

									memcpy(&unacked[i][0], packetBuffer[nxtPtr],
										sizeof(packetBuffer[nxtPtr]));

									seq += getDataLen(packetBuffer[nxtPtr]);
									ack += getDataLen(packetBuffer[nxtPtr]);
									nextSeqnum++;

									break;
								}
							nxtPtr = (nxtPtr + 1) % packetBufferSize;
							nxtFree = (nxtFree + 1) % packetBufferSize;
						}
					}
				}
				base += j;
			}

			if (getFlag(buffer) == 1)
			{
				alarm(0);
				memset(packet, 0, maxPacketSize + 1);
				memset((struct Header *) &data, 0, headerLen);
				data.srcPort = ntohs(serv_addr.sin_port);
				data.destPort = ntohs(cli_addr.sin_port);
				memcpy(packet, (struct Header *) &data, headerLen);
				for (i = 0; i < 1; i++)
				{
					n = sendto(sockfd, packet, sizeof(packet), 0,
						(struct sockaddr *) &cli_addr, sizeof(cli_addr));
					if (n < 0)
						error("ERROR writing to socket");

					printf("\nPacket Sent:\n");
					printPacket(packet);
					sleep(2);
				}

				data.flag = 1;
				memcpy(packet, (struct Header *) &data, headerLen);
				n = sendto(sockfd, packet, sizeof(packet), 0,
					(struct sockaddr *) &cli_addr, sizeof(cli_addr));
				if (n < 0)
					error("ERROR writing to socket");

				printf("\nPacket Sent:\n");
				printPacket(packet);

				alarm(dur);

				while (1)
				{
					n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
						(struct sockaddr *) &cli_addr, &sizeCliAddr);
					if (n < 0 && errno == 0)
						error("ERROR reading from socket");
					if (errno == EINTR)
					{
						errno = 0;
						break;
					}
					loss = isHit(probLoss);
					corruption = isHit(probCorruption);
					if (loss == 1)
						printf("Packet Lost:\n");
					else if (corruption == 1)
						printf("Packet Corrupted:\n");

					if (loss == 0 && corruption == 0)
						printf("\nACK Received\n");
					printPacket(buffer);
				}
			}
		}
		// if loss or corruption do nothing
		else
		{
			if (loss == 1)
				printf("Packet Lost:\n");
			else
				printf("Packet Corrupted:\n");

			loss = 0;
			corruption = 0;

			printPacket(buffer);
		}
	}
	close(sockfd);

	return 0;
}


