#define IP "10.122.6.219"//本机IP地址 
#define PORT 9999//端口号 
#define QUEUELEN 5//最多连接个数 
#define MAXSIZE 65536//缓冲区大小 
#define HTTP "http://"
struct ProxySockets {
	SOCKET user_proxy; //本地机器到PROXY 服务机的socket
	SOCKET proxy_server; //PROXY 服务机到远程主机的socket
	int IsUser_ProxyClosed; // 本地机器到PROXY服务机状态
	int IsProxy_ServerClosed; // PROXY服务机到远程主机状态
};

struct ProxyParam {
	char Address[256];    // 远程主机地址
	HANDLE User_SvrOK;    // PROXY 服务机到远程主机的联结状态
	struct ProxySockets *pPair;    // 维护一组SOCKET的指针
	int Port;         // 用来联结远程主机的端口
};                   //这个结构用来PROXY SERVER与远程主机的信息交换.
SOCKET socket_listen;   //用来监听的SOCKET。
