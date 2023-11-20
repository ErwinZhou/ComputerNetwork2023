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
		// Successfully eceive pkg, but can it be corruptied or is it SYN pkg?
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
			* ���Զ���
			*/
			// ���������
			int randomNumber = rand() % 1; //ȷ��������0-1��Χ�ڣ�һ��������

			if (randomNumber == 0) {
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
					//�������ֱ�ӷ������ִ����е������
					//������ص��ʼ����ʱ�ͻ����Ѿ��ɹ�������SYN����SYN+ACK��
					//��������Ҳ�����µȴ��ͻ��˵�SYN�����γ�������
					//Ϊ�˷�ֹ�ͻ��˳�ʱ�ش�SYN�������ർ�����ֽ�����ֱ��goto�ص�����λ��
					//�����ӣ��Ҳ���û���㰡����
					//Ŭ������һ�¡�����
					goto SendSYNACK1;
					//��Ȼ����������ɹ����ͻ��˷���SYN�������࣬Ҳ������ʧ��
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
							//ͬ��Ŭ������һ�£��ص������ش��Ŀ�ʼλ��
							goto SendSYNACK2;
						}
						max_retries_times--;
						//increase udp_2msl by one second
						udp_2msl += MSL;
						start = clock();
						cout << "Timeout, resent SYN & ACK to client." << endl;
						//ͬ������������ش�һ���Ƕ�����
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
				else if (cks == 0 && (recv_header.get_flag() & SYN)) { //�ɹ�������SYN+ACK���������յ����µ�pkg
					//��������ȴ����SYN��֤�����͵�SYN+ACK����ʧ��
					goto SendSYNACK2;
				}
				else if (cks == 0 && (recv_header.get_flag() == 0)) {//�����ǰ�յ������ݱ��ģ��������˵��ĳ����¹�
					//����RST���ĺ󣬹ر�����												
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
						//ȷ������ȥ��
						goto SendRST1;
					}
					cout << "Unexpectedly received data pkg before ACK pkg, RST sent. " << endl;
					cout << "Shaking Hands Failed!" << endl;
					cout << "-----------Stop Shaking Hands-----------" << endl;
					mode = 0;
					ioctlsocket(serverSocket, FIONBIO, &mode);
					return -1;//return failure flag
				}
				else if(cks){//pkg is corriputed
					cout << "Oops!Package from client is corriputed." << endl;
					//��ʱ�ͻ����Ѿ���������˷�����ACK�����裬�����������Ӹ��ӣ�
					//������ˣ�������Ҳ�������յ�ACK��Ϣ�ˣ��˺��յ���ֻ�������ݰ�����ΪACK�������ط�
					//��ȻҲ�����յ�����FIN�����������ˡ�����
					//������ʱ��ȡ�Ĳ�����ֱ�ӽ��������������������������ᡣ
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
						//ȷ������ȥ��
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
					//ȷ������ȥ��
					if (!times) {
						cout << "Failed to send RST pkg to client too many times." << endl;
						cout << "------------Dismissed connection-----------" << endl;
						u_long mode = 0;
						ioctlsocket(serverSocket, FIONBIO, &mode);
						return -1;//return failure flag
					}
					times--;
					goto SendRST2;
					//��Ȼ���һֱ������ȥ���ᵼ�¿ͻ��˻���һֱ�������������
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
	int addr_Client_length = sizeof(sockaddr_in);
	int log;//recording logs
	Header recv_header;
	// Keep-alive
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
		//Calculate checksum
		u_short cks = checksum(recv_buff, result);
		if (cks != 0) {
			//Corruptied
			cout << "-----New Datagram-----" << endl;
			cout << "successfully received datagram---" << recv_header.get_data_length() + recv_header.get_header_length() << "bytes in length." << endl;
			cout << "header---" << endl;
			cout << "seq: " << recv_header.get_seq() << " , ack: " << recv_header.get_ack() << ", flag: " << recv_header.get_flag() << ", checksum: " << recv_header.get_checksum() << endl;
			cout << "header length:" << recv_header.get_header_length() << ", data length:" << recv_header.get_data_length() << endl;
			Header ack_header(0, (sequence_num ^ 1), ACK, 0, 0, sizeof(Header));
			memcpy(send_buff, (char*)&ack_header, sizeof(ack_header));
			cks = checksum(send_buff, sizeof(ack_header));
			((Header*)send_buff)->checksum = cks;
			int times = 5;
		SendACK1:
			log = sendto(serverSocket, send_buff, sizeof(ack_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
			if (log == SOCKET_ERROR) {
				cout << "Oops!Failed to send ACK to client for corruptied pkg." << endl;
				cout << GetLastErrorDetails() << endl;
				cout << "Please try again later." << endl;
				//ȷ������ȥ��
				if (!times) {
					cout << "Failed to send ACK on corruptied pkg from client too many times." << endl;
					cout << "------------Dismissed connection-----------" << endl;
					//��Ȼ�����δ�����ȥ���ͻ��˻��γ�ʱ�ش��������߷�����ȥ���Է���һֱ�������������
					//�����ǰ����
					return;
				}
				times--;
				goto SendACK1;

			}
			cout << "Corruptied Package from client, checksum went wrong!" << endl;
			cout << "Ack on last package sent." << endl;
		}
		else if (recv_header.get_flag() == FIN) {//wave hands starts
			cout << "-----------Start Waving Hands-----------" << endl;
			cout << "-----Stage 1-----" << endl;
			cout << "Successfully received FIN pkg from client." << endl;
			cout << "-----Stage 2-----" << endl;
			Header ack_header(0, (recv_header.get_seq() + 1) % MAX_SEQ, ACK, 0, 0, sizeof(Header));
			memcpy(send_buff, (char*)&ack_header, sizeof(ack_header));
			u_short cks = checksum(send_buff, sizeof(ack_header));
			((Header*)send_buff)->checksum = cks;
			SendACK2:
			log = sendto(serverSocket, send_buff, sizeof(ack_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
			//����Ҳ���ܳ���socketerror�Ĵ���
			if (log == SOCKET_ERROR) {
				cout << "Oops!Failed to send ACK to client on Fin pkg." << endl;
				cout << GetLastErrorDetails() << endl;
				cout << "Please try again later." << endl;
				//�����ܷ���
				goto SendACK2;
				//������ʵ���һֱ������ȥҲ����ν����Ϊ�ͻ�������û�н��յ�ACK����
				// ������ش�FIN���ģ�ֱ����������ش�������
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
			//ACK on received pkg sequence
			cout << "-----New Datagram-----" << endl;
			cout << "successfully received datagram---" << recv_header.get_data_length() + recv_header.get_header_length() << "bytes in length." << endl;
			cout << "header---" << endl;
			cout << "seq: " << recv_header.get_seq() << " , ack: " << recv_header.get_ack() << ", flag: " << recv_header.get_flag() << ", checksum: " << recv_header.get_checksum() << endl;
			cout << "header length:" << recv_header.get_header_length() << ", data length:" << recv_header.get_data_length() << endl;



			Header ack_header(0, recv_header.get_seq(), ACK, 0, 0, sizeof(Header));


			memcpy(send_buff, (char*)&ack_header, sizeof(ack_header));
			u_short cks = checksum(send_buff, sizeof(ack_header));
			((Header*)send_buff)->checksum = cks;
			int times = 5;
		SendACK3:
			log = sendto(serverSocket, send_buff, sizeof(ack_header), 0, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
			//����Ҳ���ܳ���socketerror�Ĵ���
			if (log == SOCKET_ERROR) {
				cout << "Oops!Failed to send ACK to client on datagram." << endl;
				cout << GetLastErrorDetails() << endl;
				cout << "Please try again later." << endl;
				//�����ܷ���
				if (!times) {
					cout << "Failed to send ACK for pkg from client" << endl;
					cout << "------------Dismissed connection-----------" << endl;
					//��Ȼ�����δ�����ȥ���ͻ��˻��γ�ʱ�ش��������߷�����ȥ���Է���һֱ�������������
					//�����ǰ����
					mode = 0;
					ioctlsocket(serverSocket, FIONBIO, &mode);
					return;
				}
				times--;
				goto SendACK3;
				//������ʵ���һֱ������ȥҲ����ν����Ϊ�ͻ�������û�н��յ�ACK����
				// ������ش���ֱ����������ش�������
			}
			cout << "Successfully sent ACK pkg:" << endl;
			cout << "seq: " << recv_header.get_seq() << " , ack: " << recv_header.get_ack() << ", flag: " << recv_header.get_flag() << ", checksum: " << recv_header.get_checksum() << endl;
			cout << "header length:" << recv_header.get_header_length() << ", data length:" << recv_header.get_data_length() << endl;
			if (sequence_num == recv_header.get_seq()) {//����������Ҫ�İ�seq,RDT2.1
				sequence_num ^= 1;//״̬ת��
				//��recv_buff��header����֮�󣬼�data���ݿ�ʼ����ȡdata����
				//��data_buff + *lenλ�ÿ�ʼ����дdata_buff
				memcpy(data_buff + *curr_pos, recv_buff + sizeof(recv_header), recv_header.get_data_length());
				//����len
				*curr_pos += recv_header.get_data_length();
				cout << "Successfully received Datagram." << endl;
			}
			else {
				cout << "Received repeated datagram, DROP it away." << endl;
			}
			if (recv_header.get_flag() & LAS) {
				finished = true;
				start = clock();
				cout << "Finished receiving file." << endl;
				break;
			}
		}
	}
	mode = 0;//����ģʽ
	ioctlsocket(serverSocket, FIONBIO, &mode);
	return;
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
	//ʹ�����ݰ��׽���UDP
	serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		cout << "Oops!Failed to create socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//�����Դ
		return 0;
	}
	else
		cout << "Successfully created socket!" << endl;
	//Ϊ�׽��ְ�IP��ַ�Ͷ˿�
	cout << "-----------Binding Socket-----------" << endl;
	serverAddr.sin_family = AF_INET; // ����ΪIPv4
	serverAddr.sin_port = htons(2333); // ���ö˿�2333

	inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)); // ����IPv4��ַΪ127.0.0.1
	//��˲��ᷢ������ʵ��������Ǵ���ĵ�ַ��ʽҲӦ�����������Ϣ
	if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cout << "Oops!Failed to bind socket" << endl;
		cout << GetLastErrorDetails() << endl;
		cout << "Please start over" << endl;
		WSACleanup();//�����Դ
		return 0;
	}
	else
		cout << "Successfully binded socket!" << endl;
	//��ʼ�ȴ�
	cout << "-----------Waiting for Shaking hands-----------" << endl;
	
	if (shake_hand() != -1) {//Successfully shake hand
		/*
		* Once shake hands succceed
		* Keep alive!
		*/
		while (true) {
			char* file_data_buffer = new char[INT_MAX];//Maxium databuffer
			int file_length = 0;
			bool waved = false;
			rdt_rcv(file_data_buffer, &file_length, waved);//curr_pos = file_length��ʼ��λ��
			if (waved == true) {
				break;
			}
			//�ļ������ļ��ָ������ˣ����ҵ��ļ�������ȡbuff���մ�ֹͣ
			string file_path = "";
			int pos;
			for (pos = 0; pos < file_length; pos++) {
				if (file_data_buffer[pos] == '?')//First time '?'
					break;
				else
					file_path += file_data_buffer[pos];
			}
			//pos+"\0"=pos+1���ַ�
			cout << "-----------Result-----------" << endl;
			cout << "Successfully received file from:" + file_path << ", with length of" << file_length - (pos + 1) << " Bytes in total." << endl;
			cout << "-----------Output File----------" << endl;
			while (true) {
				string output_path;
				cout << "Please input a output path:" << endl;
				cout << "(An absolute path, or a path relative to D:\\Visual Studio 2022 Code\\Project-Computer Network\\Lab3-1-UDP RDT Server" << endl;
				cin >> output_path;
				ofstream file(output_path.c_str(), ofstream::binary);
				if (!file.is_open()) {
					cout << "Unable to open file, please start over and chose another output path." << endl;
					continue;
				}
				else {
					file.write(file_data_buffer + pos + 1, file_length - pos - 1);//���ļ����ͼ������֮��
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
	return 0;
}