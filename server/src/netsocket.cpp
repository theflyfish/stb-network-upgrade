#include <stdio.h>
#include <Winsock2.h>
#include <conio.h>
#include <process.h>
#include <stddef.h>
#include "crc32.h"
#include "des.h"
#include "netsocket.h"

#include <windows.h>
#include <windowsx.h>
#pragma comment(lib, "ws2_32.lib")

extern char OutputStr[OUTBUF_SIZE];
extern HWND hwnd;  
extern HWND hWndEdit;
char Version_File_Name[]=STB_VERSION_FILE;
char FileName[]=FILE_NAME;
char DESFileName[]=DES_FILE_NAME;
int  ReadFileAble=TRUE;                  
char keyStr[8]={'i','n','s','m','e','d','i','a'};    /*DES加密解密密钥insmedia*/
char UpdateId[4]={'u','p','i','d'};                  /*STB发送的升级消息标识*/
char CloseId[5]= {'c','l','o','s','e'};              /*STB发送的关闭连接消息标识*/
char ReSendId[6]={'r','e','s','e','n','d'};          /*STB发送的重传消息标识*/
char Ver_Buf[20];
char Print_Buf[100];




/******************Socket_Init***********************************
函数功能：服务器端socket的建立与端口绑定
输入：    无
输出：   
	 sockSrv 建立成功后返回的套接字号
	 -1      winsock服务失败
	         套接字创建失败
			 端口绑定失败
****************************************************************/
SOCKET Socket_Init(void)
{
 WORD wVersionRequested;
 WSADATA wsaData;
 int err;
 SOCKADDR_IN addrSrv;
 wVersionRequested = MAKEWORD( 1, 1 );
 
 err = WSAStartup( wVersionRequested, &wsaData );
 if ( err != 0 ) 
 {
     return -1;
 }
 if ( LOBYTE( wsaData.wVersion ) != 1 ||
        HIBYTE( wsaData.wVersion ) != 1 )
 {
    WSACleanup( );
    return -1;
 }
 LogDisplay("创建服务器socket连接！\r\n");
 SOCKET sockSrv=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP );
 if(sockSrv<0)
 {
  LogDisplay("服务器socket连接创建失败！\r\n");
  return -1;
 }

 addrSrv.sin_addr.S_un.S_addr=htonl(INADDR_ANY);                                   //ip地址为10.30.100.250
 addrSrv.sin_family=AF_INET;
 addrSrv.sin_port=htons(6000);

 if(bind(sockSrv,(SOCKADDR*)&addrSrv,sizeof(SOCKADDR_IN))!=0)                      // 绑定端口
 {
	sprintf(Print_Buf,"服务器端口绑定失败!%d\r\n", WSAGetLastError());
	LogDisplay(Print_Buf);
  return -1;
 }
  int nNetTimeout=1000;                                                            //1秒
  setsockopt(sockSrv,IPPROTO_TCP,SO_SNDTIMEO,(char *)&nNetTimeout,sizeof(int));    //yx设置发送超时时间
  return sockSrv;
}

/******************File_Crc32***********************************
函数功能：文件读入buffer并CRC32校验
参数：
	pfilebuf:	缓冲区（返回时包含了crc）；
	filelen：	返回文件的原始长度（不包括CRC）；
	fp:			文件指针;
	File_Name:	文件名指针；
返回值：   
    CRC32    crc32校验码;
	-1:      升级文件打开失败；
	         参数错误；
****************************************************************/

unsigned long File_Crc32( unsigned char*pfilebuf, unsigned long *filelen,FILE *fp,char*File_Name)
 {
   int file_block_length=0;
   unsigned long CRC32=0;
   unsigned char*temp=NULL;
   unsigned long file_length=0;
   /*参数容错*/
   if(pfilebuf==NULL||File_Name==NULL||filelen==NULL)               
   {
	   	LogDisplay("File_Crc32:parameter error!\r\n");
		return -1;
   }
   temp=pfilebuf;
   *filelen=0;
   
	if((fp=fopen(File_Name,"rb")) == NULL)
	{
		LogDisplay("升级文件打开失败！\r\n");
		return -1;
	}
	while(!feof(fp))
	{
	    /*每次读BUFFER_SIZE个字节，并返回成功读取的字节数*/
		if((file_block_length = fread(temp,sizeof(char),BUF_SIZE,fp))>0)
		{
			temp+=file_block_length;
			file_length+=file_block_length;	   
		}
	}
    fclose(fp);
	
	sprintf(Print_Buf,"File:\t%s Read In Buffer Finished!\r\n",File_Name);
	LogDisplay(Print_Buf);
	sprintf(Print_Buf,"File Length is %d\r\n",file_length);
	LogDisplay(Print_Buf);
    LogDisplay("添加CRC校验\r\n");
    CRC32=CRC_32(pfilebuf,file_length);

	*(pfilebuf+file_length)=  (CRC32&0xff000000)>>24;
    *(pfilebuf+file_length+1)=(CRC32&0x00ff0000)>>16;
	*(pfilebuf+file_length+2)=(CRC32&0x0000ff00)>>8;
	*(pfilebuf+file_length+3)=(CRC32&0x000000ff);
    *filelen=file_length;		
	return CRC32;
 }

void Send_Delay()
{
		int ii,jj;                 //发送延时，根据实际情况修改
		for(ii=0;ii<100;ii++)
			for(jj=0;jj<100;jj++);
}
/**************缓冲区文件发送*****************/
int FileBuf_Send(unsigned char* Filebuf, unsigned long filelen,SOCKET sockConn)
 {
   int send_length=0,send_num=0,len=0;
   unsigned char*temp=NULL;
   temp=Filebuf;
   while(1)
   {
    len=(filelen+4)-send_num;  //剩余未发送数据长度
	if(len>BUF_SIZE)
	{
	
		if((send_length=send(sockConn,(char*)temp,BUF_SIZE,0))<0)
		{
		    sprintf(Print_Buf,"send error:%d\r\n", WSAGetLastError());
			LogDisplay(Print_Buf);
			return -1;
		} 
		temp+=send_length;
		send_num+=send_length;
        Send_Delay();
    }
	else
	{
		if(len>(BUF_SIZE-4))
			LogDisplay("尾部溢出！！！\r\n");
		if((send_length=send(sockConn,(char*)temp,len,0))<0)
		{
			return -1;
		} 
		temp+=send_length;
		send_num+=send_length;
        Send_Delay();
		if((send_length=send(sockConn,"end",strlen("end")+1,0))<0)
		{
			return -1;
		} 	
		sprintf(Print_Buf,"send_num = %d\r\n",send_num);
		LogDisplay(Print_Buf);
		 break;
       
	}

 }
   return 1;
 }


int ReadVersion(char *Ver_Buf,char*Version_File)
{
	FILE * fpVersion=NULL;
	int read_len=0;
	int file_len=0;
	char *temp=Ver_Buf;
	if((fpVersion= fopen(Version_File,"rb")) == NULL)
	 {
		LogDisplay("文件版本数据打开失败！\r\n");
		return -1;
	}
	while(!feof(fpVersion))
	{
	    /*每次读BUFFER_SIZE个字节，并返回成功读取的字节数*/
		if((read_len = fread(temp,sizeof(char),10,fpVersion))>0)
		{
			temp+=read_len;
			file_len+=read_len;	   
		}
	}
	memset(Print_Buf,0,sizeof(Print_Buf));                      
	sprintf(Print_Buf,"update version: %s \r\n",Ver_Buf);
	LogDisplay(Print_Buf);
	return file_len;
 
}

/**********接收机顶盒消息，没有while循环只接受一次，30秒等待************/
int Receive_StbInfo(SOCKET sockConn)
{
  char recvBuf[50];
  int  recv_len=0;
  fd_set fds;
  struct timeval time_out;
  time_out.tv_sec=30;
  time_out.tv_usec=0;
  FD_ZERO(&fds);
  FD_SET(sockConn,&fds);
  if(select(sockConn+1,&fds,NULL,NULL,&time_out)>0) //判断SOCKET是否可读 30秒阻塞时间
  {
	  if(FD_ISSET(sockConn,&fds)>0)
	  {
		 memset(recvBuf,0,50);
		 recv_len=0;
		 if((recv_len=recv(sockConn,recvBuf,50,0))<0) // 接受客户端消息
			return -1;
		 if(recv_len==sizeof(UpdateId))
		 {
			if(recvBuf[0]=='u'&&recvBuf[1]=='p'&&recvBuf[2]=='i'&&recvBuf[3]=='d')
			{
				sprintf(Print_Buf,"接收机顶盒信息：%s\r\n",recvBuf);
				LogDisplay(Print_Buf);
				return UPDATE_ID;
			}
			sprintf(Print_Buf,"receive data：%s\r\n",recvBuf);
			LogDisplay(Print_Buf);
		 }
		 if(recv_len==sizeof(CloseId))
		 {
			if(recvBuf[0]=='c'&&recvBuf[1]=='l'&&recvBuf[2]=='o'&&recvBuf[3]=='s'&&recvBuf[4]=='e')
			{
				sprintf(Print_Buf,"接收机顶盒信息：%s\r\n",recvBuf);
			    LogDisplay(Print_Buf);
			    return CLOSED_ID;
			}
	        sprintf(Print_Buf,"receive data：%s\r\n",recvBuf);
		    LogDisplay(Print_Buf);
		 }
	    if(recv_len==sizeof(ReSendId))
		{
		  if(recvBuf[0]=='r'&&recvBuf[1]=='e'&&recvBuf[2]=='s'&&recvBuf[3]=='e'&&recvBuf[4]=='n'&&recvBuf[5]=='d')
		  {
			 sprintf(Print_Buf,"接收机顶盒信息：%s\r\n",recvBuf);
			 LogDisplay(Print_Buf);
			 return RESEND_ID;
		  }
	      sprintf(Print_Buf,"receive data：%s\r\n",recvBuf);
		  LogDisplay(Print_Buf);
		}
	}
  }  
  return -1; // select错误或select超时或数据接收错误
}

/******************ReadFileProcess*******************************
函数功能:读取文件进程,读取文件版本，
         读取文件,(文件加密)，加入CRC校验
函数调用:ReadVersion();File_Crc32()

参数：
	ver_len:	版本号数据长度(输出)
	pfile_buf	存储升级数据的缓冲区指针(输出)
	file_len:	升级数据长度(输出)
	fpReadIn:	文件指针(输入)
返回值：   
      1：升级文件读入成功
	 -1：文件版本读入错误
	     File_Crc32 error	     
****************************************************************/

int ReadFileProcess(int *ver_len, unsigned char*pfile_buf,  unsigned long *file_len,FILE * fpReadIn )
{

    LogDisplay("读入文件版本\r\n");
    if((*ver_len=ReadVersion(Ver_Buf,Version_File_Name))<0)
      return -1;

    #if 0

    /*文件DES加密后输出*/
    LogDisplay("对升级文件进行加密...\r\n");
    if(DES_Encrypt(FileName,keyStr,DESFileName)<0)
	{
	    sprintf(Print_Buf,"File:\t%s DES Encrypt Failed!\r\n",FileName);
	    LogDisplay(Print_Buf);
	    return -1;
	}
    sprintf(Print_Buf,"File: %s DES EncryptSucceed!\r\n",FileName);
    LogDisplay(Print_Buf);

    /*加密文件读入并加入CRC校验*/
    LogDisplay("加密文件添加CRC校验...\r\n");
    #endif

    LogDisplay("读入升级文件...\r\n");
    memset(pfile_buf,0,FLIE_BUFFER_SIZE);
	/*文件读入并CRC32校验*/
    if( File_Crc32( pfile_buf, file_len,fpReadIn,FileName)<0)
	{
        return -1;
	}
    return 1;
}

/******************QuerySocketReadble*******************************
函数功能:查询套接字是否可读，若可读，函数返回；否则直到超时，返回
         本函数的主要功能是使socket读取变为非阻塞，可外部干预的。

参数：
	ver_len:	版本号数据长度(输出)
	pfile_buf	存储升级数据的缓冲区指针(输出)
	file_len:	升级数据长度(输出)
	fpReadIn:	文件指针(输入)
返回值：   
      1：升级文件读入成功
	 -1：文件版本读入错误
	     File_Crc32 error
	     
****************************************************************/
int QuerySocketReadble( SOCKET sockSrv,int *ver_len, unsigned char*pfile_buf,  unsigned long *file_len,FILE * fpReadIn)
{
 fd_set fds;
 struct timeval timeout;
 timeout.tv_sec=0;
 timeout.tv_usec=0;
 while(1)
	 {
		FD_ZERO(&fds);
		FD_SET(sockSrv,&fds);
		if(select(sockSrv+1,&fds,NULL,NULL,&timeout)>0) //判断SOCKET是否可读
			if(FD_ISSET(sockSrv,&fds)>0)
					break;
		if(ReadFileAble==READFILE) 
		{
			LogDisplay("更新升级文件...\r\n");
			memset(Ver_Buf,0,*ver_len);                                    //更新版本号前，清空缓冲区；
			if(ReadFileProcess( ver_len, pfile_buf, file_len,fpReadIn )<0)
				return -1;
			LogDisplay("等待STB连接!waiting...\r\n\r\n");
			ReadFileAble=WAITACCEPT;
		}
		if(ReadFileAble==KILLTHREAD)
		{
			return -1;
		}
	 }	
	 return 1;
}

int FileSend(unsigned char* pfile_buf, unsigned long file_len,SOCKET sockConn)
{
	char ReSendNum=0;
	char RET=0;
	while(ReSendNum<3)
	{
		LogDisplay("发送升级数据...\r\n");
		if(FileBuf_Send(pfile_buf,file_len,sockConn)<0)
		{
			sprintf(Print_Buf,"File_Send:\t%s Failed!\r\n", FileName);
			LogDisplay(Print_Buf);
			LogDisplay("close current connect!\r\n");
			return -1;
		}
		sprintf(Print_Buf,"升级文件: %s 发送完毕!\r\n",FileName);
		LogDisplay(Print_Buf);
		LogDisplay("等待STB确认信息\r\n");
		RET=0;
		RET=Receive_StbInfo(sockConn);//接受STB消息
		if(RET==RESEND_ID)
 		{
			LogDisplay("机顶盒端要求数据重传!\r\n");
			ReSendNum++;
            continue;
		}
		if((ReSendNum>=2)&&(RET==CLOSED_ID))
		{
			LogDisplay("重传3次数据校验有误!\r\n");
			break;
		}
		break;
	}
	if(ReSendNum==3)
	{
		LogDisplay("重传3次数据发送失败!\r\n");
		return -1;
	}
	return RET;
}

void SeverSocket(void) 
{
 SOCKET sockSrv;                                   //服务器套接字号
 SOCKADDR_IN addrClient;                           //连接上的客户端ip地址
 int len=sizeof(SOCKADDR);                         //SOCKADDR结构体大小
 int flag=0;                                       //存储接收到的STB消息类型
 FILE * fpReadIn=NULL;                             // 文件指针
 unsigned char sendbuffer[BUF_SIZE];               //发送缓冲区
 unsigned char*pfile_buf=NULL;                     //存储文件的缓冲区指针
 unsigned long file_len=0;                         //文件长度
 int send_len=0;                                   //发送的数据长度
 int ver_len=0;                                    //版本数据的长度（最大19个字节）


 ReadFileAble=READFILE;                                                   //文件标识置为“正在读入”
 pfile_buf = ( unsigned char *) malloc (FLIE_BUFFER_SIZE);
 if(pfile_buf==NULL)
 {
    LogDisplay("内存分配失败！\r\n");
	return;
 }
 if(ReadFileProcess( &ver_len, pfile_buf, &file_len,fpReadIn )<0)
	return;
 sockSrv=Socket_Init();                      //服务器sokcet连接初始化
 LogDisplay("监听STB连接请求\r\n\r\n");
 if(( listen(sockSrv,20))!=0)
 {
	sprintf(Print_Buf,"监听失败!%d\r\n", WSAGetLastError());
	LogDisplay(Print_Buf);
	closesocket(sockSrv);
    free(pfile_buf);
	return ;
 }

 while(1)
 { 	  
	LogDisplay("等待STB连接!waiting...\r\n");
    memset(sendbuffer,0,sizeof(sendbuffer));
	ReadFileAble=WAITACCEPT;									//文件标识置为“等待连接”
	if(QuerySocketReadble( sockSrv,&ver_len,pfile_buf,&file_len,fpReadIn)<0)
	{
		free(pfile_buf);
		return;
	}
	SOCKET sockConn=accept(sockSrv,(SOCKADDR*)&addrClient,&len);// 接受客户端连接,获取客户端的ip地址
	if(sockConn==INVALID_SOCKET)
	{
	  sprintf(Print_Buf,"accept无效连接！%d \r\n",WSAGetLastError());
	  LogDisplay(Print_Buf);
	  continue;
	}

    ReadFileAble=CONNECTING;									//发送数据期间无法读入文件

    LogDisplay("收到STB连接请求！\r\n");
	sprintf(Print_Buf,"发送升级流版本号：%s \r\n",Ver_Buf);
	LogDisplay(Print_Buf);
	Ver_Buf[ver_len]=ver_len+1;									//在Ver_buf最后填写总的数据长度（包括最后一位）
    if((send_len=send(sockConn,(char*)Ver_Buf,ver_len+1,0))<0)
	{ 
		LogDisplay("版本号发送失败！\r\n");
		closesocket(sockConn);								    //断开连接
		closesocket(sockSrv);
		free(pfile_buf);
		return ;
	} 
	Ver_Buf[ver_len]=0;		                                     //解决发送一次后版本号尾部乱码问题
	LogDisplay("等待STB确认信息...\r\n"); 
    flag=Receive_StbInfo(sockConn);
	if(flag==CLOSED_ID)
	{ 
	   LogDisplay("机顶盒程序版本已经最新\r\n");
	   LogDisplay("释放连接\r\n\r\n");
	   closesocket(sockConn);
	   continue;
	}
	if(flag<0)
	{
	   LogDisplay("接受STB信息失败！\r\n");
	   LogDisplay("释放连接\r\n\r\n");
	   closesocket(sockConn);
	   continue;
	}
    /***************发送文件,接受STB消息*****************/
	flag=0;
    flag=FileSend(pfile_buf,file_len,sockConn);
	if(flag==RESEND_ID)
 	{
		LogDisplay("释放连接\r\n\r\n");
		closesocket(sockConn);
	}
	if(flag==CLOSED_ID)
 	{
		LogDisplay("机顶盒端接收完毕\r\n");
		LogDisplay("释放连接\r\n\r\n");
		closesocket(sockConn);
	}
	if(flag<0)
	{
		LogDisplay("连接错误，释放连接\r\n\r\n");
		closesocket(sockConn);
	}
 }

}
