/***************************************************************************************************************
name:		DES.H
author:		yuxiang
date:       2011.9.21
note：		本程序使用的是RSA 512位加密算法。默认为密钥N长度为64个字节，以字节为单元存储；
			若有变动，请及时修改RSA.h中的NSIZE与UNIT_BITS；以及各个函数参数数据类型(若UNIT_BITS改动)。
			
*************************************************************************************************************/


#ifndef __DES_H__
#define __DES_H__

/*
#ifndef	U8
typedef unsigned char  U8;
#endif
#ifndef	U16
typedef unsigned short U16;
#endif
#ifndef	U32
typedef unsigned int   U32;
#endif

#ifndef NSIZE
#define NSIZE  64      //密钥N存储的字节长度，若有变动 及时修改
#endif
#define UNIT_BITS  8   // 密钥N存储方式，现以字节为单元存储，若有变动及时修改
*/

/*解密文件 author by yx */
int DES_Decrypt(unsigned char *cipherbuf,unsigned char *keyStr,unsigned char *plainbuf, unsigned long  Num_len);

//int DES_Encrypt(char *plainFile, char *keyStr,char *cipherFile);

#endif



