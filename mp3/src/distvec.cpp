#include<stdio.h>
#include<iostream>
#include<fstream>
#include<unordered_map>
#include<unordered_set>
#include<set>
#include<vector>
#include<string.h>
#include<limits.h>
#include<stdlib.h>
#include<sstream>
#include<algorithm>

using namespace std;

typedef struct {
    int src;
    int tgt;
    string msg;
} message;

vector<message> msgs;

unordered_map<int, unordered_map<int, int>> graph;

unordered_map<int, unordered_map<int, pair<int, int>>> route_table;

unordered_set<int> nodes;

ofstream fpOut;

void update_route_table(){
    route_table.clear();
    for (int node1 : nodes) {
        for (int node2 : nodes) {
            route_table[node1][node2] = make_pair(node2, graph[node1][node2]);
        }
    }
}

void read_topo(char* infile){
    ifstream topofile;
    topofile.open(infile, ios::in);
    int src, tgt, dist;
    while (topofile >> src >> tgt >> dist) {
        graph[src][src] = 0;
        graph[tgt][tgt] = 0;
        graph[src][tgt] = dist;
        graph[tgt][src] = dist;
        nodes.insert(src);
        nodes.insert(tgt);
    }
    for (int src : nodes) {
        for (int tgt: nodes) {
            if (graph[src].count(tgt) <= 0) {
                graph[src][tgt] = INT_MAX;
            }
        }
    }
    topofile.close();
}



void bellman_ford_routing() {
    // Initialize the route_table with maximum distances
    for (int src : nodes) {
        for (int tgt : nodes) {
            route_table[src][tgt] = make_pair(-1, (src == tgt) ? 0 : INT_MAX);
        }
    }

    // Relax edges repeatedly
    for (int node1 : nodes) {
        for (int i = 0; i < nodes.size() - 1; i++) {
            for (int src : nodes) {
                for (const auto& adj_pair : graph[src]) {
                    int tgt = adj_pair.first;
                    int weight = adj_pair.second;

                    if (route_table[src][tgt].second > route_table[node1][src].second + weight) {
                        route_table[src][tgt] = make_pair(src, route_table[node1][src].second + weight);
                    }
                }
            }
        }
    }

    // Check for negative-weight cycles
    for (int src : nodes) {
        for (const auto& adj_pair : graph[src]) {
            int tgt = adj_pair.first;
            int weight = adj_pair.second;

            if (route_table[src][tgt].second > route_table[src][src].second + weight) {
                cout << "Graph contains negative weight cycle" << endl;
                return;
            }
        }
    }
}





void dv_routing(){
    bool is_change = false;
    do {
        for (int src : nodes) {
            for (int mid : nodes) {
                if (graph[src][mid] == INT_MAX) {
                    continue;
                }
                for (int tgt : nodes) {
                    if (route_table[mid][tgt].second == INT_MAX || src == tgt) {
                        continue;
                    }
                    if (route_table[src][tgt].second > route_table[src][mid].second + route_table[mid][tgt].second) {
                        route_table[src][tgt] = make_pair(mid, route_table[src][mid].second + route_table[mid][tgt].second);
                        is_change = true;
                    }

                }

            }
        }
    } while (!is_change);
}

void handle_change(int src, int tgt, int distance) {

    if (distance == -999) {
        graph[src][tgt] = INT_MAX;
        graph[tgt][src] = INT_MAX;
    } else {
        graph[src][tgt] = distance;
        graph[tgt][src] = distance;
    }
    nodes.insert(src);
    nodes.insert(tgt);
    update_route_table();
    bellman_ford_routing();
}

pair<vector<int>, int> find_path(int src, int tgt) {
    vector<int> path;
    int distance = INT_MAX;
    if (route_table.count(src) == 0 || route_table[src].count(tgt) == 0 || route_table[src][tgt].second == INT_MAX) {
        return make_pair(path, distance);
    } else if (src == tgt) {
        return make_pair(path, 0);
    } else {
        distance = route_table[src][tgt].second;
        int curr = src;
        while (curr != tgt) {
            path.push_back(curr);
            curr = route_table[curr][tgt].first;
        }
        return make_pair(path, distance);
    }
}

void send_message() {
    for (int i = 0; i < msgs.size(); i++) {
        pair<vector<int>, int> curr = find_path(msgs[i].src, msgs[i].tgt);
        if (curr.second == INT_MAX) {
            fpOut << "from " << msgs[i].src << " to " << msgs[i].tgt << " cost " << "infinite hops unreachable" << endl;
        } else {
            fpOut << "from " << msgs[i].src << " to " << msgs[i].tgt << " cost " << curr.second << " hops ";
            for (int j = 0; j < curr.first.size(); j++) {
                fpOut << curr.first[j] << " ";
            }
        }
        fpOut << "message " << msgs[i].msg << endl;
    }
}

void write_route_table() {
    vector<int> sorted_nodes(nodes.begin(), nodes.end());
    std::sort(sorted_nodes.begin(), sorted_nodes.end());

    for (int node1 : sorted_nodes) {
        for (int node2 : sorted_nodes) {
            if (route_table.count(node1) > 0 && route_table[node1].count(node2) > 0 && route_table[node1][node2].second != INT_MAX) {
                fpOut << node2 << " " << route_table[node1][node2].first << route_table[node1][node2].second << endl;
            }
        }
    }
}

void read_message(char* filename){
    ifstream infile(filename);

    string line;
    if (infile.is_open()) {
        while (getline(infile, line)) {
            stringstream ss(line);
            message msg;
            ss >> msg.src >> msg.tgt;
            ss.ignore();
            getline(ss, msg.msg);
            msgs.push_back(msg);
        }
    }
    infile.close();


}


int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }
    fpOut.open("output.txt");
    read_topo(argv[1]);
    read_message(argv[2]);
    update_route_table();
    bellman_ford_routing();
    write_route_table();
    send_message();

    // loop to handle changes.
    ifstream change_file;
    int src, tgt, dist;
    change_file.open(argv[3]);
    while (change_file >> src >> tgt >> dist) {
        handle_change(src, tgt, dist);
        write_route_table();
        send_message();
    }
    change_file.close();
    fpOut.close();

    return 0;
}

