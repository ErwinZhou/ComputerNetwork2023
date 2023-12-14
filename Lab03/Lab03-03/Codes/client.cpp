#include"client.h"
#include"Funs.h"


u_short checksum(char* data, int length) {
	/*
	* checksum on message and length
	* data: char * UDP message
	* length: bit length on message
	*/
	int size = length % 2 ? length + 1 : length;//make sure size is a even number
	int count = size / 2;
	char* buf = new char[size];
	memset(buf, 0, size);//fill with zero
	memcpy(buf, data, length);
	u_long sum = 0;//ulong to prevent potential overflow
	u_short* buf_iterator = (u_short*)buf;//to process buf in 16 bit block
	while (count--) {
		sum += *buf_iterator++;
		if (sum & 0xffff0000) { //Overflow
			sum &= 0xffff;//
			sum++;
		}
	}
	delete[]buf;
	return ~(sum & 0xffff);//clear upper 16 and reverse bits
}

int shake_hand() {
	//Initialize
	max_retries_times = UDP_SHAKE_RETRIES;
	udp_2msl = 2 * MSL;
	send_buff = new char[sizeof(Header)];
	recv_buff = new char[sizeof(Header) + MSS];
	int log;//recording logs
	cout << "-----------Start Shaking Hands-----------" << endl;
	cout << "-----Stage 1-----" << endl;
	//base = 1, ack = 0, flag = SYN, data_length = 0, header_length = sizeof(Header)
	Header syn_header(send_buffer.get_send_base(), 0, SYN, 0, 0, sizeof(Header));
	memcpy(send_buff, (char*)&syn_header, sizeof(syn_header));
	u_short cks = checksum(send_buff, sizeof(syn_header));
	((Header*)send_buff)->checksum = cks;
	//Wait for the lady to be ready
	Sleep(1000);
	/*
	* 测试丢包
	*/
	// 生成随机数
	int randomNumber = rand() % 100; //确保数字在0范围内

	if (randomNumber <= Packet_loss_range) {
		cout << "------------DROP PACKAGE ON PURPOSE!-----------" << endl;
	}
	else {
		int times = 5;
	ResendSYN1:
		log = sendto(clientSocket, send_buff, sizeof(syn_header), 0, (sockaddr*)&serverAddr, sizeof(sockaddr_in));
		if (log == SOCKET_ERROR) {
			cout << "Oops!Failed to send SYN to server." << endl;
			cout << GetLastErrorDetails() << endl;
			cout << "Please try again later." << endl;
			//此时服务器端正在那sleep等呢，别让对方等太久，
			if (!times) {
				//但是如果一直不通过，对面也会一直等，会导致死锁，尝试五次不行，直接退出。
				cout << "Failed to send SYN pkg to server too many times." << endl;
				cout << "------------Dismissed connection-----------" << endl;
				return -1;
			}
			times--;
			goto ResendSYN1;
		}
		else
			cout << "Successfully sent SYN pkg to server, a request of connection." << endl;

		cout << "-----Stage 2-----" << endl;
	}

	clock_t start = clock();
	//// non-block mode, wait for SYN and ACK
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);
	sockaddr_in tempAddr;
	int temp_addr_length = sizeof(sockaddr_in);
	while (true) {
		//continue to recvfrom wrong
		/*
		Timeout resend SYN protocol here don't need to implement by multi-threading
		Because client don't need to send anything while waiting for SYN+ACK
		*/
		while (recvfrom(clientSocket, recv_buff, sizeof(Header), 0, (sockaddr*)&tempAddr, &temp_addr_length) <= 0) {
			if (clock() - start > 1.2 * udp_2msl) {
				if (max_retries_times <= 0) {
					cout << "Reached max times on resending SYN." << endl;
					cout << "Shaking Hands Failed!" << endl;
					cout << "-----------Stop Shaking Hands-----------" << endl;
					mode = 0;
					ioctlsocket(clientSocket, FIONBIO, &mode);
					return -1;//return failure flag
				}
				int times = 5;
			SendSYN2:
				log = sendto(clientSocket, send_buff, sizeof(syn_header), 0, (sockaddr*)&serverAddr, sizeof(sockaddr_in));
				if (log == SOCKET_ERROR) {
					cout << "Oops!Failed to send SYN to server." << endl;
					cout << GetLastErrorDetails() << endl;
					cout << "Please try again later." << endl;

					if (!times) {
						//同样如果一直不通过，对面也会一直等，会导致死锁，尝试五次不行，直接退出。
						cout << "Failed to send SYN pkg to server too many times." << endl;
						cout << "------------Dismissed connection-----------" << endl;
						return -1;
					}
					//同样努力挽留一下，回到本次重传的开始位置
					times--;
					goto SendSYN2;
				}
				max_retries_times--;
				//increase udp_2msl
				udp_2msl += MSL;
				start = clock();
				cout << "Timeout, resent SYN to server." << endl;
				//此时出现重传一定出现了丢包，
				cout << "-----Stage 2-----" << endl;
			}

		}
		Header recv_header;
		memcpy(&recv_header, recv_buff, sizeof(recv_header));
		u_short cks = checksum(recv_buff, sizeof(recv_header));
		if (cks == 0 && (recv_header.get_flag() & ACK) && (recv_header.get_flag() & SYN)) {
			cout << "Successfully received SYN & ACK pkg from server." << endl;
			cout << "-----Stage 3-----" << endl;
			Header ack_header(0, 0, ACK, 0, 0, sizeof(Header));
			memcpy(send_buff, (char*)&ack_header, sizeof(ack_header));
			u_short cks = checksum(send_buff, sizeof(ack_header));
			((Header*)send_buff)->checksum = cks;
		SendACK1:
			log = sendto(clientSocket, send_buff, sizeof(ack_header), 0, (sockaddr*)&serverAddr, sizeof(sockaddr_in));
			if (log == SOCKET_ERROR) {
				cout << "Oops!Failed to send ACK to server." << endl;
				cout << GetLastErrorDetails() << endl;
				cout << "Please try again later." << endl;
				//尽量挽留一下，如果不成功，服务器端也会因为超时重传次数过多而结束，无需主动结束。
				goto SendACK1;
			}

			// block mode
			mode = 0;
			ioctlsocket(clientSocket, FIONBIO, &mode);
			//Finished shaking hands
			cout << "Successfully sent ACK pkg to Server." << endl;
			cout << "-----------Finished Shaking Hands-----------" << endl;
			return 1;
		}
		//else...如果是其他情况，直接回到开始while循环的开始重新接受SYN+ACK,因为服务器端没有接收到ACK会不断重新发SYN+ACK

	}
	//退出while，实际上不可能发生，一定会在循环中返回某个值
}



DWORD WINAPI log_thread_main(LPVOID lpParameter) {
	while (true) {
		//cout<<"Client的log线程我还活着！" << endl;
		if (send_over == true)
			return 0;
		unique_lock<mutex> log_queue_lock(log_queue_mutex);
		if (!log_queue.empty()) {
			//cout<<"Client的log线程我还活着,并且在emptyh中！" << endl;
			cout << log_queue.front();
			log_queue.pop_front();
		}
		log_queue_lock.unlock();
	}
}

DWORD WINAPI timeout_resend_thread_main(LPVOID lpParamter) {
	while (true) {
		if (send_over == true)
			return 0;
		/*
		* SR:Different from GBN, timeout resent protocol happens for every possible pkg in the slide window
		*/
		{
			lock_guard<mutex> send_buffer_lock(send_buffer_mutex);
			for (int i = 0; i < send_buffer.get_slide_window().size(); i++) {
				if (send_buffer.get_slide_window()[i]->get_dg_timer().is_timeout() == true) {
					int log = sendto(
						clientSocket,
						(char*)send_buffer.get_slide_window()[i],
						send_buffer.get_slide_window()[i]->get_header().get_data_length() + send_buffer.get_slide_window()[i]->get_header().get_header_length(),
						//dg->header.get_data_length() + dg->header.get_header_length(),
						0,
						(sockaddr*)&serverAddr,
						sizeof(sockaddr_in)
					);
					if (log == SOCKET_ERROR) {
						//重复五次，然后发送RST，结束
					}
					//Reset timer
					send_buffer.get_slide_window()[i]->get_dg_timer().start();
					{
						lock_guard<mutex> log_queue_lock(log_queue_mutex);
						log_queue.push_back("Timeout, resent datagram with seq:" + to_string(send_buffer.get_slide_window()[i]->get_header().get_seq()) + " to server." + string("\n"));
					}
					
				}
			}
		}


	}
	
}
DWORD WINAPI recv_thread_main(LPVOID lpParameter) {
	/*
	* Call from send_data
	* only return when sending is over
	*/
	//Wait for ACK from server
	// 开启非阻塞模式
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);
	recv_buff = new char[sizeof(Header)];
	Header recv_header;

	sockaddr_in tempAddr;
	int temp_addr_length = sizeof(sockaddr_in);
	while (true) {
		/*
		* Latency Test:Relatively, shorten every timeout in the send_buffer
		*/
		{
			lock_guard<mutex> send_buffer_lock(send_buffer_mutex);
			// 生成随机数
			int randomNumber = rand() % 100; // %100 确保数字在 0-99 范围内

			if (randomNumber <= Packet_loss_range) {

				lock_guard<mutex> log_queue_lock(log_queue_mutex);
				log_queue.push_back("------------DELAY TIME!-----------" + string("\n"));
				for (int i = 0; i < send_buffer.get_slide_window().size(); i++)
					send_buffer.get_slide_window()[i]->get_dg_timer().set_timeout(Latency_param * send_buffer.get_slide_window()[i]->get_dg_timer().get_timeout());
			}

		}
		

		if (send_over == true) {
			//All the pkg has been sent, and all the ACK has been received
			//Recv thread can be closed
			mode = 0;
			ioctlsocket(clientSocket, FIONBIO, &mode);
			delete[]recv_buff;
			return 0;
		}

		
		while (recvfrom(
			clientSocket,
			recv_buff,
			sizeof(recv_header),
			0,
			(sockaddr*)&tempAddr,
			&temp_addr_length
		) <= -1) {
			if (send_over) {
				//Equally, it can end up here
				mode = 0;
				ioctlsocket(clientSocket, FIONBIO, &mode);
				delete[]recv_buff;
				return 0;
			}
			//if (client_timer.is_timeout() == true) {
			//	//Timeout
			//	//Resend all the pkg in the slide window
			//	for (auto dg : send_buffer.get_slide_window()) {

			//		int log = sendto(
			//			clientSocket,
			//			(char*)dg,
			//			dg->get_header().get_data_length() + dg->get_header().get_header_length(),
			//			//dg->header.get_data_length() + dg->header.get_header_length(),
			//			0,
			//			(sockaddr*)&serverAddr,
			//			sizeof(sockaddr_in)
			//		);
			//		if (log == SOCKET_ERROR) {
			//			//重复五次，然后发送RST，结束
			//		}
			//	}
			//	lock_guard<mutex> log_queue_lock(log_queue_mutex);
			//	log_queue.push_back("Timeout, resent datagram to server." + string("\n"));
			//	client_timer.start();
			//}

		}

		
		//Receive ACK from server
		memcpy(&recv_header, recv_buff, sizeof(recv_header));//only header is useful



		{
			lock_guard<mutex> log_queue_lock(log_queue_mutex);
			log_queue.push_back("Successfully received datagram---" + to_string(recv_header.get_data_length() + recv_header.get_header_length()) + "Bytes in length." + string("\n"));
			log_queue.push_back("Header---" + string("\n"));
			log_queue.push_back("seq: " + to_string(recv_header.get_seq()) + " , ack: " + to_string(recv_header.get_ack()) + ", flag: " + to_string(recv_header.get_flag()) + ", checksum: " + to_string(recv_header.get_checksum()) + string("\n"));
			log_queue.push_back("header length:" + to_string(recv_header.get_header_length()) + ", data length:" + to_string(recv_header.get_data_length()) + string("\n"));
		}

		//checksum is a local variable, no need to lock it
		u_short cks = checksum(recv_buff, sizeof(recv_header));

		if (
			cks == 0 //not corruptied
			&&
			(recv_header.get_flag() & ACK) //ACK flag
			) {
			/*
			* If we sent 2, 3, 4, 5 in total
			* Then while we are wait for ack on 2
			* Instead, we got acknowledge on 4
			* Then we can't slide window to 4 in SR
			* We can only mark 4 as acked, and wait for ack on 2
			* But when the ack_num is just the same as send_base, we can slide window to the last not_acked
			*/
				{
					lock_guard<mutex> send_buffer_lock(send_buffer_mutex);
					int acked_num = recv_header.get_ack() - send_buffer.get_send_base();
					if (acked_num < 0 || acked_num >= send_buffer.get_next_seq_num()) {
						//ack on previous OR later pkg
						//Ignore
						{
							lock_guard<mutex> log_queue_lock(log_queue_mutex);
							log_queue.push_back("Server has acknowledged on packages:None" + string("\n"));
						}
					}
					else {
						// 0 =< acked_num < send_buffer.get_next_seq_num
						//ack on the pkg in the range of the slide window
						//mark the pkg as acked
						send_buffer.get_slide_window()[acked_num]->set_is_acked(true);
						//Stop corresponding timer
						send_buffer.get_slide_window()[acked_num]->get_dg_timer().stop();
						{
							lock_guard<mutex> log_queue_lock(log_queue_mutex);
							log_queue.push_back("Server has acknowledged on package:" + to_string(recv_header.get_ack()) + string("\n"));
						}

						if (acked_num == 0) {
							//acked_num = 0, which means the ack_num is just the same as send_base
							//slide window to the last not_acked
							while (send_buffer.get_slide_window().empty() == false
								&&
								send_buffer.get_slide_window()[0]->get_is_acked() == true) {
								send_buffer.back_edge_slide();
							}
						}
					}
				}
		

		


			{
				lock(log_queue_mutex, send_buffer_mutex);
				lock_guard<mutex> log_lock(log_queue_mutex, adopt_lock);
				lock_guard<mutex> send_buffer_lock(send_buffer_mutex, adopt_lock);
				log_queue.push_back("send_buffer:{ ");
				for (int i = 0; i < send_buffer.get_slide_window().size(); i++) {
					if (send_buffer.get_slide_window()[i]->get_is_acked() == false)
						log_queue.push_back("[" + to_string(send_buffer.get_slide_window()[i]->get_header().get_seq()) + "]" + " ");
					else
						log_queue.push_back("[" + to_string(send_buffer.get_slide_window()[i]->get_header().get_seq()) + "*" + "]" + " ");
				}

				for (int i = send_buffer.get_next_seq_num(); i <= send_buffer.get_send_base() + send_buffer_size - 1; i++)
					log_queue.push_back("[ ]" + string(" "));
				log_queue.push_back("}" + string("\n"));
			}

		}
		else if (cks == 0 //not corruptied
			&&
			recv_header.get_flag() & RST //server try to close connection
			) {
				{
					lock_guard<mutex> log_queue_lock(log_queue_mutex);
					log_queue.push_back("Server unexpected closed:Error in connection." + string("\n"));
				}
			delete[] file_data_buffer;
			delete[] send_buff;
			delete[] recv_buff;
			closesocket(clientSocket);
			system("pause");
			WSACleanup();
			return 0;
		}
		else if (
			cks != 0 //ACK pkg probably corruptied during transmisssion
			)
			continue;//continue to send pkg to server
	}
}
void rdt_send(char* data_buff, int pkg_length, bool last_pkg) {
	//Inititalize
	int flag = last_pkg ? LAS : 0;
	int log;
	bool result = true;
	
	//block the thread if the send_buffer is full
	while (send_buffer.get_next_seq_num() >= send_buffer.get_send_base() + send_buffer_size) {
		//Sleep(10);
		continue;
	}
	


	Header send_header(send_buffer.get_next_seq_num(), 0, flag, 0, pkg_length, sizeof(Header));
	//Inititalize datagram with header and data
	Datagram* datagram = new Datagram(send_header, data_buff);

	//checksum
	u_short cks = checksum((char*)datagram, pkg_length + sizeof(send_header));
	datagram->get_header().checksum = cks;




	
	/*
	* Different case from GBN, SR client(sender) has to start timer for every datagram
	*/
	datagram->get_dg_timer().start();

	{
		lock_guard<mutex> send_buffer_lock(send_buffer_mutex);
		//Slide window front edge slides
		send_buffer.front_edge_slide(datagram);
	}

	/*
	* 测试丢包
	*/


	// 生成随机数
	int randomNumber = rand() % 100; // %100 确保数字在 0-99 范围内

	if (randomNumber <= Packet_loss_range) {

		lock_guard<mutex> log_queue_lock(log_queue_mutex);
		log_queue.push_back("------------DROP PACKAGE ON PURPOSE!-----------" + string("\n"));
	}
	else {
		log = sendto(
			clientSocket,
			(char*)datagram,
			pkg_length + sizeof(send_header),//total length
			0,//no flags
			(sockaddr*)&serverAddr,
			sizeof(sockaddr_in)
		);

		if (log == SOCKET_ERROR) {
			//重复五次，然后发送RST，结束...
		}



		{
			lock(log_queue_mutex, send_buffer_mutex);
			lock_guard<mutex> log_lock(log_queue_mutex, adopt_lock);
			lock_guard<mutex> send_buffer_lock(send_buffer_mutex, adopt_lock);
			log_queue.push_back("-----New Datagram-----" + string("\n"));
			log_queue.push_back("Successfully sent datagram---" + to_string(send_header.get_data_length() + send_header.get_header_length()) + "Bytes in length." + string("\n"));
			log_queue.push_back("Header---" + string("\n"));
			log_queue.push_back("seq: " + to_string(send_header.get_seq()) + " , ack: " + to_string(send_header.get_ack()) + ", flag: " + to_string(send_header.get_flag()) + ", checksum: " + to_string(send_header.get_checksum()) + string("\n"));
			log_queue.push_back("header length:" + to_string(send_header.get_header_length()) + ", data length:" + to_string(send_header.get_data_length()) + string("\n"));

			log_queue.push_back("send_buffer:{ ");
			for (int i = 0; i < send_buffer.get_slide_window().size(); i++) {
				if (send_buffer.get_slide_window()[i]->get_is_acked() == false) 
					log_queue.push_back("[" + to_string(send_buffer.get_slide_window()[i]->get_header().get_seq()) + "]" + " ");
				else 
					log_queue.push_back("[" + to_string(send_buffer.get_slide_window()[i]->get_header().get_seq()) + "*" + "]" + " ");
			}

			for (int i = send_buffer.get_next_seq_num(); i <= send_buffer.get_send_base() + send_buffer_size - 1; i++)
				log_queue.push_back("[ ]" + string(" "));
			log_queue.push_back("}" + string("\n"));
		}


	}

}





void send_data(string file_path) {
	//binary way of open file to read
	ifstream file(file_path.c_str(), ifstream::binary);
	//Get File length
	file.seekg(0, file.end);
	file_length = file.tellg();//use file pointer to calculate length
	file.seekg(0, file.beg);//remove to original pos

	int total_length = file_path.length() + file_length + 1;
	//中间空一格区分，文件路径+”间隔符“+文件长度
	memset(file_data_buffer, 0, sizeof(char) * total_length);//虽然长度为INT_MAX,但只需要用到一部分即总长度
	//file path
	memcpy(file_data_buffer, file_path.c_str(), file_path.length());
	//seperate file path with file content
	file_data_buffer[file_path.length()] = '?';//因为Windows文件路径不能包含？，用这个一定可以区分
	//read the rest real data
	file.read(file_data_buffer + file_path.length() + 1, file_length);
	file.close();
	cout << "-----------Start Sending File-----------" << endl;
	clock_t start = clock();

	//Start receving thread
	HANDLE recv_handle = CreateThread(NULL, 0, recv_thread_main, NULL, 0, NULL);
	//Start log thread
	HANDLE log_handle = CreateThread(NULL, 0, log_thread_main, NULL, 0, NULL);
	//Start timeout resend thread
	HANDLE timeout_resend_handle = CreateThread(NULL, 0, timeout_resend_thread_main, NULL, 0, NULL);
	//Send file data
	// Send data in MSS Segements
	int curr_pos = 0;
	int log;

	while (curr_pos < total_length) {
		//cout<<"Client的send线程我还活着！" << endl;
		int pkg_length = total_length - curr_pos >= MSS ? MSS : total_length - curr_pos;
		bool last = total_length - curr_pos <= MSS ? true : false;
		rdt_send(file_data_buffer + curr_pos, pkg_length, last);
		curr_pos += MSS;
	}


	while (send_buffer.get_slide_window().size() != 0) {
		continue;
		//Wait for all the remaining pkg to be sent,then you can leave
	}

	//Communicate with recv_thread to make sure it's finished
	send_over = true;


	//Block current thread to wait for recv_thread to finish
	WaitForSingleObject(recv_handle, INFINITE);
	//the same for log_thread
	WaitForSingleObject(log_handle, INFINITE);
	//the same for timeout_resend_thread
	WaitForSingleObject(timeout_resend_handle, INFINITE);

	CloseHandle(recv_handle);
	CloseHandle(log_handle);
	CloseHandle(timeout_resend_handle);

	clock_t end = clock();
	cout << "-----------Finished Sending File-----------" << endl;
	cout << "Successfully sent file: " + file_path + " to server!" << endl;
	cout << "-----------Result Estimation----------" << endl;
	cout << "Total length sent:" << total_length << " Bytes." << endl;
	cout << "Total time:" << (end - start) * 1000 / (double)CLOCKS_PER_SEC << " ms." << endl;
	if (!(end - start))
		cout << "Flash!Time is too short to compute a throughput." << endl;
	else
		cout << "Throughput:" << total_length / ((end - start) * 1000 / (double)CLOCKS_PER_SEC) << "Bytes/ms." << endl;

	return;
}
void wave_hand() {
	//Initialize
	max_retries_times = UDP_WAVE_RETRIES;
	udp_2msl = 2 * MSL;
	send_buff = new char[sizeof(Header)];
	recv_buff = new char[sizeof(Header)]; //only need header info
	int log;//recording logs
	cout << "-----------Start Waving Hands-----------" << endl;
	cout << "-----Stage 1-----" << endl;

	//Fin sequence has the same seq with the last pkg
	u_short fin_seq = send_buffer.get_next_seq_num();
	Header fin_header(fin_seq, 0, FIN, 0, 0, sizeof(Header));

	memcpy(send_buff, (char*)&fin_header, sizeof(fin_header));
	u_short cks = checksum(send_buff, sizeof(fin_header));
	((Header*)send_buff)->checksum = cks;


	/*
	* 测试丢包
	*/


	// 生成随机数
	int randomNumber = rand() % 100; //确保数字在0范围内

	if (randomNumber <= Packet_loss_range) {
		cout << "------------DROP PACKAGE ON PURPOSE!-----------" << endl;
	}
	else {
		log = sendto(
			clientSocket,
			send_buff,
			sizeof(fin_header),
			0,
			(sockaddr*)&serverAddr,
			sizeof(sockaddr_in)
		);
		if (log == SOCKET_ERROR) {
			//send RST....
		}
		else {
			cout << "Successfully sent FIN pkg to server." << endl;
			cout << "Sequence Number:" << fin_seq << ", expects acknowledge number:" << fin_seq + 1 << "." << endl;
		}
		cout << "-----Stage 2-----" << endl;
	}


	//non-block mode
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);
	sockaddr_in tempAddr;
	int temp_addr_length = sizeof(sockaddr_in);
	//Recv
	Header recv_header;
	//Start clock
	clock_t start = clock();
	//Wait for ACK

	/*
	* Same case while shaking hands
	* There is no pipeline protocol while waving hands
	* So there is no need to implement multi-threading
	* While sentence is enough
	*/
	while (true) {
		while (recvfrom(
			clientSocket,
			recv_buff,
			sizeof(recv_header),
			0,
			(sockaddr*)&tempAddr,
			&temp_addr_length
		) <= 0) {
			if (clock() - start > 1.2 * udp_2msl) {
				if (max_retries_times <= 0) {
					cout << "Reached max times on resending FIN." << endl;
					cout << "Waving Hands Failed!" << endl;
					cout << "-----------Stop Waving Hands-----------" << endl;
					mode = 0;
					ioctlsocket(clientSocket, FIONBIO, &mode);
					return;
				}
				log = sendto(
					clientSocket,
					send_buff,
					sizeof(fin_header),
					0,
					(sockaddr*)&serverAddr,
					sizeof(sockaddr_in)
				);
				if (log == SOCKET_ERROR) {
					//
				}
				max_retries_times--;
				//increase udp_2msl by one second
				udp_2msl += MSL;
				start = clock();
				cout << "Timeout, resent FIN pkg to server." << endl;
				cout << "Sequence Number:" << fin_seq << ", expects acknowledge number:" << fin_seq + 1 << "." << endl;
				//同样出问题了一定是丢包或者延时
				cout << "-----Stage 2-----" << endl;

			}
		}

		//Indeed receive something
		memcpy(&recv_header, recv_buff, sizeof(recv_header));
		u_short cks = checksum(recv_buff, sizeof(recv_header));
		if (
			cks == 0 //not corriputied
			&&
			recv_header.get_flag() == ACK // ACK pkg
			&&
			recv_header.get_ack() == fin_seq + 1 //ensure it's not a general ack pkg of datagram
			) {
			cout << "Successfully received ACK with acknowledge:" << recv_header.get_ack() << ", in respond to Fin pkg" << endl;
			cout << "-----------Finished Waving Hands-----------" << endl;
			mode = 0;
			ioctlsocket(clientSocket, FIONBIO, &mode);
			return;
		}
		else {
			//pkg is corruptied
			continue;
			//这里只要继续回去超时重传即可，因为server重新发了ack后就关闭了
			//无死锁发生
		}
	}

}

int main() {
	//输出欢迎信息
	cout << "-----------Stable UDP----------- " << endl;
	//加载Winsock2环境
	//初始化所需的各种底层资源、参数以及可能的服务提供商的特定实现
	cout << "-----------Initializing Winsock-----------" << endl;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
		cout << "Oops!Failed to initialize Winsock " << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		return 0;
	}
	else
		cout << "Successfully initialized Winsock!" << endl;

	//套接字创建
	cout << "-----------Creating Socket-----------" << endl;
	//使用IPv4套接字
	//使用数据包套接字-UDP
	clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		cout << "Oops!Failed to create socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//清空资源
		return 0;
	}
	else
		cout << "Successfully created socket!" << endl;
	//设置Server的地址，用于握手发送SYN
	serverAddr.sin_family = AF_INET; // 设置为IPv4
	inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)); // 设置IPv4地址为127.0.0.1
	serverAddr.sin_port = htons(2333); // 设置端口2333


	//因此不会发生错误，实际上如果是错误的地址格式也应该输出错误信息

	cout << "-----------Binding Socket-----------" << endl;
	//这里其实也可以不进行bind
	//因为在使用sendto前没有通过bind给套接字分配本地端口，系统将为该套接字自动分配一个临时端口
	clientAddr.sin_family = AF_INET; // 设置为IPv4
	clientAddr.sin_port = htons(4399); // 设置端口4399
	inet_pton(AF_INET, "127.0.0.1", &(clientAddr.sin_addr)); // 设置IPv4地址为127.0.0.1
	//因此不会发生错误，实际上如果是错误的地址格式也应该输出错误信息
	if (bind(clientSocket, (SOCKADDR*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) {
		cout << "Oops!Failed to bind socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//清空资源
		return 0;
	}
	else
		cout << "Successfully binded socket!" << endl;


	if (shake_hand() == -1)
		cout << "Unable to shake hands, please start over client and server." << endl;
	else {//Successfully shaking hands, start transfering files
		while (true) {
			if (restart == false)
				break;

			cout << "-----------Packet Loss-----------" << endl;
			cout << "Please input the loss of packet in transfer:" << endl;
			cout << "Less than 0:No loss         Greater than 99:All loss" << endl;
			cout << "Packet loss rate:";
			cin >> Packet_loss_range;


			while (true) {
				cout << "-----------Latency Test-----------" << endl;
				cout << "Please input the latency of time in transfer:" << endl;
				cout << "Slight Greater than 0:Severe Latency         1:No Latency" << endl;
				cout << "Latency parameter(0-1]:";
				cin >> Latency_param;
				if (Latency_param <= 0 || Latency_param > 1) {
					cout << "Latency paramter out of range, please input again." << endl;
					continue;
				}
				else {
					break;
				}
			}



			int new_buffer_size;
			while (true) {
				cout << "-----------Buffer Size-----------" << endl;
				cout << "Please input the size of send buffer:" << endl;
				cout << "Size range[4-32]:";
				cin >> new_buffer_size;
				if (new_buffer_size < 4 || new_buffer_size > 32) {
					cout << "Size out of range, please input again." << endl;
					continue;
				}
				else {
					send_buffer_size = new_buffer_size;
					break;
				}
			}
			//Initialize of Keep-Alive
			//Reset send_buffer
			send_over = false;
			//client_timer.stop();
			send_buffer.set_send_base(1);
			send_buffer.set_next_seq_num(1);
			send_buffer.get_slide_window().clear();
			//ReInititalize log_queue
			log_queue.clear();
			//Initialize timers
			//timers = new Timer[send_buffer_size];

			string input_path;
			while (true) {
				cout << "-----------Input File-----------" << endl;
				cout << "Please input a file path:" << endl;
				cout << "(An absolute path, or a path relative to D:\\Visual Studio 2022 Code\\Project-Computer Network\\Lab03-02-UDP RDT Client)" << endl;
				cin >> input_path;
				ifstream file(input_path.c_str());
				if (!file.is_open()) {
					cout << "Unable to open file, please start over and chose another input path." << endl;
					continue;
				}
				file.close();
				break;
			}
			send_data(input_path);

			cout << "-----------Mission Accomplished-----------" << endl;
			while (true) {
				cout << "Would you like to send aother file or exit? " << endl;
				cout << "1:Send another file         2:Exit" << endl;
				cout << "You choice:";
				int choice;
				cin >> choice;
				if (choice == 1) {
					restart = true;
					break;
				}
				else if (choice == 2) {
					restart = false;
					break;
				}
				else {
					cout << "You can only choose 1 or 2, please chose again." << endl;
					continue;
				}
			}

		}

	}

	wave_hand();
	cout << "-----------Farewell, My Dear Friend-----------" << endl;
	delete[] file_data_buffer;
	delete[] send_buff;
	delete[] recv_buff;
	closesocket(clientSocket);
	system("pause");
	WSACleanup();
	return 0;
}