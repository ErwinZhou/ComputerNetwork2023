#pragma once
#include<WinSock2.h>
#include<string>
#include"Functions.h"
using namespace std;

class Client {
private:
	SOCKET client_socket;
	string name;
	int warning_times;

public:
	Client() {
		warning_times = 0;
	}
	SOCKET getClientSocket() {
		return client_socket;
	}
	void setClientSocket(SOCKET cs) {
		client_socket = cs;
	}
	string getClientName() {
		return name;
	}
	void setClientName(string na) {
		if (na == "\0") //������������Ǵ�β������û��������������
			name = "WeChatMinus User" + generateRandomString();//��������û���Ϊ��΢���û�xxxx
		else
			name = na;
	}
	int getWarningTimes() {
		return warning_times;
	}
	void setWarningTimes(int wt) {
		warning_times = wt;
	}
	bool operator==(const Client& other) const {//����==�����������ɾ��clientʱ�Ĳ���
		return client_socket == other.client_socket;  // �������SOCKET������Ϊsocket_member
	}
};
