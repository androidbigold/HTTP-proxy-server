#include <winsock2.h>
#include <process.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define IP "10.122.6.219"//本机IP地址 
#define PORT 9156//端口号 
#define QUEUELEN 5//最多连接个数 
int socket_listen;//监听socket
struct sockaddr_in my_addr;//sockaddr_in结构
struct sockaddr_in their_addr;
HANDLE User_server;//子进程句柄
HANDLE Server_user;
int is_clientclosed=0;//客户端连接是否建立
int is_proxyclosed=0;//服务器端连接是否建立
unsigned _stdcall UserToProxyThread(void *pParam);
unsigned _stdcall ProxyToServerThread(void *pParam);
int start() {
	WORD sockVersion = MAKEWORD(2,2);//双字节数,16位 将两个字节合成一个word,前面是高位,后面是低位,2表示为二进制
	//是 00000010,2和2合在一起是0000001000000010,即十进制的514
	WSADATA wsaData;
	if(WSAStartup(sockVersion, &wsaData)!=0) {//根据版本通知操作系统,启用SOCKET的DLL库 ,版本2.2
		return 1;
	}
	socket_listen=socket(AF_INET,SOCK_STREAM,0);//创建socket
	if(socket_listen == INVALID_SOCKET) {
		WSACleanup();
		printf("socket error!\n");
		return 2;
	}
	my_addr.sin_family=AF_INET;
	my_addr.sin_port=htons(PORT);
	my_addr.sin_addr.s_addr=inet_addr(IP);
	memset(&(my_addr.sin_zero),0,8);//其余部分置零
	their_addr.sin_family=AF_INET;
	their_addr.sin_port=htons(80);
	their_addr.sin_addr.s_addr=INADDR_ANY;
	memset(&(their_addr.sin_zero),0,8);
	//填写sockaddr_in结构
	if(bind(socket_listen,(struct sockaddr *)&my_addr,sizeof(my_addr))==SOCKET_ERROR) {
		WSACleanup();
		printf("bind error! %d\n",WSAGetLastError());
		return 3;
	}
	if(listen(socket_listen,QUEUELEN)) {
		WSACleanup();
		printf("listen error! %d\n",WSAGetLastError());
		return 4;
	}
	User_server=(HANDLE)_beginthreadex(NULL,0,UserToProxyThread,NULL,0,NULL);//开始监听
	return 0;
}
int stop() {
	closesocket(socket_listen);
	WSACleanup();
	return 0;
}
unsigned _stdcall UserToProxyThread(void *pParam) {
	int receive,sent;
	char buffer[65536];
	int socket_message,socket_send;
	socket_message=accept(socket_listen,(struct sockaddr*)&their_addr,(int *)sizeof(their_addr)); 
	User_server=(HANDLE)_beginthreadex(NULL,0,UserToProxyThread,NULL,0,NULL);//开启另一个进程继续监听
	if(socket_message==INVALID_SOCKET) {
		printf("accept error! %d\n",WSAGetLastError());
		return 5;
	}
	receive=recv(socket_message,buffer,sizeof(buffer),0);
	if(receive==SOCKET_ERROR) {
		printf("receive error!\n");
		if(is_clientclosed==0) {
			closesocket(socket_message);
			is_clientclosed=1;
		}
		return 6;
	} else if(receive==0) {
		printf("Client close connection\n");
		if(is_clientclosed==0) {
			closesocket(socket_message);
			is_clientclosed=1;
		}
		return 7;
	}
	buffer[receive]='\0';
	printf("Received %d bytes,data [%s] from client\n",receive,buffer);
	Server_user=(HANDLE)_beginthreadex(NULL,0,ProxyToServerThread,NULL,0,NULL);
	WaitForSingleObject(User_server,60000);
	CloseHandle(User_server);
	while(is_clientclosed==0&&is_proxyclosed==0) {
		sent=send(socket_send,buffer,sizeof(buffer),0);
		if(sent==SOCKET_ERROR) {
			printf("send failed:error %d\n",WSAGetLastError());
			if(is_proxyclosed==0) {
				closesocket(socket_send);
				is_proxyclosed=1;
			}
			continue;
		}
		receive=recv(socket_message,buffer,sizeof(buffer),0);
		if(receive==SOCKET_ERROR) {
			printf("receive error!\n");
			if(is_clientclosed==0) {
				closesocket(socket_message);
				is_clientclosed=1;
			}
			continue;
		}
		if(receive==0) {
			printf("Client close connection\n");
			if(is_clientclosed==0) {
				closesocket(socket_message);
				is_clientclosed=1;
			}
			break;
		}
		buffer[receive]='\0';
		printf("Received %d bytes,data [%s] from client\n",receive,buffer);
	}
	if(is_proxyclosed==0) {
		closesocket(socket_send);
		is_proxyclosed=1;
	}
	if(is_clientclosed==0) {
		closesocket(socket_message);
		is_clientclosed=1;
	}
	return 0;
}
unsigned _stdcall ProxyToServerThread(void *pParam) {
	char Buffer[65536];
	unsigned long address;
	unsigned short port;
	char *server_name;
	int socket_connect,socket_send;
	int receive,sent;
	struct sockaddr_in server;
	struct hostent *hp;
	server_name=inet_ntoa(their_addr.sin_addr);
	port=their_addr.sin_port;
	if(isalpha(server_name[0]))
		hp=gethostbyname(server_name);
	else {
		address=inet_addr(server_name);
		hp=gethostbyaddr((char *)&address,4,AF_INET);
	}
	if(hp==NULL) {
		fprintf(stderr,"Client:Cannot resolve address [%s] :Error %d\n",server_name,WSAGetLastError());
		SetEvent(User_server);
		return 1;
	}
	memset(&server,0,sizeof(server));
	memcpy(&(server.sin_addr),hp->h_addr,hp->h_length);
	server.sin_family=hp->h_addrtype;
	server.sin_port=htons(port);
	socket_connect=socket(AF_INET,SOCK_STREAM,0);
	if(socket_connect<0) {
		fprintf(stderr,"Client:Error opening socket: Error %d\n",WSAGetLastError());
		is_proxyclosed=1;
		SetEvent(User_server);
		return -1;
	}
	printf("Client connecting to: %s\n",hp->h_name);
	if(connect(socket_connect,(struct sockaddr *)&server,sizeof(server))==SOCKET_ERROR) {
		fprintf(stderr,"connect failed: %d\n",WSAGetLastError());
		is_proxyclosed=1;
		SetEvent(User_server);
		return -1;
	}
	is_proxyclosed=0;
	while(is_clientclosed==0&&is_proxyclosed==0) {
		receive=recv(socket_connect,Buffer,sizeof(Buffer),0);
		if(receive==SOCKET_ERROR) {
			fprintf(stderr,"receive error %d\n",WSAGetLastError());
			closesocket(socket_connect);
			is_proxyclosed=1;
			break;
		}
		if(receive==0) {
			printf("Server closed connection\n");
			closesocket(socket_connect);
			is_proxyclosed=1;
			break;
		}
		sent=send(socket_send,Buffer,sizeof(Buffer),0);
		if(sent==SOCKET_ERROR) {
			fprintf(stderr,"send failed: error %d\n",WSAGetLastError());
			closesocket(socket_send);
			is_clientclosed=1;
			break;
		}
		Buffer[receive]='\0';
		printf("Received %d bytes,data [%s] from server\n",receive,Buffer);
	}
	if(is_proxyclosed==0) {
		closesocket(socket_connect);
		is_proxyclosed=1;
	}
	if(is_clientclosed==0) {
		closesocket(socket_send);
		is_clientclosed=1;
	}
	return 0;
}
int main(int argc,char *argv[]) {
	start();
	while(1)
		if(getchar()=='q')
			break;
	stop();
	return 0;
}
