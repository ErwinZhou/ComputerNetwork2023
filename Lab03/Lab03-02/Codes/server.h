#pragma once
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <time.h>
#include <ws2tcpip.h>
#include <WinSock2.h>
#include<limits>
using namespace std;
//Link ws2_32.lib to current project
#pragma comment(lib,"ws2_32.lib")
//Flags currently using the last six spots
#define SYN 0x1
#define ACK 0x2
#define FIN 0x4
#define LAS 0x8
#define RST 0x10

// Maximum Segment Size
#define MSS 14600 //MSS = MTU-TCP Header - IP header = 15000-40=14600
// Maximum times of retries while shaking hands
#define UDP_SHAKE_RETRIES 10
// MSL estimation of clocks(1 second of clocks one trip to/from)
#define MSL CLOCKS_PER_SEC
// Maximum times of retries while waving hands
#define UDP_WAVE_RETRIES 10
// Patience waiting on sending file
#define PATIENCE CLOCKS_PER_SEC * 1000

#pragma pack(push)
#pragma pack(1)
//1Byte align£¬make it convenient to transfer to char* buffer
class Header {
public:
	u_short seq;
	u_short ack;
	u_short flag;
	u_short checksum;
	u_short data_length;
	u_short header_length;
public:
	Header() {};
	Header(u_short seq, u_short ack, u_short flag, u_short checksum, u_short data_length, u_short header_length) :
		seq(seq), ack(ack), flag(flag), checksum(checksum), data_length(data_length), header_length(header_length) {}
	u_short get_seq() {
		return seq;
	}
	u_short get_ack() {
		return ack;
	}
	u_short get_flag() {
		return flag;
	}
	u_short get_checksum() {
		return checksum;
	}
	u_short get_data_length() {
		return data_length;
	}
	u_short get_header_length() {
		return header_length;
	}

};
#pragma pack(pop)
//Resume 4Byte align

WSAData wsadata;
SOCKET serverSocket;
/*
* Automatically record a clientAddr while shaking hands
*/
sockaddr_in clientAddr;

sockaddr_in serverAddr;

char* recv_buff;
char* send_buff;
int max_retries_times = UDP_SHAKE_RETRIES;
int udp_2msl = 2 * MSL;

char* file_data_buffer = new char[INT_MAX];//Maxium databuffer
int file_length = 0;

u_short expected_sequence_num = 1;

u_short checksum(char* data, int length);
int shake_hand();
void rdt_rcv(char* data_buff, int* curr_pos, bool& waved);

//Packet loss test(1-100) On ACK
int Packet_loss_range;
//Latency test(Absolute)[0-3000:ms] On ACK
int Latency_mill_seconds;