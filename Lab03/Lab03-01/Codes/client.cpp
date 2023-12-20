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
	Header syn_header(0, 0, SYN, 0, 0, sizeof(Header));
	memcpy(send_buff, (char*)&syn_header, sizeof(syn_header));
	u_short cks = checksum(send_buff, sizeof(syn_header));
	((Header*)send_buff)->checksum = cks;
	/*
	* ���Զ���
	*/
	// ���������
	int randomNumber = rand() % 1; //ȷ��������0��Χ��

	if (randomNumber == 0) {
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
			//��ʱ��������������sleep���أ����öԷ���̫�ã�
			if (!times) {
				//�������һֱ��ͨ��������Ҳ��һֱ�ȣ��ᵼ��������������β��У�ֱ���˳���
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
	ioctlsocket(clientSocket,FIONBIO, &mode);
	sockaddr_in tempAddr;
	int temp_addr_length = sizeof(sockaddr_in);
	while (true) {
		//continue to recvfrom wrong
		while (recvfrom(clientSocket, recv_buff, sizeof(Header), 0, (sockaddr*)&tempAddr, &temp_addr_length) <= 0) {
			if (clock() - start > 1.2 * udp_2msl) {
				if (max_retries_times <= 0) {
					cout << "Reached max times on resending SYN." << endl;
					cout << "Shaking Hands Failed!" << endl;
					cout << "-----------Stop Shaking Hands-----------" << endl;
					//mode = 0;
					//ioctlsocket(clientSocket, FIONBIO, &mode);
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
						//ͬ�����һֱ��ͨ��������Ҳ��һֱ�ȣ��ᵼ��������������β��У�ֱ���˳���
						cout << "Failed to send SYN pkg to server too many times." << endl;
						cout << "------------Dismissed connection-----------" << endl;
						return -1;
					}
					//ͬ��Ŭ������һ�£��ص������ش��Ŀ�ʼλ��
					times--;
					goto SendSYN2;
				}
				max_retries_times--;
				//increase udp_2msl
				udp_2msl += MSL;
				start = clock();
				cout << "Timeout, resent SYN to server." << endl;
				//��ʱ�����ش�һ�������˶�����
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
				//��������һ�£�������ɹ�����������Ҳ����Ϊ��ʱ�ش������������������������������
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
		//else...��������������ֱ�ӻص���ʼwhileѭ���Ŀ�ʼ���½���SYN+ACK,��Ϊ��������û�н��յ�ACK�᲻�����·�SYN+ACK

	}
	//�˳�while��ʵ���ϲ����ܷ�����һ������ѭ���з���ĳ��ֵ
}

bool rdt_send(char* data_buff, int pkg_length, bool last_pkg) {
	//Inititalize
	int flag = last_pkg ? LAS : 0;
	int log;
	bool result = true;
	Header send_header(sequence_num, 0, flag, 0, pkg_length, sizeof(Header));
	send_buff = new char[sizeof(send_header) + pkg_length];
	recv_buff = new char[sizeof(Header)];//only can receive header instead of data 
	memcpy(send_buff, (char*)&send_header, sizeof(send_header));//copy header to send_buff
	memcpy(send_buff + sizeof(send_header), data_buff, pkg_length);	//copy data content to send_buff
	//Total length:data length + header length
	u_short cks = checksum(send_buff, pkg_length+sizeof(send_header));
	((Header*)send_buff)->checksum = cks;
	

	/*
	* Latency test for Packet(Absolute)
	*/
	if (Latency_mill_seconds) {
		//{
		//	lock_guard<mutex> log_queue_lock(log_queue_mutex);
		//	log_queue.push_back("------------DELAY TIME ABSOLUTE!-----------" + string("\n"));
		//}
		Sleep(Latency_mill_seconds);
	}

	/*
	* ���Զ���
	*/


	// ���������
	int randomNumber = rand() % 100; // %100 ȷ�������� 0-99 ��Χ��

	if (randomNumber < Packet_loss_range) {
		cout << "------------DROP PACKAGE ON PURPOSE!-----------" << endl;
	}
	else {
		cout << "-----New Datagram-----" << endl;
		log = sendto(
			clientSocket,
			send_buff,
			pkg_length + sizeof(send_header),//total length
			0,//no flags
			(sockaddr*)&serverAddr,
			sizeof(sockaddr_in)
		);

		if (log == SOCKET_ERROR) {
			//�ظ���Σ�Ȼ����RST������...
		}

		//logs printing
		cout << "Successfully sent datagram---" << send_header.get_data_length() + send_header.get_header_length() << "Bytes in length." << endl;
		cout << "Header---" << endl;
		cout << "seq: " << send_header.get_seq() << " , ack: " << send_header.get_ack() << ", flag: " << send_header.get_flag() << ", checksum: " << send_header.get_checksum() << endl;
		cout << "header length:" << send_header.get_header_length() << ", data length:" << send_header.get_data_length() << endl;
	}
	
	//Wait for ACK from server
	clock_t start = clock(); // ����ʱ��
	// ����������ģʽ
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);
	while (true) {
		sockaddr_in tempAddr;
		Header recv_header;
		int temp_addr_length = sizeof(sockaddr_in);
		while (recvfrom(
			clientSocket,
			recv_buff,
			sizeof(recv_header),
			0,
			(sockaddr*)&tempAddr,
			&temp_addr_length
		) <= 0) {
			if (clock() - start > 1.2 * udp_2msl) {//Timeout without receving ACK

				int random = rand() % 100;
				if (random < Packet_loss_range) {
					cout << "------------DROP PACKAGE ON PURPOSE!-----------" << endl;
				}
				else {
					log = sendto(
						clientSocket,
						send_buff,
						pkg_length + sizeof(send_header),//total length
						0,//no flags
						(sockaddr*)&serverAddr,
						sizeof(sockaddr_in)
					);
					if (log == SOCKET_ERROR) {
						//�ظ���Σ�Ȼ����RST������
					}
					cout << "Timeout, resent datagram to server." << endl;
				}
				start = clock();//restarting clock
			}
		}
		
		//Indeed Receive ACK from server

		/*
		* Relative Latency Test
		*/
		int random_number = rand() % 100;
		if (random_number < Packet_loss_range) {
			cout << "------------DELAY TIME RELATIVE!-----------" << endl;
			udp_2msl = udp_2msl * Latency_param;
		
		}
		memcpy(&recv_header, recv_buff, sizeof(recv_header));//only header is useful
		//logs printing
		cout << "Successfully receive datagram---" << recv_header.get_data_length() + recv_header.get_header_length() << "Bytes in length." << endl;
		cout << "Header---" << endl;
		cout << "seq: " << recv_header.get_seq() << " , ack: " << recv_header.get_ack() << ", flag: " << recv_header.get_flag() << ", checksum: " << recv_header.get_checksum() << endl;
		cout << "header length:" << recv_header.get_header_length() << ", data length:" << recv_header.get_data_length() << endl;
		u_short cks = checksum(recv_buff, sizeof(recv_header));
		if (
			cks == 0 //not corruptied
			&&
			(recv_header.get_flag() & ACK) //ACK flag
			&&
			(recv_header.get_ack() == sequence_num)// ACK = sequence number RDT3.0
			) {
			//udp_2msl = 0.8 * udp_2msl;//��ʱ�ط�
			//
			cout << "Server has acknowleged the datagram." << endl;
			break;
		}
		else if (
			cks == 0 //not corruptied
			&&
			recv_header.get_flag() & RST //server try to close connection
			) {
			cout << "Server unexpected closed:Error in connection." << endl;
			//sequence transfer
			result = false;
			break;
		}
		else if (
			cks != 0 //ACK pkg probably corruptied during transmisssion
			)
			continue;//continue to send pkg to server

	}
	mode = 0;
	ioctlsocket(clientSocket, FIONBIO, &mode);
	sequence_num ^= 1;
	return result;
}
void send_data(string file_path) {
	//binary way of open file to read
	ifstream file(file_path.c_str(), ifstream::binary);
	//Get File length
	file.seekg(0, file.end);
	file_length = file.tellg();//use file pointer to calculate length
	file.seekg(0, file.beg);//remove to original pos

	int total_length = file_path.length() + file_length + 1;
	//�м��һ�����֣��ļ�·��+���������+�ļ�����
	memset(file_data_buffer, 0, sizeof(char) * total_length);//��Ȼ����ΪINT_MAX,��ֻ��Ҫ�õ�һ���ּ��ܳ���
	//file path
	memcpy(file_data_buffer, file_path.c_str(), file_path.length());
	//seperate file path with file content
	file_data_buffer[file_path.length()] = '?';//��ΪWindows�ļ�·�����ܰ������������һ����������
	//read the rest real data
	file.read(file_data_buffer + file_path.length() + 1, file_length);
	file.close();
	cout << "-----------Start Sending File-----------" << endl;
	clock_t start = clock();
	// Send data in MSS Segements
	int curr_pos = 0;
	int log;

	while (curr_pos < total_length) {
		int pkg_length = total_length - curr_pos >= MSS ? MSS : total_length - curr_pos;
		bool last = total_length - curr_pos <= MSS ? true : false;
		log = rdt_send(file_data_buffer + curr_pos, pkg_length, last);
		if (!log) {
			//���rdt_send�������⣬ֻ�п����ǽ��յ����������������͵�RST������������������������
			//��ʱ�ͻ���Ҳû�б�Ҫ�ٵ��ˣ�ֱ���˳�
			delete[] file_data_buffer;
			delete[] send_buff;
			delete[] recv_buff;
			closesocket(clientSocket);
			WSACleanup();
			exit(0);
		}
		curr_pos += MSS;
	}
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
	//Random number to make sure acknowlegment
	u_short random_seq = rand() % MAX_SEQ;
	Header fin_header(random_seq, 0, FIN, 0, 0, sizeof(Header));

	memcpy(send_buff, (char*)&fin_header, sizeof(fin_header));
	u_short cks = checksum(send_buff, sizeof(fin_header));
	((Header*)send_buff)->checksum = cks;


	/*
	* ���Զ���
	*/


	// ���������
	int randomNumber = rand() % 1; //ȷ��������0��Χ��

	if (randomNumber == 0) {
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
			cout << "Sequence Number:" << random_seq << ", expects acknowledge number:" << random_seq + 1 << "." << endl;
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
				//udp_2msl += MSL;
				start = clock();
				cout << "Timeout, resent FIN pkg to server." << endl;
				cout << "Sequence Number:" << random_seq << ", expects acknowledge number:" << random_seq + 1 << "." << endl;
				//ͬ����������һ���Ƕ���������ʱ
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
			recv_header.get_ack() == (random_seq + 1) % MAX_SEQ //ensure it's not a general ack pkg of datagram
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
			//����ֻҪ������ȥ��ʱ�ش����ɣ���Ϊserver���·���ack��͹ر���
			//����������
		}
	}

}

int main() {
	//�����ӭ��Ϣ
	cout << "-----------Stable UDP----------- " << endl;
	//����Winsock2����
	//��ʼ������ĸ��ֵײ���Դ�������Լ����ܵķ����ṩ�̵��ض�ʵ��
	cout << "-----------Initializing Winsock-----------" << endl;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
		cout << "Oops!Failed to initialize Winsock " << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		return 0;
	}
	else
		cout << "Successfully initialized Winsock!" << endl;

	//�׽��ִ���
	cout << "-----------Creating Socket-----------" << endl;
	//ʹ��IPv4�׽���
	//ʹ�����ݰ��׽���-UDP
	clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		cout << "Oops!Failed to create socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//�����Դ
		return 0;
	}
	else
		cout << "Successfully created socket!" << endl;
	//����Server�ĵ�ַ���������ַ���SYN
	serverAddr.sin_family = AF_INET; // ����ΪIPv4
	inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)); // ����IPv4��ַΪ127.0.0.1
	serverAddr.sin_port = htons(2333); // ���ö˿�2333


	//��˲��ᷢ������ʵ��������Ǵ���ĵ�ַ��ʽҲӦ�����������Ϣ

	cout << "-----------Binding Socket-----------" << endl;
	//������ʵҲ���Բ�����bind
	//��Ϊ��ʹ��sendtoǰû��ͨ��bind���׽��ַ��䱾�ض˿ڣ�ϵͳ��Ϊ���׽����Զ�����һ����ʱ�˿�
	clientAddr.sin_family = AF_INET; // ����ΪIPv4
	clientAddr.sin_port = htons(4399); // ���ö˿�4399
	inet_pton(AF_INET, "127.0.0.1", &(clientAddr.sin_addr)); // ����IPv4��ַΪ127.0.0.1
	//��˲��ᷢ������ʵ��������Ǵ���ĵ�ַ��ʽҲӦ�����������Ϣ
	if (bind(clientSocket, (SOCKADDR*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) {
		cout << "Oops!Failed to bind socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//�����Դ
		return 0;
	}
	else
		cout << "Successfully binded socket!" << endl;

	
	if (shake_hand() == -1) 
		cout << "Unable to shake hands, please start over client and server." << endl;
	else{//Successfully shaking hands, start transfering files
		while (true) {
			if (restart == false)
				break;


			cout << "-----------Packet Loss-----------" << endl;
			cout << "Please input the loss of packet in transfer:" << endl;
			cout << "Less than 1:No loss         Greater than 99:All loss" << endl;
			cout << "Packet loss rate:";
			cin >> Packet_loss_range;


			while (true) {
				cout << "-----------Latency Test Relative-----------" << endl;
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

			//Input Latency in mill seconds
			while (true) {
				cout << "-----------Latency Test Absolute-----------" << endl;
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


			cout << "-----------Input File-----------" << endl;
			string input_path;
			while (true) {
				cout << "Please input a file path:" << endl;
				cout << "(An absolute path, or a path relative to D:\\Visual Studio 2022 Code\\Project-Computer Network\\Lab03-01-UDP RDT client)" << endl;
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
			bool flag = false;//��������goto�ˡ�����
			while (true) {
				if (flag == true)
					break;
				cout << "Would you like to send aother file or exit? " << endl;
				cout << "1:Send another file         2:Exit" << endl;
				cout << "You choice:";
				int choice;
				cin >> choice;
				if (choice == 1) {
					restart = true;
					flag = true;
				}
				else if(choice == 2){
					restart = false;
					flag = true;
				}
				else {
					cout << "You can only choose 1 or 2, please chose again." << endl;
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