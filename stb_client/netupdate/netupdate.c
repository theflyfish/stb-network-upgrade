
/**********************************************************************

  FileName:  	Netupdate.c
  Author:       yuxiang   
  Version :     1.0    
  Date:         2011/10
  Description:  ��ģ������5105��������    
  History:      
      <author>  <time>   <version >   <desc>
      yuxiang   11/10    1.0          build this moudle  
  Remarks:
		        ����ʹ��SourceInsight��д,��UE�²鿴�ĵ��Ű�����.  
*********************************************************************/


#include "tcpip.h"
#include <string.h>
#include <stddefs.h>
#include <stdio.h>
#include "sr_dbase.h"
#include "appltype.h"
#include "sections.h"
#include "sr_flash.h"
#include "sr_nvm.h"
#include "usif.h"
#include "task.h"
#include "ostime.h"
#include "sr_gfx.h"
#include "vid.h"

//#include "debug.h"     			for debug    yx
//#include "Des.h"       			for decript  yx
//#define FILE_NAME "d:\\main.z"    for debug    yx




#define netCRC_32               0xEDB88320                      /*CRC32У������ɶ���ʽ*/
#define REC_BUFFER_SIZE         (1024)                          /*���ջ����� 1k*/
#define FILE_SIZE               (1024*1024*3)                   /*�ļ������� 4M*/
#define VERSION_SIZE            (20)                            /*�汾��������С 20�ֽ�*/
#define TIMEOUT                 (30*WAIT_FOR_1_SEC)             /*recv()ʱ���ճ�ʱʱ��*/
#define NETUP_PROGRAM_ADDR      0x400A0000                      /*��������flash�еĴ�ŵ�ַ*/
#define LIVE_WATI_TIME          (WAIT_FOR_1_SEC*30)             /*�������ӷ�����֮��ĵȴ�ʱ��*/

extern void SetDisBuf(char *chpChar);                           /*����ǰ���LED��ʾ*/
extern BOOL AVDisabled;                                     

//semaphore_t *NetUpdateTaskAccess=NULL;
task_t *pNetUpdateTask=NULL;

unsigned char keyStr[8]={'i','n','s','m','e','d','i','a'}; /*DES��Կinsmedia  */
unsigned char UpdateId[4]={'u','p','i','d'};               /*STB���͵�������ʶ*/
unsigned char CloseId[5]={'c','l','o','s','e'};            /*STB���͵�close_socket��ʶ*/
unsigned char ReSendId[6]={'r','e','s','e','n','d'};       /*STB���͵��ش���ʶ*/
unsigned char PVersion[VERSION_SIZE];                      /*�����ļ��汾��*/


/*****************************NetMemAllcate*********************************

��������:Ϊ�ļ������������ݰ����ջ����������ڴ�,������
����:
		 recvBuf     �������ݰ����ջ�����
		 pFileStore  �ļ����ջ�����
����ֵ:
       	  1: �ڴ����ɹ�
       	 -1: �ڴ����ʧ�� 
************************************************************************/
#if 0
int NetMemAllcate(unsigned char *recvBuf,unsigned char *pFileStore)
{
	recvBuf =(unsigned char*) memory_allocate(SystemPartition,REC_BUFFER_SIZE);    /*�������ݰ����ջ�����*/
	if(recvBuf==NULL)
	{
		return -1;
	}
	pFileStore =(unsigned char*) memory_allocate(SystemPartition,FILE_SIZE);       /*�����ļ����ջ�����*/
	if(pFileStore==NULL)
	{
		return -1;
	}
	memset(pFileStore,0,FILE_SIZE);
	memset(recvBuf,0,REC_BUFFER_SIZE);
	return 1;

}
#endif 
/*****************************NetCRC_32**********************************

��������:�������� ����CRC32У��
����:
		 aPoly  CRC32��ʼ��ֵ   (����)
		 aSize  CRC��ʽ��ָ��   (���)
����ֵ:  ��

************************************************************************/

void NetBuildTable32( unsigned long aPoly ,unsigned long* Table_CRC) 
{
	unsigned long i;
	for (i = 0; i < 256; i++)
	{
	unsigned long r = i;
	  int j;
	  for (j = 0; j < 8; j++)
	    if (r & 1) 
	      r = (r >> 1) ^ aPoly;
	    else     
	      r >>= 1;
	  Table_CRC[i] = r;
	}
}

/*****************************NetCRC_32**********************************

��������:CRC32У��
����:
		 aData  ��ҪУ��Ļ�����ָ��
		 aSize  ������aData���ݳ���
����ֵ:  CRC32У����

************************************************************************/

unsigned long NetCRC_32( unsigned char * aData, unsigned long aSize ) 
{ 
	unsigned long i; 
	unsigned long nAccum = 0xffffffff; 
	unsigned long Table_CRC[256];
	NetBuildTable32( netCRC_32,Table_CRC ); 
	for ( i = 0; i < aSize; i++ )
//		nAccum = Table_CRC[((U8)(nAccum)) ^ *aData] ^ (nAccum >> 8);
//		nAccum = Table_CRC[((unsigned char)nAccum^((unsigned char*)aData)[i])] ^ (nAccum>>8);//YX ע
		nAccum = Table_CRC[((unsigned char)nAccum^(*(unsigned char*)aData++))] ^ (nAccum>>8);
//		nAccum = ( nAccum << 8 ) ^ Table_CRC[( nAccum >> 24 ) ^ *aData++]; 
	return ~nAccum;
}

/*****************************Query_Recv*********************************

��������:select��ѯsocketʱ��ɶ�,���ɶ�,���ؽ��ж�ȡ;����ȴ���ʱ
         ��select����,����������ѭ��,ֱ����ʱ
����:
		 sockClient Ҫ��ѯ���׽��ֺ�
����ֵ:
       	 0: �ȴ���ʱ��δ���ܵ�����
       	 1: �����ݵ���ɹ�����

**********************�˺���δ�޸����***********************************
���޸�:
        ȥ��whileѭ��
        ȥ��time_minus()ʱ���жϣ�
        ��select()�������ʵ���������ʱ
*************************************************************************/
int Query_Recv(int sockClient)
{
	clock_t timebegin;
	fd_set fds;
	struct timeval netouttime;
	int ret=0;
	timebegin =time_now();                                      /*��ȡ��ʼʱ��*/
	netouttime.tv_sec=1;
 	netouttime.tv_usec=0;

	while(1)
	{
		FD_ZERO(&fds);
		FD_SET(sockClient,&fds);
		if(select(sockClient+1,&fds,NULL,NULL,&netouttime)>0)   /*�ж�SOCKET�Ƿ�ɶ�*/
		{
			if(FD_ISSET(sockClient,&fds)>0)
				break;
		}
		if(time_minus(time_now(),timebegin)>TIMEOUT)
			{
			STTBX_Print(("�������ݳ�ʱ!\n"));
			return 0;
			}
	
	}
	return 1;


}

/*******************Net_Socket_Close**************************************

��������:����������͹ر���Ϣ,���رձ����׽���
����:
		sockClient:�����׽��ֺ�
����ֵ:
		 0:���ͳɹ�,�رճɹ�;
		-1:����ʧ�ܻ�ر�ʧ��;
		
*************************************************************************/

int  Net_Socket_Close(int sockClient )
{
	STTBX_Print(("֪ͨ�������ͷ�����\n"));
	if(( send(sockClient, CloseId,sizeof(CloseId),0))<0)                 /*��������*/
	{
		 STTBX_Print(("����CLOSE��Ϣʧ��\n"));
		 return -1;
	}
	 if(( close(sockClient))<0)                                          /*�ر�����*/
	{
		STTBX_Print(("�ر��׽�ʧ������!\n"));
		return -1; 
	}
	 STTBX_Print(("�ر�����\n"));
	 return 0;

}

/*******************Receive_VerInfo***************************************

��������:����server�����İ汾����,ֻ�н��յ���ȷ�İ汾����,�����Ż᷵��;
		 ���򷵻ؽ��ճ�ʱ.
����:
		sockClient:�����׽��ֺ�
����ֵ:
		 1:���յ���ȷ�İ汾����
		-1:�׽��ֺŴ���
		-2:���ճ�ʱ��δ�յ�����
		-3:���ճ�ʱ�����յ���������

*************************************************************************/
int Receive_VerInfo(int sockClient )
{

   clock_t timebe;
   unsigned long recv_len=0;
   timebe =time_now();
 	if(sockClient<0)
	{
	  STTBX_Print(("socket error! \n"));
	  return -1;
	}
    while(1)
  	{
  	    if( Query_Recv(sockClient)<=0)
			return -2;
	 	 memset(PVersion,0,VERSION_SIZE);
		 if( (recv_len=recv(sockClient,PVersion,VERSION_SIZE,0))>0);     /*���ܵ���ȷ�汾����Ϣ*/
	  	{
	  		if(recv_len==PVersion[recv_len-1])
			{
			PVersion[recv_len-1]=0;                                      /*��� ����ͳ���ֽ�*/
	   		STTBX_Print(("���յ�����������汾�ţ�%s\n",PVersion));
	        break;
	   		}		
	   		STTBX_Print(("receive data��%s\n",PVersion));
			if(time_minus(time_now(),timebe)>TIMEOUT)                    /*���յ�������Ϣ*/
			{
			STTBX_Print(("���ճ�ʱ:�汾��������!\n"));
			return -3;
			}
	  	}
    }
	return 1;

}
/****************************Compare_Version****************************
��������:�Ƚ��յ��ĳ���汾�뱾�ذ汾
����:
		PVersion;���Server����汾�ŵ��ַ�ָ��
����ֵ: 
		0: ���汾���
		1: ���汾��ͬ
	   -1: PVersionָ�����
	
***********************************************************************/
int Compare_Version(unsigned char *PVersion)
{
	unsigned char PVersionOld[VERSION_SIZE];
	unsigned char PVersionNew[VERSION_SIZE];
	unsigned int iReaded,i;
	if(PVersion==NULL)
	{
	STTBX_Print(("PVersion is null!\n"));
	return -1;
	}
	STTBX_Print(("����STB�е�ǰ�汾����\n"));
	memset(PVersionOld,0,VERSION_SIZE);
	memcpy(PVersionNew,PVersion,VERSION_SIZE);
	STFLASH_Read(hFLASHDevice,NET_PVERSION_ADDR, PVersionOld, VERSION_SIZE,&iReaded);  /*��ȡflash��NET_PVERSION_ADDR�汾�Ŵ�ŵ�ַ*/
	STTBX_Print(("STB�汾��;%s\n",PVersionOld));
	for(i=0;i<VERSION_SIZE;i++)
	{
		if(PVersionNew[i]!=PVersionOld[i])
		break;
	}
	if(i==VERSION_SIZE)                                                                /*˵�������汾���*/
	{
		 return 0;  
	}
	else
	{
         return 1;
	}

	
}
/****************************Net_Socket_Init****************************
��������:����socket,���ӷ�����
����: 
		sockClient:Ҫ������socket�׽���
����ֵ:
       	sockClient:�����ɹ����socket�׽���
                -1:socket����ʧ�ܣ�
                -2:����serverʧ�ܣ�
                -3;�ر�socketʧ�ܣ�      
       
***********************************************************************/
int Net_Socket_Init(void)
{
    int sockClient=0;
    struct sockaddr_in addrSrv;
	int ret=-1;
	sockClient=socket(AF_INET,SOCK_STREAM,0);                           /*AF_INET ..tcp����*/
	if(sockClient<0)
	{
	  STTBX_Print(("�ͻ����׽��ִ���ʧ�ܣ����� \n"));
	  return -1;
	}
	STTBX_Print(("STB�����׽��ֳɹ�! \n"));                             /*��ʼ��������˿ں� */
	addrSrv.sin_addr.s_addr=htonl(	pstBoxInfo->net_info.BrowserServer);

	//addrSrv.sin_addr.s_addr=htonl(pstBoxInfo->net_info.SocketUpdateServer);
	STTBX_Print(("Server IP: %x\n",	pstBoxInfo->net_info.BrowserServer));
	//addrSrv.sin_addr.s_addr=htonl(0x0a1e64fa);                          /*��������ַ10.30.100.250 */
	addrSrv.sin_family=AF_INET;
	addrSrv.sin_port=htons(6000);                                       /* ���ö˿ں� */
	STTBX_Print(("��ʼ���ӷ�����...\n"));
	ret=connect(sockClient,(struct sockaddr *)&addrSrv,sizeof(struct sockaddr_in));
	if(ret!=0)
		{
	     STTBX_Print(("���ӷ�����ʧ��,30������� \n"));
		  if(( close(sockClient))<0) 							
			{
				STTBX_Print(("�ر��׽�ʧ������!\n"));
				return -3; 
			}
         return -2;
		}
	 return sockClient;

}

/**************************File_Recv_Crc()******************************
��������:�����ļ�������CRCУ��
����:
		sockClient: �������ݵ�socket��
		recvBuf:   	�洢�������ݰ��Ļ�����ָ��    (���)
		pFileStore:	�洢�����ļ��Ļ�����ָ��      (���)
		Num_Length:	���������ļ���ʵ�ʳ���        (���)
����ֵ:
        1;�ļ����ճɹ���У�����
        0:�ļ�������ɵ�CRCУ�����
       -1:��������
       -2:�����ݿɶ������ճ�ʱ
       -3:���ݽ��մ���:
************************************************************************/
int File_Recv_Crc(int sockClient,unsigned char* recvBuf,unsigned char *pFileStore,unsigned long *Num_Length)
{
	unsigned long crc32=0;
	unsigned long crc32_old=0;
	int LIV_Recvlength = 0;                                                     /*�洢ÿ�ν��յ������ݰ�ʵ�ʳ���*/
	unsigned char *temp=NULL;
	unsigned long Rev_Num=0;
	if(sockClient<0||recvBuf==NULL||pFileStore==NULL||Num_Length==NULL)
	{
		STTBX_Print(("File_Recv_Crc:parameters error\n"));
		return -1;
	}
	temp=pFileStore;
	while(1)
	{
	    if(Query_Recv(sockClient)<=0)                                           /*��ѯ�����Ƿ���,�׽����Ƿ�ɶ�*/
	    {
		   	return -2;                                                          /*δ�յ�����.���ճ�ʱ*/
	    }
		LIV_Recvlength = recv(sockClient,recvBuf,REC_BUFFER_SIZE,0);
		if(LIV_Recvlength < 0)
		{
			return -3;                                                          /*���ݽ��մ���*/
		}
		if(LIV_Recvlength<REC_BUFFER_SIZE)
		{
			if(recvBuf[LIV_Recvlength-4]=='e'&&recvBuf[LIV_Recvlength-3]=='n'
			   &&recvBuf[LIV_Recvlength-2]=='d'&&recvBuf[LIV_Recvlength-1]=='\0')
			{
				memcpy(temp,recvBuf,LIV_Recvlength-4);
				temp+=(LIV_Recvlength-4);
				Rev_Num+=LIV_Recvlength-4;                                      /*ȥ��'end '��β��ʶ����'\0'*/
				memset(recvBuf,0,REC_BUFFER_SIZE);                              /*����*/
				STTBX_Print(("Recieve File From Server Finished!\n"));
				break;
			}
		//	STTBX_Print(("Recieve Data Length<REC_BUFFER_SIZE!!:%d!\n",LIV_Recvlength)); // for debug Ӱ�����ݽ���
		}
		memcpy(temp,recvBuf,LIV_Recvlength);
   		temp+=LIV_Recvlength;
   		Rev_Num+=LIV_Recvlength;
		memset(recvBuf,0,REC_BUFFER_SIZE);                                      /*����*/
		LIV_Recvlength = 0; 
	}

	crc32_old=(*(temp-4)<<24)|(*(temp-3)<<16)|(*(temp-2))<<8|(*(temp-1));
	crc32=NetCRC_32(pFileStore,Rev_Num-4);
	if(crc32!=crc32_old)                                                        /*CRCУ�����*/
	{
		   return 0;
	}
	*(temp-4)=0;                                                                /*��CRCУ�鲿������*/
	*(temp-3)=0;
	*(temp-2)=0;
	*(temp-1)=0;
	Rev_Num=Rev_Num-4;                                                          /*��ȥCRCУ�鲿��*/
	STTBX_Print(("Recieve File CRC32 OK!\n"));
	STTBX_Print(("Recieve File Length:%d!\n",Rev_Num));
	*Num_Length=Rev_Num;                                                        /*���س�ȥend��CRCУ�������ļ�����*/
	return 1;
        
}


/******************************File_Recv*****************************
��������:����Server���������ݣ�CRCУ��ɹ��򷵻�;
		 �����ش���ֱ���ش���������,����.
����:
		sockClient: �������ݵ�socket��
		recvBuf:   	�洢�������ݰ��Ļ�����ָ��    (���)
		pFileStore:	�洢�����ļ��Ļ�����ָ��      (���)
		Num_Length:	���������ļ���ʵ�ʳ���        (���)
����ֵ:
         1: �����ļ����ճɹ�,��У��ͨ��
		-1: ������������
		-2: �����������3��
		-3: �����ش���Ϣʱ���ʹ���
		-4: �����ļ�����ʧ��
**********************************************************************/

int File_Recv(int sockClient,unsigned char* recvBuf,unsigned char *pFileStore,unsigned long *Num_Length)
{
	unsigned char ReSendNum=0;
    int RET=0;
	if(sockClient<0||recvBuf==NULL||pFileStore==NULL||Num_Length==NULL)
	{
		STTBX_Print(("File_Recv:parameters error\n"));
		return -1;
	}
	while(1)
	{
	 	RET=File_Recv_Crc(sockClient, recvBuf,pFileStore,Num_Length);
	 	if(RET == 0)                                                               /*�ļ�������ɵ�CRCУ�����*/
		{	
		 	STTBX_Print(("CRC32У��ʧ��\n"));
		    if(ReSendNum==2)                                                       /*���������յ������ļ�ʱ�����ٷ����ش���ʶ*/
		   	{
		   		STTBX_Print(("�ش���������\n"));
		   		return -2;
		   	}
		 	STTBX_Print(("֪ͨ�������ش�\n"));
			if(( send(sockClient, ReSendId,sizeof(ReSendId),0))<0)                 /*�����ش���ʶ*/
		 	{
		  		STTBX_Print(("send data error!\n"));
		   	 	return -3; 
		 	}
		 	ReSendNum++;
		 	continue;

		}
	  	if(RET < 0)
	  	{
			return -4;                                                             /*�ļ�����ʧ��*/
	  	}
	  	if(RET > 0)
	  	{
			break;                                                                 /*�����ճɹ���У��û�д���ֱ������while*/
	  	}
	}
	
	return 1;

}
#if 0
/*********���������кŷ���***δ�����******/
void Send_SerialNo(int sockClient)
{
	char SerialNoChar=0;			
	SRNVM_GetSerialNo((char *)&SerialNoChar);
	STTBX_Print(("SerialNo from NVM=%s\n",(char *)&SerialNoChar]));
	if(( send(sockClient, UpdateId,sizeof(UpdateId),0))<0)		    
	 {
		   STTBX_Print(("send data error!\n"));
		   Net_Socket_Close(sockClient);
		   task_delay(LIVE_WATI_TIME);                           
		   continue;
	  	}
	
}
#endif
/******************************NET_Update*****************************
��������:ͨ��socket�������Ӷ�5105�������������
		 ���ӷ�����,���հ汾����,ͨ���Ƚϰ汾,�ж��Ƿ�����������,
		 ������������ɺ�д��flash
		 ����������
		 
����:    ��
����ֵ:  ��
���ú���:

�ڲ�����:
         Net_Socket_Init();
         Receive_VerInfo();
         Compare_Version();
         File_Recv();
         Net_Socket_Close();
�ⲿ����:
    	task_delay(); 
    	send();
    	FLASH_WriteData();
    	UpdateExit();
        SetDisBuf();
        DisBoxMessage();
        SingapplRestart();
                	 
**********************************************************************/

void NET_Update(void *pParam)
{

	int sockClient=-1;                                                              /*�����׽���*/
    ST_ErrorCode_t ErrCode=ST_NO_ERROR;
	unsigned long  Recv_length_Sum = 0;                                            /*�洢ʵ�ʽ��ܵ����ļ�����*/
	unsigned char *pFileStore = NULL;                                              /*�洢�������ļ�������ָ��*/
	unsigned char *recvBuf=NULL;                                                   /*�洢һ�����ݰ��Ļ�����ָ��*/   
	//long int debug_fp;                                                           /*yx for debug*/
	STLAYER_GlobalAlpha_t Alpha;
	int RetValue=0;                                                                /*�洢�����ķ���ֵ�����ڷ�֧�ж�*/

	STTBX_Print(("************���������߳� *************\n"));
	
	recvBuf =(unsigned char*) memory_allocate(SystemPartition,REC_BUFFER_SIZE);    /*�������ݰ����ջ�����*/
	if(recvBuf==NULL)
	{
		return ;
	}
	pFileStore =(unsigned char*) memory_allocate(SystemPartition,FILE_SIZE);       /*�����ļ����ջ�����*/
	if(pFileStore==NULL)
	{
		return ;
	}
	memset(pFileStore,0,FILE_SIZE);
	memset(recvBuf,0,REC_BUFFER_SIZE);                   
	while(TRUE)
	{

		if((sockClient=Net_Socket_Init())<0)                                      /*��ʼ���׽��ֲ����ӷ�����*/
		{                                                                         /*����ԭ���������ע��*/
			 STTBX_Print(("socket init error!\n"));                               /*Net_Socket_Init()�ڲ��ѹر�socket,�����ٵ���closesocket*/	
			 STTBX_Print(("�ر�����\n"));
			 task_delay(LIVE_WATI_TIME); 
			 continue;        
		}
		
		STTBX_Print(("�ɹ����ӵ�������!\n"));
		STTBX_Print(("�ȴ����ճ���汾��...\n"));                                 /*���հ汾������*/ 
		if(Receive_VerInfo(sockClient)<0)                                          		
		{
            STTBX_Print(("receive version error!\n"));                            /*����ԭ���������ע��*/                     
            Net_Socket_Close(sockClient);                                        
            task_delay(LIVE_WATI_TIME);                         
            continue;
		}
		
		RetValue=Compare_Version(PVersion);                                       /*�Ƚϰ汾��*/                           
        if(RetValue<0)
		{
	  		STTBX_Print(("Compare_Version failed!\n"));                           /*����ԭ���������ע��*/
	  		Net_Socket_Close(sockClient);
			task_delay(LIVE_WATI_TIME);                         
			continue;
	  	}		
	  	if(RetValue==0)
	    {
	  		STTBX_Print(("������������!\n"));
	  		Net_Socket_Close(sockClient);
			task_delay(LIVE_WATI_TIME);                        
			continue;
	  	}

		STTBX_Print(("������������\n"));                                         /*�����������������Ϣ*/
	  	if(( send(sockClient, UpdateId,sizeof(UpdateId),0))<0)		    
	   	{
		   STTBX_Print(("send data error!\n"));
		   Net_Socket_Close(sockClient);
		   task_delay(LIVE_WATI_TIME);                           
		   continue;
	  	}
		
        #if 1
		
		SRGFX_ClearFullArea();
		SRGFX_DrawSTILL();				                                         /*����ͼ����*/								 
		STLAYER_GetViewPortAlpha(LAYER_ViewPortHandle[LAYER_VIDEO],&Alpha);
		if((Alpha.A0!=0)||(Alpha.A1!=0))
		{
			STVID_DisableOutputWindow(VID_ViewPortHandle);
			Alpha.A0 = 0;
			Alpha.A1 = 0;
			STLAYER_SetViewPortAlpha(LAYER_ViewPortHandle[LAYER_VIDEO], &Alpha);
		}
		AVDisabled=TRUE;
		
       #endif
	
		SRGFX_ClearFullArea();	
	    DisBoxMessage(CENTER_JUST,150, 130, 3, 2,
					  "\n   ��⵽������ ���ڽ�������...����رյ�Դ!  \n","\n    ȡ�������˳�����    \n");	
		SetDisBuf("e100");                                                       /*LED��ʾE100*/
	    STTBX_Print(("��ʼ��������������....\n")); 

		if(File_Recv(sockClient, recvBuf,pFileStore,&Recv_length_Sum)<0)         /*�����ļ���buffer��*/
	    {	
	   		STTBX_Print(("�������ݽ���ʧ��,�Ժ�����\n"));                        /*�ļ�����ʧ��*/ 
			SetDisBuf("e1ff");                                                   /*LED��ʾE1FF*/
	   		UpdateExit(5,"\n ���ݽ���ʧ�� �ֶ��˳�\n",TRUE);
			AVDisabled=FALSE;
			Net_Socket_Close(sockClient);
			task_delay(LIVE_WATI_TIME);       				  
			continue;
		}
        STTBX_Print((" �ļ�������ɣ�CRCУ��ͨ��!\n"));
		Net_Socket_Close(sockClient);                                            /*֪ͨ�������ͷ�����*/
		
	  	#if 0
		//�ļ�����
		STTBX_Print(("Recieve File DES Decryping...\n"));
		if(DES_Decrypt(pFileStore,keyStr,pFileStore, Num_Length)<0)
			{
	   		STTBX_Print(("Recieve File DES Decrypt failed!\n"));
	  		Net_Socket_Close(sockClient);
			task_delay(WAIT_FOR_1_SEC*30);					  
			continue;
    		}
		STTBX_Print(("Recieve File DES Decrypted OK!\n"));
		#endif	
		
		#if 0
		memset(filename,0,30);
		sprintf(filename,FILE_NAME);	
		debug_fp=debugopen(filename,"wb");
		STTBX_Print(("\\\\\\\\\\\\\\debugopen!\n"));
		debugwrite(debug_fp,pFileStore,(size_t)(Num_Length));
		STTBX_Print(("\\\\\\\\\\\\\\debugwrite!\n"));
		debugclose(debug_fp);	
		#endif		

		SRGFX_ClearFullArea();
	    DisBoxMessage(CENTER_JUST,150, 130, 3, 2,"\n  ����������...����رյ�Դ!  \n"); 
		SetDisBuf("ee00");                                                        /*LED��ʾEE00*/
		ErrCode=FLASH_WriteData(pFileStore,Recv_length_Sum,NETUP_PROGRAM_ADDR);   /*����������������д��flash��*/
		if(ErrCode!=ST_NO_ERROR)
		{
			STTBX_Print(("\n program flash write error\n"));
			memory_deallocate( SystemPartition,pFileStore);
			pFileStore=NULL;
			SetDisBuf("eeff");                                                    /*LED��ʾEEFF*/
			UpdateExit(5,"\n    ������д��flash��������   �ֶ��˳�   \n",TRUE);
			AVDisabled=FALSE;
			task_delay(LIVE_WATI_TIME);       
			continue;
		}  
		if(ErrCode==ST_NO_ERROR)
		{
			memory_deallocate( SystemPartition,pFileStore);                       /*���汾��д��flash��*/
		    pFileStore=NULL;
			ErrCode=FLASH_WriteData(PVersion,VERSION_SIZE,NET_PVERSION_ADDR+0x40000000);
		    if(ErrCode!=ST_NO_ERROR)
		    {
			    STTBX_Print(("\n  version flash write error\n"));
				SetDisBuf("eeff"); 
				UpdateExit(5,"\n    �汾��Ϣд��flash��������  �ֶ��˳�  \n",TRUE);     
				AVDisabled=FALSE;
				task_delay(LIVE_WATI_TIME); 
				continue;      	
		    }
			UpdateExit(0,NULL,FALSE);
			STTBX_Print(("Successfully---->Program,please restart\n"));
			SetDisBuf("eee8");
			SRGFX_ClearFullArea();
			DisBoxMessage(CENTER_JUST,200, 130, 3, 2,"\n    �������,  ��������������...\n");
			task_delay(2*WAIT_FOR_1_SEC);                                         /*��ʱ2������,ȷ��OSD��ʾ*/
			SingapplRestart();

		}                  
    }
} 

/****************************InitNETUpdateTask****************************

��������: �������������߳�
����:     ��
�߳����: NET_Update()
����ֵ:
       	  TRUE: �̴߳����ɹ�
       	  FALSE:�̴߳���ʧ��
       
***********************************************************************/

BOOL InitNETUpdateTask(void)
{
    /*
	NetUpdateTaskAccess=semaphore_create_fifo(1);
	if(FilterTaskAccess==NULL)
	{
		STTBX_Print(("ERROR:NetUpdateTaskAccess create failed!\n"));
	}
    */	
	pNetUpdateTask=(task_t *)CreateTask(NET_Update, NULL, 2048,3, "NetUpdate", 0);
	if(pNetUpdateTask)
	{
		return TRUE;
	}
	else
		return FALSE;
}









































