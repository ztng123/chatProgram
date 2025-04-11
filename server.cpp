#include <iostream>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/prepared_statement.h> 

const std::string DB_HOST = "tcp://127.0.0.1:3306";
const std::string DB_USER = "root";
const std::string DB_PASS = "1234"; 
const std::string DB_NAME = "chat_program_db";
int clientSocket;

void handleClient(int clientSocket) {
    std::vector<char> buffer(1024);  
    while (true) {
    int recvLen = recv(clientSocket, buffer.data(), buffer.size(), 0);

    if (recvLen <= 0) {
        close(clientSocket);
        return;
    }

    std::string msg(buffer.data(), recvLen);
    std::cout << "[RECV] " << msg << std::endl;

    if (msg.rfind("LOGIN:", 0) == 0) {
        size_t pos1 = msg.find(":", 6);

        if (pos1 == std::string::npos) {
            std::string error = "Invalid Format";
            send(clientSocket, error.c_str(), error.length(), 0);
            close(clientSocket);
            return;
        }

        std::string username = msg.substr(6, pos1 - 6);
        std::string password = msg.substr(pos1 + 1);

        try {
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
            std::unique_ptr<sql::Connection> conn(
                driver->connect(DB_HOST, DB_USER, DB_PASS)
            );
            conn->setSchema(DB_NAME);

            std::unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT * FROM users WHERE username = ? AND password = ?")
            );
            pstmt->setString(1, username);
            pstmt->setString(2, password);

            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            std::string result = res->next() ? "Login Success" : "Login Failed";
            send(clientSocket, result.c_str(), result.length(), 0);
        }
        catch (sql::SQLException& e) {
            std::cerr << "MySQL Error: " << e.what() << std::endl;
            std::string err = "DB Error";
            send(clientSocket, err.c_str(), err.length(), 0);
        }
    }
    else if (msg.rfind("CHAT:", 0) == 0) {
            std::string chat = msg.substr(5);
            std::string echo = "ECHO:" + chat;
            send(clientSocket, echo.c_str(), echo.length(), 0);
        }
    else if (msg.rfind("REGISTER:",0)== 0){
        std::string regist = "새 사용자가 등록되었습니다";
        send(clientSocket, regist.c_str(), regist.length(), 0);
    }
    else if (msg.rfind("exit",0) == 0) {
            std::string bye = "연결종료";
            send(clientSocket, bye.c_str(), bye.length(), 0);
        }
    else if (msg.rfind("이미 존재하는 사용자:",0)==0){
            std::string bye = "이미 존재하는 사용자 이름 입니다";
            send(clientSocket, bye.c_str(), bye.length(), 0);
    }    
    else {
        std::string error = "Unknown Command";
        send(clientSocket, error.c_str(), error.length(), 0);
    }
}
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        perror("socket() failed");
        return 1;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // bind 검사
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind() failed");
        close(serverSocket);
        return 1;
    }

    // listen 검사
    if (listen(serverSocket, 5) < 0) {
        perror("listen() failed");
        close(serverSocket);
        return 1;
    }

    std::cout << " Server listening ...\n";

    sockaddr_in clientAddr;
    socklen_t clientSize = sizeof(clientAddr);
    while(true){
    clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
    if (clientSocket < 0) {
        perror("accept() failed");
        close(serverSocket);
        return 1;
    }

    std::cout << " Client connected!\n";
    std::thread(handleClient, clientSocket).detach();
    }

    char buffer[1024];
    int recvLen;

    while ((recvLen = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[recvLen] = '\0';
        std::cout << " Received: " << buffer << std::endl;
        send(clientSocket, buffer, recvLen, 0);
    }

    close(clientSocket);
    close(serverSocket);
    return 0;
}
