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

/*
* ��������generateRandomString
* ���ã�Ϊû���û������û������������
*/
string generateRandomString() {
    srand(time(NULL));
    int length = rand() % 10 + 1;  // �������1��10֮��ĳ���
    const string CHAR_POOL = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    string result;

    for (int i = 0; i < length; ++i) {
        int randomIndex = rand() % CHAR_POOL.size();
        result += CHAR_POOL[randomIndex];
    }

    return result;
}

/*
* ��������filterSensitiveWords
* ���ã������κ���������κο��ܵ����дʣ�
* ���أ����˺���ַ������Ƿ������д�flag
*/
string filterSensitiveWords(const string& text, bool& flag) {
	vector<string> sensitiveWords;//���д�vector

	// ��ȡ���д��ļ�
	ifstream file("sensitiveWords.txt");
	if (file.is_open()) {
		string word;
		while (getline(file, word)) {//ÿ�ζ�ȡһ��
			sensitiveWords.push_back(word);
		}
		file.close();
	}

	// �������д�
	string filteredText = text;
	for (const string& word : sensitiveWords) {//for each�ṹ��ÿ�δ��ж�ȡһ��word
		size_t pos = filteredText.find(word);//ʹ��size_t�������洢�ַ�����λ��
		while (pos != string::npos) {//һֱִ�У�ֱ�� pos ��ֵ����string::npos;
			//string::npos ��һ������� size_tֵ����ʾδ�ҵ�ƥ������ַ�����
			filteredText.replace(pos, word.length(), string(word.length(), '*'));//��*�滻
			flag = true;
			pos = filteredText.find(word, pos + 1);
		}
	}

	return filteredText;
}

