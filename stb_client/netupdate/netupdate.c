
/**********************************************************************

  FileName:  	Netupdate.c
  Author:       yuxiang   
  Version :     1.0    
  Date:         2011/10
  Description:  本模块用于5105网络升级    
  History:      
      <author>  <time>   <version >   <desc>
      yuxiang   11/10    1.0          build this moudle  
  Remarks:
		        代码使用SourceInsight编写,在UE下查看文档排版会变乱.  
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




#define netCRC_32               0xEDB88320                      /*CRC32校验的生成多项式*/
#define REC_BUFFER_SIZE         (1024)                          /*接收缓冲区 1k*/
#define FILE_SIZE               (1024*1024*3)                   /*文件缓冲区 4M*/
#define VERSION_SIZE            (20)                            /*版本缓冲区大小 20字节*/
#define TIMEOUT                 (30*WAIT_FOR_1_SEC)             /*recv()时接收超时时间*/
#define NETUP_PROGRAM_ADDR      0x400A0000                      /*主程序在flash中的存放地址*/
#define LIVE_WATI_TIME          (WAIT_FOR_1_SEC*30)             /*两次连接服务器之间的等待时间*/

extern void SetDisBuf(char *chpChar);                           /*设置前面板LED显示*/
extern BOOL AVDisabled;                                     

//semaphore_t *NetUpdateTaskAccess=NULL;
task_t *pNetUpdateTask=NULL;

unsigned char keyStr[8]={'i','n','s','m','e','d','i','a'}; /*DES密钥insmedia  */
unsigned char UpdateId[4]={'u','p','i','d'};               /*STB发送的升级标识*/
unsigned char CloseId[5]={'c','l','o','s','e'};            /*STB发送的close_socket标识*/
unsigned char ReSendId[6]={'r','e','s','e','n','d'};       /*STB发送的重传标识*/
unsigned char PVersion[VERSION_SIZE];                      /*升级文件版本号*/


/*****************************NetMemAllcate*********************************

函数功能:为文件缓冲区与数据包接收缓冲区分配内存,并清零
参数:
		 recvBuf     单个数据包接收缓冲区
		 pFileStore  文件接收缓冲区
返回值:
       	  1: 内存分配成功
       	 -1: 内存分配失败 
************************************************************************/
#if 0
int NetMemAllcate(unsigned char *recvBuf,unsigned char *pFileStore)
{
	recvBuf =(unsigned char*) memory_allocate(SystemPartition,REC_BUFFER_SIZE);    /*分配数据包接收缓冲区*/
	if(recvBuf==NULL)
	{
		return -1;
	}
	pFileStore =(unsigned char*) memory_allocate(SystemPartition,FILE_SIZE);       /*分配文件接收缓冲区*/
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

函数功能:建表函数 用于CRC32校验
参数:
		 aPoly  CRC32初始化值   (输入)
		 aSize  CRC余式表指针   (输出)
返回值:  无

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

函数功能:CRC32校验
参数:
		 aData  需要校验的缓冲区指针
		 aSize  缓冲区aData数据长度
返回值:  CRC32校验码

************************************************************************/

unsigned long NetCRC_32( unsigned char * aData, unsigned long aSize ) 
{ 
	unsigned long i; 
	unsigned long nAccum = 0xffffffff; 
	unsigned long Table_CRC[256];
	NetBuildTable32( netCRC_32,Table_CRC ); 
	for ( i = 0; i < aSize; i++ )
//		nAccum = Table_CRC[((U8)(nAccum)) ^ *aData] ^ (nAccum >> 8);
//		nAccum = Table_CRC[((unsigned char)nAccum^((unsigned char*)aData)[i])] ^ (nAccum>>8);//YX 注
		nAccum = Table_CRC[((unsigned char)nAccum^(*(unsigned char*)aData++))] ^ (nAccum>>8);
//		nAccum = ( nAccum << 8 ) ^ Table_CRC[( nAccum >> 24 ) ^ *aData++]; 
	return ~nAccum;
}

/*****************************Query_Recv*********************************

函数功能:select查询socket时候可读,若可读,返回进行读取;否则等待超时
         若select错误,不处理继续循环,直到超时
参数:
		 sockClient 要查询的套接字号
返回值:
       	 0: 等待超时，未接受到数据
       	 1: 有数据到达，成功返回

**********************此函数未修改完毕***********************************
待修改:
        去掉while循环
        去掉time_minus()时间判断；
        在select()中设置适当的阻塞延时
*************************************************************************/
int Query_Recv(int sockClient)
{
	clock_t timebegin;
	fd_set fds;
	struct timeval netouttime;
	int ret=0;
	timebegin =time_now();                                      /*获取开始时间*/
	netouttime.tv_sec=1;
 	netouttime.tv_usec=0;

	while(1)
	{
		FD_ZERO(&fds);
		FD_SET(sockClient,&fds);
		if(select(sockClient+1,&fds,NULL,NULL,&netouttime)>0)   /*判断SOCKET是否可读*/
		{
			if(FD_ISSET(sockClient,&fds)>0)
				break;
		}
		if(time_minus(time_now(),timebegin)>TIMEOUT)
			{
			STTBX_Print(("接收数据超时!\n"));
			return 0;
			}
	
	}
	return 1;


}

/*******************Net_Socket_Close**************************************

函数功能:向服务器发送关闭消息,并关闭本地套接字
参数:
		sockClient:本地套接字号
返回值:
		 0:发送成功,关闭成功;
		-1:发送失败或关闭失败;
		
*************************************************************************/

int  Net_Socket_Close(int sockClient )
{
	STTBX_Print(("通知服务器释放连接\n"));
	if(( send(sockClient, CloseId,sizeof(CloseId),0))<0)                 /*发送数据*/
	{
		 STTBX_Print(("发送CLOSE信息失败\n"));
		 return -1;
	}
	 if(( close(sockClient))<0)                                          /*关闭连接*/
	{
		STTBX_Print(("关闭套接失败请检查!\n"));
		return -1; 
	}
	 STTBX_Print(("关闭连接\n"));
	 return 0;

}

/*******************Receive_VerInfo***************************************

函数功能:接收server发来的版本数据,只有接收到正确的版本数据,函数才会返回;
		 否则返回接收超时.
参数:
		sockClient:本地套接字号
返回值:
		 1:接收到正确的版本数据
		-1:套接字号错误
		-2:接收超时，未收到数据
		-3:接收超时，且收到错误数据

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
		 if( (recv_len=recv(sockClient,PVersion,VERSION_SIZE,0))>0);     /*接受到正确版本号消息*/
	  	{
	  		if(recv_len==PVersion[recv_len-1])
			{
			PVersion[recv_len-1]=0;                                      /*清除 长度统计字节*/
	   		STTBX_Print(("接收到服务器程序版本号：%s\n",PVersion));
	        break;
	   		}		
	   		STTBX_Print(("receive data：%s\n",PVersion));
			if(time_minus(time_now(),timebe)>TIMEOUT)                    /*接收到错误信息*/
			{
			STTBX_Print(("接收超时:版本数据有误!\n"));
			return -3;
			}
	  	}
    }
	return 1;

}
/****************************Compare_Version****************************
函数功能:比较收到的程序版本与本地版本
参数:
		PVersion;存放Server程序版本号的字符指针
返回值: 
		0: 两版本相等
		1: 两版本不同
	   -1: PVersion指针错误
	
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
	STTBX_Print(("读出STB中当前版本数据\n"));
	memset(PVersionOld,0,VERSION_SIZE);
	memcpy(PVersionNew,PVersion,VERSION_SIZE);
	STFLASH_Read(hFLASHDevice,NET_PVERSION_ADDR, PVersionOld, VERSION_SIZE,&iReaded);  /*读取flash，NET_PVERSION_ADDR版本号存放地址*/
	STTBX_Print(("STB版本号;%s\n",PVersionOld));
	for(i=0;i<VERSION_SIZE;i++)
	{
		if(PVersionNew[i]!=PVersionOld[i])
		break;
	}
	if(i==VERSION_SIZE)                                                                /*说明两个版本相等*/
	{
		 return 0;  
	}
	else
	{
         return 1;
	}

	
}
/****************************Net_Socket_Init****************************
函数功能:创建socket,连接服务器
参数: 
		sockClient:要建立的socket套接字
返回值:
       	sockClient:建立成功后的socket套接字
                -1:socket创建失败；
                -2:连接server失败；
                -3;关闭socket失败；      
       
***********************************************************************/
int Net_Socket_Init(void)
{
    int sockClient=0;
    struct sockaddr_in addrSrv;
	int ret=-1;
	sockClient=socket(AF_INET,SOCK_STREAM,0);                           /*AF_INET ..tcp连接*/
	if(sockClient<0)
	{
	  STTBX_Print(("客户机套接字创建失败，请检查 \n"));
	  return -1;
	}
	STTBX_Print(("STB创建套接字成功! \n"));                             /*初始化连接与端口号 */
	addrSrv.sin_addr.s_addr=htonl(	pstBoxInfo->net_info.BrowserServer);

	//addrSrv.sin_addr.s_addr=htonl(pstBoxInfo->net_info.SocketUpdateServer);
	STTBX_Print(("Server IP: %x\n",	pstBoxInfo->net_info.BrowserServer));
	//addrSrv.sin_addr.s_addr=htonl(0x0a1e64fa);                          /*服务器地址10.30.100.250 */
	addrSrv.sin_family=AF_INET;
	addrSrv.sin_port=htons(6000);                                       /* 设置端口号 */
	STTBX_Print(("开始连接服务器...\n"));
	ret=connect(sockClient,(struct sockaddr *)&addrSrv,sizeof(struct sockaddr_in));
	if(ret!=0)
		{
	     STTBX_Print(("连接服务器失败,30秒后重试 \n"));
		  if(( close(sockClient))<0) 							
			{
				STTBX_Print(("关闭套接失败请检查!\n"));
				return -3; 
			}
         return -2;
		}
	 return sockClient;

}

/**************************File_Recv_Crc()******************************
函数功能:接收文件并进行CRC校验
参数:
		sockClient: 接收数据的socket号
		recvBuf:   	存储单个数据包的缓冲区指针    (输出)
		pFileStore:	存储升级文件的缓冲区指针      (输出)
		Num_Length:	返回升级文件的实际长度        (输出)
返回值:
        1;文件接收成功且校验完成
        0:文件接收完成但CRC校验错误
       -1:参数错误
       -2:无数据可读，接收超时
       -3:数据接收错误:
************************************************************************/
int File_Recv_Crc(int sockClient,unsigned char* recvBuf,unsigned char *pFileStore,unsigned long *Num_Length)
{
	unsigned long crc32=0;
	unsigned long crc32_old=0;
	int LIV_Recvlength = 0;                                                     /*存储每次接收到的数据包实际长度*/
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
	    if(Query_Recv(sockClient)<=0)                                           /*查询数据是否到来,套接字是否可读*/
	    {
		   	return -2;                                                          /*未收到数据.接收超时*/
	    }
		LIV_Recvlength = recv(sockClient,recvBuf,REC_BUFFER_SIZE,0);
		if(LIV_Recvlength < 0)
		{
			return -3;                                                          /*数据接收错误*/
		}
		if(LIV_Recvlength<REC_BUFFER_SIZE)
		{
			if(recvBuf[LIV_Recvlength-4]=='e'&&recvBuf[LIV_Recvlength-3]=='n'
			   &&recvBuf[LIV_Recvlength-2]=='d'&&recvBuf[LIV_Recvlength-1]=='\0')
			{
				memcpy(temp,recvBuf,LIV_Recvlength-4);
				temp+=(LIV_Recvlength-4);
				Rev_Num+=LIV_Recvlength-4;                                      /*去掉'end '结尾标识符与'\0'*/
				memset(recvBuf,0,REC_BUFFER_SIZE);                              /*清零*/
				STTBX_Print(("Recieve File From Server Finished!\n"));
				break;
			}
		//	STTBX_Print(("Recieve Data Length<REC_BUFFER_SIZE!!:%d!\n",LIV_Recvlength)); // for debug 影响数据接收
		}
		memcpy(temp,recvBuf,LIV_Recvlength);
   		temp+=LIV_Recvlength;
   		Rev_Num+=LIV_Recvlength;
		memset(recvBuf,0,REC_BUFFER_SIZE);                                      /*清零*/
		LIV_Recvlength = 0; 
	}

	crc32_old=(*(temp-4)<<24)|(*(temp-3)<<16)|(*(temp-2))<<8|(*(temp-1));
	crc32=NetCRC_32(pFileStore,Rev_Num-4);
	if(crc32!=crc32_old)                                                        /*CRC校验错误*/
	{
		   return 0;
	}
	*(temp-4)=0;                                                                /*将CRC校验部分清零*/
	*(temp-3)=0;
	*(temp-2)=0;
	*(temp-1)=0;
	Rev_Num=Rev_Num-4;                                                          /*除去CRC校验部分*/
	STTBX_Print(("Recieve File CRC32 OK!\n"));
	STTBX_Print(("Recieve File Length:%d!\n",Rev_Num));
	*Num_Length=Rev_Num;                                                        /*返回除去end与CRC校验码后的文件长度*/
	return 1;
        
}


/******************************File_Recv*****************************
函数功能:接收Server升级流数据，CRC校验成功则返回;
		 否则重传，直到重传次数超限,返回.
参数:
		sockClient: 接收数据的socket号
		recvBuf:   	存储单个数据包的缓冲区指针    (输出)
		pFileStore:	存储升级文件的缓冲区指针      (输出)
		Num_Length:	返回升级文件的实际长度        (输出)
返回值:
         1: 升级文件接收成功,且校验通过
		-1: 参数传入有误
		-2: 传输次数超过3次
		-3: 发送重传消息时发送错误
		-4: 升级文件接收失败
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
	 	if(RET == 0)                                                               /*文件接收完成但CRC校验错误*/
		{	
		 	STTBX_Print(("CRC32校验失败\n"));
		    if(ReSendNum==2)                                                       /*当第三次收到错误文件时，不再发送重传标识*/
		   	{
		   		STTBX_Print(("重传次数超限\n"));
		   		return -2;
		   	}
		 	STTBX_Print(("通知服务器重传\n"));
			if(( send(sockClient, ReSendId,sizeof(ReSendId),0))<0)                 /*发送重传标识*/
		 	{
		  		STTBX_Print(("send data error!\n"));
		   	 	return -3; 
		 	}
		 	ReSendNum++;
		 	continue;

		}
	  	if(RET < 0)
	  	{
			return -4;                                                             /*文件接收失败*/
	  	}
	  	if(RET > 0)
	  	{
			break;                                                                 /*若接收成功且校验没有错误，直接跳出while*/
	  	}
	}
	
	return 1;

}
#if 0
/*********机顶盒序列号发送***未完待续******/
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
函数功能:通过socket网络连接对5105主程序进行升级
		 连接服务器,接收版本数据,通过比较版本,判断是否下载升级流,
		 升级流接收完成后写入flash
		 重启机顶盒
		 
参数:    无
返回值:  无
调用函数:

内部函数:
         Net_Socket_Init();
         Receive_VerInfo();
         Compare_Version();
         File_Recv();
         Net_Socket_Close();
外部函数:
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

	int sockClient=-1;                                                              /*本地套接字*/
    ST_ErrorCode_t ErrCode=ST_NO_ERROR;
	unsigned long  Recv_length_Sum = 0;                                            /*存储实际接受到的文件长度*/
	unsigned char *pFileStore = NULL;                                              /*存储整个大文件缓冲区指针*/
	unsigned char *recvBuf=NULL;                                                   /*存储一个数据包的缓冲区指针*/   
	//long int debug_fp;                                                           /*yx for debug*/
	STLAYER_GlobalAlpha_t Alpha;
	int RetValue=0;                                                                /*存储函数的返回值，便于分支判断*/

	STTBX_Print(("************网络升级线程 *************\n"));
	
	recvBuf =(unsigned char*) memory_allocate(SystemPartition,REC_BUFFER_SIZE);    /*分配数据包接收缓冲区*/
	if(recvBuf==NULL)
	{
		return ;
	}
	pFileStore =(unsigned char*) memory_allocate(SystemPartition,FILE_SIZE);       /*分配文件接收缓冲区*/
	if(pFileStore==NULL)
	{
		return ;
	}
	memset(pFileStore,0,FILE_SIZE);
	memset(recvBuf,0,REC_BUFFER_SIZE);                   
	while(TRUE)
	{

		if((sockClient=Net_Socket_Init())<0)                                      /*初始化套接字并连接服务器*/
		{                                                                         /*错误原因详见函数注释*/
			 STTBX_Print(("socket init error!\n"));                               /*Net_Socket_Init()内部已关闭socket,无须再调用closesocket*/	
			 STTBX_Print(("关闭连接\n"));
			 task_delay(LIVE_WATI_TIME); 
			 continue;        
		}
		
		STTBX_Print(("成功连接到服务器!\n"));
		STTBX_Print(("等待接收程序版本号...\n"));                                 /*接收版本号数据*/ 
		if(Receive_VerInfo(sockClient)<0)                                          		
		{
            STTBX_Print(("receive version error!\n"));                            /*错误原因详见函数注释*/                     
            Net_Socket_Close(sockClient);                                        
            task_delay(LIVE_WATI_TIME);                         
            continue;
		}
		
		RetValue=Compare_Version(PVersion);                                       /*比较版本号*/                           
        if(RetValue<0)
		{
	  		STTBX_Print(("Compare_Version failed!\n"));                           /*错误原因详见函数注释*/
	  		Net_Socket_Close(sockClient);
			task_delay(LIVE_WATI_TIME);                         
			continue;
	  	}		
	  	if(RetValue==0)
	    {
	  		STTBX_Print(("程序无须升级!\n"));
	  		Net_Socket_Close(sockClient);
			task_delay(LIVE_WATI_TIME);                        
			continue;
	  	}

		STTBX_Print(("发送升级请求\n"));                                         /*向服务器发送升级消息*/
	  	if(( send(sockClient, UpdateId,sizeof(UpdateId),0))<0)		    
	   	{
		   STTBX_Print(("send data error!\n"));
		   Net_Socket_Close(sockClient);
		   task_delay(LIVE_WATI_TIME);                           
		   continue;
	  	}
		
        #if 1
		
		SRGFX_ClearFullArea();
		SRGFX_DrawSTILL();				                                         /*背景图绘制*/								 
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
					  "\n   检测到升级流 正在接收数据...请勿关闭电源!  \n","\n    取消按“退出”键    \n");	
		SetDisBuf("e100");                                                       /*LED显示E100*/
	    STTBX_Print(("开始接收升级流数据....\n")); 

		if(File_Recv(sockClient, recvBuf,pFileStore,&Recv_length_Sum)<0)         /*接收文件到buffer中*/
	    {	
	   		STTBX_Print(("升级数据接收失败,稍后再试\n"));                        /*文件接收失败*/ 
			SetDisBuf("e1ff");                                                   /*LED显示E1FF*/
	   		UpdateExit(5,"\n 数据接收失败 手动退出\n",TRUE);
			AVDisabled=FALSE;
			Net_Socket_Close(sockClient);
			task_delay(LIVE_WATI_TIME);       				  
			continue;
		}
        STTBX_Print((" 文件接收完成，CRC校验通过!\n"));
		Net_Socket_Close(sockClient);                                            /*通知服务器释放连接*/
		
	  	#if 0
		//文件解密
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
	    DisBoxMessage(CENTER_JUST,150, 130, 3, 2,"\n  升级主程序...请勿关闭电源!  \n"); 
		SetDisBuf("ee00");                                                        /*LED显示EE00*/
		ErrCode=FLASH_WriteData(pFileStore,Recv_length_Sum,NETUP_PROGRAM_ADDR);   /*将主程序升级数据写入flash中*/
		if(ErrCode!=ST_NO_ERROR)
		{
			STTBX_Print(("\n program flash write error\n"));
			memory_deallocate( SystemPartition,pFileStore);
			pFileStore=NULL;
			SetDisBuf("eeff");                                                    /*LED显示EEFF*/
			UpdateExit(5,"\n    主程序写入flash操作错误   手动退出   \n",TRUE);
			AVDisabled=FALSE;
			task_delay(LIVE_WATI_TIME);       
			continue;
		}  
		if(ErrCode==ST_NO_ERROR)
		{
			memory_deallocate( SystemPartition,pFileStore);                       /*将版本号写入flash中*/
		    pFileStore=NULL;
			ErrCode=FLASH_WriteData(PVersion,VERSION_SIZE,NET_PVERSION_ADDR+0x40000000);
		    if(ErrCode!=ST_NO_ERROR)
		    {
			    STTBX_Print(("\n  version flash write error\n"));
				SetDisBuf("eeff"); 
				UpdateExit(5,"\n    版本信息写入flash操作错误  手动退出  \n",TRUE);     
				AVDisabled=FALSE;
				task_delay(LIVE_WATI_TIME); 
				continue;      	
		    }
			UpdateExit(0,NULL,FALSE);
			STTBX_Print(("Successfully---->Program,please restart\n"));
			SetDisBuf("eee8");
			SRGFX_ClearFullArea();
			DisBoxMessage(CENTER_JUST,200, 130, 3, 2,"\n    升级完成,  机顶盒正在重启...\n");
			task_delay(2*WAIT_FOR_1_SEC);                                         /*延时2秒重启,确保OSD显示*/
			SingapplRestart();

		}                  
    }
} 

/****************************InitNETUpdateTask****************************

函数功能: 创建网络升级线程
参数:     无
线程入口: NET_Update()
返回值:
       	  TRUE: 线程创建成功
       	  FALSE:线程创建失败
       
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










































