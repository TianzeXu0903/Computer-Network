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
#include<queue>

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
            route_table[node2][node1] = make_pair(node1, graph[node2][node1]);
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
            if (graph[src].count(tgt) == 0) {
                graph[src][tgt] = INT_MAX;
                graph[tgt][src] = INT_MAX;
            }
        }
    }
    topofile.close();
}


void dijkstra_routing() {
    for (int src : nodes) {
        // Create a priority queue to store vertices that are being preprocessed
        priority_queue< pair<int, int>, vector <pair<int, int>>, greater<pair<int, int>> > pq;

        // Initialize distances to all vertices as infinite and route_table[src] as undefined
        vector<int> dist(nodes.size(), INT_MAX);
        unordered_map<int, int> pred;

        // Insert source itself in priority queue and initialize its distance as 0
        pq.push(make_pair(0, src));
        dist[src] = 0;

        while (!pq.empty()) {
            // The first vertex in pair is the minimum distance vertex, extract it from priority queue
            int u = pq.top().second;
            pq.pop();

            // Traverse through all the adjacent vertices
            for (const auto& adj_pair : graph[u]) {
                int v = adj_pair.first;
                int weight = adj_pair.second;

                // Skip if weight is INT_MAX which implies no direct link
                if (weight == INT_MAX) continue;

                // If there is a shorter path to v through u
                if (dist[v] > dist[u] + weight) {
                    // Update distance of v
                    dist[v] = dist[u] + weight;
                    pq.push(make_pair(dist[v], v));
                    pred[v] = u;
                }
            }
        }

        // Update route_table for source node
        for (int tgt : nodes) {
            if (dist[tgt] != INT_MAX) {
                route_table[src][tgt] = make_pair(pred[tgt], dist[tgt]);
            }
        }
    }
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
    dijkstra_routing();
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
    dijkstra_routing();
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

