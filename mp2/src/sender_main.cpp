/*
 * File:   sender_main.cpp
 * Author: Tianze XU
 * Created on Oct. 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <iostream>
#include <vector>
#include <functional>
#include <queue>
#include <math.h>

#define MSS 4000
#define TIMEOUT 10000

enum packet_t {
    SYN,
    SYNACK,
    DATA,
    ACK,
    FIN,
    FINACK
};

enum status_t {
    SLOW_START,
    CONGESTION_AVOID,
    FAST_RECOVERY
};

typedef struct {
    int dataSize;
    uint64_t ack;
    uint64_t seq;
    packet_t type;
    char data[MSS];
} packet;


using namespace std;

struct sockaddr_in si_other;
int s, slen;

FILE* fp;
int byteToTransfer;
int numPackets;
int byteToSend;
int numRecv;
int numDup;
int seqIdx;
status_t status;
float cwnd;
float ssthresh;
queue <packet> sentNotAck; // the packets that have been sent but not ack-ed
queue <packet> toBeSent; // the packets to be sent.


void diep(char *s) {
    perror(s);
    exit(1);
}

void sendPacket(packet pkt) {
    if (sendto(s, &pkt, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
        diep("Fail to send packet");
    }
}


void readFilesAndPack() {
    char buffer[MSS] = {0};
    packet pkt;
    
    int packetsToRead = ceil((cwnd - sentNotAck.size() * MSS) / 1.0 * MSS);
    for(int i = 0; i < packetsToRead && byteToSend > 0; ++i) {
        int readBytes = fread(buffer, 1, (byteToSend < MSS) ? byteToSend : MSS, fp);
        if (readBytes <= 0) {
            break; // Break the loop if no data is read or an error occurred.
        }
        
        pkt.seq = seqIdx;
        pkt.dataSize = readBytes;
        pkt.type = DATA;
        memcpy(pkt.data, buffer, readBytes);
        
        toBeSent.push(pkt);
        
        seqIdx += readBytes;
        byteToSend -= readBytes;
        
        memset(buffer, 0, MSS);
    }
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */
	
    byteToSend = bytesToTransfer;
    byteToTransfer = bytesToTransfer;
    numPackets = ceil(bytesToTransfer/MSS);

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    /* set a timer for timeout*/
    timeval RTO;
    RTO.tv_sec = 0;
    RTO.tv_usec = TIMEOUT;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &RTO, sizeof(RTO)) == -1) {
        diep("Fail to set timer");
    }
    /*init the variables*/
    numRecv = 0;
    numDup = 0;
    seqIdx = 0;
    status = SLOW_START;
    cwnd = 1.0 * MSS;
    ssthresh = (float)(cwnd * 400.0);

    /*wait for syn and send back synack, establish the connection*/
    /*actually, I think request the connection should be completed by receiver
      but it makes no change on the file transmission if the sender request the
      connection. if the connection request is initially send by receiver, then the
      augument of the receiver should be 
      hostname of server(sender), UdpPort, destination file
      
      but this is different with the handout, 
      so here the connection is requested with sender,
      I kept the "pseudo code" of the way that the connection is request by receiver
      in receiver_main.cpp, it almost keep sending SYN and when it receive SYNACK from
      sender, it send back a ACK, the connection then is built.
     */
    
    packet pktSyn;
    pktSyn.type = SYN;
    sendPacket(pktSyn);
    packet recvSynAck;
    while(true) {
        if (recvfrom(s, &recvSynAck, sizeof(packet), 0, NULL, NULL) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                diep("Fail to receive");
            }
            else{
            	sendPacket(pktSyn);
            }
        }
        else if (recvSynAck.type == SYNACK) {
            cout << "SYNACK from receiver is received" << endl;
            break;
        }
    }
    
    
    
    packet pkt;
    readFilesAndPack();
    while (!toBeSent.empty()) {
    	    packet pkt = toBeSent.front();
            sendPacket(pkt);
            sentNotAck.push(pkt);
            toBeSent.pop();
    }
    while (numRecv < numPackets) {

        if (recvfrom(s, &pkt, sizeof(packet), 0, NULL, NULL) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                diep("Fail to receive");
            }
            if (!sentNotAck.empty()) {
		    ssthresh = max(cwnd / 2, (float)(MSS * 200.0));
		    cwnd = (float)(MSS*200);
		    status = SLOW_START;
		    queue<packet> tmpQueue = sentNotAck;
		    while (!tmpQueue.empty()) {
			sendPacket(tmpQueue.front());
			tmpQueue.pop();
		    }
		    numDup = 0;
	    }
        }

        if (pkt.type == ACK) {
            if (pkt.ack < sentNotAck.front().seq) {
        	continue;
    	    }
            // handleACK(pkt);
            if (pkt.ack == sentNotAck.front().seq) {
		numDup++;
		if (status == FAST_RECOVERY) {
		    cwnd += MSS;
		}
		else if (numDup >= 3 && (status == SLOW_START || status == CONGESTION_AVOID)) {
		    ssthresh = max(cwnd / 2, float(MSS));
		    cwnd = ssthresh + 3 * MSS;
		    status = FAST_RECOVERY;
		    if (!sentNotAck.empty()) {
		        sendPacket(sentNotAck.front());
		    }
		    numDup = 0;
		}
		continue;
	    }
	    else {
		numDup = 0;
		if (status == SLOW_START) {
		    if (cwnd >= ssthresh) {
		        status = CONGESTION_AVOID;
		    } else {
		        cwnd += MSS;
		    }
		}
		else if (status == CONGESTION_AVOID) {
		    cwnd += MSS * floor(1.0 / cwnd);
		}
		else {
		    cwnd = ssthresh;
		    status = CONGESTION_AVOID;
		}
		
		if (cwnd < MSS) {
		    cwnd = (float)MSS;
		}
		
		
		int numPkt = ceil((pkt.ack - sentNotAck.front().seq)/MSS);
		numRecv += numPkt;
		for (int i = 0; i < numPkt; i++) {
		    if(sentNotAck.empty()) {
		    	break;
		    }
		    sentNotAck.pop();
		}
		if (byteToSend > 0) {
		    readFilesAndPack();
		    while (!toBeSent.empty()) {
		    	    packet pkt = toBeSent.front();
			    sendPacket(pkt);
			    sentNotAck.push(pkt);
			    toBeSent.pop();
		    }
		}
		
	    }
        }
    }

    packet finPkt;
    finPkt.type = FIN;
    sendPacket(finPkt);
    packet recvFin;
    while (true) {

        if (recvfrom(s, &recvFin, sizeof(packet), 0, NULL, NULL) == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                diep("Fail to receive");
            }
            else {
                sendPacket(finPkt);
            }
        }
        else {
            if (recvFin.type == FINACK) {
                cout << "Connection end" << endl;
                break;
            }
        }
    }

    fclose(fp);
	/* Send data and receive acknowledgements on s*/
    printf("Closing the socket\n");
    close(s);
    return;
    }

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);
    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
    return (EXIT_SUCCESS);
}

