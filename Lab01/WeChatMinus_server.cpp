#include<WinSock2.h>//Windows 上的套接字编程接口
#include<iostream>//IO流
#include<vector>
#include<mutex>
#include <ws2tcpip.h>//使用Inet_pton转换IP地址字符串
#include"Functions.h"
#include"Client.h"
#pragma comment(lib,"ws2_32.lib")//告诉编译器链接ws2_32.lib库文件
using namespace std;

#ifndef EXIT_WORD
#define EXIT_WORD "EXIT"
#endif // !EXIT_WORD



WSAData wsadata;
SOCKET serverSocket;
sockaddr_in serverAddr;
vector<Client> clients;

char sendbuf[255];//定义输入信息的缓冲区
char recvbuf[255];//用来定义接收新来的用户的用户名或者其它信息的缓冲区
time_t t;//时序性：保存当前时间
char str_time[26];
string str;
string warning_str;
bool yellowflag = false;//记录这位客官有没有发言违规
bool redflag = false;//记录这位客官是否已经违规达到3次
mutex clients_mutex;//互斥锁保证线程安全
/*
* 函数名：SendMessageTread
* 作用：用于客户端发布一些系统信息；
* 但是必须符合CreateThread的函数签名：DWORD WINAPI FunctionName(LPVOID lpParameter)
*/
DWORD WINAPI SendMessageThread(LPVOID lpParameter) {
	SOCKET ss = (SOCKET)lpParameter;

	int log;
	do {
		memset(sendbuf, 0, sizeof(sendbuf)); //输入信息缓冲区重置为0
		cin.getline(sendbuf, 255);//读取一行的内容放入缓冲区

		//将系统信息发送给所有人
		// 获取当前时间
		time(&t);
		ctime_s(str_time, sizeof str_time, &t);

		// 从ctime输出中移除尾部换行符
		str_time[strlen(str_time) - 1] = '\0';

		// 连接时间，服务器标签和消息
		str = "<WeChatMinus::Server @ " + string(str_time) + " # Message>: " + string(sendbuf);//Server发言协议格式：
		//<WeChat::Server @ Wed Oct 18 09:31:15 2023 # Message>

		for (int i = 0; i < clients.size(); i++)
			log = send(clients[i].getClientSocket(), str.data(), 255, 0);//调用send对所有人广播发送信息
	} while (log != SOCKET_ERROR && log != 0); //直到来凝结被关闭即log=0或者SOCKET_ERROR再退出

	return 0;
}

/*
* 函数名：RecvMessageTread
* 作用：用于客户端接受其他用户的信息；
* 但是必须符合CreateThread的函数签名：DWORD WINAPI FunctionName(LPVOID lpParameter)
*/
DWORD WINAPI RecvMessageThread(LPVOID lpParameter) {
	SOCKET cs = (SOCKET)lpParameter;

	//接收登录进来的客户的用户名
	memset(recvbuf, 0, sizeof(recvbuf));//缓冲区重置为0，准备接收用户名
	int log;
	log = recv(cs, recvbuf, 255, 0);//用来接收用户名放入缓冲区并且返回log;
	string username;
	Client c;
	if (recvbuf[0] == '\0') {//如果该用户没有输入用户名，随机生成用户名
		c.setClientSocket(cs);
		c.setClientName("\0");//随机生成
		{	
			lock_guard<mutex> lock(clients_mutex);//使用clients_mutex保护锁定clients
			clients.push_back(c);
		}
		username = c.getClientName();
	}
	else {
		c.setClientSocket(cs);
		c.setClientName(string(recvbuf));
		{
			lock_guard<mutex> lock(clients_mutex);//使用clients_mutex保护锁定clients
			clients.push_back(c);
		}
		username = c.getClientName();
	}
	//将系统信息发送给所有人
	// 获取当前时间
	time(&t);
	ctime_s(str_time, sizeof str_time, &t);
	// 从ctime输出中移除尾部换行符
	str_time[strlen(str_time) - 1] = '\0';
	// 连接时间，服务器标签和消息
	string str = "<WeChatMinus::Server @ " + string(str_time) + " # Notice>: " + "Welcome <" + username + "> join the ChatGroup!";//Server发言公告协议格式：
	//<WeChat::Server @ Wed Oct 18 09:31:15 2023 # Notice>
	cout << str << endl;

	for (int i = 0; i < clients.size(); i++)
		send(clients[i].getClientSocket(), str.data(), 255, 0);//调用send对所有人广播发送信息


	log = 0;//重置log
	do {
		memset(recvbuf, 0, sizeof(recvbuf));//重置接收信息的缓冲区
		log = recv(cs, recvbuf, 255, 0);//接收信息

		//若输入关键词"EXIT"退出聊天程序
		if (string(recvbuf) == EXIT_WORD) {
			break;
		}

		//获取当前时间
		time(&t);
		ctime_s(str_time, sizeof str_time, &t);

		// 从ctime输出中移除尾部换行符
		str_time[strlen(str_time) - 1] = '\0';

		yellowflag = false;
		redflag = false;
		string filteredtext = filterSensitiveWords(string(recvbuf), yellowflag);//调用过滤器过滤文字
		if (yellowflag) {//如果是违规了
			c.setWarningTimes(c.getWarningTimes()+1);//警告次数+1
			if (c.getWarningTimes() > 3) {//警告次数达到3次以上，直接结束对话
				redflag = true;
				break;
			}	
		}
		// 连接时间，服务器标签和消息
		str = "<WeChatMinus::" + username + " @ " + string(str_time) + " # Message>: " + filteredtext;//Server发言协议格式：
		//<WeChat::Server @ Wed Oct 18 09:31:15 2023 # Message>
		cout << str << endl;

		for (int i = 0; i < clients.size(); i++)
			send(clients[i].getClientSocket(), str.data(), 255, 0);//调用send对所有人广播发送信息

	} while (log != SOCKET_ERROR && log != 0); //直到连接被关闭即log=0或者SOCKET_ERROR再退出



	//将系统信息发送给所有人
	// 获取当前时间
	time(&t);
	ctime_s(str_time, sizeof str_time, &t);

	// 从ctime输出中移除尾部换行符
	str_time[strlen(str_time) - 1] = '\0';

	if (redflag) {//对所有人发送公告
		// 连接时间，服务器标签和消息
		str = "<WeChatMinus::Server @ " + string(str_time) + " # Notice>: " + "<" + username + "> has been kicked out of the ChatGroup.";//Server发言协议格式：
		//<WeChat::Server @ Wed Oct 18 09:31:15 2023 # Notice>
		cout << str << endl;
		warning_str = "<WeChatMinus::Server @ " + string(str_time) + " # Warning>: ";
		string str1 = "网络不是法外之地，在网络上散布不当言论需要承担法律责任。对于在互联网上发布不当言论，扰乱社会秩序，公安机关将坚决依法处理，造成恶劣影响，情节严重的，警方将依法追究其法律责任！";
		string str2 = "每个公民都要对自己的网络言行负责，希望每个公民、网民，自觉抵制不当言论、恶意攻击等不良信息，不轻信、不转发，请广大网民自觉遵守相关法律法规，共同维护和谐网络环境。";//Server发言协议格式：
		//<WeChat::Server @ Wed Oct 18 09:31:15 2023 # Warning>
		cout << warning_str << endl;
		cout << str1 << endl;
		cout << str2 << endl;
		for (int i = 0; i < clients.size(); i++) {
			send(clients[i].getClientSocket(), str.data(), 255, 0);//调用send对所有人广播发送信息
			send(clients[i].getClientSocket(), warning_str.data(), 255, 0);
			send(clients[i].getClientSocket(), str1.data(), 255, 0);
			send(clients[i].getClientSocket(), str2.data(), 255, 0);
		}


		{
			lock_guard<mutex> lock(clients_mutex);//使用clients_mutex保护锁定clients
			//警告完你再把你踢出去
			//删除Client c
			clients.erase(std::remove(clients.begin(), clients.end(), c), clients.end());
		}
		

		return 0;
	}
	// 连接时间，服务器标签和消息
	str = "<WeChatMinus::Server @ " + string(str_time) + " # Notice>: " + "<" + username + "> left the ChatGroup.";//Server发言协议格式：
	//<WeChat::Server @ Wed Oct 18 09:31:15 2023 # Notice>
	cout << str << endl;

	{
		lock_guard<mutex> lock(clients_mutex);//使用clients_mutex保护锁定clients
		//删除Client c
		clients.erase(std::remove(clients.begin(), clients.end(), c), clients.end());
	}

	//等你退出后再把你的消息告诉别人
	for (int i = 0; i < clients.size(); i++)
		send(clients[i].getClientSocket(), str.data(), 255, 0);//调用send对所有人广播发送信息
	
	
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
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
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
	serverAddr.sin_family = AF_INET; // 设置为IPv4
	serverAddr.sin_port = htons(2333); // 设置端口2333
	inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)); // 设置IPv4地址为127.0.0.1
	//因此不会发生错误，实际上如果是错误的地址格式也应该输出错误信息
	if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cout << "Oops!Failed to bind socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//清空资源
		return 0;
	}
	else
		cout << "Successfully binded socket!" << endl;
	//开始监听
	cout << "-----------Start Listening-----------" << endl;
	if (listen(serverSocket, 999) != 0) {
		cout << "Oops!Failed to listen for connections" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//清空资源
		return 0;
	}
	else
		cout << "Successfully started listening!" << endl;

	//服务器等待
	cout << "Now let's wait......until somone knocks...." << endl;
	cout << "-----------Listening-----------" << endl;

	
	// 创建Server发送消息的线程并关闭线程句柄
	CloseHandle(CreateThread(NULL, 0, SendMessageThread, (LPVOID)serverSocket, 0, NULL));

	while (true) {
		sockaddr_in addrClient;

		int len = sizeof(addrClient);
		SOCKET cc = accept(serverSocket, (sockaddr*)&addrClient, &len);

		if (cc == INVALID_SOCKET) {
			cout << "Oops!Failed to connect a client" << endl;
			cout << GetLastErrorDetails() << endl;
		}

		//创建接收该Client的线程并释放句柄
		CloseHandle(CreateThread(NULL, 0, RecvMessageThread, (LPVOID)cc, 0, NULL));

	}
	closesocket(serverSocket);
	WSACleanup();
	return 0;



}