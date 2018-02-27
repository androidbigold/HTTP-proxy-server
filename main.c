#include <winsock2.h>
#include <process.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "head.h"
int flag;//是否在黑名单中
HANDLE User_server;//监听进程句柄 
unsigned _stdcall UserToProxyThread(void *pParam);
unsigned _stdcall ProxyToServerThread(void *pParam);
int GetAddressAndPort(char *str,char *address,int *port) {    //对请求的分析
	char buffer[MAXSIZE],command[MAXSIZE],proto[MAXSIZE],*p;
	int j,i;
	sscanf(str,"%s%s%s",command,buffer,proto);  //从str读取指定格式相符的数据到command,buffer,proto
	p=strstr(buffer,HTTP);
	//HTTP
	if(p) {
		p+=strlen(HTTP);
		for(i=0; i< strlen(p); i++)
			if( *(p+i)=='/') break;
		*(p+i)='\0';
		strcpy(address,p);
		p=strstr(str,HTTP);
		for(j=0; j< i+strlen(HTTP); j++)
			*(p+j)=' ';  //去掉远程主机名: GET http://www.njust.edu.cn/ HTTP1.1  == > GET / HTTP1.1
		*port=80;      //缺省的 http 端口
	}
	return 0;
}
int start() {
	struct sockaddr_in local;//本地地址结构 
	WORD sockVersion = MAKEWORD(2,2);//双字节数,16位 将两个字节合成一个word,前面是高位,后面是低位,2表示为二进制
	//是 00000010,2和2合在一起是0000001000000010,即十进制的514
	WSADATA wsaData;
	if(WSAStartup(sockVersion, &wsaData)!=0) {//根据版本通知操作系统,启用SOCKET的DLL库 ,版本2.2
		return -1;
	}
	socket_listen=socket(AF_INET,SOCK_STREAM,0);//创建socket
	if(socket_listen == INVALID_SOCKET) {
		WSACleanup();
		printf("socket error!\n");
		return -1;
	}
	local.sin_family=AF_INET;
	local.sin_port=htons(PORT);
	local.sin_addr.s_addr=inet_addr(IP);
	memset(&(local.sin_zero),0,8);//其余部分置零
	//填写sockaddr_in结构
	if(bind(socket_listen,(struct sockaddr *)&local,sizeof(local))==SOCKET_ERROR) {
		printf("bind error! %d\n",WSAGetLastError());
		WSACleanup();
		return -1;
	}
	if(listen(socket_listen,QUEUELEN)==SOCKET_ERROR) {
		printf("listen error! %d\n",WSAGetLastError());
		WSACleanup();
		return -1;
	}
	User_server=(HANDLE)_beginthreadex(NULL,0,UserToProxyThread,NULL,0,NULL);//开始监听
	return 0;
}
int stop() {
	closesocket(socket_listen);//关闭监听套接字 
	WSACleanup();
	return 0;
}
unsigned _stdcall UserToProxyThread(void *pParam) {
	int receive,sent,len;//接收，发送的消息长度 
	int remotelen;//地址结构体长度 
	char buffer[MAXSIZE];//缓冲区 
	struct ProxySockets PS;
	struct ProxyParam PP;
	struct sockaddr_in remote_addr;//请求地址结构体 
	SOCKET socket_message;//代理服务器与浏览器通信的套接字 
	HANDLE Server_user;//服务器与代理服务器通信进程 
	flag=0;
	printf("wait for a link\n");
	remotelen=sizeof(remote_addr);
	socket_message=accept(socket_listen,(struct sockaddr*)&remote_addr,&remotelen);
	User_server=(HANDLE)_beginthreadex(NULL,0,UserToProxyThread,NULL,0,NULL);//开启另一个进程继续监听
	if(socket_message==INVALID_SOCKET) {
		printf("accept error! %d\n",WSAGetLastError());
		return -1;
	}

	PS.IsUser_ProxyClosed=0;
	PS.IsProxy_ServerClosed=1;
	PS.user_proxy=socket_message;
	receive=recv(socket_message,buffer,sizeof(buffer),0);
	if(receive==SOCKET_ERROR) {
		printf("receive error!\n");
		if(PS.IsUser_ProxyClosed==0) {
			closesocket(PS.user_proxy);
			PS.IsUser_ProxyClosed=1;
		}
	}
	len=receive;
	if(receive==0) {
		printf("Client close connection\n");
		if(PS.IsUser_ProxyClosed==0) {
			closesocket(PS.user_proxy);
			PS.IsUser_ProxyClosed=1;
		}
	}
	buffer[len]='\0';
	printf("Received %d bytes,data [%s] from client\n",receive,buffer);

	PS.IsUser_ProxyClosed=0;
	PS.IsProxy_ServerClosed=1;
	PS.user_proxy=socket_message;
	PP.pPair=&PS;
	PP.User_SvrOK=CreateEvent(NULL,TRUE,FALSE,NULL);

	GetAddressAndPort(buffer,PP.Address,&PP.Port);
	Server_user=(HANDLE)_beginthreadex(NULL,0,ProxyToServerThread,&PP,0,NULL);
	WaitForSingleObject(PP.User_SvrOK,60000);
	CloseHandle(PP.User_SvrOK);

	WaitForSingleObject(Server_user,1000);

	while(PS.IsUser_ProxyClosed==0&&PS.IsProxy_ServerClosed==0) {
		sent=send(PS.proxy_server,buffer,len,0);
		if(sent==SOCKET_ERROR) {
			printf("send failed:error %d\n",WSAGetLastError());
			if(PS.IsProxy_ServerClosed==0) {
				closesocket(PS.proxy_server);
				PS.IsProxy_ServerClosed=1;
			}
			continue;
		}

		receive=recv(PS.user_proxy,buffer,sizeof(buffer),0);
		if(receive==SOCKET_ERROR) {
			printf("receive error!\n");
			if(PS.IsUser_ProxyClosed==0) {
				closesocket(PS.user_proxy);
				PS.user_proxy=1;
			}
			continue;
		}
		len=receive;
		if(receive==0) {
			printf("Client close connection\n");
			if(PS.IsUser_ProxyClosed==0) {
				closesocket(PS.user_proxy);
				PS.IsUser_ProxyClosed=1;
			}
			break;
		}
		buffer[len]='\0';
	}
	if(PS.IsProxy_ServerClosed==0) {
		closesocket(PS.proxy_server);
		PS.IsProxy_ServerClosed=1;
	}
	if(PS.IsUser_ProxyClosed==0) {
		closesocket(PS.user_proxy);
		PS.IsUser_ProxyClosed=1;
	}
	WaitForSingleObject(Server_user,20000);
	return 0;
}
unsigned _stdcall ProxyToServerThread(void *pParam) {
	struct ProxyParam *PParam=(struct ProxyParam *)pParam;//保存传入的参数 
	char *server_name;//请求地址 
	char Buffer[MAXSIZE];//缓冲区 
	int receive,sent,len;//接收，发送的消息长度 
	unsigned long addr;//IP地址 
	unsigned short port;//端口 
	struct hostent *hp;//记录主机信息的结构体 
	SOCKET socket_connect;//服务器与代理服务器通信的套接字 
	struct sockaddr_in server;//服务器地址结构体 
	FILE *fp;//文件指针 
	int number;//发送消息长度 
	char buf[20];//缓冲区 
	char host[256];// 主机名 
	server_name=PParam->Address;
	port=PParam->Port;

	if(isalpha(server_name[0])) { //如果server_name是一个域名
		hp=gethostbyname(server_name);//返回别名和IP地址
	} else { //如果server_name是一个IP地址
		addr=inet_addr(server_name);
		hp=gethostbyaddr((char *)&addr,4,AF_INET);
	}
	if (hp == NULL) {
		fprintf(stderr,"Client: Cannot resolve address [%s]: Error %d\n",
		        server_name,WSAGetLastError());
		SetEvent(PParam->User_SvrOK);
		return -1;
	}
	memset(&server,0,sizeof(server));
	memcpy(&(server.sin_addr),hp->h_addr,hp->h_length);
	server.sin_family = hp->h_addrtype;
	server.sin_port = htons(port);

	socket_connect=socket(AF_INET,SOCK_STREAM,0);
	if (socket_connect<0) {
		fprintf(stderr,"Client: Error Opening socket: Error %d\n",
		        WSAGetLastError());
		PParam->pPair->IsProxy_ServerClosed=TRUE;
		SetEvent(PParam->User_SvrOK);
		return -1;
	}

	printf("Client connecting to: %s\n",hp->h_name);
	if(connect(socket_connect,(struct sockaddr *)&server,sizeof(server))==SOCKET_ERROR) {
		fprintf(stderr,"connect failed: %d\n",WSAGetLastError());
		PParam->pPair->IsProxy_ServerClosed=1;
		SetEvent(PParam->User_SvrOK);
		return -1;
	}
	PParam->pPair->proxy_server=socket_connect;
	PParam->pPair->IsProxy_ServerClosed=0;
	SetEvent(PParam->User_SvrOK);

	fp=fopen("黑名单.txt","r");
	while(fgets(host,sizeof(host),fp)!=NULL) {
		if(strcmp(host,hp->h_name)==0) {
			flag=1;
			break;
		}
	}
	fclose(fp);

	while(PParam->pPair->IsUser_ProxyClosed==0&&PParam->pPair->IsProxy_ServerClosed==0) {
		if(flag==1) {
			fp=fopen("警告页面.html","r");
			number=fread(buf,sizeof(char),sizeof(buf),fp);
			while(number==sizeof(buf)) {
				sent=send(PParam->pPair->user_proxy,buf,number,0);
				if(sent==SOCKET_ERROR) {
					fprintf(stderr,"send failed: error %d\n",WSAGetLastError());
					closesocket(PParam->pPair->user_proxy);
					PParam->pPair->IsUser_ProxyClosed=1;
					continue;
				}
				number=fread(buf,sizeof(char),sizeof(buf),fp);
			}
			sent=send(PParam->pPair->user_proxy,buf,number,0);
			fclose(fp);
			break;
		} else {
			receive=recv(socket_connect,Buffer,sizeof(Buffer),0);
			if(receive==SOCKET_ERROR) {
				fprintf(stderr,"receive error! %d\n",WSAGetLastError());
				closesocket(socket_connect);
				PParam->pPair->IsProxy_ServerClosed=1;
				continue;
			}
			len=receive;
			if(receive==0) {
				printf("Server closed connection\n");
				closesocket(socket_connect);
				PParam->pPair->IsProxy_ServerClosed=1;
				break;
			}

			sent=send(PParam->pPair->user_proxy,Buffer,len,0);
			if(sent==SOCKET_ERROR) {
				fprintf(stderr,"send failed: error %d\n",WSAGetLastError());
				closesocket(PParam->pPair->user_proxy);
				PParam->pPair->IsUser_ProxyClosed=1;
				continue;
			}
			Buffer[len]='\0';
		}
	}

	if(PParam->pPair->IsProxy_ServerClosed==0) {
		closesocket(PParam->pPair->proxy_server);
		PParam->pPair->IsProxy_ServerClosed=1;
	}
	if(PParam->pPair->IsUser_ProxyClosed==0) {
		closesocket(PParam->pPair->user_proxy);
		PParam->pPair->IsUser_ProxyClosed=1;
	}
	return 0;
}
int main(int argc,char *argv[]) {
	start();
	while(1)
		if(getchar()=='q'||getchar()=='Q')
			break;
	stop();
	return 0;
}
