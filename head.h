#define IP "10.122.6.219"//����IP��ַ 
#define PORT 9999//�˿ں� 
#define QUEUELEN 5//������Ӹ��� 
#define MAXSIZE 65536//��������С 
#define HTTP "http://"
struct ProxySockets {
	SOCKET user_proxy; //���ػ�����PROXY �������socket
	SOCKET proxy_server; //PROXY �������Զ��������socket
	int IsUser_ProxyClosed; // ���ػ�����PROXY�����״̬
	int IsProxy_ServerClosed; // PROXY�������Զ������״̬
};

struct ProxyParam {
	char Address[256];    // Զ��������ַ
	HANDLE User_SvrOK;    // PROXY �������Զ������������״̬
	struct ProxySockets *pPair;    // ά��һ��SOCKET��ָ��
	int Port;         // ��������Զ�������Ķ˿�
};                   //����ṹ����PROXY SERVER��Զ����������Ϣ����.
SOCKET socket_listen;   //����������SOCKET��
