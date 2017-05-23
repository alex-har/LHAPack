// LHAPack.h: interface for the LHAPack class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LHAPACK_H__14A268E4_D61D_489C_9752_47C08E1872AA__INCLUDED_)
#define AFX_LHAPACK_H__14A268E4_D61D_489C_9752_47C08E1872AA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <time.h>
#include <limits.h>

#define METHOD_TYPE_STORAGE     5
#define FILENAME_LENGTH         1024

#define CHAR_BIT 8
//#define UCHAR_MAX ((1<<(sizeof(unsigned char)*8))-1)


typedef struct LHAHeader {
    size_t          header_size;
    int             size_field_length;
    char            method[METHOD_TYPE_STORAGE];
    size_t          packed_size;
    size_t          original_size;
    unsigned char   attribute;
    unsigned char   header_level;
    char            name[FILENAME_LENGTH];
    char            realname[FILENAME_LENGTH];/* real name for symbolic link */
    unsigned int    crc;        /* file CRC */
    BOOL            has_crc;    /* file CRC */
    unsigned int    header_crc; /* header CRC */
    unsigned char   extend_type;
    unsigned char   minor_version;

    /* extend_type == EXTEND_UNIX  and convert from other type. */
    time_t          unix_last_modified_stamp;	
    unsigned short  unix_mode;
    unsigned short  unix_uid;
    unsigned short  unix_gid;
    char            user[256];
    char            group[256];
}  LHAHeader;

class LHAPack  
{
public:
	bool get_header(const char *pMem, LHAHeader *hdr);
	int calc_sum(char *p,int len);
	LHAPack();
	virtual ~LHAPack();
public:
	/* extend for me. */
	SYSTEMTIME      win32_systemtime;
	int             dataoffset;
	bool            generic_format;
private:
	SYSTEMTIME unix_to_win32_systemtime(time_t t);
	FILETIME unix_to_win32_filetime(time_t t);
	unsigned int calccrc(unsigned int crc, unsigned char *p, unsigned int n);
	void make_crctable();
	bool get_header_level3(LHAHeader *hdr, char *data);
	bool get_header_level2(LHAHeader *hdr, char *data);
	bool get_header_level1(LHAHeader *hdr, char *data);
	bool get_header_level0(LHAHeader *hdr, char* data);
	void write_unix_info(LHAHeader *hdr);
	size_t write_header_level0(LHAHeader *hdr, char* data, char* pathname);
	size_t write_header_level1(LHAHeader *hdr, char* data, char* pathname);
	size_t write_header_level2(LHAHeader *hdr, char* data, char* pathname);
	int get_extended_header(LHAHeader *hdr, size_t header_size, unsigned int *hcrc);
	unsigned long wintime_to_unix_stamp();
	long unix_to_generic_stamp(time_t t);
	time_t generic_to_unix_stamp(long t);
	void put_bytes(char* buf, int len);
	int get_bytes(char *buf, int len, int size);
	void put_longword(long v);
	long get_longword();
	void put_word(unsigned int v);
	int get_word();
	
	char    *get_ptr;
	char    *mem_ptr;
	unsigned int crctable[UCHAR_MAX + 1];

};

#endif // !defined(AFX_LHAPACK_H__14A268E4_D61D_489C_9752_47C08E1872AA__INCLUDED_)
