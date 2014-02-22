/*******************************************************************
程序：Netupdate
文件：main.c
功能：Windows应用程序窗口框架
********************************************************************/
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include "resource.h"
#include "netsocket.h"

#define PATH_SIZE 20
extern	BOOL ReadFileAble;
TCHAR   szFileName[PATH_SIZE]=TEXT ("NetUpdate.log");

char *	OutputStr;
int		OutputLen=0;
int		Buf_Num=1;                                    //缓冲区块的个数
int		iFileLength=0;
int     RealLength=0;
HWND	hwnd;  
HANDLE	hSemaphore;                                     //窗口句柄
HWND	hWndEdit;
BOOL    InitWindow (HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WinProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
DWORD	WINAPI SocketThread(LPVOID lpParam) ;
int		QuerySaveFile(HWND hWnd);
/*******************************************************************
函数：WinMain ()
功能：Win32 应用程序入口函数。创建主窗口，处理消息循环
********************************************************************/
int WINAPI WinMain (HINSTANCE hInstance,         //当前实例句柄
                    HINSTANCE hPrevInstance,     //前一个实例句柄
                    PSTR szCmdLine,              //命令行字符
                    int iCmdShow)                //窗口显示方式
{
	MSG msg;
	char *p;
	//创建主窗口
	if (!InitWindow (hInstance, iCmdShow))
		return FALSE;

	p=0;
	//进入消息循环：从该应用程序的消息队列中检取消息，
	//送到消息处理过程，当检取到WM_QUIT消息时，退出消息循环。
	while (GetMessage (&msg, NULL, 0, 0))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	return msg.wParam;
}

/*******************************************************************
函数：InitWindow ()
功能：创建窗口。
*******************************************************************/
static BOOL InitWindow (HINSTANCE hInstance, int iCmdShow)
{
	static TCHAR szAppName[] = TEXT ("EasyWin");  //应用程序名称

	WNDCLASS wcMainWnd;                           //窗口类结构
	ATOM a;
	HMENU hMenu;
    hMenu = LoadMenu (hInstance, MAKEINTRESOURCE(IDR_MENU1));

	//填充窗口类结构
	wcMainWnd.style = CS_VREDRAW | CS_HREDRAW;									//窗口的风格
	wcMainWnd.lpfnWndProc = WinProc;											//指定窗口的消息处理函数的远指针
	wcMainWnd.cbClsExtra = 0;													//指定分配给窗口类结构之后的额外字节数
	wcMainWnd.cbWndExtra = 0;													//指定分配给窗口实例之后的额外字节数
	wcMainWnd.hInstance = hInstance;											//指定窗口过程所对应的实例句柄
	wcMainWnd.hIcon = LoadIcon (hInstance, IDI_APPLICATION);					//指定窗口的图标
//	wcMainWnd.hCursor = LoadCursor (hInstance, MAKEINTRESOURCE(IDC_CURSOR1));	//指定窗口的鼠标
	wcMainWnd.hCursor = LoadCursor (NULL, IDC_ARROW);
	wcMainWnd.hbrBackground = (HBRUSH)GetStockObject (WHITE_BRUSH);				//指定窗口的背景画刷
	wcMainWnd.lpszMenuName = NULL;												//窗口的菜单资源名称
	wcMainWnd.lpszClassName = szAppName;										//该窗口类的名称

	//注册窗口类
	a = RegisterClass (&wcMainWnd);
		
	if (!a)
	{
		MessageBox (NULL, TEXT ("注册窗口类失败！"), szAppName,
                    MB_ICONERROR);
		return 0;
	}

	//创建主窗口
	hwnd = CreateWindowEx (
						WS_EX_DLGMODALFRAME,
						szAppName, 										//窗口类名称
	                    TEXT ("机顶盒网络升级"),						//窗口标题
                     	WS_OVERLAPPEDWINDOW,		                    //窗口风格
                        400,											//窗口位置的x坐标
                        100,											//窗口位置的y坐标
                        500,											//窗口的宽度
                        500,											//窗口的高度
                        NULL,											//父窗口句柄
                        hMenu,											//菜单句柄
                        hInstance,										//应用程序实例句柄
                        NULL);											//窗口创建数据指针

	if( !hwnd ) return FALSE;

	//显示并更新窗口
	ShowWindow( hwnd, iCmdShow );
	UpdateWindow( hwnd );
	return TRUE;
}
/*****************************************************************************
函数：  QuerySaveFile(HWND, TCHAR *);
功能：  弹出消息框，提醒文件内容改变，询问是否要求保存。
		当用户选择“是”按钮时执行文件保存操作。
返回值：IDYES（保存）；IDNO（不保存）；IDCANCEL（取消操作）
*************************************************************************/
int QuerySaveFile(HWND hWnd)
{
    TCHAR szBuffer[PATH_SIZE*3];
	int iResponse;

	wsprintf (szBuffer, TEXT ("文件 %s 的内容已经更改，是否需要保存?"),szFileName ) ;
    
	iResponse = MessageBox (hWnd, szBuffer, TEXT ("文件操作演示程序"),
							MB_YESNO| MB_ICONQUESTION) ;
   
	if (iResponse == IDYES)
		if (!SendMessage (hWnd, WM_COMMAND, MY_FILE_SAVE, 0))
			iResponse = IDCANCEL;
    
	return (iResponse);
}
/******************************************************************
函数：WinProc ()
功能：处理主窗口消息
*******************************************************************/
LRESULT CALLBACK WinProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC         hdc;
	PAINTSTRUCT ps;
	RECT        rect;
    DWORD dwThreadParam, dwThreadID;
	HANDLE hThread;
	int print_len=0,printed_len=0;
    static HMENU hMenuPop;

	static HANDLE         hFile;
	static BOOL          bFileChange = FALSE;
	DWORD                dwBytes;
	int                  Fileoffset=0;
	switch (message)
	{
		case WM_CREATE:    //创建消息

		hWndEdit=CreateWindow (TEXT ("edit"),			                         //窗口类名
							  NULL,			                                     //无标题
							  WS_CHILD|WS_VISIBLE|WS_HSCROLL|	                 //编辑控件风格
							  WS_VSCROLL|WS_BORDER|ES_LEFT|
							  ES_MULTILINE|ES_AUTOHSCROLL|
							  ES_AUTOVSCROLL,
							  0,0,0,0,	
							  hWnd,		                                         //父窗口句柄
							  (HMENU)1,		                                     //编辑控件子窗口标识
							  (HINSTANCE) GetWindowLong (hWnd, GWL_HINSTANCE),
							  NULL);
	    /*读入日志文件*/
	    hFile = CreateFile (szFileName, GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_ALWAYS , 0, NULL);
		if ((DWORD)hFile == -1)
		{
			MessageBox (hWnd, TEXT ("打开指定文件操作失败"),
			TEXT ("文件操作演示程序"), MB_OK | MB_ICONERROR);
			szFileName[0]  = '\0';
		}
		/*读取文件内容*/
		else
		{
			iFileLength=GetFileSize (hFile, NULL);								 //文件的实际大小
			RealLength= min (iFileLength,                        
							(DWORD)SendMessage(hWndEdit, EM_GETLIMITTEXT, 0, 0));//编辑框能容纳的最大文本字数
			if(RealLength < iFileLength)                                         //若无法读取全部文件数据到编辑框中
			{                                                                    //则只读入文件后半部分数据
				Fileoffset=RealLength;                                           //文件指针的便宜位置
				RealLength=iFileLength-RealLength;
			}
			OutputLen=RealLength;                                                //将实际读入的文本长度赋予OutputLen
            if(OutputLen>OUTBUF_SIZE)
			{
				Buf_Num=(OutputLen/OUTBUF_SIZE)+1;
			}
			OutputStr=( char *) malloc (OUTBUF_SIZE*Buf_Num);
			memset(OutputStr,0,OUTBUF_SIZE*Buf_Num);
			SetFilePointer(hFile,Fileoffset,0,FILE_BEGIN);
			ReadFile (hFile, OutputStr, OutputLen, &dwBytes, NULL);
			CloseHandle (hFile);
			OutputStr[OutputLen] = '\0';
			SetWindowText (hWndEdit, OutputStr);
		}
       
		hSemaphore = CreateSemaphore(NULL,1,1,NULL);                                //创建信号量 ，最大计数值为1
		hThread =CreateThread(NULL, 0, SocketThread, &dwThreadParam, 0,&dwThreadID);//创建进程	
		return 0;

  	case WM_SIZE:
		MoveWindow (hWndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		return 0;
	case WM_COMMAND: 

  			switch(LOWORD(wParam))
			{
			case MY_FILE_SAVE:
                    hFile = CreateFile (szFileName, GENERIC_WRITE, 0, 
                                    NULL, OPEN_EXISTING, 0, NULL);
					if ((DWORD)hFile == -1)
					{
						MessageBox (hWnd, TEXT ("打开指定文件操作失败"),
							TEXT ("文件操作演示程序"), MB_OK | MB_ICONERROR);
				       break;
					}
					else
					{	
						SetFilePointer(hFile,0,0,FILE_END);
						WriteFile (hFile, OutputStr+RealLength, (OutputLen-RealLength) * sizeof (TCHAR), 
									&dwBytes, NULL);
								
						if (((OutputLen-RealLength )* sizeof (TCHAR)) != (int) dwBytes)
						{
							MessageBox (hWnd, TEXT ("文件写入操作失败"),
								TEXT ("文件操作演示程序"), MB_OK | MB_USERICON);
							break;
						}
						else
						{
							RealLength=OutputLen;
							CloseHandle (hFile);
							MessageBox (hWnd, TEXT ("日志保存成功"),
								TEXT ("文件操作"), MB_OK );
							break;
						}
					}
				break;
			case MY_FILE_UPDATE:
				
				switch(ReadFileAble)
				{
				case CONNECTING:
					 MessageBox(hWnd,TEXT("文件正在传输，请稍后再试！"),TEXT("操作提示"),MB_OK);
					 break;
				case READFILE:
					 MessageBox(hWnd,TEXT("正在导入文件，请稍后再试！"),TEXT("操作提示"),MB_OK);
					 break;
				case WAITACCEPT:
			         ReadFileAble=READFILE;
					 break;
				}
				break;	
            case MY_CLOSE:
				SendMessage (hWnd, WM_CLOSE, 0, 0);
				break;


				} 

			return 0;
  
  	case WM_SETFOCUS:
		SetFocus (hWndEdit);
		return 0;



	case WM_PAINT:																//客户区重绘消息
			
		hdc = BeginPaint (hWnd, &ps); 											//取得设备环境句柄			
		GetClientRect (hWnd, &rect);											//取得窗口客户区矩形	
	//	SetTextColor (hdc, RGB(0,0,255));										//设置文字颜色
	//	DrawText(hdc,OutputStr,OutputLen,&rect,DT_WORDBREAK|DT_EXPANDTABS);		//输出文字
		EndPaint (hWnd, &ps);
		return 0;

	case WM_CLOSE:  
		ReadFileAble=KILLTHREAD;
		if(	RealLength<OutputLen)
		QuerySaveFile(hWnd);
		LogDisplay("退出程序\r\n\r\n");
		hFile = CreateFile (szFileName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if ((DWORD)hFile == -1)
		{
			MessageBox (hWnd, TEXT ("打开指定文件操作失败"),
							TEXT ("文件操作演示程序"), MB_OK | MB_ICONERROR);
			break;
		}
		SetFilePointer(hFile,0,0,FILE_END);
		WriteFile (hFile, OutputStr+RealLength, (OutputLen-RealLength)*sizeof (TCHAR), &dwBytes, NULL);
		CloseHandle (hFile);
		DestroyWindow(hWnd); 
		return 0;
	case WM_DESTROY:
		free(OutputStr);
		PostQuitMessage(0);
		return 0;

}

return DefWindowProc (hWnd, message, wParam, lParam);	    //调用缺省消息处理过程

}                                                           //函数 WinProc 结束

DWORD WINAPI SocketThread(LPVOID lpParam) 
{
	SeverSocket(); 
	LogDisplay("网络线程已退出\r\n");
	return 0;
}


void LogDisplay( char *str)
{
  char temp_buf[200];										//buf的大小根据要输出显示的最大字串长度而定
  char * extend_buf;
  char  time_buf[28];
  SYSTEMTIME systime;
  GetLocalTime(&systime);									//获取本地时间；

  if(systime.wSecond<=9)
	wsprintf(time_buf,"%d-%d-%d %d:%d:0%d  ",
			systime.wYear,systime.wMonth,systime.wDay,
			systime.wHour,systime.wMinute,systime.wSecond);//解决输出时9无法被刷新的问题
  else
	wsprintf(time_buf,"%d-%d-%d %d:%d:%d  ",
			systime.wYear,systime.wMonth,systime.wDay,
		    systime.wHour,systime.wMinute,systime.wSecond);

  memset(temp_buf,0,200);
  sprintf(temp_buf,str);
  WaitForSingleObject(hSemaphore,INFINITE);					//等待信号量
  if(OutputLen+200>OUTBUF_SIZE*Buf_Num)
  {
	extend_buf=( char *) malloc (OUTBUF_SIZE*Buf_Num);	    //如果缓冲区将满，扩展缓冲区
	memset(extend_buf,0,OUTBUF_SIZE*Buf_Num);
	memcpy(extend_buf,OutputStr,OutputLen);
	free(OutputStr);
	Buf_Num++;
 	OutputStr=( char *) malloc (OUTBUF_SIZE*Buf_Num);
	memset(OutputStr,0,OUTBUF_SIZE*Buf_Num);
	memcpy(OutputStr,extend_buf,OutputLen);
	free(extend_buf);
  }

  memcpy(OutputStr+OutputLen,time_buf,strlen(time_buf));    //将时间与字符串保存到输出缓冲区OutputStr
   OutputLen+=strlen(time_buf);
  memcpy(OutputStr+OutputLen,temp_buf,strlen(temp_buf));
  OutputLen+=strlen(temp_buf);
  ReleaseSemaphore(hSemaphore,1,NULL);
  SetWindowText (hWndEdit,OutputStr);
  SendMessage(hWndEdit,WM_VSCROLL,(WPARAM)SB_BOTTOM,0);     //直接滚动到底部
  //SendMessage(hWndEdit,EM_LINESCROLL,0,0);                //设置滚动条滚动20行（行数自定）
  //InvalidateRect(hwnd, NULL, TRUE);
  //UpdateWindow(hwnd);//修改
}

