/***************************************************************************************************************
name:		DES.H
author:		yuxiang
date:       2011.9.21
note��		������ʹ�õ���RSA 512λ�����㷨��Ĭ��Ϊ��ԿN����Ϊ64���ֽڣ����ֽ�Ϊ��Ԫ�洢��
			���б䶯���뼰ʱ�޸�RSA.h�е�NSIZE��UNIT_BITS���Լ���������������������(��UNIT_BITS�Ķ�)��
			
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
#define NSIZE  64      //��ԿN�洢���ֽڳ��ȣ����б䶯 ��ʱ�޸�
#endif
#define UNIT_BITS  8   // ��ԿN�洢��ʽ�������ֽ�Ϊ��Ԫ�洢�����б䶯��ʱ�޸�
*/

/*�����ļ� author by yx */
int DES_Decrypt(unsigned char *cipherbuf,unsigned char *keyStr,unsigned char *plainbuf, unsigned long  Num_len);

//int DES_Encrypt(char *plainFile, char *keyStr,char *cipherFile);

#endif



