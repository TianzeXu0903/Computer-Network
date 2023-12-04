/*
 * File:   receiver_main.cpp
 * Author: Tianze XU
 *
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

struct compare {
    bool operator()(packet a, packet b) {
        return a.seq > b.seq;
    }
};

using namespace std;

struct sockaddr_in si_me, si_other;
int s, slen;
int ack_idx = 0;

priority_queue<packet, vector<packet>, compare> waitQueue;


void diep(char *s) {
    perror(s);
    exit(1);
}


void send_ack(packet_t type) {
    packet pkt;
    pkt.type = type;
    pkt.ack = ack_idx;
    if (sendto(s, &pkt, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t)sizeof(si_other)) == -1) {
        diep("Fail to send ack");
    }
    cout << "curr ack number" << ack_idx << endl;
}


void updateQueue(FILE* fp){
   while (!waitQueue.empty() && waitQueue.top().seq == ack_idx) {
        packet curr_pkt = waitQueue.top();
        waitQueue.pop();
        fwrite(curr_pkt.data, sizeof(char), curr_pkt.dataSize, fp);
        ack_idx += curr_pkt.dataSize;
    }
	
}

void write(packet pkt, FILE* fp) {
    cout << "Writing packets" << endl;
    fwrite(pkt.data, sizeof(char), pkt.dataSize, fp);
    ack_idx += pkt.dataSize;
    updateQueue(fp);
}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");

    FILE *fp = fopen(destinationFile, "wb");
    if (fp == NULL) {
        diep("Fail to open the destination file");
    }
    
    
    while (true) {
        cout << "Waiting connection request from sender" << endl;
        packet waitSyn;
        if (recvfrom(s, &waitSyn, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t*)&slen) == -1) {
            diep("Fail to receive packets");
        }
        
        else if (waitSyn.type == SYN) {
            cout << "Receive connection request from sender" << endl;
            send_ack(SYNACK);
            break;
        }
    }
    

    while (true) {
        cout << "dealing with data and fin" << endl;
        packet curr_recv;
        if (recvfrom(s, &curr_recv, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t*)&slen) == -1) {
            diep("Fail to receive packets");
        }
        

        else if (curr_recv.type == FIN) {
            cout << "Receive Fin" << endl;
            send_ack(FINACK);
            break;
        }

        else if (curr_recv.type == DATA) {
            cout << "Receive data" << endl;

            if (curr_recv.seq < ack_idx) {
                cout << "Receive passed packet, ignore it" << endl;
            }

            else if (curr_recv.seq > ack_idx) {
                cout << "Recieve advanced packet, push it into the queue, waiting to be written" << endl;
                waitQueue.push(curr_recv);
                
            }

            else if (curr_recv.seq == ack_idx) {
                cout << "Start to write packets" << endl;
                write(curr_recv, fp);
            }
            send_ack(ACK);
        }
    }
    fclose(fp);
	/* Now receive data and send acknowledgements */
    close(s);
	printf("%s received.", destinationFile);
    return;
}



int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}
