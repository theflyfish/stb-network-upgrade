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
char keyStr[8]={'i','n','s','m','e','d','i','a'};    /*DES���ܽ�����Կinsmedia*/
char UpdateId[4]={'u','p','i','d'};                  /*STB���͵�������Ϣ��ʶ*/
char CloseId[5]= {'c','l','o','s','e'};              /*STB���͵Ĺر�������Ϣ��ʶ*/
char ReSendId[6]={'r','e','s','e','n','d'};          /*STB���͵��ش���Ϣ��ʶ*/
char Ver_Buf[20];
char Print_Buf[100];




/******************Socket_Init***********************************
�������ܣ���������socket�Ľ�����˿ڰ�
���룺    ��
�����   
	 sockSrv �����ɹ��󷵻ص��׽��ֺ�
	 -1      winsock����ʧ��
	         �׽��ִ���ʧ��
			 �˿ڰ�ʧ��
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
 LogDisplay("����������socket���ӣ�\r\n");
 SOCKET sockSrv=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP );
 if(sockSrv<0)
 {
  LogDisplay("������socket���Ӵ���ʧ�ܣ�\r\n");
  return -1;
 }

 addrSrv.sin_addr.S_un.S_addr=htonl(INADDR_ANY);                                   //ip��ַΪ10.30.100.250
 addrSrv.sin_family=AF_INET;
 addrSrv.sin_port=htons(6000);

 if(bind(sockSrv,(SOCKADDR*)&addrSrv,sizeof(SOCKADDR_IN))!=0)                      // �󶨶˿�
 {
	sprintf(Print_Buf,"�������˿ڰ�ʧ��!%d\r\n", WSAGetLastError());
	LogDisplay(Print_Buf);
  return -1;
 }
  int nNetTimeout=1000;                                                            //1��
  setsockopt(sockSrv,IPPROTO_TCP,SO_SNDTIMEO,(char *)&nNetTimeout,sizeof(int));    //yx���÷��ͳ�ʱʱ��
  return sockSrv;
}

/******************File_Crc32***********************************
�������ܣ��ļ�����buffer��CRC32У��
������
	pfilebuf:	������������ʱ������crc����
	filelen��	�����ļ���ԭʼ���ȣ�������CRC����
	fp:			�ļ�ָ��;
	File_Name:	�ļ���ָ�룻
����ֵ��   
    CRC32    crc32У����;
	-1:      �����ļ���ʧ�ܣ�
	         ��������
****************************************************************/

unsigned long File_Crc32( unsigned char*pfilebuf, unsigned long *filelen,FILE *fp,char*File_Name)
 {
   int file_block_length=0;
   unsigned long CRC32=0;
   unsigned char*temp=NULL;
   unsigned long file_length=0;
   /*�����ݴ�*/
   if(pfilebuf==NULL||File_Name==NULL||filelen==NULL)               
   {
	   	LogDisplay("File_Crc32:parameter error!\r\n");
		return -1;
   }
   temp=pfilebuf;
   *filelen=0;
   
	if((fp=fopen(File_Name,"rb")) == NULL)
	{
		LogDisplay("�����ļ���ʧ�ܣ�\r\n");
		return -1;
	}
	while(!feof(fp))
	{
	    /*ÿ�ζ�BUFFER_SIZE���ֽڣ������سɹ���ȡ���ֽ���*/
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
    LogDisplay("���CRCУ��\r\n");
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
		int ii,jj;                 //������ʱ������ʵ������޸�
		for(ii=0;ii<100;ii++)
			for(jj=0;jj<100;jj++);
}
/**************�������ļ�����*****************/
int FileBuf_Send(unsigned char* Filebuf, unsigned long filelen,SOCKET sockConn)
 {
   int send_length=0,send_num=0,len=0;
   unsigned char*temp=NULL;
   temp=Filebuf;
   while(1)
   {
    len=(filelen+4)-send_num;  //ʣ��δ�������ݳ���
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
			LogDisplay("β�����������\r\n");
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
		LogDisplay("�ļ��汾���ݴ�ʧ�ܣ�\r\n");
		return -1;
	}
	while(!feof(fpVersion))
	{
	    /*ÿ�ζ�BUFFER_SIZE���ֽڣ������سɹ���ȡ���ֽ���*/
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

/**********���ջ�������Ϣ��û��whileѭ��ֻ����һ�Σ�30��ȴ�************/
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
  if(select(sockConn+1,&fds,NULL,NULL,&time_out)>0) //�ж�SOCKET�Ƿ�ɶ� 30������ʱ��
  {
	  if(FD_ISSET(sockConn,&fds)>0)
	  {
		 memset(recvBuf,0,50);
		 recv_len=0;
		 if((recv_len=recv(sockConn,recvBuf,50,0))<0) // ���ܿͻ�����Ϣ
			return -1;
		 if(recv_len==sizeof(UpdateId))
		 {
			if(recvBuf[0]=='u'&&recvBuf[1]=='p'&&recvBuf[2]=='i'&&recvBuf[3]=='d')
			{
				sprintf(Print_Buf,"���ջ�������Ϣ��%s\r\n",recvBuf);
				LogDisplay(Print_Buf);
				return UPDATE_ID;
			}
			sprintf(Print_Buf,"receive data��%s\r\n",recvBuf);
			LogDisplay(Print_Buf);
		 }
		 if(recv_len==sizeof(CloseId))
		 {
			if(recvBuf[0]=='c'&&recvBuf[1]=='l'&&recvBuf[2]=='o'&&recvBuf[3]=='s'&&recvBuf[4]=='e')
			{
				sprintf(Print_Buf,"���ջ�������Ϣ��%s\r\n",recvBuf);
			    LogDisplay(Print_Buf);
			    return CLOSED_ID;
			}
	        sprintf(Print_Buf,"receive data��%s\r\n",recvBuf);
		    LogDisplay(Print_Buf);
		 }
	    if(recv_len==sizeof(ReSendId))
		{
		  if(recvBuf[0]=='r'&&recvBuf[1]=='e'&&recvBuf[2]=='s'&&recvBuf[3]=='e'&&recvBuf[4]=='n'&&recvBuf[5]=='d')
		  {
			 sprintf(Print_Buf,"���ջ�������Ϣ��%s\r\n",recvBuf);
			 LogDisplay(Print_Buf);
			 return RESEND_ID;
		  }
	      sprintf(Print_Buf,"receive data��%s\r\n",recvBuf);
		  LogDisplay(Print_Buf);
		}
	}
  }  
  return -1; // select�����select��ʱ�����ݽ��մ���
}

/******************ReadFileProcess*******************************
��������:��ȡ�ļ�����,��ȡ�ļ��汾��
         ��ȡ�ļ�,(�ļ�����)������CRCУ��
��������:ReadVersion();File_Crc32()

������
	ver_len:	�汾�����ݳ���(���)
	pfile_buf	�洢�������ݵĻ�����ָ��(���)
	file_len:	�������ݳ���(���)
	fpReadIn:	�ļ�ָ��(����)
����ֵ��   
      1�������ļ�����ɹ�
	 -1���ļ��汾�������
	     File_Crc32 error	     
****************************************************************/

int ReadFileProcess(int *ver_len, unsigned char*pfile_buf,  unsigned long *file_len,FILE * fpReadIn )
{

    LogDisplay("�����ļ��汾\r\n");
    if((*ver_len=ReadVersion(Ver_Buf,Version_File_Name))<0)
      return -1;

    #if 0

    /*�ļ�DES���ܺ����*/
    LogDisplay("�������ļ����м���...\r\n");
    if(DES_Encrypt(FileName,keyStr,DESFileName)<0)
	{
	    sprintf(Print_Buf,"File:\t%s DES Encrypt Failed!\r\n",FileName);
	    LogDisplay(Print_Buf);
	    return -1;
	}
    sprintf(Print_Buf,"File: %s DES EncryptSucceed!\r\n",FileName);
    LogDisplay(Print_Buf);

    /*�����ļ����벢����CRCУ��*/
    LogDisplay("�����ļ����CRCУ��...\r\n");
    #endif

    LogDisplay("���������ļ�...\r\n");
    memset(pfile_buf,0,FLIE_BUFFER_SIZE);
	/*�ļ����벢CRC32У��*/
    if( File_Crc32( pfile_buf, file_len,fpReadIn,FileName)<0)
	{
        return -1;
	}
    return 1;
}

/******************QuerySocketReadble*******************************
��������:��ѯ�׽����Ƿ�ɶ������ɶ����������أ�����ֱ����ʱ������
         ����������Ҫ������ʹsocket��ȡ��Ϊ�����������ⲿ��Ԥ�ġ�

������
	ver_len:	�汾�����ݳ���(���)
	pfile_buf	�洢�������ݵĻ�����ָ��(���)
	file_len:	�������ݳ���(���)
	fpReadIn:	�ļ�ָ��(����)
����ֵ��   
      1�������ļ�����ɹ�
	 -1���ļ��汾�������
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
		if(select(sockSrv+1,&fds,NULL,NULL,&timeout)>0) //�ж�SOCKET�Ƿ�ɶ�
			if(FD_ISSET(sockSrv,&fds)>0)
					break;
		if(ReadFileAble==READFILE) 
		{
			LogDisplay("���������ļ�...\r\n");
			memset(Ver_Buf,0,*ver_len);                                    //���°汾��ǰ����ջ�������
			if(ReadFileProcess( ver_len, pfile_buf, file_len,fpReadIn )<0)
				return -1;
			LogDisplay("�ȴ�STB����!waiting...\r\n\r\n");
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
		LogDisplay("������������...\r\n");
		if(FileBuf_Send(pfile_buf,file_len,sockConn)<0)
		{
			sprintf(Print_Buf,"File_Send:\t%s Failed!\r\n", FileName);
			LogDisplay(Print_Buf);
			LogDisplay("close current connect!\r\n");
			return -1;
		}
		sprintf(Print_Buf,"�����ļ�: %s �������!\r\n",FileName);
		LogDisplay(Print_Buf);
		LogDisplay("�ȴ�STBȷ����Ϣ\r\n");
		RET=0;
		RET=Receive_StbInfo(sockConn);//����STB��Ϣ
		if(RET==RESEND_ID)
 		{
			LogDisplay("�����ж�Ҫ�������ش�!\r\n");
			ReSendNum++;
            continue;
		}
		if((ReSendNum>=2)&&(RET==CLOSED_ID))
		{
			LogDisplay("�ش�3������У������!\r\n");
			break;
		}
		break;
	}
	if(ReSendNum==3)
	{
		LogDisplay("�ش�3�����ݷ���ʧ��!\r\n");
		return -1;
	}
	return RET;
}

void SeverSocket(void) 
{
 SOCKET sockSrv;                                   //�������׽��ֺ�
 SOCKADDR_IN addrClient;                           //�����ϵĿͻ���ip��ַ
 int len=sizeof(SOCKADDR);                         //SOCKADDR�ṹ���С
 int flag=0;                                       //�洢���յ���STB��Ϣ����
 FILE * fpReadIn=NULL;                             // �ļ�ָ��
 unsigned char sendbuffer[BUF_SIZE];               //���ͻ�����
 unsigned char*pfile_buf=NULL;                     //�洢�ļ��Ļ�����ָ��
 unsigned long file_len=0;                         //�ļ�����
 int send_len=0;                                   //���͵����ݳ���
 int ver_len=0;                                    //�汾���ݵĳ��ȣ����19���ֽڣ�


 ReadFileAble=READFILE;                                                   //�ļ���ʶ��Ϊ�����ڶ��롱
 pfile_buf = ( unsigned char *) malloc (FLIE_BUFFER_SIZE);
 if(pfile_buf==NULL)
 {
    LogDisplay("�ڴ����ʧ�ܣ�\r\n");
	return;
 }
 if(ReadFileProcess( &ver_len, pfile_buf, &file_len,fpReadIn )<0)
	return;
 sockSrv=Socket_Init();                      //������sokcet���ӳ�ʼ��
 LogDisplay("����STB��������\r\n\r\n");
 if(( listen(sockSrv,20))!=0)
 {
	sprintf(Print_Buf,"����ʧ��!%d\r\n", WSAGetLastError());
	LogDisplay(Print_Buf);
	closesocket(sockSrv);
    free(pfile_buf);
	return ;
 }

 while(1)
 { 	  
	LogDisplay("�ȴ�STB����!waiting...\r\n");
    memset(sendbuffer,0,sizeof(sendbuffer));
	ReadFileAble=WAITACCEPT;									//�ļ���ʶ��Ϊ���ȴ����ӡ�
	if(QuerySocketReadble( sockSrv,&ver_len,pfile_buf,&file_len,fpReadIn)<0)
	{
		free(pfile_buf);
		return;
	}
	SOCKET sockConn=accept(sockSrv,(SOCKADDR*)&addrClient,&len);// ���ܿͻ�������,��ȡ�ͻ��˵�ip��ַ
	if(sockConn==INVALID_SOCKET)
	{
	  sprintf(Print_Buf,"accept��Ч���ӣ�%d \r\n",WSAGetLastError());
	  LogDisplay(Print_Buf);
	  continue;
	}

    ReadFileAble=CONNECTING;									//���������ڼ��޷������ļ�

    LogDisplay("�յ�STB��������\r\n");
	sprintf(Print_Buf,"�����������汾�ţ�%s \r\n",Ver_Buf);
	LogDisplay(Print_Buf);
	Ver_Buf[ver_len]=ver_len+1;									//��Ver_buf�����д�ܵ����ݳ��ȣ��������һλ��
    if((send_len=send(sockConn,(char*)Ver_Buf,ver_len+1,0))<0)
	{ 
		LogDisplay("�汾�ŷ���ʧ�ܣ�\r\n");
		closesocket(sockConn);								    //�Ͽ�����
		closesocket(sockSrv);
		free(pfile_buf);
		return ;
	} 
	Ver_Buf[ver_len]=0;		                                     //�������һ�κ�汾��β����������
	LogDisplay("�ȴ�STBȷ����Ϣ...\r\n"); 
    flag=Receive_StbInfo(sockConn);
	if(flag==CLOSED_ID)
	{ 
	   LogDisplay("�����г���汾�Ѿ�����\r\n");
	   LogDisplay("�ͷ�����\r\n\r\n");
	   closesocket(sockConn);
	   continue;
	}
	if(flag<0)
	{
	   LogDisplay("����STB��Ϣʧ�ܣ�\r\n");
	   LogDisplay("�ͷ�����\r\n\r\n");
	   closesocket(sockConn);
	   continue;
	}
    /***************�����ļ�,����STB��Ϣ*****************/
	flag=0;
    flag=FileSend(pfile_buf,file_len,sockConn);
	if(flag==RESEND_ID)
 	{
		LogDisplay("�ͷ�����\r\n\r\n");
		closesocket(sockConn);
	}
	if(flag==CLOSED_ID)
 	{
		LogDisplay("�����ж˽������\r\n");
		LogDisplay("�ͷ�����\r\n\r\n");
		closesocket(sockConn);
	}
	if(flag<0)
	{
		LogDisplay("���Ӵ����ͷ�����\r\n\r\n");
		closesocket(sockConn);
	}
 }

}
