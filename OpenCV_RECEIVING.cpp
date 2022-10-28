#pragma comment(lib, "opencv_highgui231.lib")
#pragma comment(lib, "opencv_imgproc231.lib")
#pragma comment(lib, "opencv_core231.lib")
#pragma comment(lib, "ws2_32")

#include <opencv2/opencv.hpp>
#include <winsock2.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>

auto CLINENT_VIEW_WINDOW_NAME = "Client View";
#define BUFSIZE 511
#define SERVER_PORT 10555

typedef enum
{
	CONNECT_SERVER=1,
	DISCONNECT=2,
	EXIT=3
}Menu;

typedef enum
{
	SUCCESS,
	SOCKET_FAIL,
	CONNECT_FAIL,
	ALREADY_CONNECT,
	NOT_CONNECT
}Result;

typedef enum
{
	ST_CONNECT, ST_DISCONNECT
}ConnectState;

typedef struct
{
	SOCKET *sock;
	ConnectState *state;
}SocketAState;

using namespace std;

void printErrorMessage(Result r)
{
	char erMsg[BUFSIZE] = "\0";
	switch( r )
	{
	case SOCKET_FAIL:
		printf(erMsg, "[!] SOCKET Fail!!");
		break;
	case CONNECT_FAIL:
		printf(erMsg, "[!] Connect Fail!!");
		break;
	case ALREADY_CONNECT:
		printf(erMsg, "[!] �̹� ������ ����Ǿ� �ֽ��ϴ�.");
		break;
	case NOT_CONNECT:
		printf(erMsg, "[!] ������ ����� ���°� �ƴմϴ�.");
		break;
	}
	cout << erMsg << endl << endl;
}

void printSuccessMessage(Menu m)
{
	char erMsg[BUFSIZE] = "\0";
	switch( m )
	{
	case CONNECT_SERVER:
		sprintf(erMsg, "  <Ŭ���̾�Ʈ �˸�> ������ ����Ǿ����ϴ�");
		break;
	case DISCONNECT:
		sprintf(erMsg, "  <Ŭ���̾�Ʈ �˸�> ������ ������ �������ϴ�.");
		break;
	}
	cout << erMsg << endl << endl;
}

int inputMenu()
{
	int input = 0;

	cout << "-----------------" << endl;
	cout << "1. ���� ����" << endl;
	cout << "2. ���� ����" << endl;
	cout << "3. ����" << endl;
	cout << "-----------------" << endl;

	while( input < 1 || input > 3 )
	{
		cout << ">> ";
		cin >> input;
	}

	return input;
}

// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while(left > 0){
		received = recv(s, ptr, left, flags);
		if(received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if(received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

DWORD WINAPI getImageFromServer(LPVOID arg)
{
	SocketAState *sas = (SocketAState*)arg;
	SOCKET *sock = sas->sock;
	ConnectState *state = sas->state;

	cvNamedWindow(CLINENT_VIEW_WINDOW_NAME, 1);
	int FPS = 0;
	time_t startTime = time(NULL);
	// ������ ������ ���
	while( (*state) == ST_CONNECT )
	{
		int length = -1;
		int type = -1;
		int len = -1;
		int retval = -1;
		
		retval = recvn((*sock), (char *)&len, sizeof(int), 0);
		if( (retval) == -1 || len < 0 )
		{
			break;
		}
		retval = recvn((*sock), (char *)&length, len, 0);
		if( (retval) == -1 || length < 0 )
		{
			break;
		}
		retval = recvn((*sock), (char *)&len, sizeof(int), 0);
		if( (retval) == -1 || len < 0 )
		{
			break;
		}
		retval = recvn((*sock), (char *)&type, len, 0);
		if( (retval) == -1 || type < 0 )
		{
			break;
		}
		retval = recvn((*sock), (char *)&len, sizeof(int), 0);
		if( (retval) == -1 || len < 0 )
		{
			break;
		}
		char *captureData = new char[len+1];
		memset(captureData, 0, sizeof(char)*(len+1));
		retval = recvn((*sock), (char *)captureData, len, 0);
		if( (retval) == -1 )
		{
			delete captureData;
			break;
		}
		captureData[len] = '\0';

		if( len != length )
		{
			delete captureData;
			continue;
		}
		CvMat *eMat = cvCreateMat(1,length,type);
		memcpy( eMat->data.ptr, captureData, length);
		IplImage *showImage = cvDecodeImage(eMat);
		cvShowImage(CLINENT_VIEW_WINDOW_NAME, showImage);
		cvWaitKey(10);

		FPS++;

		time_t endTime = time(NULL);
		if( startTime != endTime )
		{
			//cout << "FPS : " << FPS << endl;
			FPS = 0;
			startTime = endTime;
		}

		if (eMat != NULL)			cvReleaseMat(&eMat);
		if (showImage != NULL)		cvReleaseImage(&showImage);
		if( captureData != NULL )	delete captureData;
	}
	cout << "[!] ������ ������ ���������ϴ�." << endl;
	(*state) = ST_DISCONNECT;
	cvDestroyWindow(CLINENT_VIEW_WINDOW_NAME);
	ExitThread(0);
	return 0;
}

void inputServerIp(char s_ip[])
{
	cout << "Input Server IP : " ;
	cin >> s_ip;
}

Result connectServer(SOCKET *sock, SOCKADDR_IN *serveraddr, ConnectState *state)
{
	if( (*state) == ST_CONNECT )
	{
		return ALREADY_CONNECT;
	}

	// socket()
	(*sock) = socket(AF_INET, SOCK_STREAM, 0);
	if((*sock) == INVALID_SOCKET)
	{
		return SOCKET_FAIL;
	}

	char serverIP[BUFSIZE] = {'\0'};

	inputServerIp(serverIP);

	// connect()
	ZeroMemory(serveraddr, sizeof(serveraddr));
	serveraddr->sin_family = AF_INET;
	serveraddr->sin_addr.s_addr = inet_addr(serverIP);
	serveraddr->sin_port = htons(SERVER_PORT);

	int retval = connect((*sock), (SOCKADDR *)serveraddr, sizeof(*serveraddr));
	if(retval == SOCKET_ERROR)
	{
		closesocket((*sock));
		return CONNECT_FAIL;
	}
	(*state) = ST_CONNECT;

	return SUCCESS;
}

Result disconnectServer(SOCKET *sock, ConnectState *state)
{
	if( (*state) == ST_DISCONNECT )
	{
		return NOT_CONNECT;
	}
	(*state) = ST_DISCONNECT;

	return SUCCESS;
}

int main(int argc, char *argv[])
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
	{
		return 1;
	}

	SOCKET sock;
	SOCKADDR_IN serveraddr;
	ConnectState state = ST_DISCONNECT;
	HANDLE hThread;
	while(1)
	{
		Menu menu = (Menu)inputMenu();
		if( menu == EXIT)
		{
			break;
		}

		Result r;
		SocketAState sas = {&sock, &state};
		switch( menu )
		{
		case CONNECT_SERVER:
			r = connectServer(&sock, &serveraddr, &state);
			hThread = CreateThread(NULL,0,getImageFromServer,(LPVOID)&sas,0,NULL);
			break;
		case DISCONNECT:
			r = disconnectServer(&sock, &state);
			closesocket(sock);
			break;
		}

		if( r == SUCCESS )
		{
			printSuccessMessage(menu);
		}
		else if( r != SUCCESS )
		{
			printErrorMessage(r);
		}
	}

	// ���� ����
	WSACleanup();
	return 0;
}