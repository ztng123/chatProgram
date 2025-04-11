#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <thread>
#include <queue>
#include <unistd.h>             // close()
#include <cstring>              // memset, strlen, strcmp
#include <sys/socket.h>         // socket(), connect(), send(), recv()
#include <netinet/in.h>         // sockaddr_in
#include <arpa/inet.h>          // inet_pton()
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/prepared_statement.h> 
using namespace std;
std::unique_ptr<sql::Connection> con;
string username, password;
int clientSocket;
bool run;

// thread 순서 조정이 어려워 일단 PASS

// void receiveThread() {
//     vector<char> buffer(1024);
//     while (true) {
//         int bytesReceived = recv(clientSocket, buffer.data(), buffer.size(), 0);
//         if (bytesReceived > 0) {
//             string response(buffer.data(), bytesReceived);
//             if (response == "exit") {
//                 cout << "연결종료" << endl;
//                 break;
//             }
//             cout << "응답 : "<< response << endl;
//         } 
//         else if (bytesReceived == 0){
//             break;
//         }
//         else if (run) {
//             cerr << "[recv 실패] errno: " << strerror(errno) << endl;
//             break;
//         }
//     }
// }

void receive() {
    vector<char> buffer(1024);
    int bytesReceived = recv(clientSocket, buffer.data(), buffer.size(), 0);
    if (bytesReceived > 0) {
        string response(buffer.data(), bytesReceived);
        if (response == "exit") {
            cout << "연결종료" << endl;
        }
        cout << "응답 : "<< response << endl;
    } 
}

void join(){
    cout << "이름을 입력하세요 : " << endl;
    getline(cin, username);
    cout << "비밀번호를 입력하세요 : " << endl;
    getline(cin, password);
    
    try {
        unique_ptr<sql::PreparedStatement> checkQuery(
            con->prepareStatement("SELECT COUNT(*) FROM users WHERE username = ?")
        );
        checkQuery->setString(1, username);

        unique_ptr<sql::ResultSet> res(checkQuery->executeQuery());
        res->next();

        if (res->getInt(1) > 0) {
            string message = "이미 존재하는 사용자:" + username + ":" + password + ":";
            send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
        } 
        else {
            unique_ptr<sql::PreparedStatement> insertQuery(
                con->prepareStatement("INSERT INTO users (username, password) VALUES (?, ?)")
            );
            insertQuery->setString(1, username);
            insertQuery->setString(2, password);

            int result = insertQuery->executeUpdate();

            string message = "REGISTER:" + username + ":" + password + ":";
            send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
        }
        receive();
    }
    catch (sql::SQLException& e) {
        cerr << "SQL 예외 발생: " << e.what() << endl;
    }

}


void login(string username,string password){
    
    try {
        unique_ptr<sql::PreparedStatement> checkQuery(
            con->prepareStatement("SELECT COUNT(*) FROM users WHERE username = ? AND password = ?")
        );
        checkQuery->setString(1, username);
        checkQuery->setString(2, password);

        unique_ptr<sql::ResultSet> res(checkQuery->executeQuery());
        res->next();

        if (res->getInt(1) > 0) {
            unique_ptr<sql::PreparedStatement> selectQuery(
            con->prepareStatement("SELECT user_id FROM users WHERE username = ?")
            );
            selectQuery->setString(1, username);
            unique_ptr<sql::ResultSet> res2(selectQuery->executeQuery());

            int user_id = -1;
            if (res2->next()) {
                user_id = res2->getInt("user_id");
            } else {
                cerr << "해당 username을 가진 사용자가 없습니다: " << username << endl;
                
            }
            unique_ptr<sql::PreparedStatement> loginQuery(
                con->prepareStatement("INSERT INTO user_sessions (user_id) VALUES(?)"));
            loginQuery->setInt(1, user_id);
            loginQuery->executeQuery();
            string message = "LOGIN:" + username + ":" + password;
            send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
        } 
        else {
            cout << "로그인 실패" << endl;

            string message = "로그인 실패:" + username + ":" + password + ":";
            send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
        }
        receive();
    }
    catch (sql::SQLException& e) {
        cerr << "SQL 예외 발생: " << e.what() << endl;
    }
}


bool chat(string message) {
    try {
        unique_ptr<sql::PreparedStatement> selectQuery(
            con->prepareStatement("SELECT user_id FROM users WHERE username = ?")
        );
        selectQuery->setString(1, username);
        unique_ptr<sql::ResultSet> res(selectQuery->executeQuery());

        int user_id = -1;
        if (res->next()) {
            user_id = res->getInt("user_id");
        } else {
            cerr << "해당 username을 가진 사용자가 없습니다: " << username << endl;
            
        }
        if (message == "/exit"){
            unique_ptr<sql::PreparedStatement> exitQuery(
                con->prepareStatement("UPDATE user_sessions SET logout_time = NOW() WHERE user_id = ?"));
            exitQuery->setInt(1, user_id);
            exitQuery->executeUpdate();
            string byeMsg = "exit";
            send(clientSocket, byeMsg.c_str(), static_cast<int>(byeMsg.length()), 0);
            receive();
            return false;
        }
        else{
            unique_ptr<sql::PreparedStatement> insertQuery(
                con->prepareStatement("INSERT INTO message_log (sender_id, content) VALUES (?, ?)")
            );
            insertQuery->setInt(1, user_id);
            insertQuery->setString(2, message);
            insertQuery->executeUpdate();

        
            string echoMsg = "CHAT:" + message;
            send(clientSocket, echoMsg.c_str(), static_cast<int>(echoMsg.length()), 0);
            receive();
            return true;
        }
    }
    catch (sql::SQLException& e) {
        cerr << "메시지 저장 실패: " << e.what() << endl;
    }
}


bool menu(int choice){
    if (choice == 1){
        join();
    }
    else if (choice == 2){
        cout << "이름을 입력하세요 : " << endl;
        getline(cin, username);
        cout << "비밀번호를 입력하세요 : " << endl;
        getline(cin, password);
        login(username,password);
        cout << "---------------------" << endl;
        cout << "1. 채팅하기 " << endl;
        cout << "2. 로그아웃 " << endl;
        cout << "3. 종료" << endl;
        int n;
        cin >> n;
        cin.ignore();
        if (n == 1)
        {
            while(true){
                string message;
                cout << "메시지를 입력하세요: ";
                getline(cin, message);
                if(!chat(message))break;
            }
        }
        else if (n == 2){
            chat("/exit");
        }
        else if (n == 3) return false;
        else
            cout << "잘못된 입력입니다 " << endl;
    }
    else if (choice == 3)
        return false;
    else
        cout << "잘못된 입력입니다 " << endl;
    return true;
}

int main(){
    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    const string server = "tcp://127.0.0.1:3306";
    const string name = "root";
    const string password = "1234";
    con = std::unique_ptr<sql::Connection>(driver->connect(server, name, password));
    con->setSchema("chat_program_db");

    sockaddr_in serverAddr;
    vector<char> buffer(1024);
    // 1. 소켓 생성
    clientSocket = socket(AF_INET, SOCK_STREAM,  0); 
    if (clientSocket == -1) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    // 2. 서버 주소 설정
    memset(&serverAddr, 0, sizeof(serverAddr));  // 0으로 초기화
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);  // 서버 포트와 동일하게 설정

    // 3. IP 주소 변환 및 저장 (127.0.0.1 = localhost)
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        return 1;
    }

    // 4. 서버에 연결 요청
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    std::cout << "Connected to server.\n";

    // std::thread recvThread(receiveThread);
    // recvThread.detach(); 

    while(true){
        cout << "---------------------" << endl;
        cout << "1. 회원가입 " << endl;
        cout << "2. 로그인" << endl;
        cout << "3. 종료" << endl;
        int choice;
        cin >> choice;
        cin.ignore();
        run = menu(choice);
        if(!run)
            break;
    }

    close(clientSocket);
}


