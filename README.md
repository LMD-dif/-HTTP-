项目简介：
基于 C++17 实现的轻量级多线程 HTTP 服务器，支持网页的访问。
采用 线程池 处理客户端请求，能够同时响应多个浏览器访问，
功能特性：
支持 HTTP/1.0 协议，每个请求独立连接，避免浏览器 keep-alive 阻塞。
支持多客户端并发访问，采用 线程池 提升性能。
静态文件服务：支持 HTML文件。
按块读取文件并发送，支持大文件传输。
日志输出：打印客户端请求、访问文件和处理状态。
技术栈：
语言：C++17
网络编程：POSIX Socket API（socket, bind, listen, accept, recv, send）
并发编程：std::thread, std::mutex, std::condition_variable
文件系统：std::ifstream, std::filesystem
构建工具：g++
运行环境：Linux
编译与运行：
git clone https://github.com/your-username/cpp-http-server.git
cd cpp-http-server
编译项目：
g++ server.cpp -o server -pthread -std=c++17
运行服务器：
./server
在浏览器中访问：
http://localhost:8080
假设你有一个 music.html 页面：
<!DOCTYPE html>
<html>
<head>
    <title>My Music Page</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <h1>Hello from C++ HTTP Server!</h1>
    <p>This is a test page.</p>
</body>
</html>
在浏览器访问 http://localhost:8080 即可看到页面效果。
