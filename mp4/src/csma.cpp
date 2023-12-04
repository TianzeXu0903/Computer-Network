#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<vector>
#include<fstream>
#include<iostream>
using namespace std;

class node {
    public:
        int m_id;
        int m_backoff;
        int m_backindex;

    node (int ID, int BACKOFF, int BACKINDEX) {
        m_id = ID;
        m_backoff =  BACKOFF;
        m_backindex =  BACKINDEX;
    }
};

int N = 0;
int L = 0;
int M = 0;
vector<int> R;
vector<node> nodes;
int T = 0;
bool channelOccupied = false;
int occupyIndex = -1;
int occupyTime = 0;
int startOccupyTime = 0;

void read_params(string file) {
    ifstream infile(file);
    string line;
    int tmp = 0;
    getline(infile, line);
    N = std::stoi(line.substr(2, line.length()));
    getline(infile, line);
    L = std::stoi(line.substr(2, line.length()));
    getline(infile, line);
    M = std::stoi(line.substr(2, line.length()));
    getline(infile, line);
    line = line.substr(2, line.length());
    while (line.find(' ') != -1) {
        int space_index = line.find(' ');
        tmp = std::stoi(line.substr(0, space_index));
        R.push_back(tmp);
        line = line.substr(space_index + 1, line.length());
    }
    R.push_back(std::stoi(line));
    getline(infile, line);
    T = std::stoi(line.substr(2, line.length()));
}

void init_nodes(int n) {
    for(int i = 0; i < n; i++) {
        int curr_backoff = i % R[0];
        nodes.push_back(node(i, curr_backoff, 0));
    }
}

void simulate() {
    int curr_time = 0;
    while (curr_time < T) {
        cout << curr_time << "th: " << nodes[0].m_backoff << nodes[1].m_backoff << nodes[2].m_backoff << nodes[3].m_backoff << endl;
        vector<int> ready_send = {};
        if (curr_time - startOccupyTime == L) {
            channelOccupied = false;
        }

        if (channelOccupied) {
            curr_time += 1;
            continue;
        }
        for(int i = 0; i < N; i++) {
            if (nodes[i].m_backoff == 0) {
                ready_send.push_back(i);
            }
        }
        if (ready_send.empty()) {
            for (int j = 0; j < N; j++) {
                nodes[j].m_backoff -= 1;
            }

        } else if (ready_send.size() == 1) {
            // only one ready to send, this one start to send
            channelOccupied = true;
            startOccupyTime = curr_time;
            occupyIndex = ready_send[0];
            occupyTime += min(L, T - curr_time);
            nodes[ready_send[0]].m_backindex = 0;
            nodes[ready_send[0]].m_backoff = (nodes[ready_send[0]].m_id + curr_time + L) % R[0];
        } else {
            // collision happends
            for (int j = 0; j < ready_send.size(); j++) {
                nodes[ready_send[j]].m_backindex = (nodes[ready_send[j]].m_backindex + 1) % M;
                nodes[ready_send[j]].m_backoff = (nodes[ready_send[j]].m_id + curr_time + 1) % R[nodes[ready_send[j]].m_backindex];
            }
        }
        curr_time += 1;
    }
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    read_params(argv[1]);
    init_nodes(N);
    simulate();
    double rate = (float)occupyTime / (float)T;
    cout << "rate: " << rate << endl;

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    fprintf(fpOut, "%.2f", rate);
    fclose(fpOut);

    return 0;
}

