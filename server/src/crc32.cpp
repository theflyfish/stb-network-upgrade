/*********************************************************************************************
 *		C r c 3 2 . C
 *
 *		2008-5-9  Liuchl
 *
 *********************************************************************************************/
#include <stdio.h>
#include <string.h>
//#include "stddefs.h"

#define cnCRC_32 0xEDB88320
 
void BuildTable32( unsigned long aPoly ,unsigned long* Table_CRC) 
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

unsigned long CRC_32( unsigned char * aData, unsigned long aSize ) 
{ 
	unsigned long i; 
	unsigned long nAccum = 0xffffffff; 
	unsigned long Table_CRC[256];
	BuildTable32( cnCRC_32,Table_CRC ); 
	for ( i = 0; i < aSize; i++ )
//		nAccum = Table_CRC[((U8)(nAccum)) ^ *aData] ^ (nAccum >> 8);
//		nAccum = Table_CRC[((unsigned char)nAccum^((unsigned char*)aData)[i])] ^ (nAccum>>8);//YX ע
		nAccum = Table_CRC[((unsigned char)nAccum^(*(unsigned char*)aData++))] ^ (nAccum>>8);
//		nAccum = ( nAccum << 8 ) ^ Table_CRC[( nAccum >> 24 ) ^ *aData++]; 
	return ~nAccum;
}
/*
void main()
{
    FILE *file = NULL;
    unsigned long crc32;
    unsigned long temp;
	unsigned char original_buf[10]={0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x40};
	unsigned char crced_buf[14];
	int i=0;
	memset(crced_buf,0,14);
	memcpy(crced_buf,original_buf,10);
	while(i++<13)
	{
		printf("%x,",crced_buf[i]);
	}
    printf("\n");
	i=0;
	getch();
	crc32=CRC_32(original_buf,10);
    printf("0x%X\n",crc32);
	crced_buf[10]=(crc32&0xff000000)>>24;
    crced_buf[11]=(crc32&0x00ff0000)>>16;
	crced_buf[12]=(crc32&0x0000ff00)>>8;
	crced_buf[13]=(crc32&0x000000ff);
	i=0;
	while(i++<13)
	{
		printf("%x,",crced_buf[i]);
	}
	printf("\n");
	getch();  
	crc32=0;
	crc32=CRC_32(original_buf,10);
	printf("0x%X\n",crc32);
	getch();
	temp=(crced_buf[10]<<24)|(crced_buf[11]<<16)|(crced_buf[12])<<8|(crced_buf[13]);
	printf("temp=0x%X\n",temp);
	if(temp==crc32)
	printf("it is OK!\n");
		getch();
}
*/


