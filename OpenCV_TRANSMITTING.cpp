#pragma comment(lib, "opencv_highgui231.lib")
#pragma comment(lib, "opencv_imgproc231.lib")
#pragma comment(lib, "opencv_core231.lib")
#pragma comment(lib, "ws2_32")

#include <opencv2/opencv.hpp>
#include <winsock2.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>

#define BUFSIZE 256
#define SERVERPORT 10555

#define CAPTURE_WIDTH 640
#define CAPTURE_HEIGHT 480

#define SERVER_VIEW_WINDOW_NAME "Server View"
//#define CAPTURE_TARGET cvCaptureFromFile("o.avi")
#define CAPTURE_TARGET cvCaptureFromCAM(0)

typedef enum
{
	SERVER_OPEN = 1,
	SERVER_CLOSE = 2,
	EXIT = 3
}Menu;

typedef enum
{
	ST_SERVER_OPEN,
	ST_SERVER_CLOSE,
}ServerState;

typedef enum
{
	SUCCESS,
	SOCKET_FAIL,
	BIND_FAIL,
	LISTEN_FAIL,
	ALREADY_OPEN,
	NOT_OPENED
}Result;

CRITICAL_SECTION cs;

using namespace std;

class CEncodeData
{
private:
	char *captureData;
	int length;
	int type;
	int imageID;
public:
	CEncodeData()
	{
		captureData = NULL;
		length = 0;
		type = 0;
		imageID = 0;
	}
	~CEncodeData()
	{
		if( captureData != NULL )
		{
			delete captureData;
		}
	}

	void release()
	{
		if( captureData != NULL )
		{
			delete captureData;
		}
	}
	void setCaptureData(CvMat* mat)
	{
		if( captureData != NULL )
		{
			delete captureData;
		}
		length = mat->step;
		type = mat->type;

		captureData = new char[length+1];
		for(int i=0; i<length; ++i)
		{
			captureData[i] = mat->data.ptr[i];
		}
		captureData[length] = '\0';
	}
	
	char* getCaptureData(){ return captureData; }
	int getDataLength(){ return length; }
	int getDataType(){ return type; }
	int getImageID(){ return imageID; }

	void setDataLength(int l){ length = l; }
	void setDataType(int t){ type = t; }
	void setImageID(int id){ imageID = id; }
};

class CClient
{
private:
	SOCKET sock;
	SOCKADDR_IN addr;
	CEncodeData **encodeData;
	ServerState *state;

public:
	CClient()
	{
		sock = NULL;
		state = NULL;
		encodeData = NULL;
	}
	CClient(SOCKET s, CEncodeData **e, ServerState *st)
	{
		sock = s;
		encodeData = e;
		int addrlen = sizeof(addr);
		getpeername(sock, (SOCKADDR *)&addr, &addrlen);
		state = st;
	}
	SOCKET getSocket(){ return sock; }
	SOCKADDR_IN getAddr(){ return addr; }
	CEncodeData **getEncodeData(){ return encodeData; }
	ServerState *getServerState(){ return state; }
};

class CCaptureData
{
private:
	CEncodeData **encodeData;
	CvCapture *capture;
	ServerState *state;

public:
	CCaptureData(){ encodeData=NULL; capture=NULL; state=NULL;}
	CCaptureData(CvCapture *c, CEncodeData **e, ServerState *s) { encodeData = e; capture = c; state = s;}

	CEncodeData **getEncodeData(){ return encodeData; }
	CvCapture *getCapturePointer(){ return capture; }
	ServerState *getServerState(){ return state; }

	void setServerState(ServerState s){ (*state) = s; }
};

class CSendData
{
private:
	SOCKET sock;
	CEncodeData **encodeData;
	ServerState *state;

public:
	CSendData(){ sock=NULL; encodeData=NULL; state=NULL; }
	CSendData(SOCKET s, CEncodeData **e, ServerState *st){ sock=s; encodeData=e; state=st;}

	SOCKET getSocket(){ return sock; }
	CEncodeData **getEncodeData(){ return encodeData; }
	ServerState *getServerState(){ return state; }
};

void printErrorMessage(Result r)
{
	char erMsg[BUFSIZE] = {'\0'};
	switch( r )
	{
	case SOCKET_FAIL:
		sprintf(erMsg, "[!] Socket 생성에 실패하였습니다.");
		break;
	case BIND_FAIL:
		sprintf(erMsg, "[!] Bind에 실패하였습니다.");
		break;
	case LISTEN_FAIL:
		sprintf(erMsg, "[!] Listen에 실패하였습니다.");
		break;
	case ALREADY_OPEN:
		sprintf(erMsg, "[!] 이미 서버가 열려있습니다.");
		break;
	case NOT_OPENED:
		sprintf(erMsg, "[!] 서버가 열려있지 않습니다.");
		break;
	}

	cout << erMsg << endl << endl;
}

void printSuccessMessage(Menu m)
{
	char erMsg[BUFSIZE] = {'\0'};
	switch( m )
	{
	case SERVER_OPEN:
		sprintf(erMsg, "  <서버알림> 전송용 영상 캡쳐시작!\n  <서버알림> 클라이언트 접속대기시작!");
		break;
	case SERVER_CLOSE:
		sprintf(erMsg, "  <서버알림> 클라이언트 접속대기종료!\n  <서버알림> 전송용 영상 캡쳐종료!");
		break;
	}

	cout << erMsg << endl << endl;
}

Menu inputMenu()
{
	cout << "1. 서버 열기" << endl;
	cout << "2. 서버 닫기" << endl;
	cout << "3. 종료" << endl;
	cout << "------------------------------------" << endl;
	cout << ">> ";
	
	int input = 0;
	while( input < 1 || input > 3 )
	{
		cin >> input;
	}

	return (Menu)input;
}

DWORD WINAPI displayCamToClient(LPVOID arg)
{
	CClient *client;
	client = (CClient *)arg;
	CEncodeData **encodeData = client->getEncodeData();
	SOCKET client_socket = client->getSocket();
	SOCKADDR_IN clientaddr = client->getAddr();
	ServerState *state = client->getServerState();

	int sendImageID = (*encodeData)->getImageID();
	
	while((*state) == ST_SERVER_OPEN )
	{
		cvWaitKey(10);
		if( (*encodeData)->getImageID() < sendImageID )
		{
			continue;
		}
		CEncodeData newEncodeData;
		
		EnterCriticalSection(&cs);
		sendImageID = (*encodeData)->getImageID();
		
		newEncodeData.setDataLength((*encodeData)->getDataLength());
		newEncodeData.setDataType((*encodeData)->getDataType());

		char *buffer = new char[newEncodeData.getDataLength()+1];
		memcpy( buffer, (*encodeData)->getCaptureData(), newEncodeData.getDataLength());
		buffer[newEncodeData.getDataLength()] = '\0';
		LeaveCriticalSection(&cs);

		int len = 0;

		len = sizeof( newEncodeData.getImageID() );
		int retval = -1;
		
		if( (*state) == ST_SERVER_CLOSE )
		{
			break;
		}
		retval = send( client_socket, (char*)&(len), sizeof(int), 0 );

		if ( retval == -1 )
		{
			cout << "  <접속종료> IP 주소=" << inet_ntoa(clientaddr.sin_addr) << ", 포트 번호=" << ntohs(clientaddr.sin_port) << endl;
			cout << "           클라이언트에게 캡쳐 영상 보내기 중지" << endl;
			break;
		}

		int length = newEncodeData.getDataLength();
		if( (*state) == ST_SERVER_CLOSE )
		{
			break;
		}
		send( client_socket, (char*)&(length), len, 0);

		int type = newEncodeData.getDataType();
		len = sizeof( type );
		if( (*state) == ST_SERVER_CLOSE )
		{
			break;
		}
		send( client_socket, (char*)&(len), sizeof(int), 0 );
		if( (*state) == ST_SERVER_CLOSE )
		{
			break;
		}
		send( client_socket, (char*)&(type), len, 0);

		len = newEncodeData.getDataLength();
		if( (*state) == ST_SERVER_CLOSE )
		{
			break;
		}
		send( client_socket, (char*)&(len), sizeof(int), 0 );

		int index = 0;
		char sendBuffer[BUFSIZE] = {'\0'};
		while( index < len )
		{
			int sendCount = (len-index)>=(BUFSIZE-1)?(BUFSIZE-1):(len-index);
			memcpy( sendBuffer, buffer+index, sendCount);
			sendBuffer[sendCount] = '\0';
			if( (*state) == ST_SERVER_CLOSE )
			{
				break;
			}
			send(client_socket, sendBuffer, sendCount, 0);

			index += sendCount;
		}
		delete buffer;
	}
	closesocket(client_socket);
	ExitThread(0);
	return 0;
}

DWORD WINAPI waitClient(LPVOID arg)
{
	CSendData *sendData = (CSendData*)arg;

	SOCKET sock = sendData->getSocket();
	CEncodeData **encodeData = sendData->getEncodeData();
	ServerState *state = sendData->getServerState();

	while((*state) == ST_SERVER_OPEN )
	{
		SOCKET client_sock;
		SOCKADDR_IN clientaddr;
		// accept()
		int addrlen = sizeof(clientaddr);
		client_sock = accept(sock, (SOCKADDR *)&(clientaddr), &addrlen);

		if(client_sock == INVALID_SOCKET){
			continue;
		}
		CClient new_client(client_sock,encodeData,state);

		CreateThread(NULL, 0, displayCamToClient, (LPVOID)&new_client, 0, NULL);
		// 접속한 클라이언트 정보 출력
		cout << "  <접속정보> IP 주소=" << inet_ntoa(clientaddr.sin_addr) << ", 포트 번호=" << ntohs(clientaddr.sin_port) << endl;
		cout << "           클라이언트에게 캡쳐 영상 보내기 시작!" << endl;
	}

	ExitThread(0);
	return 0;
}

DWORD WINAPI displayCamImage(LPVOID arg)
{
	CCaptureData *captureData = (CCaptureData*)arg;
	CEncodeData **encodeData = captureData->getEncodeData();
	CvCapture *capture = captureData->getCapturePointer();
	ServerState *state = captureData->getServerState();
	 // 타이틀 이름 입력

	time_t startTime = time(NULL);
	int FPS = 0;
	while((*state) == ST_SERVER_OPEN)
	{
		IplImage *captureImage = cvQueryFrame(capture); //캡쳐한 이미지를 넣는다
		
		CvMat *eMat = cvEncodeImage(".jpg", captureImage);
		cvShowImage(SERVER_VIEW_WINDOW_NAME, captureImage);

		EnterCriticalSection(&cs);
		(*encodeData)->setCaptureData(eMat);
		(*encodeData)->setImageID((*encodeData)->getImageID()+1);
		LeaveCriticalSection(&cs);
		cvReleaseMat(&eMat);

		FPS++;
		time_t endTime = time(NULL);
		if( endTime != startTime )
		{
			//cout << "FPS : " << FPS << endl;
			FPS = 0;
			startTime = endTime;
		}
		
		cvWaitKey(10);
	}
	cvReleaseCapture(&capture);
	(*encodeData)->release();

	ExitThread(0);

	return 0;
}

Result serverOpen(SOCKET *sock, ServerState *state)
{
	if( (*state) != ST_SERVER_CLOSE )
	{
		return ALREADY_OPEN;
	}

	int retval; 

	// socket()
	(*sock) = socket(AF_INET, SOCK_STREAM, 0);
	if((*sock)== INVALID_SOCKET)
	{
		return SOCKET_FAIL;
	}

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);

	retval = bind((*sock), (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR)
	{
		return BIND_FAIL;
	}

	// listen()
	retval = listen((*sock), SOMAXCONN);
	if(retval == SOCKET_ERROR)
	{
		return LISTEN_FAIL;
	}

	return SUCCESS;
}

Result serverClose(ServerState *state)
{
	if( (*state) == ST_SERVER_CLOSE )
	{
		return NOT_OPENED;
	}

	return SUCCESS;
}

int main()
{
	WSADATA wsa; // 윈속 초기화
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
	{
		return 1;
	}

	HANDLE threadCaptureFrame = NULL;
	HANDLE threadWaitClient = NULL;

	CvCapture *capture = NULL;
	CEncodeData *encodeData = NULL;
	ServerState state = ST_SERVER_CLOSE;
	CCaptureData captureData;
	CSendData sendData;
	SOCKET sock;

	InitializeCriticalSection(&cs);

	while(1)
	{
		Menu menu = inputMenu();
		if( menu == EXIT )
		{
			break;
		}

		Result r;
		switch( menu )
		{
		case SERVER_OPEN:
			r = serverOpen(&sock, &state);
			if( r == SUCCESS )
			{
				(state) = ST_SERVER_OPEN;
				(capture) = CAPTURE_TARGET;
				(encodeData) = new CEncodeData();

				cvSetCaptureProperty((capture), CV_CAP_PROP_FRAME_WIDTH, CAPTURE_WIDTH);
				cvSetCaptureProperty((capture), CV_CAP_PROP_FRAME_HEIGHT, CAPTURE_HEIGHT);

				captureData = CCaptureData((capture), &encodeData, &state);
				threadCaptureFrame = CreateThread(NULL, 0, displayCamImage, (LPVOID)&captureData, 0, NULL);

				sendData = CSendData(sock, &encodeData, &state);
				threadWaitClient = CreateThread(NULL, 0, waitClient, (LPVOID)&sendData,0,NULL);
			}
			break;
		case SERVER_CLOSE:
			r = serverClose(&state);
			if( r == SUCCESS )
			{
				closesocket(sock);
				(state) = ST_SERVER_CLOSE;
			}
			break;
		}
			

		if( r == SUCCESS )
		{
			printSuccessMessage(menu);
		}
		else
		{
			printErrorMessage(r);
		}
	}
	DeleteCriticalSection(&cs);

	WSACleanup();

	return 0;
}

/*int main()
{
	CvCapture *capture; //캠에 대한 정보
	capture = cvCaptureFromFile("o.avi");//cvCaptureFromCAM(1);

	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, CAPTURE_WIDTH);
	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, CAPTURE_HEIGHT);

	CEncodeData *encodeData = new CEncodeData();
	CCaptureData captureData(capture, &encodeData);
	
	HANDLE hThread = CreateThread(NULL, 0, displayCamImage, (LPVOID)&captureData, 0, NULL);

	int retval; 

	WSADATA wsa; // 윈속 초기화
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
	{
		return 1;
	}

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock == INVALID_SOCKET)
	{
		err_quit("socket()");
	}

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);

	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR)
	{
		err_quit("bind()");
	}

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if(retval == SOCKET_ERROR)
	{
		err_quit("listen()");
	}

	cout << "<서버정보>  포트 : " << SERVERPORT << endl;
	cout << "----------------------------------" << endl;

	InitializeCriticalSection(&cs);
	while(1){
		cout << "클라이언트 접속대기..." << endl;

		SOCKET client_sock;
		SOCKADDR_IN clientaddr;
		// accept()
		int addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&(clientaddr), &addrlen);
		if(client_sock == INVALID_SOCKET){
			err_display("accept()");
			break;
		}
		CClient new_client(client_sock,&encodeData);
		HANDLE ClientThread = CreateThread(NULL, 0, displayCamToClient, (LPVOID)&new_client, 0, NULL);
		// 접속한 클라이언트 정보 출력
		cout << "<접속정보>  IP 주소=" << inet_ntoa(clientaddr.sin_addr) << ", 포트 번호=" << ntohs(clientaddr.sin_port) << endl;
	}
	DeleteCriticalSection(&cs);

	delete encodeData;

	return 0;
}*/