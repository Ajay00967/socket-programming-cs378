#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <cstdint>
using namespace std;

constexpr int INF = 2E9;

struct Node {
    int tcpfd = -1;
    in_addr ip{};
    int udpPort = 0;
};

bool recvAll(int fd, void *data, size_t size) {
    char *p = (char *) data;
    size_t done = 0;
    while (done < size) {
        ssize_t n = recv(fd, p + done, size - done, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += n;
    }
    return true;
}

vector<char> makeLinkState(vector<Node> &clients, vector<vector<int>> &adj, int n, int id){
    vector<int> entries;
    entries.push_back(id);
    if (id < n) {
        for (int j = 0; j < n; j++) {
            if (j != id && adj[id][j] != INF)
                entries.push_back(j);
        }
    }

    vector<char> msg;
    msg.push_back((char)entries.size());
    for (int idn : entries) {
        const Node &target = clients[idn];
        msg.push_back((char)('A' + idn));

        auto *ip = (char *)(&target.ip.s_addr);
        msg.insert(msg.end(), ip, ip + 4);

        uint16_t netPort = htons(target.udpPort);
        const auto *portBytes = (char *)(&netPort);
        msg.insert(msg.end(), portBytes, portBytes + 2);

        int cost = (idn == id) ? 0 : adj[id][idn];
        int netCost = htonl(cost);
        auto *costBytes = (char *)(&netCost);
        msg.insert(msg.end(), costBytes, costBytes + 4);
    }
    return msg;
}

bool sendLinkState(int id, vector<Node> &clients, vector<vector<int>> &adj, int n) {
    if (clients[id].tcpfd < 0) return false;

    vector<char> msg = makeLinkState(clients, adj, n, id);

    char *p = msg.data();
    size_t size = msg.size();
    size_t done = 0;

    while (done < size) {
        ssize_t sent = send(clients[id].tcpfd, p + done, size - done, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return false;
        done += static_cast<size_t>(sent);
    }

    return true;
}

long long getModifiedTime(const string &path) {
    struct stat fileInfo{};
    if (stat(path.c_str(), &fileInfo) != 0) return -1;
    return (long long)fileInfo.st_mtim.tv_sec * 1000000000LL + fileInfo.st_mtim.tv_nsec;
}

bool readConfig(const string &path, vector<vector<int>> &adj) {
    ifstream file(path);
    
    if (!file.is_open()) {
        cerr << "Couldn't open file. Exiting\n";
        return false;
    }
    
    vector<vector<int>> rows;
    string line;
    
    while (getline(file, line)) {
        stringstream ss(line);
        vector<int> row;
        int x;

        if (line.empty() || line[0] == '#') continue;

        while (ss >> x) row.push_back(x);
        if (!row.empty()) rows.push_back(row);
    }

    int n = rows.size() + 1;
    for (int i = 0; i < (int)rows.size(); i++) {
        if ((int)rows[i].size() != n - 1 - i) {
            cerr << "Config has wrong row length, ignoring\n";
            return false;
        }
    }
    adj.assign(n, vector<int>(n, -1));

    for (int i = 0; i < n; i++) adj[i][i] = 0;

    for (int i = 0; i < (int)rows.size(); i++) {
        for (int j = 0; j < (int)rows[i].size(); j++) {
            adj[i][i + j + 1] = adj[i + j + 1][i] = (rows[i][j] == -1 ? INF : rows[i][j]);
        }
    }
    return true;
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        cerr << "Expected a configuration file. Exiting\n";
        return 1;
    }

    vector<vector<int>> adj;
    if (!readConfig(argv[1], adj)) {
        cerr << "Invalid configuration file\n";
        return 1;
    }
    int n = adj.size();

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        cerr << "Failed to create socket. Exiting\n"; 
        return 1; 
    }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5000);

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "Failed to bind port 5000 (already in use). Exiting\n";
        close(s);
        return 1;
    }

    listen(s, 26);

    vector<Node> clients(n);

    for (int i = 0; i < n; i++) {
        int c = accept(s, nullptr, nullptr);

        if (c < 0) {
            cerr << "Failed to accept connection. Exiting\n";
            close(s);
            return 1;
        }

        unsigned char msg[6];

        if (!recvAll(c, msg, sizeof(msg))) {
            cerr << "Failed to receive CONNECT from node "<< char('A' + i) << '\n';
            close(c);
            close(s);
            return 1;
        }

        clients[i].tcpfd = c;

        memcpy(&clients[i].ip.s_addr, msg, 4);

        uint16_t netPort;
        memcpy(&netPort, msg + 4, 2);
        clients[i].udpPort = ntohs(netPort);

        char ipstr[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &clients[i].ip, ipstr, sizeof(ipstr)) == nullptr) {
            cerr << "Failed to convert node IP address. Exiting\n";
            close(c);
            close(s);
            return 1;
        }

        cout << "Node " << char('A' + i) << " connected: " << ipstr << ':' << clients[i].udpPort << endl;

    }

    for (int i = 0; i < n; i++) {
        if (!sendLinkState(i, clients, adj, n)) {
            cerr << "Failed to send LINK-STATE to node " << char('A' + i) << '\n';
        }
    }


    long long lastModified = getModifiedTime(argv[1]);

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(s, &readSet);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(s + 1, &readSet, nullptr, nullptr, &timeout);

        if (ready > 0 && FD_ISSET(s, &readSet)) {
            int c = accept(s, nullptr, nullptr);

            if (c >= 0) {
                if (clients.size() >= 26) {
                    close(c);
                } else {
                    unsigned char msg[6];

                    if (!recvAll(c, msg, sizeof(msg))) {
                        close(c);
                    } else {
                        Node extra;
                        extra.tcpfd = c;
                        memcpy(&extra.ip.s_addr, msg, 4);

                        uint16_t netPort;
                        memcpy(&netPort, msg + 4, 2);
                        extra.udpPort = ntohs(netPort);

                        clients.push_back(extra);
                        int id = clients.size() - 1;

                        char ipstr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &extra.ip, ipstr, sizeof(ipstr));

                        cout << "Node " << char('A' + id) << " connected (isolated): " << ipstr << ':' << extra.udpPort << endl;

                        if (!sendLinkState(id, clients, adj, n)) {
                            cerr << "Failed to send LINK-STATE to node " << char('A' + id) << '\n';
                        }
                    }
                }
            }
        }

        long long currentModified = getModifiedTime(argv[1]);

        if (currentModified == -1 || currentModified == lastModified)
            continue;

        vector<vector<int>> newAdj;

        if (!readConfig(argv[1], newAdj) || newAdj.size() < adj.size() || newAdj.size() > clients.size())
            continue;

        adj = newAdj;
        n = adj.size();

        for (int i = 0; i < (int)clients.size(); i++) {
            if (!sendLinkState(i, clients, adj, n)) {
                cerr << "Failed to send LINK-STATE to node "<< char('A' + i) << '\n';
            }
        }

        cout << "Config modified, LINK-STATE sent to all nodes\n";

        lastModified = currentModified;
    }


    return 0;
}
