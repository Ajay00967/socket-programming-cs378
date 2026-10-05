#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
using namespace std;

int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(s, (sockaddr*)&addr, sizeof(addr));

    string msg;
    char buf[1024];

    while (true) {
        cout << "Enter message: ";
        getline(cin, msg);

        if (msg == "exit") break;

        send(s, msg.c_str(), msg.size(), 0);

        memset(buf, 0, sizeof(buf));
        recv(s, buf, sizeof(buf)-1, 0);

        cout << "Server: " << buf << endl;
    }

    close(s);
}
