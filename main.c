#include <winsock2.h>
#include <process.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "head.h"
int flag;//�Ƿ��ں�������
HANDLE User_server;//�������̾�� 
unsigned _stdcall UserToProxyThread(void *pParam);
unsigned _stdcall ProxyToServerThread(void *pParam);
int GetAddressAndPort(char *str,char *address,int *port) {    //������ķ���
	char buffer[MAXSIZE],command[MAXSIZE],proto[MAXSIZE],*p;
	int j,i;
	sscanf(str,"%s%s%s",command,buffer,proto);  //��str��ȡָ����ʽ��������ݵ�command,buffer,proto
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
			*(p+j)=' ';  //ȥ��Զ��������: GET http://www.njust.edu.cn/ HTTP1.1  == > GET / HTTP1.1
		*port=80;      //ȱʡ�� http �˿�
	}
	return 0;
}
int start() {
	struct sockaddr_in local;//���ص�ַ�ṹ 
	WORD sockVersion = MAKEWORD(2,2);//˫�ֽ���,16λ �������ֽںϳ�һ��word,ǰ���Ǹ�λ,�����ǵ�λ,2��ʾΪ������
	//�� 00000010,2��2����һ����0000001000000010,��ʮ���Ƶ�514
	WSADATA wsaData;
	if(WSAStartup(sockVersion, &wsaData)!=0) {//���ݰ汾֪ͨ����ϵͳ,����SOCKET��DLL�� ,�汾2.2
		return -1;
	}
	socket_listen=socket(AF_INET,SOCK_STREAM,0);//����socket
	if(socket_listen == INVALID_SOCKET) {
		WSACleanup();
		printf("socket error!\n");
		return -1;
	}
	local.sin_family=AF_INET;
	local.sin_port=htons(PORT);
	local.sin_addr.s_addr=inet_addr(IP);
	memset(&(local.sin_zero),0,8);//���ಿ������
	//��дsockaddr_in�ṹ
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
	User_server=(HANDLE)_beginthreadex(NULL,0,UserToProxyThread,NULL,0,NULL);//��ʼ����
	return 0;
}
int stop() {
	closesocket(socket_listen);//�رռ����׽��� 
	WSACleanup();
	return 0;
}
unsigned _stdcall UserToProxyThread(void *pParam) {
	int receive,sent,len;//���գ����͵���Ϣ���� 
	int remotelen;//��ַ�ṹ�峤�� 
	char buffer[MAXSIZE];//������ 
	struct ProxySockets PS;
	struct ProxyParam PP;
	struct sockaddr_in remote_addr;//�����ַ�ṹ�� 
	SOCKET socket_message;//����������������ͨ�ŵ��׽��� 
	HANDLE Server_user;//����������������ͨ�Ž��� 
	flag=0;
	printf("wait for a link\n");
	remotelen=sizeof(remote_addr);
	socket_message=accept(socket_listen,(struct sockaddr*)&remote_addr,&remotelen);
	User_server=(HANDLE)_beginthreadex(NULL,0,UserToProxyThread,NULL,0,NULL);//������һ�����̼�������
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
	struct ProxyParam *PParam=(struct ProxyParam *)pParam;//���洫��Ĳ��� 
	char *server_name;//�����ַ 
	char Buffer[MAXSIZE];//������ 
	int receive,sent,len;//���գ����͵���Ϣ���� 
	unsigned long addr;//IP��ַ 
	unsigned short port;//�˿� 
	struct hostent *hp;//��¼������Ϣ�Ľṹ�� 
	SOCKET socket_connect;//����������������ͨ�ŵ��׽��� 
	struct sockaddr_in server;//��������ַ�ṹ�� 
	FILE *fp;//�ļ�ָ�� 
	int number;//������Ϣ���� 
	char buf[20];//������ 
	char host[256];// ������ 
	server_name=PParam->Address;
	port=PParam->Port;

	if(isalpha(server_name[0])) { //���server_name��һ������
		hp=gethostbyname(server_name);//���ر�����IP��ַ
	} else { //���server_name��һ��IP��ַ
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

	fp=fopen("������.txt","r");
	while(fgets(host,sizeof(host),fp)!=NULL) {
		if(strcmp(host,hp->h_name)==0) {
			flag=1;
			break;
		}
	}
	fclose(fp);

	while(PParam->pPair->IsUser_ProxyClosed==0&&PParam->pPair->IsProxy_ServerClosed==0) {
		if(flag==1) {
			fp=fopen("����ҳ��.html","r");
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
