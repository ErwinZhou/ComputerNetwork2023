#pragma once
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <time.h>
#include <ws2tcpip.h>
#include <WinSock2.h>
#include <deque>//double deque for slide window
#include<mutex>//For lock on mutlti-thread programming
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
// Maximum times of retries while waving hands
#define UDP_WAVE_RETRIES 10
// MSL estimation 
#define MSL CLOCKS_PER_SEC/20
// Patience waiting on sending file
#define PATIENCE CLOCKS_PER_SEC * 1000 


//Timer
class Timer {
private:
	clock_t start_time;
	bool started;
	int timeout;
	mutex timer_lock;//For case of multi-thread, the same timer could be operated by different threads
	//So we need to lock it to avoid competition
public:
	Timer() {
		started = false;
		timeout = 1.2 * 2 * MSL;//before udp_2msl is set, use default 2 seconds
	}

	Timer(int timeout) {
		started = false;
		this->timeout = timeout;
	}
	int get_timeout() {
		return timeout;
	}
	void set_timeout(int timeout) {
		timer_lock.lock();
		this->timeout = timeout;
		timer_lock.unlock();
	}
	void start(int udp_2msl) {//start timer
		timer_lock.lock();//lock it first
		start_time = clock();
		started = true;
		timeout = udp_2msl;//using current udp_2msl to estimate timeout
		timer_lock.unlock();//unlock it
	}
	void start() {
		timer_lock.lock();
		start_time = clock();
		started = true;
		timer_lock.unlock();
	}
	bool is_timeout() {
		timer_lock.lock();//lock it first
		if (started) {
			if (clock() - start_time > timeout) {
				timer_lock.unlock();
				return true;
			}
			else {//not timeout
				timer_lock.unlock();
				return false;
			}
		}
		else {//timer not started
			timer_lock.unlock();
			return false;
		}
	}
	void stop() {//stop timer
		timer_lock.lock();
		started = false;
		timer_lock.unlock();
	}

};


#pragma pack(push)
#pragma pack(1)
//1Byte align��make it convenient to transfer to char* buffer
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

//align 1Byte same as Header
#pragma pack(push)
#pragma pack(1)
//Datagram = Header + data
class Datagram {
private:
	Header header;
	char data[MSS];
	Timer dg_timer;
	//For SR, if the datagram is acked, is_acked=true
	bool is_acked;
public:
	Datagram() {};
	Datagram(Header header, char* data) :header(header) {
		memcpy(this->data, data, header.data_length);
		is_acked = false;
	}
	Header& get_header() {
		return header;
	}
	char* get_data() {
		return data;
	}
	Timer& get_dg_timer() {
		return dg_timer;
	}
	bool& get_is_acked() {
		return is_acked;
	}
	void set_is_acked(bool is_acked) {
		this->is_acked = is_acked;
	}

};
#pragma pack(pop)
//Resume 4Byte align



//Sender Buffer
class Sendbuffer {
private:
	u_short send_base;
	//slide window is between send_base and next_seq_num
	deque<Datagram*> slide_window;
	u_short next_seq_num;
	//usable, not sent data are actully in send_buffer but not stored
	//because we don't need to store them, it just need to be put in send_buff and send to server,
	//and then put in slide_window
	mutex buffer_lock;
public:
	Sendbuffer() {
		send_base = 1;
		next_seq_num = 1;
	}
	u_short get_send_base() {
		return send_base;
	}
	void set_send_base(u_short send_base) {
		buffer_lock.lock();
		this->send_base = send_base;
		buffer_lock.unlock();
	}
	void back_edge_slide() {
		buffer_lock.lock();
		slide_window.pop_front();//dqueue front go out
		send_base++;
		buffer_lock.unlock();
	}
	void front_edge_slide(Datagram* datagram) {
		//lock it first
		buffer_lock.lock();
		slide_window.push_back(datagram);//put in the queue end
		next_seq_num++;
		buffer_lock.unlock();
	}
	deque<Datagram*>& get_slide_window() {
		return slide_window;
	}
	u_short get_next_seq_num() {
		return next_seq_num;
	}
	void set_next_seq_num(u_short next_seq_num) {
		buffer_lock.lock();
		this->next_seq_num = next_seq_num;
		buffer_lock.unlock();
	}
};


deque<string> log_queue;//log queue for multi-thread
mutex log_queue_mutex;//lock for log queue






WSAData wsadata;
SOCKET clientSocket;
sockaddr_in clientAddr;
/*
* Previously in server, there is no need for a clientAddr, for server just need to receive SYN from client(without keep-alive)
* But in client, it has to need a serverAddr to send SYN to start shaking hands
*/
SOCKADDR_IN serverAddr;


char* recv_buff;
//send_buff is not send_buffer
//send_buff is used to send data, for temporary use
//send_buffer is used to store data
char* send_buff;
int max_retries_times = UDP_SHAKE_RETRIES;
int udp_2msl = 2 * MSL;//Default:udp2MSL:2 seconds(Clocks)


char* file_data_buffer = new char[INT_MAX];//Maxium databuffer
int file_length = 0;
bool restart = true;//keep-alive

//GBN
//Timer client_timer;
Sendbuffer send_buffer;
//Mutex for send_buffer
mutex send_buffer_mutex;
//Maxium size of send buffer
int send_buffer_size;

//Mutex
mutex log_lock;
/*
Global variable: for mutil-thread communication
Distinguish in the case(base=next_seq_num) :
(1)All acked, remained pkg in the send_buffer need to be sent！！send_over=false
(2)All acked, and there are nothing remaining！！send_over=true
*/
bool send_over = false;


u_short checksum(char* data, int length);
int shake_hand();
void rdt_send(char* data_buff, int pkg_length, bool last_pkg);
void send_data(string file_path);
void wave_hand();

//GBN+SR
DWORD WINAPI recv_thread_main(LPVOID lpParameter);//Multi-thread
DWORD WINAPI log_thread_main(LPVOID lpParam);//log thread in resolving the competition of console output
DWORD WINAPI timeout_resend_thread_main(LPVOID lpParam);//timeout resend thread


//Packet loss test(Absolute)[0-99] On Packet
int Packet_loss_range;
//Latency(Relatively)[0-1] On Everything(ACK from server, data from client)
double Latency_param;
//Latency test(Absolute)[0-3000:ms] On Packet
int Latency_mill_seconds;
/*
  file_data_buffer{

	(Length:file_length)
	already acked data
	sendbuffer(

	(Length:SEND_BUFFER_SIZE)
	* sendbase
	sent but not acked data
	usable, not sent data

	sendbuff[

	(Length:1)
	* next_seq_num
	current to be sent data

	]

	)
	not usable data
}
*/