// JPGdate.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include<io.h>
#include "JPGdate.h"
#include <sys/utime.h>


void PrintHelp()
{// v1.0 2013, v2.0 /offset 2014
	  printf("JPGdate - fix JPG file date, v2.0 (C) PKAR 2014\n");
	  printf("Usage:\n");
	  printf("jpgdate /COMMAND FILEMASK\n");
	  printf("where COMMAND is one of:\n");
	  printf("  view - print file createdate and createtime JFIF tag\n");
	  printf("  fix - set file createdate from createtime JFIF tag\n");
	  printf("  debug = view, z info w trakcie skanowania JPG\n");
	  printf("/fix /offset WhatSignCount , where What=y/n/d/h/m/s Sign=+/- Count=how many\n");
	  printf("\n");
}


#define BUFMAX (1000*1024)


unsigned char b[BUFMAX+10];
int fsize;
int iIndian=0;

#define Get16LE(ind) ( (b[ind+1]<<8) | (b[ind]) )
#define Get16BE(ind) ( (b[ind]<<8) | (b[ind+1]) )

int Get16(int ind)
{
	if(iIndian==1) return Get16BE(ind);
	return Get16LE(ind);
}

int Get32(int ind)
// ograniczenie do 16 bitow, ale i tak lepiej :)
{
	if(iIndian==1) return Get16BE(ind);
	return Get16LE(ind-2);
}


// main work
int	GetExifDate(char*sFileName,bool bDebug)
{
 if(!strcmp(sFileName,".")) return 0;
 if(!strcmp(sFileName,"..")) return 0;

  FILE* f=fopen(sFileName, "rb");
  if(!f)
  {
	  printf(" strange ERROR: File not found\n");
	  return -2;
  }

 fsize=fread(b,1,BUFMAX,f);
 fclose(f);
 if(b[0]!=0xff || b[1]!=0xd8)
 {
	 printf(" probably not JPG (bad magic)\n");
	 return -3;
 }

 int i,len;
 i=2;
 while(i<fsize)
 {
	 if(b[i]-0xff)
			{printf(" JPG struct error\n"); return -4;}

	 len=Get16BE(i+2);
	 switch(b[i+1])
	 { // http://www.digicamsoft.com/itu/itu-t81-36.html
	 case 0xda: // printf(" %04x Start Of Scan, ",i); 
		 i++;
		 while((b[i]!=0xff || b[i+1]==0)&& i<fsize) i++;
		 if(i<fsize)
		 {
			 if(bDebug) printf("next segment mark found at %d\n",i);
			 len=0;
			 i-=2;
		 }
		 else
		 {
			 if(bDebug) printf(", and buffer ends :) \n");
//!!			 return 0;
		 }
		 break;
	 case 0xe1: // exif, czyli dane o obrazku
				if(memcmp(&b[i+4],"Exif\0\0",6)) break;
			 if(bDebug) printf(" %04x Application specific %d, len=%x\n",i,b[i+1]&15,len);
			 if(bDebug) printf("   Exif data, ");
				if(!memcmp(&b[i+10],"MM",2)) iIndian=1;
				if(!memcmp(&b[i+10],"II",2)) iIndian=2;
				if(!iIndian) { printf(" Exif struct error 01 (endianness)\n"); return -5;}

				if(iIndian==1)
				{
					if(b[i+13]-0x2a || b[i+12])
						{printf(" EXIF struct error 02\n"); return -6;}
					if(b[i+14] || b[i+15] || b[i+16] || (b[i+17]-8))
						{printf(" EXIF struct error 03\n"); return -7;}
				}
				else
				{
					if(b[i+12]-0x2a || b[i+13])
						{printf(" EXIF struct error 02\n"); return -6;}
					if(b[i+17] || b[i+16] || b[i+15] || (b[i+14]-8))
						{printf(" EXIF struct error 03\n"); return -7;}
				}

//					{printf(" unsupported offset IFD (!=8)\n"); break;}
				
				// na starcie: i = poczatek segmentu, i+10: zero dla offsetow
				{int j;
				
				 j=18;	// pointer wewnatrz Exif
				 
				 while(j<len)
				 {int cnt;
					// count of elements w tej tabelce
 				    cnt=Get16(i+j);

					j+=2;	// poczatek tabelki

					for(;cnt;cnt--, j+=12)	// przeiterowanie jednego zestawu tagow
					{int tag,ttype,tlen,toff;
					 tag=Get16(i+j); ttype=Get16(i+j+2);
					 tlen=Get32(i+j+6); toff=Get32(i+j+10); // *PK* i+4 zakladam 0,0

					// data tag (ttype=2, tag=0x132)
					 if(bDebug)
					 {
						 printf("     tag=%4x, first bytes = %02x %02x %02x %02x\n",tag,b[i+10+toff],b[i+10+toff+1],b[i+10+toff+2],b[i+10+toff+3]);
					 }
					 if(tag==0x132)
						 return i+10+toff;

					} // for

					j=Get32(i+j+2)+10;	// niby Get32BE(i+j), ale...
					if(j==10) break;	// offset nastepnego = 0, czyli koniec zabawt
				 } // while
				} // inicjalizacja zmiennych (zeby nie bylo initialization of 'identifier' is skipped by 'case' label)
				break;


	 }
	 i+=2+len;

 }
 return -99;	// file sie skonczyl w buforze
}

char*Ftime2Ascii(__time64_t tTime)
{static char sBuff[128];

 strftime(sBuff,120,"%Y:%m:%d %H:%M:%S",localtime(&tTime));
 return sBuff;
}

// iCmd: 0=view, 1=fix
int oY,oN,oD,oH,oM,oS;	// offset

int OneFile(int iCmd,struct _finddata_t *cfile,bool bDebug)
{time_t tt;
 struct tm t;
 int Y,M,D,h,m,s;

	if(iCmd>1) return -1;
	
	printf("%s",cfile->name);

	int iExifDate;
	iExifDate=GetExifDate(cfile->name,bDebug);
	if(iExifDate<0)
	{
		if(strncmp(cfile->name,"WP_20",5))
		{
			printf(" cannot get Exif date\n");
			return -2;	// error w GetExifDate
		}
		else
		{
			printf(", no Exif date, file date = %s\n",Ftime2Ascii(cfile->time_write));
			sscanf((const char*)&cfile->name[3],"%4d%2d%2d_%d_%d_%d",&Y,&M,&D,&h,&m,&s);
		}

	}
	else	// mamy Exif
	{
		//iExifDate=ptr w b na "2013:07:18 16:59:42"
		printf(", dates: Exif = %s, file = %s\n", &b[iExifDate],Ftime2Ascii(cfile->time_write) );

		if(iCmd==1)
			 sscanf((const char*)&b[iExifDate],"%d:%d:%d %d:%d:%d",&Y,&M,&D,&h,&m,&s);
	}

	// ustalone Y M D h m s	

	if(iCmd==1)	// fix
	{
		t.tm_year=Y-1900+oY;
		t.tm_mon=M-1+oN;
		t.tm_mday=D+oD;
		t.tm_hour=h+oH;
		t.tm_min=m+oM;
		t.tm_sec=s+oS;
		tt=mktime(&t);
		 // teraz tt oraz cfile->time_write maj¹ ten sam typ, time_t, czyli sekundy od Epoch
		 int iSec;
		 iSec=abs(difftime(tt,cfile->time_write));
	//	 iSec=tt-(cfile->time_write);
		 if(iSec > 30)
		 {struct _utimbuf ut;
			ut.actime=tt;
			ut.modtime=tt;
			_utime(cfile->name,&ut);
			printf("   (file time corrected using %s", (iExifDate<0)?"filename":"Exif value");
			if(oY||oN||oD||oH||oM||oS) printf(" with offset %d:%d:%d %d:%d:%d",oY,oN,oD,oH,oM,oS);
			printf(")\n");
		 }
		 else
			 printf("   (times almost same, %d)\n",iSec);
	} // iCmd=1
 return 0;
}

int main(int argc, char* argv[])
{
 int iCmd=0,iMask=1;bool bDebug=false;

 if(argc>1)
 {	

	  if(!strcmp(argv[1],"/?"))
	  {
		PrintHelp();
		exit(0);
	  }

	 oD=oM=oY=oH=oN=oS=0;

	 if(argv[1][0]=='/')
	 {
		 iCmd=-1;
		 iMask=2;
	 }

	 if(!strcmp(argv[1],"/view")) iCmd=0;
	 if(!strcmp(argv[1],"/debug")) {iCmd=0; bDebug=true;}

	 if(!strcmp(argv[1],"/fix"))
	 {
		 iCmd=1;
		 if(argc>2 && !strcmp(argv[2],"/offset"))
		 {
			 if(argc<4)
			 {
				 printf("ERROR: switch /offset requires offset\n");
				 exit(1);
			 }
			 for(int i=0;argv[3][i];i++)
			 {// proste, ale i tak po znalezieniu 'y' ptr bedzie pare pustych przebiegow petli az trafi
			  // na koniec albo na nastepny itemek
				if(argv[3][i]=='y')
					oY=atoi(&(argv[3][i])+1);
				if(argv[3][i]=='n')
					oN=atoi(&(argv[3][i])+1);
				if(argv[3][i]=='d')
					oD=atoi(&(argv[3][i])+1);
				if(argv[3][i]=='h')
					oH=atoi(&(argv[3][i])+1);
				if(argv[3][i]=='m')
					oM=atoi(&(argv[3][i])+1);
				if(argv[3][i]=='s')
					oS=atoi(&(argv[3][i])+1);

			 }
			 iMask=4;
			 printf("Using offset: %d:%d:%d %d:%d:%d\n",oY,oN,oD,oH,oM,oS);
		 }
	 }
 }
 
 char*sMask;
 if(argc-1 == iMask)
	sMask=argv[iMask];
 else
	 sMask="*";

 if(iCmd==-1)
 {
	  printf("ERROR: unrecognized command/switch used (%s)\n",argv[1]);
	  exit(2);
 }

 
 // file loop

   struct _finddata_t c_file;
   intptr_t hFile;

   if( (hFile = _findfirst( sMask, &c_file )) == -1L )
   {
	   printf("ERROR: cannot find files matching %s\n",sMask);
	   exit(3);
   }


   do
   {
	if( (c_file.attrib&0x016) == 0) OneFile(iCmd, &c_file,bDebug);	// nie dir, hidden, system,
   } while( _findnext( hFile, &c_file ) == 0 );

   _findclose( hFile );

}

