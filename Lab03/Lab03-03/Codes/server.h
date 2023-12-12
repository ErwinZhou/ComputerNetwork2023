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
#include <mutex>//For lock on mutlti-thread programming
#include <deque>//double deque for slide window
#include <vector>//vector for slide window to buffer data in disorder
#include<algorithm>




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



//There is a need to define a new class Datagram for server to buffer data in disorder for SR
//align 1Byte same as Header
#pragma pack(push)
#pragma pack(1)
//Datagram = Header + data
class Datagram {
private:
	Header header;
	char data[MSS];
	//For SR, if the datagram is received(in the receive buffers), is_received=true
	bool is_received;
public:
	Datagram() {};
	Datagram(Header header, char* data) :header(header) {
		memcpy(this->data, data, header.data_length);
	}
	Header& get_header() {
		return header;
	}
	char* get_data() {
		return data;
	}
	bool get_is_received() {
		return is_received;
	}
	void set_is_received(bool is_received) {
		this->is_received = is_received;
	}

};
#pragma pack(pop)



//Maxium size of receive buffer
int receive_buffer_size;


//Resume 4Byte align
class Receivebuffer {
private:
	u_short receive_base;
	//SR for server(receiver), sliding window has the size as the buffer
	vector<Datagram*> slide_window;
	u_short receive_end;
	mutex buffer_lock;
public:
	Receivebuffer() {
		receive_base = 1;
		receive_end = 1 + receive_buffer_size - 1;
	}
	u_short get_receive_base() {
		return receive_base;
	}
	void set_receive_base(u_short receive_base) {
		buffer_lock.lock();
		this->receive_base = receive_base;
		buffer_lock.unlock();
	}
	vector<Datagram*>& get_slide_window() {
		return slide_window;
	}
	void front_edge_slide() {
		/*
		* Actually, it's the accpectable range getting bigger
		*/
		buffer_lock.lock();
		receive_end++;
		buffer_lock.unlock();
	}
	Datagram* window_edge_slide() {
		buffer_lock.lock();
		receive_base++;
		// Create a copy of the first datagram in slide_window
		Datagram* datagram = new Datagram(*slide_window[0]);
		slide_window.erase(slide_window.begin());
		receive_end++;
		buffer_lock.unlock();
		// Return the first datagram in slide_window to give it to application layer
		return datagram;
	}
	Datagram* back_edge_slide() {
		buffer_lock.lock();
		receive_base++;
		//return the first datagram in slide_window to give it to application layer
		Datagram* datagram = slide_window[0];
		slide_window.erase(slide_window.begin());
		buffer_lock.unlock();
		return datagram;
	}
	void set_receive_end(u_short receive_end) {
		buffer_lock.lock();
		this->receive_end = receive_end;
		buffer_lock.unlock();
	}
	u_short get_receive_end() {
		return receive_end;
	}
	void buff_datagram(Datagram* datagram) {
		buffer_lock.lock();
		slide_window.push_back(datagram);

		//Sort the slide_window by sequence number every time a new datagram is put in
		sort(slide_window.begin(), slide_window.end(), [](Datagram* a, Datagram* b) {
			return a->get_header().get_seq() < b->get_header().get_seq();
		});
		buffer_lock.unlock();
	}

};





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



//Log queue for multi-thread
deque<string> log_queue;
mutex log_queue_mutex;

//SR
Receivebuffer receive_buffer;
//receive_buffer_lock
mutex receive_buffer_mutex;

//u_short expected_sequence_num = 1;

u_short checksum(char* data, int length);
int shake_hand();
void rdt_rcv(char* data_buff, int* curr_pos, bool& waved);

//SR
DWORD WINAPI send_thread_main(LPVOID lpParameter);//Multi-thread
DWORD WINAPI log_thread_main(LPVOID lpParam);//log thread in resolving the competition of console output

//Mutil-thread communication
bool receive_over = false;

//Sequence number of ack needed to be sent, using in multi-thread communication
u_short ack_seq_num = -1;
//Mutex for ack_seq_num
mutex ack_seq_num_mutex;



//
deque<u_short>ack_deque;
mutex ack_deque_mutex;



//Packet loss test(1-100)
int Packet_loss_range;