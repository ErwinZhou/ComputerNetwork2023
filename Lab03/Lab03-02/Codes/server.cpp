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

			if (randomNumber <= Packet_loss_range) {
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

void rdt_rcv(char* data_buff, int* curr_pos, bool& waved) {
	/*
	*
	*/
	bool finished = false;
	send_buff = new char[sizeof(Header)];
	recv_buff = new char[sizeof(Header) + MSS];
	memset(recv_buff, 0, MSS + sizeof(Header));
	memset(send_buff, 0, sizeof(Header));


	//Initialize
	((Header*)send_buff)->flag = ACK;
	((Header*)send_buff)->ack = expected_sequence_num - 1;//expected_sequence_num is 1 at first, ack on 0
	((Header*)send_buff)->header_length = sizeof(Header);
	((Header*)send_buff)->checksum = checksum(send_buff, sizeof(Header));

	
	int addr_Client_length = sizeof(sockaddr_in);
	int log;//recording logs
	Header recv_header;


	// non-blocking mode
	/*
	* Actully same case here for GBN
	* For server, there is no need to program in mutil-thread
	* Because it can complete it with a while sentence and a non-block mode
	* All he have to do is wait for current window to be filled
	* He don't need to send anything else while recvfrom(ing)
	* For short:All he sending behavior happens after recvfrom(ed)
	* But for client, it is different, he have to send data while recvfrom(ing)[pipeline]
	*/
	u_long mode = 1;
	ioctlsocket(serverSocket, FIONBIO, &mode);
	clock_t start = clock();
	cout << "-----------Waiting for File or Waving hands-----------" << endl;



	while (true) {
		int result;
		// MSS + Header this time
		// recv_buff:  MSS + header
		while ((result = recvfrom(serverSocket, recv_buff, MSS + sizeof(Header), 0, (sockaddr*)&clientAddr, &addr_Client_length)) <= 0) {
			if (clock() - start > PATIENCE) {// finished sent file, but not received furthur request
				cout << "Patience has run out, connection dismissed." << endl;
				cout << "------------Dismissed connection-----------" << endl;
				closesocket(serverSocket);
				WSACleanup();
				exit(0);
			}
		}
		//Indeed received file
		memcpy(&recv_header, recv_buff, sizeof(recv_header));

		cout << "-----New Datagram-----" << endl;
		cout << "Successfully received datagram---" << recv_header.get_data_length() + recv_header.get_header_length() << "bytes in length." << endl;
		cout << "Header---" << endl;
		cout << "seq: " << recv_header.get_seq() << " , ack: " << recv_header.get_ack() << ", flag: " << recv_header.get_flag() << ", checksum: " << recv_header.get_checksum() << endl;
		cout << "header length:" << recv_header.get_header_length() << ", data length:" << recv_header.get_data_length() << endl;
		
		//Calculate checksum
		u_short cks = checksum(recv_buff, result);
		if (cks != 0) {
			//Corruptied pkg
			int times = 5;
		SendACK1:
			//Send ACK on last correctedly received package
			log = sendto(serverSocket, send_buff, sizeof(Header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
			if (log == SOCKET_ERROR) {
				cout << "Oops!Failed to send ACK to client for corruptied pkg." << endl;
				cout << GetLastErrorDetails() << endl;
				cout << "Please try again later." << endl;
				//确保传过去了
				if (!times) {
					cout << "Failed to send ACK on corruptied pkg from client too many times." << endl;
					cout << "------------Dismissed connection-----------" << endl;
					//当然如果多次传不过去，客户端会多次超时重传，最后这边发不过去，对方还一直发，会造成死锁
					//因此提前结束
					return;
				}
				times--;
				goto SendACK1;

			}
			cout << "Corruptied Package from client, checksum went wrong!" << endl;
			cout << "Ack on last correctedly received package sent." << endl;
		}
		else if (recv_header.get_flag() == FIN) {//wave hands starts
			cout << "-----------Start Waving Hands-----------" << endl;
			cout << "-----Stage 1-----" << endl;
			cout << "Successfully received FIN pkg from client." << endl;
			cout << "-----Stage 2-----" << endl;
			Header ack_header(0, recv_header.seq + 1, ACK, 0, 0, sizeof(Header));
			memcpy(send_buff, (char*)&ack_header, sizeof(ack_header));
			//send_buff has been changed, so checksum again
			u_short cks = checksum(send_buff, sizeof(ack_header));
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
			cout << "Recieved RST request from client." << endl;
			cout << "------------Dismissed connection-----------" << endl;
			closesocket(serverSocket);
			WSACleanup();
			exit(0);
		}
		else {//not corruptied and not FIN, a correct datagram received
		
			//Check sequence number, accumulate acknowledge protocol
			if (recv_header.get_seq() == expected_sequence_num) {
				//Expected datagram received
				cout<<"GBN expected datagram received!"<<endl;
				//Send ACK
				Header ack_header(0, expected_sequence_num, ACK, 0, 0, sizeof(Header));

				//at the same time, send_buff has been changed
				//And also the next time, a corruptied or unexpected pkg received, we can just send the send_buff
				//expected_sequence_num will update later, so the ack in send_buff is still expected_sequence_num - 1
				memcpy(send_buff, (char*)&ack_header, sizeof(ack_header));

				//Update expected_sequence_num
				expected_sequence_num++;
				//show receiver sliding window
				cout << "receiver buffer:{ [" << expected_sequence_num << "] }" << endl;
				//so checksum again
				u_short cks = checksum(send_buff, sizeof(ack_header));
				((Header*)send_buff)->checksum = cks;
				
				/*
				* 测试丢包
				*/

				// 生成随机数
				int randomNumber = rand() % 100; // %100 确保数字在 0-99 范围内

				if (randomNumber <= Packet_loss_range) {
					cout << "------------DROP PACKAGE ON PURPOSE!-----------" << endl;
				}
				else {
					int times = 5;
				SendACK3:
					log = sendto(serverSocket, send_buff, sizeof(ack_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
					//这里也可能出现socketerror的错误
					if (log == SOCKET_ERROR) {
						cout << "Oops!Failed to send ACK to client on datagram." << endl;
						cout << GetLastErrorDetails() << endl;
						cout << "Please try again later." << endl;
						//尽可能发给
						if (!times) {
							cout << "Failed to send ACK for pkg from client" << endl;
							cout << "------------Dismissed connection-----------" << endl;
							//当然如果多次传不过去，客户端会多次超时重传，最后这边发不过去，对方还一直发，会造成死锁
							//因此提前结束
							mode = 0;
							ioctlsocket(serverSocket, FIONBIO, &mode);
							return;
						}
						times--;
						goto SendACK3;
						//这里其实如果一直发不过去也无所谓，因为客户端由于没有接收到ACK报文
						// 会继续重传，直到超过最大重传次数。
					}
					cout << "Successfully sent ACK pkg:" << endl;
					cout << "seq: " << ack_header.get_seq() << " , ack: " << ack_header.get_ack() << ", flag: " << ack_header.get_flag() << ", checksum: " << ack_header.get_checksum() << endl;
					cout << "header length:" << ack_header.get_header_length() << ", data length:" << ack_header.get_data_length() << endl;
				}
			//	int times = 5;
			//SendACK3:
			//	log = sendto(serverSocket, send_buff, sizeof(ack_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
			//	//这里也可能出现socketerror的错误
			//	if (log == SOCKET_ERROR) {
			//		cout << "Oops!Failed to send ACK to client on datagram." << endl;
			//		cout << GetLastErrorDetails() << endl;
			//		cout << "Please try again later." << endl;
			//		//尽可能发给
			//		if (!times) {
			//			cout << "Failed to send ACK for pkg from client" << endl;
			//			cout << "------------Dismissed connection-----------" << endl;
			//			//当然如果多次传不过去，客户端会多次超时重传，最后这边发不过去，对方还一直发，会造成死锁
			//			//因此提前结束
			//			mode = 0;
			//			ioctlsocket(serverSocket, FIONBIO, &mode);
			//			return;
			//		}
			//		times--;
			//		goto SendACK3;
			//		//这里其实如果一直发不过去也无所谓，因为客户端由于没有接收到ACK报文
			//		// 会继续重传，直到超过最大重传次数。
			//	}
			//	cout << "Successfully sent ACK pkg:" << endl;
			//	cout << "seq: " << ack_header.get_seq() << " , ack: " << ack_header.get_ack() << ", flag: " << ack_header.get_flag() << ", checksum: " << ack_header.get_checksum() << endl;
			//	cout << "header length:" << ack_header.get_header_length() << ", data length:" << ack_header.get_data_length() << endl;

				//从recv_buff的header内容之后，即data内容开始，读取data内容
				//从data_buff + *curr_pos位置开始继续写data_buff
				memcpy(data_buff + *curr_pos, recv_buff + sizeof(recv_header), recv_header.get_data_length());
				//后移curr_pos
				*curr_pos += recv_header.get_data_length();
				if (recv_header.get_flag() & LAS) {
					finished = true;
					start = clock();
					cout << "Finished receiving file." << endl;
					break;
				}

			}
			else {
				//Unexpected datagram received
				//Send ACK on last correctedly received package
				int times = 5;
			SendACK4:
				log = sendto(serverSocket, send_buff, sizeof(Header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
				if (log == SOCKET_ERROR) {
					cout << "Oops!Failed to send ACK to client on unexpected datagram." << endl;
					cout << GetLastErrorDetails() << endl;
					cout << "Please try again later." << endl;
					//确保传过去了
					if (!times) {
						cout << "Failed to send ACK on unexpected pkg from client too many times." << endl;
						cout << "------------Dismissed connection-----------" << endl;
						//当然如果多次传不过去，客户端会多次超时重传，最后这边发不过去，对方还一直发，会造成死锁
						//因此提前结束
						mode = 0;
						ioctlsocket(serverSocket, FIONBIO, &mode);
						return;
					}
					times--;
					goto SendACK4;
				}
				cout << "Received unexpected datagram, DROP it away." << endl;
				//show receiver sliding window
				cout << "receiver buffer:{ [" << expected_sequence_num << "] }" << endl;
				cout << "Ack on last correctedly received package sent." << endl;
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
			//Reset expected_sequence_num
			expected_sequence_num = 1;

			cout << "-----------Packet Loss-----------" << endl;
			cout << "Please input the loss of packet in transfer:" << endl;
			cout << "Less than 0:No loss         Greater than 99:All loss" << endl;
			cout << "Packet loss rate:";
			cin >> Packet_loss_range;

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