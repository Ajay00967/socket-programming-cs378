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
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5000);

    bind(s, (sockaddr*)&addr, sizeof(addr));
    listen(s, 5);

    int c = accept(s, nullptr, nullptr);

    char buf[1024];

    while (true) {
        memset(buf, 0, sizeof(buf));
        int n = recv(c, buf, sizeof(buf)-1, 0);
        if (n <= 0) break;

        cout << "Client: " << buf << endl;

        string msg = "Message received!";
        send(c, msg.c_str(), msg.size(), 0);
    }

    close(c);
    close(s);
}
