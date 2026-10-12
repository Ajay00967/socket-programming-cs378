#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

constexpr int INF = 2E9;

int main(int argc, char* argv[]) {

    if (argc != 2) {
        cerr << "Expected a configuration file. Exiting\n";
        return 1;
    }

    ifstream file(argv[1]);
    
    if (!file.is_open()) {
        cerr << "Couldn't open file. Exiting\n";
        return 1;
    }
    
    vector<vector<int>> rows;
    string line;
    
    while (getline(file, line)) {
        stringstream ss(line);
        vector<int> row;
        int x;

        while (ss >> x) row.push_back(x);
        if (!row.empty()) rows.push_back(row);
    }

    int n = rows.size() + 1;
    vector adj(n, vector<int>(n, -1));

    for (int i = 0; i < n; i++) adj[i][i] = 0;

    for (int i = 0; i < rows.size(); i++) {
        for (int j = 0; j < rows[i].size(); j++) {
            adj[i][i + j + 1] = adj[i + j + 1][i] = (rows[i][j] == -1 ? INF : rows[i][j]);
        }
    }

    // for (auto& row : adj) {
    //     for (int x : row) cout << x << ' ';
    //     cout << '\n';
    // }

    int s = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5000);

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "Port in use\n";
        close(s);
        return 1;
    }

    listen(s, 5);

    vector<int> clients(n);

    for (int i = 0; i < n; i++) {
        int c = accept(s, nullptr, nullptr);

        if (c < 0) {
            close(s);
            return 1;
        }

        clients[i] = c;
        cout << "Node " << char('A' + i) << " connected!\n";
    }

    cout << "All VNs connected.\n";

    return 0;
}
