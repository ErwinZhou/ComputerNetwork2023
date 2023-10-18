#include<iostream>//IO����
#include<WinSock2.h>//WinSock�ӿ�
#include"Functions.h"
#include"Client.h"
#include <ws2tcpip.h>//ʹ��Inet_ptonת��IP��ַ�ַ���
#include<vector>
#include<mutex>
#pragma comment(lib, "ws2_32.lib")////���߱���������ws2_32.lib���ļ�


#ifndef EXIT_WORD
#define EXIT_WORD "EXIT"
#endif // !EXIT_WORD



char sendbuf[255] = { 0 };//���巢����Ϣ�Ļ�����
char recvbuf[255] = { 0 };//���������Ϣ�Ļ�����
SOCKET clientSocket;
WSAData wsadata;
sockaddr_in clientAddr;
string name;//�û���



/*
* ��������
*/
DWORD WINAPI RecvMessageThread(LPVOID lpParamter) {
	SOCKET cc = (SOCKET)lpParamter;
	int log = 0;//��־��¼��Ϣ
	do
	{
		memset(recvbuf, 0, sizeof(recvbuf));
		log = recv(cc, recvbuf, 255, 0);//ʹ��recv������Ϣ
		cout << string(recvbuf) << endl;
	} while (log != SOCKET_ERROR && log != 0);//ֱ�����ӱ��رռ�log=0����SOCKET_ERROR���˳�;
	return 0;
}

int main() {
	//�����ӭ��Ϣ
	cout << "Welcome to WeChat Minus! " << endl;
	//����Winsock2����
	//��ʼ������ĸ��ֵײ���Դ�������Լ����ܵķ����ṩ�̵��ض�ʵ��
	cout << "-----------Initializing Winsock-----------" << endl;
	if (WSAStartup(MAKEWORD(2, 0), &wsadata) != 0) {
		cout << "Oops!Failed to initialize Winsock " << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		return 0;
	}
	else
		cout << "Successfully initialized Winsock!" << endl;

	//�׽��ִ���
	cout << "-----------Creating Socket-----------" << endl;
	//ʹ��IPv4�׽���
	//ʹ��TCPЭ�����ʽ�׽���
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		cout << "Oops!Failed to create socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//�����Դ
		return 0;
	}
	else
		cout << "Successfully created socket!" << endl;
	//Ϊ�׽��ְ�IP��ַ�Ͷ˿�
	cout << "-----------Binding Socket-----------" << endl;
	clientAddr.sin_family = AF_INET; // ����ΪIPv4
	clientAddr.sin_port = htons(2333); // ���ö˿�2333
	inet_pton(AF_INET, "127.0.0.1", &(clientAddr.sin_addr)); // ����IPv4��ַΪ127.0.0.1
	//��˲��ᷢ������ʵ��������Ǵ���ĵ�ַ��ʽҲӦ�����������Ϣ
	cout << "Successfully binded socket!" << endl;

	//���ӷ�������
	cout << "-----------Connecting Server-----------" << endl;
	if (connect(clientSocket, (SOCKADDR*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) {
		cout << "Failed to connect server" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//�����Դ
		return 0;
	}
	else
		cout << "Successfully connecting server!" << endl;

	//�����û��������û�
	memset(sendbuf, 0, sizeof(sendbuf));
	cout << "Please enter your name(No more than 255 words):" << endl;
	getline(cin, name);//����һ���У������ո�
	send(clientSocket, name.data(), 255, 0);//���͸������������û�

	//������������������Ӻ󣬴������߳̽�����Ϣ�����̻߳�������������Ϣ
	HANDLE ThreadHandle = CreateThread(NULL, 0, RecvMessageThread, (LPVOID)clientSocket, 0, NULL);
	CloseHandle(ThreadHandle);

	//������Ϣ
	int log = 0;//��־��¼��Ϣ
	do
	{
		memset(sendbuf, 0, sizeof(sendbuf));
		cin.getline(sendbuf, 255);//����һ����Ϣ
		if (string(sendbuf) == EXIT_WORD)
			break;
		log = send(clientSocket, sendbuf, 255, 0);
	} while (log != SOCKET_ERROR && log != 0);//ֱ�����ӱ��رռ�log=0����SOCKET_ERROR���˳�;

	// �ر����ӣ��ͷ���Դ
	closesocket(clientSocket);
	WSACleanup();
	return 0;

}
