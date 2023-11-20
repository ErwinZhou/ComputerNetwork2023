#pragma once
#include<string>
#include<WinSock2.h>
#include<iostream>
#include<fstream>
#include<vector>
#include<string>
using namespace std;

/*
* 函数名：GetLastErrorDetails
* 作用：（1）使用WSAGetLastError获取错误代码
*       （2）使用FormatMessageA获取错误信息
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