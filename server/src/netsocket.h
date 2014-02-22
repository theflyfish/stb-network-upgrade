#ifndef __NETSOCKET_H__
#define __NETSOCKET_H__

#include <stdio.h>
#include <Winsock2.h>
#include <conio.h>
#include <process.h>
#include <stddef.h>
#include "crc32.h"
#include "des.h"

#define UPDATE_ID        1
#define CLOSED_ID        2
#define RESEND_ID        3

#define KILLTHREAD       -2
#define CONNECTING       -1
#define READFILE         0
#define WAITACCEPT       4

#define BUF_SIZE         1024
#define FLIE_BUFFER_SIZE 1024*1024*4  
#define FILE_NAME        "main.z" 
#define DES_FILE_NAME    "encrypt_main.z" 
#define STB_VERSION_FILE "version.bin"

#define OUTBUF_SIZE      1024*1024

void LogDisplay( char *str); //´òÓ¡Êä³ö
void  SeverSocket(void);

#endif

