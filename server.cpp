#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace fs = std::filesystem;

// ----------------- 线程池类 -----------------
class ThreadPool {
public:
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this]() { this->worker(); });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cond_var.notify_all();
        for (std::thread &t : workers) {
            if (t.joinable()) t.join();
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(task);
        }
        cond_var.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cond_var;
    bool stop;

    void worker() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cond_var.wait(lock, [this]() { return stop || !tasks.empty(); });
                if (stop && tasks.empty()) return;
                task = tasks.front();
                tasks.pop();
            }
            task();
        }
    }
};

// ----------------- HTTP服务器函数 -----------------
int create_socket(uint16_t port = 8080) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in saddr{};
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) { perror("bind"); return -1; }
    if (listen(sockfd, 50) == -1) { perror("listen"); return -1; }  // 增大 backlog
    return sockfd;
}

std::string get_content_type(const std::string &filename) {
    if (filename.size() >= 4 && filename.rfind(".css") == filename.size() - 4) return "text/css";
    if (filename.size() >= 3 && filename.rfind(".js") == filename.size() - 3) return "application/javascript";
    if (filename.size() >= 4 && filename.rfind(".png") == filename.size() - 4) return "image/png";
    if ((filename.size() >= 4 && filename.rfind(".jpg") == filename.size() - 4) ||
        (filename.size() >= 5 && filename.rfind(".jpeg") == filename.size() - 5)) return "image/jpeg";
    if (filename.size() >= 4 && filename.rfind(".gif") == filename.size() - 4) return "image/gif";
    if (filename.size() >= 4 && filename.rfind(".ico") == filename.size() - 4) return "image/x-icon";
    return "text/html";
}

std::string get_filename(const std::string &request) {
    std::istringstream ss(request);
    std::string method, path, protocol;
    ss >> method >> path >> protocol;
    std::cout << "请求方法: " << method << "\n";
    return path;
}

void handle_client(int client_sock) {
    char buffer[1024]{};
    int n = recv(client_sock, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) { close(client_sock); return; }

    std::cout << "Received request:\n" << buffer << "\n";

    std::string filename = get_filename(buffer);
    if (filename == "/") filename = "/music.html";

    std::string full_path = "/home/lmd/项目" + filename;

    if (!fs::exists(full_path)) {
        std::string not_found = "HTTP/1.0 404 Not Found\r\nContent-Length:0\r\nConnection: close\r\n\r\n";
        send(client_sock, not_found.c_str(), not_found.size(), 0);
        close(client_sock);
        return;
    }

    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    std::streamsize filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::ostringstream header;
    header << "HTTP/1.0 200 OK\r\n"
           << "Server: myweb_cpp_threadpool\r\n"
           << "Content-Length: " << filesize << "\r\n"
           << "Content-Type: " << get_content_type(filename) << "\r\n"
           << "Connection: close\r\n"   // ✅ 每个请求独立连接
           << "\r\n";

    std::string header_str = header.str();
    send(client_sock, header_str.c_str(), header_str.size(), 0);

    char buf[512];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        send(client_sock, buf, file.gcount(), 0);
    }

    close(client_sock);
    std::cout << "Client closed\n";
}

// ----------------- main -----------------
int main() {
    int sockfd = create_socket();
    if (sockfd == -1) return -1;

    std::cout << "Server listening on port 8080...\n";

    size_t num_threads = std::thread::hardware_concurrency() * 2;
    ThreadPool pool(num_threads);

    while (true) {
        sockaddr_in caddr{};
        socklen_t len = sizeof(caddr);
        int client_sock = accept(sockfd, (struct sockaddr*)&caddr, &len);
        if (client_sock < 0) { perror("accept"); continue; }

        std::cout << "Accepted client: " << client_sock << "\n";

        pool.enqueue([client_sock]() { handle_client(client_sock); });
    }

    close(sockfd);
    return 0;
}

