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

/*
* 函数名：generateRandomString
* 作用：为没有用户名的用户随机生成名字
*/
string generateRandomString() {
    srand(time(NULL));
    int length = rand() % 10 + 1;  // 随机生成1到10之间的长度
    const string CHAR_POOL = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    string result;

    for (int i = 0; i < length; ++i) {
        int randomIndex = rand() % CHAR_POOL.size();
        result += CHAR_POOL[randomIndex];
    }

    return result;
}

/*
* 函数名：filterSensitiveWords
* 作用：过滤任何人输入的任何可能的敏感词，
* 返回：过滤后的字符串和是否含有敏感词flag
*/
string filterSensitiveWords(const string& text, bool& flag) {
	vector<string> sensitiveWords;//敏感词vector

	// 读取敏感词文件
	ifstream file("sensitiveWords.txt");
	if (file.is_open()) {
		string word;
		while (getline(file, word)) {//每次读取一行
			sensitiveWords.push_back(word);
		}
		file.close();
	}

	// 过滤敏感词
	string filteredText = text;
	for (const string& word : sensitiveWords) {//for each结构，每次从中读取一个word
		size_t pos = filteredText.find(word);//使用size_t类型来存储字符串的位置
		while (pos != string::npos) {//一直执行，直到 pos 的值等于string::npos;
			//string::npos 是一个特殊的 size_t值，表示未找到匹配的子字符串。
			filteredText.replace(pos, word.length(), string(word.length(), '*'));//用*替换
			flag = true;
			pos = filteredText.find(word, pos + 1);
		}
	}

	return filteredText;
}

