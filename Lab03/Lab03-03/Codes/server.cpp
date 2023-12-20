#include "server.h"
#include "Funs.h"

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
	/*
	*
	*/

	//Initialize
	max_retries_times = UDP_SHAKE_RETRIES;
	udp_2msl = 2 * MSL;
	send_buff = new char[sizeof(Header)];
	recv_buff = new char[sizeof(Header) + MSS];

	int addr_client_length = sizeof(sockaddr_in);
	int log;//recording logs

	while (true) {

		// Only header is useful
		log = recvfrom(serverSocket, recv_buff, sizeof(Header), 0, (sockaddr*)&clientAddr, &addr_client_length);
		if (log == SOCKET_ERROR) {
			cout << "Oops!Failed to recv FIN from client." << endl;
			cout << GetLastErrorDetails() << endl;
			cout << "Please try again later." << endl;
			Sleep(1000);
			continue;
		}
		cout << "-----------Start Shaking Hands-----------" << endl;
		cout << "-----Stage 1-----" << endl;
		// Successfully receive pkg, but can it be corruptied or is it SYN pkg?
		Header recv_header;
		memcpy(&recv_header, recv_buff, sizeof(recv_header));
		u_short cks = checksum(recv_buff, sizeof(recv_header));//check if the Fin pkg is corruptied
		if (cks == 0 && (recv_header.get_flag() & SYN)) {
			cout << "Successfully received connecting request(SYN pkg) from Client." << endl;
			// Send Fin and ACK to client
			cout << "-----Stage 2-----" << endl;
			//Set SYN and ACK,
			Header send_header(0, 0, ACK + SYN, 0, 0, sizeof(Header));
			memcpy(send_buff, (char*)&send_header, sizeof(send_header));
			u_short cks = checksum(send_buff, sizeof(send_header));
			((Header*)send_buff)->checksum = cks;
			/*
			* 测试丢包
			*/
			// 生成随机数
			int randomNumber = rand() % 100; //确保数字在0-1范围内

			if (randomNumber < Packet_loss_range) {
				cout << "------------DROP PACKAGE ON PURPOSE!-----------" << endl;
			}
			else {
				//Send 
			SendSYNACK1:
				log = sendto(serverSocket, send_buff, sizeof(send_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
				if (log == SOCKET_ERROR) {
					cout << "Oops!Failed to send SYN & ACK to client." << endl;
					cout << GetLastErrorDetails() << endl;
					cout << "Please try again later." << endl;
					//这里如果直接返回握手错误有点草率了
					//但如果回到最开始，此时客户端已经成功发送了SYN，等SYN+ACK；
					//服务器端也会重新等待客户端的SYN，会形成死锁！
					//为了防止客户端超时重传SYN次数过多导致握手结束，直接goto回到发送位置
					//“燕子，我不能没有你啊！”
					//努力挽留一下。。。
					goto SendSYNACK1;
					//当然如果挽留不成功，客户端发送SYN次数过多，也会连接失败
				}
				else
					cout << "Successfully sent SYN & ACK pkg to Client." << endl;

				//Wait for ACK from Client
				cout << "-----Stage 3-----" << endl;
			}

			// non-blocking mode
			u_long mode = 1;
			ioctlsocket(serverSocket, FIONBIO, &mode);
			clock_t start = clock();
			/*
			* Timeout Resent SYN & ACK
			* Same case: we don't need to program in mutil-thread
			* For we can complete it with a while sentence and a non-block mode
			*/
			while (true) {
				int result;
				while ((result = recvfrom(serverSocket, recv_buff, sizeof(Header), 0, (sockaddr*)&clientAddr, &addr_client_length)) <= 0) {
					if (clock() - start > 1.2 * udp_2msl) {
						if (max_retries_times <= 0) {
							cout << "Reached max times on resending SYN & ACK." << endl;
							cout << "Shaking Hands Failed!" << endl;
							cout << "-----------Stop Shaking Hands-----------" << endl;
							mode = 0;
							ioctlsocket(serverSocket, FIONBIO, &mode);
							return -1;//return failure flag
						}
					SendSYNACK2:
						log = sendto(serverSocket, send_buff, sizeof(send_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
						if (log == SOCKET_ERROR) {
							cout << "Oops!Failed to send SYN & ACK to client." << endl;
							cout << GetLastErrorDetails() << endl;
							cout << "Please try again later." << endl;
							//同样努力挽留一下，回到本次重传的开始位置
							goto SendSYNACK2;
						}
						max_retries_times--;
						//increase udp_2msl by one second
						udp_2msl += MSL;
						start = clock();
						cout << "Timeout, resent SYN & ACK to client." << endl;
						//同样如果出现了重传一定是丢包了
						cout << "-----Stage 3-----" << endl;
					}
				}

				// Successfully eceive pkg, but can it be corruptied or is it ACK pkg?
				memcpy(&recv_header, recv_buff, sizeof(recv_header));
				u_short cks = checksum(recv_buff, result);
				if (cks == 0 && (recv_header.get_flag() & ACK)) {
					cout << "Successfully received ACK pkg from Client." << endl;
					cout << "-----------Finished Shaking Hands-----------" << endl;
					mode = 0;
					ioctlsocket(serverSocket, FIONBIO, &mode);
					return 1;
				}
				else if (cks == 0 && (recv_header.get_flag() & SYN)) { //成功发出了SYN+ACK，又重新收到了新的pkg
					//但是内容却还是SYN，证明发送的SYN+ACK包丢失了
					goto SendSYNACK2;
				}
				else if (cks == 0 && (recv_header.get_flag() == 0)) {//如果提前收到了数据报文，服务器端担心出现事故
					//发送RST报文后，关闭连接												
					Header rst_header(0, 0, RST, 0, 0, sizeof(Header));
					memcpy(send_buff, (char*)&rst_header, sizeof(rst_header));
					int cks = checksum(send_buff, sizeof(rst_header));
					((Header*)send_buff)->checksum = cks;
				SendRST1:
					log = sendto(serverSocket, send_buff, sizeof(rst_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
					if (log == SOCKET_ERROR) {
						cout << "Oops!Failed to send RST to client." << endl;
						cout << GetLastErrorDetails() << endl;
						cout << "Please try again later." << endl;
						//确保传过去了
						goto SendRST1;
					}
					cout << "Unexpectedly received data pkg before ACK pkg, RST sent. " << endl;
					cout << "Shaking Hands Failed!" << endl;
					cout << "-----------Stop Shaking Hands-----------" << endl;
					mode = 0;
					ioctlsocket(serverSocket, FIONBIO, &mode);
					return -1;//return failure flag
				}
				else if (cks) {//pkg is corriputed
					cout << "Oops!Package from client is corriputed." << endl;
					//此时客户端已经向服务器端发送了ACK（假设，否则情况会更加复杂）
					//如果损坏了，服务器也不会再收到ACK信息了，此后收到的只会是数据包。因为ACK包不会重发
					//当然也存在收到的是FIN包，不过损坏了。。。
					//这里暂时采取的策略是直接结束。避免死锁情况或者其他误会。
					Header rst_header(0, 0, RST, 0, 0, sizeof(Header));
					memcpy(send_buff, (char*)&rst_header, sizeof(rst_header));
					u_short cks = checksum(send_buff, sizeof(rst_header));
					((Header*)send_buff)->checksum = cks;
				SendRST3:
					log = sendto(serverSocket, send_buff, sizeof(rst_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
					if (log == SOCKET_ERROR) {
						cout << "Oops!Failed to send RST to client." << endl;
						cout << GetLastErrorDetails() << endl;
						cout << "Please try again later." << endl;
						//确保传过去了
						goto SendRST3;
					}
					cout << "Unexpectedly received corruptied pkg instead of ACK, RST sent. " << endl;
					cout << "Shaking Hands Failed!" << endl;
					cout << "-----------Stop Shaking Hands-----------" << endl;
					return -1;
				}
			}
		}
		else {//Fin pkg is corriupted or is not Fin flagged
			if (!(recv_header.get_flag() & SYN)) {//It is not SYN flagged, unexpected received data or ACK
				//Sent RST and close connection
				Header rst_header(0, 0, RST, 0, 0, sizeof(Header));
				memcpy(send_buff, (char*)&rst_header, sizeof(rst_header));
				u_short cks = checksum(send_buff, sizeof(rst_header));
				((Header*)send_buff)->checksum = cks;
				int times = 5;
			SendRST2:
				log = sendto(serverSocket, send_buff, sizeof(rst_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
				if (log == SOCKET_ERROR) {
					cout << "Oops!Failed to send RST to client." << endl;
					cout << GetLastErrorDetails() << endl;
					cout << "Please try again later." << endl;
					//确保传过去了
					if (!times) {
						cout << "Failed to send RST pkg to client too many times." << endl;
						cout << "------------Dismissed connection-----------" << endl;
						u_long mode = 0;
						ioctlsocket(serverSocket, FIONBIO, &mode);
						return -1;//return failure flag
					}
					times--;
					goto SendRST2;
					//当然如果一直传不过去，会导致客户端还在一直发，造成死锁，
				}
				cout << "Unexpectedly received pkg before SYN pkg, RST sent. " << endl;
				cout << "Shaking Hands Failed!" << endl;
				cout << "-----------Stop Shaking Hands-----------" << endl;
				return -1;
			}
			else {//If it is Fin flagged, then it is corriputed
				cout << "Oops!Package from client is corriputed." << endl;
				cout << "Please try again later." << endl;
				Sleep(3000);
				continue;

			}

		}

	}

}

DWORD WINAPI log_thread_main(LPVOID lpParameter) {
	while (true) {
		//cout<<"Server的log线程我还活着！"<<endl;
		if (receive_over == true)
			return 0;
		unique_lock<mutex> log_queue_lock(log_queue_mutex);
		if (!log_queue.empty()) {
			cout << log_queue.front();
			log_queue.pop_front();
		}
		log_queue_lock.unlock();
	}
}

DWORD WINAPI send_thread_main(LPVOID lpParamter) {
	/*
	* Call from rdt_rcv function
	* only return when receive_over == true
	* In multi-thread, receiver(server) need to send ACK to sender(client)
	* Main thread is used to receive data from sender(client)
	*/

	int log;
	send_buff = new char[sizeof(Header)];
	while (true) {
		{
			lock_guard<mutex> ack_lock(ack_deque_mutex);
			if (ack_deque.empty() == false)
				ack_seq_num = ack_deque.front();
			else {
				//If rdt_recv(Main Thread) received a LAS pkg, receive_over is changed
				if (receive_over == true) {
					//Send Thread finished
					delete[] send_buff;
					return 0;
				}
				continue;
			}
			ack_deque.pop_front();
		}

			Header ack_header(0, ack_seq_num, ACK, 0, 0, sizeof(Header));
			memcpy(send_buff, (char*)&ack_header, sizeof(ack_header));
			//checksum
			u_short cks = checksum(send_buff, sizeof(ack_header));
			((Header*)send_buff)->checksum = cks;
			/*
			* Packet loss test
			*/

			// Generate random number
			int randomNumber = rand() % 100; // %100 Make sure the number is in the range of 0-99

			if (randomNumber < Packet_loss_range) {
				lock_guard<mutex> log_lock(log_queue_mutex);
				log_queue.push_back("------------DROP PACKAGE ON PURPOSE!-----------" + string("\n"));
			}
			else {


				/*
				* Latency test for Packet(Absolute)
				*/
				if (Latency_mill_seconds) {
					/*{
						lock_guard<mutex> log_queue_lock(log_queue_mutex);
						log_queue.push_back("------------DELAY TIME ABSOLUTE!-----------" + string("\n"));
					}*/
					Sleep(Latency_mill_seconds);
				}


				int times = 5;
			SendACK:
				log = sendto(serverSocket, send_buff, sizeof(ack_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
				//这里也可能出现socketerror的错误
				if (log == SOCKET_ERROR) {
					{
						lock_guard<mutex> log_lock(log_queue_mutex);
						log_queue.push_back("Oops!Failed to send ACK to client on datagram." + string("\n"));
						log_queue.push_back(GetLastErrorDetails() + string("\n"));
						log_queue.push_back("Please try again later." + string("\n"));
					}
					//尽可能发给
					if (!times) {
						lock_guard<mutex> log_lock(log_queue_mutex);
						log_queue.push_back("Failed to send ACK for pkg from client." + string("\n"));
						log_queue.push_back("------------Dismissed connection-----------" + string("\n"));
						//当然如果多次传不过去，客户端会多次超时重传，最后这边发不过去，对方还一直发，会造成死锁
						//因此提前结束程序
						closesocket(serverSocket);
						WSACleanup();
						system("pause");
						exit(0);
					}
					times--;
					goto SendACK;
				}
				{
					lock_guard<mutex> log_lock(log_queue_mutex);
					log_queue.push_back("-----Sent ACK-----" + string("\n"));
					log_queue.push_back("Successfully sent ACK pkg:" + string("\n"));
					log_queue.push_back("seq: " + to_string(ack_header.get_seq()) + " , ack: " + to_string(ack_header.get_ack()) + ", flag: " + to_string(ack_header.get_flag()) + ", checksum: " + to_string(ack_header.get_checksum()) + string("\n"));
					log_queue.push_back("header length:" + to_string(ack_header.get_header_length()) + ", data length:" + to_string(ack_header.get_data_length()) + string("\n"));
				}
			}
	}
	return 0;
}
void rdt_rcv(char* data_buff, int* curr_pos, bool& waved) {


	send_buff = new char[sizeof(Header)];
	recv_buff = new char[sizeof(Header) + MSS];
	memset(recv_buff, 0, MSS + sizeof(Header));
	memset(send_buff, 0, sizeof(Header));



	int addr_Client_length = sizeof(sockaddr_in);
	int log;//recording logs
	Header recv_header;


	/*
	* Different case for SR
	* For server, there is alse need to implement in mutil-thread
	* Because it need to send ACK to client(For the correctly received pkg) while receving other packages
	* for GBG, it is easy, for it is a stop-and-wait protocol(Server do not need to send anything whle receving, sending happens after the receving behavior)
	* For client, he also need to implement in multi-thread[pipeline]
	*/
	//Start sending thread
	HANDLE send_handle = CreateThread(NULL, 0, send_thread_main, NULL, 0, NULL);
	//Start log thread
	HANDLE log_handle = CreateThread(NULL, 0, log_thread_main, NULL, 0, NULL);


	//Main thread will go on receiving in rdt_rcv function
	u_long mode = 1;
	ioctlsocket(serverSocket, FIONBIO, &mode);
	clock_t start = clock();
	
	{
		lock_guard<mutex> log_lock(log_queue_mutex);
		log_queue.push_back("-----------Waiting for File or Waving hands-----------"+string("\n"));
	}




	while (true) {
		int result;
		// MSS + Header this time
		// recv_buff:  MSS + header
		while ((result = recvfrom(serverSocket, recv_buff, MSS + sizeof(Header), 0, (sockaddr*)&clientAddr, &addr_Client_length)) <= 0) {
			if (clock() - start > PATIENCE) {// finished sent file, but not received furthur request
				{
					lock_guard<mutex> log_lock(log_queue_mutex);
					log_queue.push_back("Patience has run out, connection dismissed." + string("\n"));
					log_queue.push_back("------------Dismissed connection-----------" + string("\n"));
				}
				mode = 0;
				ioctlsocket(serverSocket, FIONBIO, &mode);
				closesocket(serverSocket);
				WSACleanup();
				system("pause");
				exit(0);
			}
		}
		//Indeed received file, only copy the header data to recv_header
		memcpy(&recv_header, recv_buff, sizeof(recv_header));
		

		//Calculate checksum
		u_short cks = checksum(recv_buff, result);
		if (cks != 0) {
			//Corruptied pkg
			lock_guard<mutex> log_lock(log_queue_mutex);
			log_queue.push_back("CORRUPTIED package from client, checksum went wrong!" + string("\n"));
			log_queue.push_back("Wait for following packages......" + string("\n"));
			continue;
		}
		else if (recv_header.get_flag() == FIN) {
			//wave hands starts
			//Wait for send_thread to finish
			cout << "-----------Start Waving Hands-----------" << endl;
			cout << "-----Stage 1-----" << endl;
			cout << "Successfully received FIN pkg from client." << endl;
			cout << "-----Stage 2-----" << endl;
			Header ack_header(0, recv_header.seq + 1, ACK, 0, 0, sizeof(Header));
			memcpy(send_buff, (char*)&ack_header, sizeof(ack_header));
			//send_buff has been changed, so checksum again
			u_short cks = checksum(send_buff, sizeof(Header));
			((Header*)send_buff)->checksum = cks;
		SendACK2:
			log = sendto(serverSocket, send_buff, sizeof(ack_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
			//这里也可能出现socketerror的错误
			if (log == SOCKET_ERROR) {
				cout << "Oops!Failed to send ACK to client on Fin pkg." << endl;
				cout << GetLastErrorDetails() << endl;
				cout << "Please try again later." << endl;
				//尽可能发给
				goto SendACK2;
				//这里其实如果一直发不过去也无所谓，因为客户端由于没有接收到ACK报文
				// 会继续重传FIN报文，直到超过最大重传次数。
			}
			else
				cout << "Successfully sent ACK pkg in respond to FIN pkg from client." << endl;

			cout << "-----------Finished Waving Hands-----------" << endl;
			waved = true;
			break;
		}
		else if (recv_header.get_flag() & RST) {
			{
				lock_guard<mutex> log_lock(log_queue_mutex);
				log_queue.push_back("Recieved RST request from client." + string("\n"));
				log_queue.push_back("------------Dismissed connection-----------" + string("\n"));
			}
			mode = 0;
			ioctlsocket(serverSocket, FIONBIO, &mode);
			closesocket(serverSocket);
			WSACleanup();
			exit(0);
		}
		else {//not corruptied and not FIN, a correct datagram received


			//Log
			{
				lock_guard<mutex> log_lock(log_queue_mutex);
				log_queue.push_back("-----New Datagram-----" + string("\n"));
				log_queue.push_back("Successfully received datagram---" + to_string(recv_header.get_data_length() + recv_header.get_header_length()) + "bytes in length." + string("\n"));
				log_queue.push_back("Header---" + string("\n"));
				log_queue.push_back("seq: " + to_string(recv_header.get_seq()) + " , ack: " + to_string(recv_header.get_ack()) + ", flag: " + to_string(recv_header.get_flag()) + ", checksum: " + to_string(recv_header.get_checksum()) + string("\n"));
				log_queue.push_back("header length:" + to_string(recv_header.get_header_length()) + ", data length:" + to_string(recv_header.get_data_length()) + string("\n"));
			}

			//Check sequence number is in receiver sliding window range
			if (recv_header.get_seq() <= receive_buffer.get_receive_end() 
				&&
				recv_header.get_seq() >= receive_buffer.get_receive_base()
				) {
				//acceptable datagram received
					{
						lock_guard<mutex> log_lock(log_queue_mutex);
						log_queue.push_back("Acceptable datagram in range received!" + string("\n"));
					}

					{
						lock_guard<mutex> ack_lock(ack_deque_mutex);
						ack_deque.push_back(recv_header.get_seq());
					}


					//Check if the datagram has been buffered before
					bool is_there = false;
					
					{
						lock_guard<mutex> receive_buffer_lock(receive_buffer_mutex);
						for(int i = 0;i < receive_buffer.get_slide_window().size();i++)
							if (receive_buffer.get_slide_window()[i]->get_header().get_seq() == recv_header.get_seq()) {
								is_there = true;
								break;
							}
					}
				
				if (!is_there) {
					//That is a new datagram which has not been buffered before
					//Put it in the buffer
					Datagram* datagram = new Datagram(recv_header, recv_buff + sizeof(recv_header));
					receive_buffer.buffer_datagram(datagram);
					lock_guard<mutex> log_lock(log_queue_mutex);
					log_queue.push_back("Buff new datagram in receive_buffer." + string("\n"));
				}

				if (recv_header.get_seq() == receive_buffer.get_receive_base()) {
					/*
					* If the sequence number of the received datagram is the same as the base of the receive window
					* The just now we put it in the receiver buffer,and now it should be sorted at the front of the buffer
					* Front/Back Edge Slide!!!
					* Then pop up and give the continusly sequence of datagrams to the application layer
					* Until the receiver buffer is empty or the sequence number of the datagram is not the same as the base of the receive window
					* 					
					*/
					while (
						receive_buffer.get_slide_window().empty() == false //When the buffer is not empty
						&&
						receive_buffer.get_slide_window()[0]->get_header().get_seq() == receive_buffer.get_receive_base()//When the first datagram in the buffer has the same sequence number as the base of the receive window
						) {
						//Back Edge Slide
						//Front Edge Slide
						Datagram* datagram = receive_buffer.window_edge_slide();
						//从data_buff + *curr_pos位置开始继续写data_buff
						memcpy(data_buff + *curr_pos, datagram->get_data(), datagram->get_header().get_data_length());
						//后移curr_pos
						*curr_pos += datagram->get_header().get_data_length();
						{
							lock_guard<mutex> log_lock(log_queue_mutex);
							log_queue.push_back("-----Given Application Layer-----"+string("\n"));
							log_queue.push_back("New datagram with sequence number of "
								+ to_string(datagram->get_header().get_seq())+
								" has been given to application layer." + string("\n"));
						}


						//Check if it is the last datagram, it can arrive early and then get buffed in buffer
						if (datagram->get_header().get_flag() & LAS) {
							//Last datagram
							{
								lock_guard<mutex> log_lock(log_queue_mutex);
								log_queue.push_back("Last datagram given to application layer." + string("\n"));
								log_queue.push_back("Finish receiving file." + string("\n"));
							}
							//Wait for send_thread to finish
							receive_over = true;
							//Following steps are waving hands, while doing so, there is no need to implete mutil-thread
							//Because waving hands follow stop-and-wait protocol, which is a single thread protocol
							WaitForSingleObject(send_handle, INFINITE);
							WaitForSingleObject(log_handle, INFINITE);
							CloseHandle(send_handle);
							CloseHandle(log_handle);




							start = clock();
							//getout the while(true) to output file(Keep-Alive), just return 
							mode = 0;//阻塞模式
							ioctlsocket(serverSocket, FIONBIO, &mode);
							return;
						}
	
					}



				}
				else {
					/*
					* Other case: if the sequence number of the received datagram is not the same as the base of the receive window
					* The it is the datagram received in disorder
					*/
					lock_guard<mutex> log_lock(log_queue_mutex);
					log_queue.push_back("Datagram received in DISORDER." + string("\n"));
				}


				//Show the receive buffer
				{	
					lock(log_queue_mutex, receive_buffer_mutex);
					lock_guard<mutex> lock(log_queue_mutex, adopt_lock);
					lock_guard<mutex> receive_buffer_lock(receive_buffer_mutex, adopt_lock);
					log_queue.push_back("receive_buffer:{ ");
					int index = 0;
					for (u_short i = receive_buffer.get_receive_base(); i <= receive_buffer.get_receive_end(); i++) {
						if (receive_buffer.get_slide_window().empty() == true)
							goto ShowLabel;
						else {
						ShowLabel:
							log_queue.push_back("[" + to_string(i) + +"]" + " ");
						}

					}
					log_queue.push_back("}" + string("\n"));
				}


			}
			else {
				/*
				* Unexpected datagram received,could be larger than receiver sliding window range or less than
				* Send Corresponding ack and then drop it away
				*/

				{
					lock_guard<mutex> ack_lock(ack_deque_mutex);
					ack_deque.push_back(recv_header.get_seq());
				}

				//Show the receive buffer
				{
					lock(log_queue_mutex, receive_buffer_mutex);
					lock_guard<mutex> log_lock(log_queue_mutex, adopt_lock);
					lock_guard<mutex> receive_buffer_lock(receive_buffer_mutex, adopt_lock);
					log_queue.push_back("Received unexpected datagram, DROP it away." + string("\n"));
					log_queue.push_back("receive_buffer:{ ");
					int index = 0;
					for (int i = receive_buffer.get_receive_base(); i <= receive_buffer.get_receive_end(); i++) {
						if(receive_buffer.get_slide_window().empty()==true)
							goto ShowLabel2;
						else {
							ShowLabel2:
							log_queue.push_back("[" + to_string(i) + +"]" + " ");
						}

					}
					log_queue.push_back("}" + string("\n"));
				}

			}

		}
	}
	mode = 0;//阻塞模式
	ioctlsocket(serverSocket, FIONBIO, &mode);
	return;
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
	//使用数据包套接字UDP
	serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		cout << "Oops!Failed to create socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//清空资源
		return 0;
	}
	else
		cout << "Successfully created socket!" << endl;
	//为套接字绑定IP地址和端口
	cout << "-----------Binding Socket-----------" << endl;
	serverAddr.sin_family = AF_INET; // 设置为IPv4
	serverAddr.sin_port = htons(2333); // 设置端口2333

	inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)); // 设置IPv4地址为127.0.0.1
	//因此不会发生错误，实际上如果是错误的地址格式也应该输出错误信息
	if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cout << "Oops!Failed to bind socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//清空资源
		return 0;
	}
	else
		cout << "Successfully binded socket!" << endl;
	//开始等待
	cout << "-----------Waiting for Shaking hands-----------" << endl;

	if (shake_hand() != -1) {//Successfully shake hand
		/*
		* Once shake hands succceed
		* Keep alive!
		*/
		while (true) {
			file_data_buffer = new char[INT_MAX];//Maxium databuffer
			file_length = 0;
			bool waved = false;
			ack_seq_num = (u_short)-1;
			receive_over = false;



			cout << "-----------Packet Loss-----------" << endl;
			cout << "Please input the loss of packet in transfer:" << endl;
			cout << "Less than 1:No loss         Greater than 99:All loss" << endl;
			cout << "Packet loss rate:";
			cin >> Packet_loss_range;

			//Input Latency for ack
			while (true) {
				cout << "-----------Latency Test-----------" << endl;
				cout << "Please input the latency of time in transfer(ms):" << endl;
				cout << "0:No Latency       3000:3000ms(3s)" << endl;
				cout << "Latency mill seconds[0-3000]:";
				cin >> Latency_mill_seconds;
				if (Latency_mill_seconds < 0 || Latency_mill_seconds > 3000) {
					cout << "Latency mill seconds out of range, please input again." << endl;
					continue;
				}
				else {
					break;
				}
			}

			//Input receive buffer size
			int new_buffer_size;
			while (true) {
				cout << "-----------Buffer Size-----------" << endl;
				cout << "Please input the size of receive buffer:" << endl;
				cout << "Size range[4-32]:";
				cin >> new_buffer_size;
				if (new_buffer_size < 4 || new_buffer_size > 32) {
					cout << "Size out of range, please input again." << endl;
					continue;
				}
				else {
					receive_buffer_size = new_buffer_size;
					break;
				}
			}
			//ReInitialize receive_buffer
			receive_buffer.set_receive_base(1);
			receive_buffer.set_receive_end(receive_buffer.get_receive_base() + receive_buffer_size - 1);
			receive_buffer.get_slide_window().clear();
			//ReInititalize log_queue
			log_queue.clear();

			rdt_rcv(file_data_buffer, &file_length, waved);//curr_pos = file_length开始的位置
			if (waved == true) {
				break;
			}
			//文件名和文件分隔开来了，先找到文件名，读取buff到空串停止
			string file_path = "";
			int pos;
			for (pos = 0; pos < file_length; pos++) {
				if (file_data_buffer[pos] == '?')//First time '?'
					break;
				else
					file_path += file_data_buffer[pos];
			}
			//pos+"\0"=pos+1个字符
			cout << "-----------Result-----------" << endl;
			cout << "Successfully received file from:" + file_path << ", with length of " << file_length - (pos + 1) << " Bytes in total." << endl;


			while (true) {
				cout << "-----------Output File----------" << endl;
				string output_path;
				cout << "Please input a output path:" << endl;
				cout << "(An absolute path, or a path relative to D:\\Visual Studio 2022 Code\\Project-Computer Network\\Lab03-02-UDP RDT Server" << endl;
				cin >> output_path;
				ofstream file(output_path.c_str(), ofstream::binary);
				if (!file.is_open()) {
					cout << "Unable to open file, please start over and chose another output path." << endl;
					continue;
				}
				else {
					file.write(file_data_buffer + pos + 1, file_length - pos - 1);//在文件名和间隔符号之后
					file.close();
					cout << "Successfully output file in path:" + output_path << "." << endl;
					break;
				}
			}


		}

	}
	else {
		cout << "Unable to shake hands, please start over client and server." << endl;
	}
	cout << "-----------Mission Accomplished-----------" << endl;
	cout << "-----------Farewell, My Dear Friend-----------" << endl;
	closesocket(serverSocket);
	WSACleanup();
	system("pause");
	return 0;
}