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
		if (na == "\0") //若输入进来的是串尾符，则没有正常输入名字
			name = "WeChatMinus User" + generateRandomString();//随机生成用户名为：微信用户xxxx
		else
			name = na;
	}
	int getWarningTimes() {
		return warning_times;
	}
	void setWarningTimes(int wt) {
		warning_times = wt;
	}
	bool operator==(const Client& other) const {//重载==运算符，用于删除client时的查找
		return client_socket == other.client_socket;  // 假设你的SOCKET变量名为socket_member
	}
};
