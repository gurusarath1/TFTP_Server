
/*

Team 9

Team Members - Guru Sarath , Doyoung Kwak
UIN: 829009551 , 927000467

Assignment 3 ------
TFTP Server
ECEN - 602

*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NETASCII_str "netascii"
#define OCTET_str "octet"
#define MAIL_str "mail" 

#define MAX_BUFFER_SIZE 700
#define MAX_TFTP_DATA_SIZE 512
#define MAX_RETRY 10
#define MAX_WAIT 1

enum opcode {

	RRQ = 1,
	WRQ,
	DATA,
	ACK,
	ERR
};

enum errorCode {

	Not_defined = 0,
 	File_not_found,
    Access_violation,
    Disk_full,
    Illegal_TFTP_operation,
    Unknown_transfer_ID,
    File_already_exists,
    No_such_user

};


int readable_timeo(int fd,int sec);
int getOpcodeFromPacket(unsigned char buf[]);
int getFileNameAndModeFromPacket(unsigned char buf[], char filename[], char mode[]);
uint16_t getBlockNumberFromPacket(unsigned char buf[]);
int IsCorrectACK(unsigned char buf[], uint16_t expectedBlock);

int handle_ReceivedMessage(unsigned char buf[]);
int handle_RRQ_Message(unsigned char buf[]);
int handle_WRQ_Message(unsigned char buf[]);
int handle_Unexpected_ACK_Message(unsigned char buf[]);
int handle_Unexpected_DATA_Message(unsigned char buf[]);
int handle_Unexpected_ERR_Message(unsigned char buf[]);

int handle_netasciiMode_RRQ(char filename[]);
int handle_octetMode_RRQ(char filename[]);
int handle_netasciiMode_WRQ(char filename[]);
int handle_octetMode_WRQ(char filename[]);

int createACKPacket(int blockNum, char returnMessage[]);
int createErrorPacket(int errorCode, char ErrorMessage[], char returnMessage[]);
int createDataPacket_Netascii(FILE* fp, int blockNum, char returnMessage[], int* lastPacketFlag);
int createDataPacket_Octet(int fp, int blockNum, char returnMessage[]);


int ServerPortNum; // Port number of the TFTP sever
int sd_server; // Main Socket of server
int fromLen;
struct sockaddr_in ServerAddr_server;
unsigned char RcvBuffer[MAX_BUFFER_SIZE]; // Message receive buffer
int rcv_len; // Length of the received message

int childProcessFlag = 0; // This flag indicates if a process is child process
int sd_child; // Socket of the child process
struct sockaddr_in ServerAddr_child;

int nextchar = -1;


int main(int argc, char* argv[])
{

    printf("\n\n#### TFTP SERVER ####\n");

    if(argc < 2)
    {
        printf("Insufficient arguments !\nPort nmber required as an command line argument\n");
        return 1;
    } else {
		
        ServerPortNum = atoi(argv[1]);
    }

    /*
    printf("\nPort Number- ");
	scanf("%d", &ServerPortNum);
	*/

	
    /* MAIN CODE STARTS FROM HERE */
    int sd_temp;


	// Set all memory locations to zero
    memset(&ServerAddr_server, 0, sizeof(ServerAddr_server));

    ServerAddr_server.sin_family = AF_INET; //IPv4
    ServerAddr_server.sin_addr.s_addr = INADDR_ANY;
    ServerAddr_server.sin_port = htons( ServerPortNum ); // Host to network order conversion


    if( (sd_temp = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) // Create socket
    {
        printf("\nCould not create socket\n");
        return 1;
    } else {

        sd_server = sd_temp;

    }

    if(bind(sd_server, (struct sockaddr *)&ServerAddr_server, sizeof(ServerAddr_server)) < 0) // Bind to socket
    {
        printf("\nBind action failed !\n");
        return 1;
    }

    fromLen = sizeof(ServerAddr_server);

    while(1)
    {
    	puts("\nServer is ready for new request !!");
    	memset(RcvBuffer, 0, MAX_BUFFER_SIZE);
    	rcv_len = recvfrom(sd_server, RcvBuffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&ServerAddr_server, &fromLen);
	
		if (rcv_len == -1)
		{
			puts("\nUnexpected Receive Error");
			close(sd_server);
			return 0;
		}

		puts("\nReceived new Packet");


		// Handle all the incoming messages
		// This is the main jump of the program
		//------------------------------------------------------------
		handle_ReceivedMessage(RcvBuffer);
		//------------------------------------------------------------

		if(childProcessFlag)
		{
			printf("\nChild Porcess Exit !!");
			printf("\nServer is ready for new request (PORT: %d)",ServerPortNum);
			close(sd_child);
			return 0;
		}

    }


    close(sd_server);
	return 0;
}


/*
handle_ReceivedMessage - This function handles newly messages received by the main server process
*/

int handle_ReceivedMessage(unsigned char buf[])
{
	int opcodeX = getOpcodeFromPacket(RcvBuffer);

	switch(opcodeX)
	{
		case RRQ:
			handle_RRQ_Message(buf);
			break;

		case WRQ:
			handle_WRQ_Message(buf);
			break;

		case DATA:
			handle_Unexpected_DATA_Message(buf);
			break;

		case ACK:
			handle_Unexpected_ACK_Message(buf);
			break;

		case ERR:
			handle_Unexpected_ERR_Message(buf);
			break;

		default:
			return -1;

	}

	return 0;
}


/*
handle_RRQ_Message - This function handles a new read request
*/
int handle_RRQ_Message(unsigned char buf[])
{
	if(fork() == 0)
	{
		childProcessFlag = 1;
		close(sd_server);

		char MessageToDisplayOnScreen[200];
		char fileName[50];
		char mode[15];
		getFileNameAndModeFromPacket(buf, fileName, mode);

		strcat(MessageToDisplayOnScreen, "\nRRQ- ");
		strcat(MessageToDisplayOnScreen, fileName);
		strcat(MessageToDisplayOnScreen, "\nMode- ");
		strcat(MessageToDisplayOnScreen, mode);
		puts(MessageToDisplayOnScreen);


		// ---------------------------------------------------

		// Set all memory locations to zero
	    memset(&ServerAddr_child, 0, sizeof(ServerAddr_child));

	    ServerAddr_child.sin_family = AF_INET; //IPv4
	    ServerAddr_child.sin_addr.s_addr = INADDR_ANY;
	    ServerAddr_child.sin_port = htons( 0 ); // New port


	    if( (sd_child = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) // Create socket
	    {
	        printf("Could not create socket");
	        return 1;
	    }

	    printf("\nChild SD = %d", sd_child);

	    if(bind(sd_child, (struct sockaddr *)&ServerAddr_child, sizeof(ServerAddr_child)) < 0) // Bind to socket
	    {
	        printf("\nBind action failed !");
	        return 1;
	    }

	    if( !strcmp(mode, NETASCII_str) )
	    {
	    	handle_netasciiMode_RRQ(fileName);
	    	printf("\nNetascii handle complete!");
	    	return 0;
	    }

	    else if( !strcmp(mode, OCTET_str) )
	    {
	    	handle_octetMode_RRQ(fileName);
	    	return 0;
	    }


	} else {

		return 0;
	}

	return 0;
}

/*
handle_WRQ_Message - This function handles a new write request
*/
int handle_WRQ_Message(unsigned char buf[])
{
	if(fork() == 0)
	{
		childProcessFlag = 1;
		close(sd_server);

		char MessageToDisplayOnScreen[200];
		char fileName[50];
		char mode[15];
		getFileNameAndModeFromPacket(buf, fileName, mode);

		strcat(MessageToDisplayOnScreen, "\nWRQ- ");
		strcat(MessageToDisplayOnScreen, fileName);
		strcat(MessageToDisplayOnScreen, "\nMode- ");
		strcat(MessageToDisplayOnScreen, mode);
		puts(MessageToDisplayOnScreen);


		// ---------------------------------------------------

		// Set all memory locations to zero
	    memset(&ServerAddr_child, 0, sizeof(ServerAddr_child));

	    ServerAddr_child.sin_family = AF_INET; //IPv4
	    ServerAddr_child.sin_addr.s_addr = INADDR_ANY;
	    ServerAddr_child.sin_port = htons( 0 ); // New port


	    if( (sd_child = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) // Create socket
	    {
	        printf("Could not create socket");
	        return 1;
	    }

	    printf("\nChild SD = %d", sd_child);

	    if(bind(sd_child, (struct sockaddr *)&ServerAddr_child, sizeof(ServerAddr_child)) < 0) // Bind to socket
	    {
	        printf("\nBind action failed !");
	        return 1;
	    }

	    if( !strcmp(mode, NETASCII_str) )
	    {
	    	handle_netasciiMode_WRQ(fileName);
	    	printf("\nNetascii handle complete!");
	    	return 0;
	    }

	    else if( !strcmp(mode, OCTET_str) )
	    {
	    	handle_octetMode_WRQ(fileName);
	    	return 0;
	    }


	} else {

		return 0;
	}

	return 0;
}

/*
handle_Unexpected_DATA_Message - This function discards unintended DATA packet
*/
int handle_Unexpected_DATA_Message(unsigned char buf[])
{
	printf("\nDATA packet received at unexpected time !");
	printf("\nPacket Discarded !");
	return 0;
}

/*
handle_Unexpected_ACK_Message - This function discards unintended ACK packet
*/
int handle_Unexpected_ACK_Message(unsigned char buf[])
{
	printf("\nACK packet received at unexpected time !");
	printf("\nPacket Discarded !");
	return 0;
}

/*
handle_Unexpected_ERR_Message - This function discards unintended ERR packet
*/
int handle_Unexpected_ERR_Message(unsigned char buf[])
{
	printf("\nERR packet received at unexpected time !");
	printf("\nPacket Discarded !");
	return 0;
}

/*
handle_netasciiMode_RRQ - This function handles netascii mode read request
*/
int handle_netasciiMode_RRQ(char filename[])
{

	FILE *fileToRead;

	fileToRead = fopen(filename,"r");

	if(fileToRead == NULL)
	{
		
		printf("\nFile NOT present :(");
		unsigned char errorPacket[100];
		
		int errorPacketLen = createErrorPacket(File_not_found, "Requested File Does not exist !", errorPacket);
		
		//int toLen = sizeof(ServerAddr_child);
		printf("\nError packet Length = %d",errorPacketLen);
		if (sendto(sd_child, errorPacket, errorPacketLen , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
		{
			printf("\ncould not send error message !!");
		}
		


	} else {

		

		printf("\nFile is present :)");

		uint16_t blockNumber = 1;
		uint16_t blockNumber_ACK_Received = 0;
		int len_data_packet = 0;
		unsigned char dataPacket[MAX_TFTP_DATA_SIZE+10];
		int numberOfAttempts = 0;
		int timer_return = -1;
		int lastPacketFlag = 0;

		
		do {
				memset(dataPacket, 0, MAX_TFTP_DATA_SIZE+10);
				len_data_packet = createDataPacket_Netascii(fileToRead, blockNumber, dataPacket, &lastPacketFlag);

				printf("\nDATA Packet sent : Block = %d : Length = %d ", blockNumber, len_data_packet);

				if (sendto(sd_child, dataPacket, len_data_packet , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
				{
					printf("\ncould not send data packet !!");
				}

				
				numberOfAttempts = 0;
				do
				{

					timer_return = readable_timeo(sd_child, 1);

					if(timer_return > 0)
					{
						break;

					} else {
						puts("\nTime Out !");

						if (sendto(sd_child, dataPacket, len_data_packet , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
						{
							printf("\ncould not send data packet !!");
						}

						numberOfAttempts++;
					}

				}while(numberOfAttempts < MAX_RETRY);




				if (numberOfAttempts >= MAX_RETRY)
				{

					printf("\n--- MAX RETRY REACHED ---");
					unsigned char errorPacket[100];
					
					int errorPacketLen = createErrorPacket(Not_defined, "Maximum retry attempt reached !", errorPacket);
					
					//int toLen = sizeof(ServerAddr_child);
					printf("\nError packet sent (Length = %d)",errorPacketLen);
					if (sendto(sd_child, errorPacket, errorPacketLen , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
					{
						printf("\ncould not send error message !!");
					}

					puts("\n--- SENT FAILED ---");
					fclose(fileToRead);
					return 1;

				} 




				if(timer_return > 0)
				{
					memset(RcvBuffer, 0, MAX_BUFFER_SIZE);

					rcv_len = recvfrom(sd_server, RcvBuffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&ServerAddr_server, &fromLen);

					if(rcv_len > 0)
					{
						printf("\nPacket Recevied from Client");
						if(IsCorrectACK(RcvBuffer, blockNumber))
						{
							blockNumber_ACK_Received += 1;
							blockNumber += 1;
						}
					}

				} else {

					printf("\nUnknown Error :(");
					unsigned char errorPacket[100];
					
					int errorPacketLen = createErrorPacket(Not_defined, "Unknown Error!", errorPacket);
					
					//int toLen = sizeof(ServerAddr_child);
					printf("\nError packet Length = %d",errorPacketLen);
					if (sendto(sd_child, errorPacket, errorPacketLen , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
					{
						printf("\ncould not send error message !!");
					}
					
					puts("\n--- SENT FAILED ---");
					fclose(fileToRead);
					return 1;


				} //else


			} while(!lastPacketFlag);


		fclose(fileToRead);
		printf("\n--- SENT COMPLETE ---");
		

	} //else

	return 0;
}

/*
handle_octetMode_RRQ - This function handles octet mode read request
*/
int handle_octetMode_RRQ(char filename[])
{
	int fileToRead;

	fileToRead = open(filename,O_RDONLY);

	if(fileToRead == -1)
	{
		
		printf("\nFile NOT present :(");
		unsigned char errorPacket[100];
		
		int errorPacketLen = createErrorPacket(File_not_found, "Requested File Does not exist !", errorPacket);
		
		//int toLen = sizeof(ServerAddr_child);
		printf("\nError packet Length = %d",errorPacketLen);
		if (sendto(sd_child, errorPacket, errorPacketLen , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
		{
			printf("\ncould not send error message !!");
		}
		


	} else {

		

		printf("\nFile is present :)");

		uint16_t blockNumber = 1;
		uint16_t blockNumber_ACK_Received = 0;
		int len_data_packet = 0;
		unsigned char dataPacket[MAX_TFTP_DATA_SIZE+10];
		int numberOfAttempts = 0;
		int timer_return = -1;
		int lastPacketFlag = 0;

		
		do {
				memset(dataPacket, 0, MAX_TFTP_DATA_SIZE+10);
				len_data_packet = createDataPacket_Octet(fileToRead, blockNumber, dataPacket);

				printf("\nDATA Packet sent : Block = %d : Length = %d ", blockNumber, len_data_packet);

				if (sendto(sd_child, dataPacket, len_data_packet , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
				{
					printf("\ncould not send data packet !!");
				}

				
				numberOfAttempts = 0;
				do
				{

					timer_return = readable_timeo(sd_child, 1);

					if(timer_return > 0)
					{
						break;

					} else {
						puts("\nTime Out !");

						if (sendto(sd_child, dataPacket, len_data_packet , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
						{
							printf("\ncould not send data packet !!");
						}

						numberOfAttempts++;
					}

				}while(numberOfAttempts < MAX_RETRY);




				if (numberOfAttempts >= MAX_RETRY)
				{

					printf("\n--- MAX RETRY REACHED ---");
					unsigned char errorPacket[100];
					
					int errorPacketLen = createErrorPacket(Not_defined, "Maximum retry attempt reached !", errorPacket);
					
					//int toLen = sizeof(ServerAddr_child);
					printf("\nError packet sent (Length = %d)",errorPacketLen);
					if (sendto(sd_child, errorPacket, errorPacketLen , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
					{
						printf("\ncould not send error message !!");
					}

					puts("\n--- SENT FAILED ---");
					close(fileToRead);
					return 1;

				} 




				if(timer_return > 0)
				{
					memset(RcvBuffer, 0, MAX_BUFFER_SIZE);

					rcv_len = recvfrom(sd_child, RcvBuffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&ServerAddr_server, &fromLen);

					if(rcv_len > 0)
					{
						printf("\nPacket Recevied from Client");
						if(IsCorrectACK(RcvBuffer, blockNumber))
						{
							blockNumber_ACK_Received += 1;
							blockNumber += 1;
						}
					}

				} else {

					printf("\nUnknown Error :(");
					unsigned char errorPacket[100];
					
					int errorPacketLen = createErrorPacket(Not_defined, "Unknown Error!", errorPacket);
					
					//int toLen = sizeof(ServerAddr_child);
					printf("\nError packet Length = %d",errorPacketLen);
					if (sendto(sd_child, errorPacket, errorPacketLen , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
					{
						printf("\ncould not send error message !!");
					}
					
					puts("\n--- SENT FAILED ---");
					close(fileToRead);
					return 1;


				} //else


			} while(len_data_packet == 516);


		close(fileToRead);
		printf("\n--- SENT COMPLETE ---");

	} // else

}

/*
handle_netasciiMode_WRQ - This function handles netascii mode write request
*/
int handle_netasciiMode_WRQ(char filename[])
{

	FILE* fileTowrite;

	fileTowrite = fopen(filename, "w+");

	if(fileTowrite == NULL)
	{

		printf("\nUnable to create file !!");

		unsigned char errorPacket[100];
		
		int errorPacketLen = createErrorPacket(File_already_exists, "Unable to create the file !", errorPacket);
		
		//int toLen = sizeof(ServerAddr_child);
		printf("\nError packet Length = %d",errorPacketLen);
		if (sendto(sd_child, errorPacket, errorPacketLen , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
		{
			printf("\ncould not send error message !!");
		}

		return 1;

	} else {

		printf("\nNew file created !");

		uint16_t blockNumber = 0;
		int len_ACK_packet = 0;
		unsigned char ACK_packet[6];
		int lastPacketFlag = 0;


		memset(ACK_packet, 0, 6);
		len_ACK_packet = createACKPacket(blockNumber, ACK_packet);
		printf("\nACK Packet sent : Block = %d : Length = %d ", blockNumber, len_ACK_packet);
		if (sendto(sd_child, ACK_packet, len_ACK_packet , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
		{
			printf("\ncould not send ACK !!");
		}


		do
		{
			memset(RcvBuffer, 0, MAX_BUFFER_SIZE);
			rcv_len = recvfrom(sd_server, RcvBuffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&ServerAddr_server, &fromLen);
			printf("\nSize of received packet = %d", rcv_len);
			if(rcv_len > 0)
			{
				printf("\nPacket Recevied from Client");
				if( getOpcodeFromPacket(RcvBuffer) ==  DATA && getBlockNumberFromPacket(RcvBuffer) == blockNumber+1 )
				{
					printf("\nRecevied DATA packet from Client");
					for(int i=4; i<rcv_len; i++)
					{
						if(RcvBuffer[i] != '\0')
						{
							
							putc(RcvBuffer[i],fileTowrite);

						}
					}
					blockNumber++;
				}
			}

			memset(ACK_packet, 0, 6);
			len_ACK_packet = createACKPacket(blockNumber, ACK_packet);
			printf("\nACK Packet sent : Block = %d : Length = %d ", blockNumber, len_ACK_packet);
			if (sendto(sd_child, ACK_packet, len_ACK_packet , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
			{
				printf("\ncould not send ACK !!");
			}

			if(blockNumber == 0xFFFF) blockNumber = 0;


		}while(rcv_len == MAX_TFTP_DATA_SIZE+4);


		fclose(fileTowrite);

	} //else

	return 0;

}

/*
handle_octetMode_WRQ - This function handles octet mode write request
*/
int handle_octetMode_WRQ(char filename[])
{

	int fileTowrite;

	fileTowrite = open(filename,O_WRONLY|O_CREAT|O_EXCL,0644);

	if(fileTowrite == -1)
	{

		printf("\nUnable to create file !!");

		unsigned char errorPacket[100];
		
		int errorPacketLen = createErrorPacket(File_already_exists, "Unable to create the file !", errorPacket);
		
		//int toLen = sizeof(ServerAddr_child);
		printf("\nError packet Length = %d",errorPacketLen);
		if (sendto(sd_child, errorPacket, errorPacketLen , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
		{
			printf("\ncould not send error message !!");
		}

		return 1;

	} else {

		printf("\nNew file created !");

		uint16_t blockNumber = 0;
		int len_ACK_packet = 0;
		unsigned char ACK_packet[6];
		int lastPacketFlag = 0;


		memset(ACK_packet, 0, 6);
		len_ACK_packet = createACKPacket(blockNumber, ACK_packet);
		printf("\nACK Packet sent : Block = %d : Length = %d ", blockNumber, len_ACK_packet);
		if (sendto(sd_child, ACK_packet, len_ACK_packet , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
		{
			printf("\ncould not send ACK !!");
		}


		do
		{
			memset(RcvBuffer, 0, MAX_BUFFER_SIZE);
			rcv_len = recvfrom(sd_server, RcvBuffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&ServerAddr_server, &fromLen);
			printf("\nSize of received packet = %d", rcv_len);
			if(rcv_len > 0)
			{
				printf("\nPacket Recevied from Client");
				if( getOpcodeFromPacket(RcvBuffer) ==  DATA && getBlockNumberFromPacket(RcvBuffer) == blockNumber+1 )
				{
					printf("\nRecevied DATA packet from Client");
					write(fileTowrite, &RcvBuffer[4], rcv_len-4);
					blockNumber++;
				}
			}

			memset(ACK_packet, 0, 6);
			len_ACK_packet = createACKPacket(blockNumber, ACK_packet);
			printf("\nACK Packet sent : Block = %d : Length = %d ", blockNumber, len_ACK_packet);
			if (sendto(sd_child, ACK_packet, len_ACK_packet , 0, (struct sockaddr *)&ServerAddr_server, fromLen) < 0)
			{
				printf("\ncould not send ACK !!");
			}

			if(blockNumber == 0xFFFF) blockNumber = 0;


		}while(rcv_len == MAX_TFTP_DATA_SIZE+4);


		close(fileTowrite);

	} //else



	return 0;
}


int readable_timeo(int fd,int sec) 
{ 	
	fd_set rset;
	struct timeval tv;
	
	FD_ZERO(&rset);
	FD_SET(fd,&rset);
	
	tv.tv_sec= sec;
	tv.tv_usec=0;
	return (select(fd+1,&rset,NULL,NULL,&tv));
}


/*
getOpcodeFromPacket - This function gets opcode from TFTP packet
*/
int getOpcodeFromPacket(unsigned char buf[])
{
	return ( (buf[0] << 8) + buf[1] );
}

/*
getFileNameAndModeFromPacket - This function gets filename and mode from TFTP packet
*/
int getFileNameAndModeFromPacket(unsigned char buf[], char filename[], char mode[])
{
	strcpy(filename,&buf[2]);
	filename[strlen(filename)]='\0';

	if(buf[2 + strlen(filename)] == 0)
	{
		strcpy(mode, &buf[2 + strlen(filename) + 1]);
		if(buf[2 + strlen(filename) + 1 + strlen(mode)] == 0)
		{
			return strlen(filename);
		
		} else {
			printf("\nNo 0 after Mode !!");
			printf("\nInvalid message Received !!");
			return -1;
		}

	} else {

		printf("\nNo 0 after Filename !!");
		printf("\nInvalid message Received !!");
		return -1;
	}

}

/*
getBlockNumberFromPacket - This function gets block number from TFTP packet
*/
uint16_t getBlockNumberFromPacket(unsigned char buf[])
{
	return (buf[2] << 8) | (buf[3] & 0xFF);
}

/*
createACKPacket - This function creates an ACK packet
*/
int createACKPacket(int blockNum, char returnMessage[])
{
	int len = 0;
	char byte_1, byte_2;

	byte_1 = (blockNum & 0xFF00) >> 8;
	byte_2 = (blockNum & 0xFF);

	returnMessage[0] = 0;
	returnMessage[1] = ACK;
	returnMessage[2] = byte_1;
	returnMessage[3] = byte_2;

	len = 4;

	return len;
}

/*
createErrorPacket - This function creates an error packet
*/
int createErrorPacket(int errorCode, char ErrorMessage[], char returnMessage[])
{
	char *ptrC = returnMessage;
	int len = 0;


	returnMessage[0] = 0;
	returnMessage[1] = ERR;
	returnMessage[2] = 0;
	returnMessage[3] = errorCode;

	len = 4;
	ptrC += 4;

	strcat(ptrC, ErrorMessage);
	ptrC += strlen(ErrorMessage);
	len += strlen(ErrorMessage);

	len += 1;

	return len;

}

/*
createDataPacket_Octet - This function creates an octet data packet
*/
int createDataPacket_Octet(int fp, int blockNum, char returnMessage[])
{
	char *ptrC = returnMessage;
	int len = 0;
	char c, byte_1, byte_2;


	byte_1 = (blockNum & 0xFF00) >> 8;
	byte_2 = (blockNum & 0xFF);

	returnMessage[0] = 0;
	returnMessage[1] = DATA;
	returnMessage[2] = byte_1;
	returnMessage[3] = byte_2;

	len = 4;
	ptrC += 4;

	len = len + read(fp,ptrC,512);

	return len;
}

/*
createDataPacket_Netascii - This function creates netascii data packet
*/
int createDataPacket_Netascii(FILE* fp, int blockNum, char returnMessage[], int* lastPacketFlag)
{
	char *ptrC = returnMessage;
	int len = 0;
	char c, byte_1, byte_2;


	byte_1 = (blockNum & 0xFF00) >> 8;
	byte_2 = (blockNum & 0xFF);

	returnMessage[0] = 0;
	returnMessage[1] = DATA;
	returnMessage[2] = byte_1;
	returnMessage[3] = byte_2;

	len = 4;
	ptrC += 4;

	int i;
	for(i=0; i<MAX_TFTP_DATA_SIZE; i++)
	{

		if(nextchar >= 0)
		{
			*ptrC++ = nextchar;
			nextchar = -1;
			continue;
		}

		c = getc(fp);
		//printf("%c",c);

		if(c == EOF)
		{
			if(ferror(fp))
			{
				printf("\nFile Read error !!");
			}

			printf("\nEOF");
			*lastPacketFlag = 1;
			return (i+len);

		}else if(c == '\n')
		{
			c = '\r';
			nextchar = '\n';

		} else if (c == '\r')
		{
			nextchar = '\0';

		} else {
			nextchar = -1;

		}

		*ptrC++ = c;
	}

	*lastPacketFlag = 0;
	return (len + i);

}

/*
IsCorrectACK - This function validates a received ACK packet
*/
int IsCorrectACK(unsigned char buf[], uint16_t expectedBlock)
{
	uint16_t op,block;
	op = getOpcodeFromPacket(buf);
	block  = getBlockNumberFromPacket(buf);

	if(op == ACK && block == expectedBlock) // Check op code and expected block number
	{
		printf("\nACK Received for block %u", block);
		return 1; // Correct ACK
	}
	else
	{
		return 0; // Invalid ACK
	}

}