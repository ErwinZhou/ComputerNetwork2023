#pragma once
#include<string>
#include<WinSock2.h>
#include<iostream>
#include<fstream>
#include<vector>
#include<string>
using namespace std;

/*
* ��������GetLastErrorDetails
* ���ã���1��ʹ��WSAGetLastError��ȡ�������
*       ��2��ʹ��FormatMessageA��ȡ������Ϣ
*/
string GetLastErrorDetails() {
	int error_code = WSAGetLastError();

	char errorMsg[256] = { 0 };
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		errorMsg,
		sizeof(errorMsg) - 1,
		NULL
	);

	return string("Error code: ") + to_string(error_code) + ", Details: " + errorMsg;
}