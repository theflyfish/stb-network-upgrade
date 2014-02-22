/*******************************************************************
����Netupdate
�ļ���main.c
���ܣ�WindowsӦ�ó��򴰿ڿ��
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
int		Buf_Num=1;                                    //��������ĸ���
int		iFileLength=0;
int     RealLength=0;
HWND	hwnd;  
HANDLE	hSemaphore;                                     //���ھ��
HWND	hWndEdit;
BOOL    InitWindow (HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WinProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
DWORD	WINAPI SocketThread(LPVOID lpParam) ;
int		QuerySaveFile(HWND hWnd);
/*******************************************************************
������WinMain ()
���ܣ�Win32 Ӧ�ó�����ں��������������ڣ�������Ϣѭ��
********************************************************************/
int WINAPI WinMain (HINSTANCE hInstance,         //��ǰʵ�����
                    HINSTANCE hPrevInstance,     //ǰһ��ʵ�����
                    PSTR szCmdLine,              //�������ַ�
                    int iCmdShow)                //������ʾ��ʽ
{
	MSG msg;
	char *p;
	//����������
	if (!InitWindow (hInstance, iCmdShow))
		return FALSE;

	p=0;
	//������Ϣѭ�����Ӹ�Ӧ�ó������Ϣ�����м�ȡ��Ϣ��
	//�͵���Ϣ������̣�����ȡ��WM_QUIT��Ϣʱ���˳���Ϣѭ����
	while (GetMessage (&msg, NULL, 0, 0))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	return msg.wParam;
}

/*******************************************************************
������InitWindow ()
���ܣ��������ڡ�
*******************************************************************/
static BOOL InitWindow (HINSTANCE hInstance, int iCmdShow)
{
	static TCHAR szAppName[] = TEXT ("EasyWin");  //Ӧ�ó�������

	WNDCLASS wcMainWnd;                           //������ṹ
	ATOM a;
	HMENU hMenu;
    hMenu = LoadMenu (hInstance, MAKEINTRESOURCE(IDR_MENU1));

	//��䴰����ṹ
	wcMainWnd.style = CS_VREDRAW | CS_HREDRAW;									//���ڵķ��
	wcMainWnd.lpfnWndProc = WinProc;											//ָ�����ڵ���Ϣ��������Զָ��
	wcMainWnd.cbClsExtra = 0;													//ָ�������������ṹ֮��Ķ����ֽ���
	wcMainWnd.cbWndExtra = 0;													//ָ�����������ʵ��֮��Ķ����ֽ���
	wcMainWnd.hInstance = hInstance;											//ָ�����ڹ�������Ӧ��ʵ�����
	wcMainWnd.hIcon = LoadIcon (hInstance, IDI_APPLICATION);					//ָ�����ڵ�ͼ��
//	wcMainWnd.hCursor = LoadCursor (hInstance, MAKEINTRESOURCE(IDC_CURSOR1));	//ָ�����ڵ����
	wcMainWnd.hCursor = LoadCursor (NULL, IDC_ARROW);
	wcMainWnd.hbrBackground = (HBRUSH)GetStockObject (WHITE_BRUSH);				//ָ�����ڵı�����ˢ
	wcMainWnd.lpszMenuName = NULL;												//���ڵĲ˵���Դ����
	wcMainWnd.lpszClassName = szAppName;										//�ô����������

	//ע�ᴰ����
	a = RegisterClass (&wcMainWnd);
		
	if (!a)
	{
		MessageBox (NULL, TEXT ("ע�ᴰ����ʧ�ܣ�"), szAppName,
                    MB_ICONERROR);
		return 0;
	}

	//����������
	hwnd = CreateWindowEx (
						WS_EX_DLGMODALFRAME,
						szAppName, 										//����������
	                    TEXT ("��������������"),						//���ڱ���
                     	WS_OVERLAPPEDWINDOW,		                    //���ڷ��
                        400,											//����λ�õ�x����
                        100,											//����λ�õ�y����
                        500,											//���ڵĿ��
                        500,											//���ڵĸ߶�
                        NULL,											//�����ھ��
                        hMenu,											//�˵����
                        hInstance,										//Ӧ�ó���ʵ�����
                        NULL);											//���ڴ�������ָ��

	if( !hwnd ) return FALSE;

	//��ʾ�����´���
	ShowWindow( hwnd, iCmdShow );
	UpdateWindow( hwnd );
	return TRUE;
}
/*****************************************************************************
������  QuerySaveFile(HWND, TCHAR *);
���ܣ�  ������Ϣ�������ļ����ݸı䣬ѯ���Ƿ�Ҫ�󱣴档
		���û�ѡ���ǡ���ťʱִ���ļ����������
����ֵ��IDYES�����棩��IDNO�������棩��IDCANCEL��ȡ��������
*************************************************************************/
int QuerySaveFile(HWND hWnd)
{
    TCHAR szBuffer[PATH_SIZE*3];
	int iResponse;

	wsprintf (szBuffer, TEXT ("�ļ� %s �������Ѿ����ģ��Ƿ���Ҫ����?"),szFileName ) ;
    
	iResponse = MessageBox (hWnd, szBuffer, TEXT ("�ļ�������ʾ����"),
							MB_YESNO| MB_ICONQUESTION) ;
   
	if (iResponse == IDYES)
		if (!SendMessage (hWnd, WM_COMMAND, MY_FILE_SAVE, 0))
			iResponse = IDCANCEL;
    
	return (iResponse);
}
/******************************************************************
������WinProc ()
���ܣ�������������Ϣ
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
		case WM_CREATE:    //������Ϣ

		hWndEdit=CreateWindow (TEXT ("edit"),			                         //��������
							  NULL,			                                     //�ޱ���
							  WS_CHILD|WS_VISIBLE|WS_HSCROLL|	                 //�༭�ؼ����
							  WS_VSCROLL|WS_BORDER|ES_LEFT|
							  ES_MULTILINE|ES_AUTOHSCROLL|
							  ES_AUTOVSCROLL,
							  0,0,0,0,	
							  hWnd,		                                         //�����ھ��
							  (HMENU)1,		                                     //�༭�ؼ��Ӵ��ڱ�ʶ
							  (HINSTANCE) GetWindowLong (hWnd, GWL_HINSTANCE),
							  NULL);
	    /*������־�ļ�*/
	    hFile = CreateFile (szFileName, GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_ALWAYS , 0, NULL);
		if ((DWORD)hFile == -1)
		{
			MessageBox (hWnd, TEXT ("��ָ���ļ�����ʧ��"),
			TEXT ("�ļ�������ʾ����"), MB_OK | MB_ICONERROR);
			szFileName[0]  = '\0';
		}
		/*��ȡ�ļ�����*/
		else
		{
			iFileLength=GetFileSize (hFile, NULL);								 //�ļ���ʵ�ʴ�С
			RealLength= min (iFileLength,                        
							(DWORD)SendMessage(hWndEdit, EM_GETLIMITTEXT, 0, 0));//�༭�������ɵ�����ı�����
			if(RealLength < iFileLength)                                         //���޷���ȡȫ���ļ����ݵ��༭����
			{                                                                    //��ֻ�����ļ���벿������
				Fileoffset=RealLength;                                           //�ļ�ָ��ı���λ��
				RealLength=iFileLength-RealLength;
			}
			OutputLen=RealLength;                                                //��ʵ�ʶ�����ı����ȸ���OutputLen
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
       
		hSemaphore = CreateSemaphore(NULL,1,1,NULL);                                //�����ź��� ��������ֵΪ1
		hThread =CreateThread(NULL, 0, SocketThread, &dwThreadParam, 0,&dwThreadID);//��������	
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
						MessageBox (hWnd, TEXT ("��ָ���ļ�����ʧ��"),
							TEXT ("�ļ�������ʾ����"), MB_OK | MB_ICONERROR);
				       break;
					}
					else
					{	
						SetFilePointer(hFile,0,0,FILE_END);
						WriteFile (hFile, OutputStr+RealLength, (OutputLen-RealLength) * sizeof (TCHAR), 
									&dwBytes, NULL);
								
						if (((OutputLen-RealLength )* sizeof (TCHAR)) != (int) dwBytes)
						{
							MessageBox (hWnd, TEXT ("�ļ�д�����ʧ��"),
								TEXT ("�ļ�������ʾ����"), MB_OK | MB_USERICON);
							break;
						}
						else
						{
							RealLength=OutputLen;
							CloseHandle (hFile);
							MessageBox (hWnd, TEXT ("��־����ɹ�"),
								TEXT ("�ļ�����"), MB_OK );
							break;
						}
					}
				break;
			case MY_FILE_UPDATE:
				
				switch(ReadFileAble)
				{
				case CONNECTING:
					 MessageBox(hWnd,TEXT("�ļ����ڴ��䣬���Ժ����ԣ�"),TEXT("������ʾ"),MB_OK);
					 break;
				case READFILE:
					 MessageBox(hWnd,TEXT("���ڵ����ļ������Ժ����ԣ�"),TEXT("������ʾ"),MB_OK);
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



	case WM_PAINT:																//�ͻ����ػ���Ϣ
			
		hdc = BeginPaint (hWnd, &ps); 											//ȡ���豸�������			
		GetClientRect (hWnd, &rect);											//ȡ�ô��ڿͻ�������	
	//	SetTextColor (hdc, RGB(0,0,255));										//����������ɫ
	//	DrawText(hdc,OutputStr,OutputLen,&rect,DT_WORDBREAK|DT_EXPANDTABS);		//�������
		EndPaint (hWnd, &ps);
		return 0;

	case WM_CLOSE:  
		ReadFileAble=KILLTHREAD;
		if(	RealLength<OutputLen)
		QuerySaveFile(hWnd);
		LogDisplay("�˳�����\r\n\r\n");
		hFile = CreateFile (szFileName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if ((DWORD)hFile == -1)
		{
			MessageBox (hWnd, TEXT ("��ָ���ļ�����ʧ��"),
							TEXT ("�ļ�������ʾ����"), MB_OK | MB_ICONERROR);
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

return DefWindowProc (hWnd, message, wParam, lParam);	    //����ȱʡ��Ϣ�������

}                                                           //���� WinProc ����

DWORD WINAPI SocketThread(LPVOID lpParam) 
{
	SeverSocket(); 
	LogDisplay("�����߳����˳�\r\n");
	return 0;
}


void LogDisplay( char *str)
{
  char temp_buf[200];										//buf�Ĵ�С����Ҫ�����ʾ������ִ����ȶ���
  char * extend_buf;
  char  time_buf[28];
  SYSTEMTIME systime;
  GetLocalTime(&systime);									//��ȡ����ʱ�䣻

  if(systime.wSecond<=9)
	wsprintf(time_buf,"%d-%d-%d %d:%d:0%d  ",
			systime.wYear,systime.wMonth,systime.wDay,
			systime.wHour,systime.wMinute,systime.wSecond);//������ʱ9�޷���ˢ�µ�����
  else
	wsprintf(time_buf,"%d-%d-%d %d:%d:%d  ",
			systime.wYear,systime.wMonth,systime.wDay,
		    systime.wHour,systime.wMinute,systime.wSecond);

  memset(temp_buf,0,200);
  sprintf(temp_buf,str);
  WaitForSingleObject(hSemaphore,INFINITE);					//�ȴ��ź���
  if(OutputLen+200>OUTBUF_SIZE*Buf_Num)
  {
	extend_buf=( char *) malloc (OUTBUF_SIZE*Buf_Num);	    //�����������������չ������
	memset(extend_buf,0,OUTBUF_SIZE*Buf_Num);
	memcpy(extend_buf,OutputStr,OutputLen);
	free(OutputStr);
	Buf_Num++;
 	OutputStr=( char *) malloc (OUTBUF_SIZE*Buf_Num);
	memset(OutputStr,0,OUTBUF_SIZE*Buf_Num);
	memcpy(OutputStr,extend_buf,OutputLen);
	free(extend_buf);
  }

  memcpy(OutputStr+OutputLen,time_buf,strlen(time_buf));    //��ʱ�����ַ������浽���������OutputStr
   OutputLen+=strlen(time_buf);
  memcpy(OutputStr+OutputLen,temp_buf,strlen(temp_buf));
  OutputLen+=strlen(temp_buf);
  ReleaseSemaphore(hSemaphore,1,NULL);
  SetWindowText (hWndEdit,OutputStr);
  SendMessage(hWndEdit,WM_VSCROLL,(WPARAM)SB_BOTTOM,0);     //ֱ�ӹ������ײ�
  //SendMessage(hWndEdit,EM_LINESCROLL,0,0);                //���ù���������20�У������Զ���
  //InvalidateRect(hwnd, NULL, TRUE);
  //UpdateWindow(hwnd);//�޸�
}

