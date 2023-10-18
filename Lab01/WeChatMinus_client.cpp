#include<iostream>//IO功能
#include<WinSock2.h>//WinSock接口
#include"Functions.h"
#include"Client.h"
#include <ws2tcpip.h>//使用Inet_pton转换IP地址字符串
#include<vector>
#include<mutex>
#pragma comment(lib, "ws2_32.lib")////告诉编译器链接ws2_32.lib库文件


#ifndef EXIT_WORD
#define EXIT_WORD "EXIT"
#endif // !EXIT_WORD



char sendbuf[255] = { 0 };//定义发送消息的缓冲区
char recvbuf[255] = { 0 };//定义接收信息的缓冲区
SOCKET clientSocket;
WSAData wsadata;
sockaddr_in clientAddr;
string name;//用户名



/*
* 函数名：
*/
DWORD WINAPI RecvMessageThread(LPVOID lpParamter) {
	SOCKET cc = (SOCKET)lpParamter;
	int log = 0;//日志记录消息
	do
	{
		memset(recvbuf, 0, sizeof(recvbuf));
		log = recv(cc, recvbuf, 255, 0);//使用recv接收信息
		cout << string(recvbuf) << endl;
	} while (log != SOCKET_ERROR && log != 0);//直到连接被关闭即log=0或者SOCKET_ERROR再退出;
	return 0;
}

int main() {
	//输出欢迎信息
	cout << "Welcome to WeChat Minus! " << endl;
	//加载Winsock2环境
	//初始化所需的各种底层资源、参数以及可能的服务提供商的特定实现
	cout << "-----------Initializing Winsock-----------" << endl;
	if (WSAStartup(MAKEWORD(2, 0), &wsadata) != 0) {
		cout << "Oops!Failed to initialize Winsock " << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		return 0;
	}
	else
		cout << "Successfully initialized Winsock!" << endl;

	//套接字创建
	cout << "-----------Creating Socket-----------" << endl;
	//使用IPv4套接字
	//使用TCP协议的流式套接字
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		cout << "Oops!Failed to create socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//清空资源
		return 0;
	}
	else
		cout << "Successfully created socket!" << endl;
	//为套接字绑定IP地址和端口
	cout << "-----------Binding Socket-----------" << endl;
	clientAddr.sin_family = AF_INET; // 设置为IPv4
	clientAddr.sin_port = htons(2333); // 设置端口2333
	inet_pton(AF_INET, "127.0.0.1", &(clientAddr.sin_addr)); // 设置IPv4地址为127.0.0.1
	//因此不会发生错误，实际上如果是错误的地址格式也应该输出错误信息
	cout << "Successfully binded socket!" << endl;

	//连接服务器端
	cout << "-----------Connecting Server-----------" << endl;
	if (connect(clientSocket, (SOCKADDR*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) {
		cout << "Failed to connect server" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//清空资源
		return 0;
	}
	else
		cout << "Successfully connecting server!" << endl;

	//发送用户名创建用户
	memset(sendbuf, 0, sizeof(sendbuf));
	cout << "Please enter your name(No more than 255 words):" << endl;
	getline(cin, name);//读入一整行，包括空格
	send(clientSocket, name.data(), 255, 0);//发送给服务器创建用户

	//在与服务器建立了连接后，创建副线程接收信息，主线程还是用来发送信息
	HANDLE ThreadHandle = CreateThread(NULL, 0, RecvMessageThread, (LPVOID)clientSocket, 0, NULL);
	CloseHandle(ThreadHandle);

	//发送消息
	int log = 0;//日志记录消息
	do
	{
		memset(sendbuf, 0, sizeof(sendbuf));
		cin.getline(sendbuf, 255);//读入一行信息
		if (string(sendbuf) == EXIT_WORD)
			break;
		log = send(clientSocket, sendbuf, 255, 0);
	} while (log != SOCKET_ERROR && log != 0);//直到连接被关闭即log=0或者SOCKET_ERROR再退出;

	// 关闭连接，释放资源
	closesocket(clientSocket);
	WSACleanup();
	return 0;

}
