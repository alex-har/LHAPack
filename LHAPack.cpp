// LHAPack.cpp: implementation of the LHAPack class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "LHAPack.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define CRCPOLY             0xA001      /* CRC-16 (x^16+x^15+x^2+1) */
#define INITIALIZE_CRC(crc) ((crc) = 0)
#define UPDATE_CRC(crc, c)  (crctable[((crc) ^ (c)) & 0xFF] ^ ((crc) >> CHAR_BIT))

#define LZHEADER_STORAGE 4096

#define GET_BYTE()       (*get_ptr++ & 0xff)

#define get_byte()       GET_BYTE()
#define setup_get(PTR)   (get_ptr    = (PTR))
#define skip_bytes(len)  (get_ptr   += (len))

#define put_ptr          get_ptr
#define setup_put(PTR)   ( put_ptr   = (PTR))
#define put_byte(c)      (*put_ptr++ = (char)(c))

LHAPack::LHAPack()
{
	make_crctable();
	generic_format = false;
}

LHAPack::~LHAPack()
{

}

void LHAPack::make_crctable()
{
	unsigned int    i, j, r;
	
    for (i = 0; i <= UCHAR_MAX; i++) {
        r = i;
        for (j = 0; j < CHAR_BIT; j++)
            if (r & 1)
                r = (r >> 1) ^ CRCPOLY;
            else
                r >>= 1;
			crctable[i] = r;
    }
}

unsigned int LHAPack::calccrc(unsigned int crc, unsigned char *p, unsigned int n)
{
	while (n-- > 0)
        crc = UPDATE_CRC(crc, *p++);
    return crc;
}

int LHAPack::calc_sum(char *p, int len)
{
    int sum = 0;
    while (len--) sum += *p++;
    return sum & 0xff;
}

int LHAPack::get_word()
{
	int b0, b1;
    int w;
	
    b0 = GET_BYTE();
    b1 = GET_BYTE();
    w = (b1 << 8) + b0;
	
    return w;
}

void LHAPack::put_word(unsigned int v)
{
	put_byte(v);
    put_byte(v >> 8);
}

long LHAPack::get_longword()
{
	long b0, b1, b2, b3;
    long l;
	
    b0 = GET_BYTE();
    b1 = GET_BYTE();
    b2 = GET_BYTE();
    b3 = GET_BYTE();
    l = (b3 << 24) + (b2 << 16) + (b1 << 8) + b0;
	
    return l;
}

void LHAPack::put_longword(long v)
{
	put_byte(v);
    put_byte(v >> 8);
    put_byte(v >> 16);
    put_byte(v >> 24);
}

int LHAPack::get_bytes(char *buf, int len, int size)
{
	for (int i = 0; i < len && i < size; i++)
        buf[i] = get_ptr[i];
	
    get_ptr += len;
    return i;
}

void LHAPack::put_bytes(char *buf, int len)
{	
    for (int i = 0; i < len; i++)
        put_byte(buf[i]);
}

/*
* Generic (MS-DOS style) time stamp format (localtime):
*
*  31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
* |<---- year-1980 --->|<- month ->|<--- day ---->|
*
*  15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
* |<--- hour --->|<---- minute --->|<- second/2 ->|
*
 */

time_t LHAPack::generic_to_unix_stamp(long t)
{
	#define subbits(n, off, len) (((n) >> (off)) & ((1 << (len))-1))

	tm lt;

    lt.tm_sec  = subbits(t,  0, 5) * 2;
    lt.tm_min  = subbits(t,  5, 6);
    lt.tm_hour = subbits(t, 11, 5);
    lt.tm_mday = subbits(t, 16, 5);
    lt.tm_mon  = subbits(t, 21, 4) - 1;
    lt.tm_year = subbits(t, 25, 7) + 80;
    lt.tm_isdst = -1;
	
    return mktime(&lt);
}

long LHAPack::unix_to_generic_stamp(time_t t)
{
 	SYSTEMTIME st = unix_to_win32_systemtime(t);
 	
     st.wYear  -= 80;
     st.wMonth += 1;
 	
     return ((long)(st.wYear << 25) + (st.wMonth  << 21) + (st.wDay << 16) +
		           (st.wHour << 11) + (st.wMinute << 5)  + (st.wSecond / 2));
}

unsigned long LHAPack::wintime_to_unix_stamp()
{
	unsigned __int64 t;
    unsigned __int64 epoch = ((unsigned __int64)0x019db1de << 32) + 0xd53e8000;
	/* 0x019db1ded53e8000ULL: 1970-01-01 00:00:00 (UTC) */
	
    t = (unsigned long)get_longword();
    t |= (unsigned __int64)(unsigned long)get_longword() << 32;
    t = (t - epoch) / 10000000;
    return (unsigned long)t;
}

FILETIME LHAPack::unix_to_win32_filetime(time_t t)
{
	// Note that LONGLONG is a 64-bit value
	FILETIME ft;
	LONGLONG ll;
	
	ll = Int32x32To64(t, 10000000) + 116444736000000000;
	ft.dwLowDateTime = (DWORD)ll;
	ft.dwHighDateTime = (DWORD)(ll >> 32);

	return ft;
}

SYSTEMTIME LHAPack::unix_to_win32_systemtime(time_t t)
{
	FILETIME ft,lt;
	SYSTEMTIME st;
	
	ft = unix_to_win32_filetime(t);
	LocalFileTimeToFileTime(&ft,&lt);
	FileTimeToSystemTime(&lt, &st);

	return st;
}

/*
* extended header
*
*             size  field name
*  --------------------------------
*  base header:         :
*           2 or 4  next-header size  [*1]
*  --------------------------------------
*  ext header:   1  ext-type            ^
*                ?  contents            | [*1] next-header size
*           2 or 4  next-header size    v
*  --------------------------------------
*
*  on level 1, 2 header:
*    size field is 2 bytes
*  on level 3 header:
*    size field is 4 bytes
*/
int LHAPack::get_extended_header(LHAHeader *hdr, size_t header_size, unsigned int *hcrc)
{
	char data[LZHEADER_STORAGE];
	char dirname[FILENAME_LENGTH];

	int i;
	int ext_type;
    int name_length;    
    int dir_length = 0;            
    int n = 1 + hdr->size_field_length; /* `ext-type' + `next-header size' */

	int whole_size = header_size;

    if (hdr->header_level == 0)
        return 0;

    name_length = strlen(hdr->name);

    while (header_size) 
	{
        setup_get(data);
        if (sizeof(data) < header_size) 
		{
            return 1;
        }

		if(IsBadReadPtr(mem_ptr,header_size) != 0)
		{
			return 1;
		}

		memcpy(data,mem_ptr,header_size);
		mem_ptr += header_size;

        ext_type = get_byte();
        switch (ext_type) 
		{
        case 0:
            /* header crc (CRC-16) */
            hdr->header_crc = get_word();

            /* clear buffer for CRC calculation. */
            data[1] = data[2] = 0;
            skip_bytes(header_size - n - 2);
            break;
        case 1:
            /* filename */
            name_length = get_bytes(hdr->name, header_size-n, sizeof(hdr->name)-1);
            hdr->name[name_length] = 0;
            break;
        case 2:
            /* directory */
            dir_length = get_bytes(dirname, header_size-n, sizeof(dirname)-1);
            dirname[dir_length] = 0;
            break;
        case 0x40:
            /* MS-DOS attribute */
            hdr->attribute = get_word();
            break;
        case 0x41:
            /* Windows time stamp (FILETIME structure) */
            /* it is time in 100 nano seconds since 1601-01-01 00:00:00 */

            skip_bytes(8); /* create time is ignored */

            /* set last modified time */
            if (hdr->header_level >= 2)
                skip_bytes(8);  /* time_t has been already set */
            else
                hdr->unix_last_modified_stamp = wintime_to_unix_stamp();

            skip_bytes(8); /* last access time is ignored */

            break;
        case 0x50:
            /* UNIX permission */
            hdr->unix_mode = get_word();
            break;
        case 0x51:
            /* UNIX gid and uid */
            hdr->unix_gid = get_word();
            hdr->unix_uid = get_word();
            break;
        case 0x52:
            /* UNIX group name */
            i = get_bytes(hdr->group, header_size-n, sizeof(hdr->group)-1);
            hdr->group[i] = '\0';
            break;
        case 0x53:
            /* UNIX user name */
            i = get_bytes(hdr->user, header_size-n, sizeof(hdr->user)-1);
            hdr->user[i] = '\0';
            break;
        case 0x54:
            /* UNIX last modified time */
            hdr->unix_last_modified_stamp = (time_t) get_longword();
            break;
        default:
            /* other headers */
            /* 0x39: multi-disk header
               0x3f: uncompressed comment
               0x42: 64bit large file size
               0x48-0x4f(?): reserved for authenticity verification
               0x7d: encapsulation
               0x7e: extended attribute - platform information
               0x7f: extended attribute - permission, owner-id and timestamp
                     (level 3 on OS/2)
               0xc4: compressed comment (dict size: 4096)
               0xc5: compressed comment (dict size: 8192)
               0xc6: compressed comment (dict size: 16384)
               0xc7: compressed comment (dict size: 32768)
               0xc8: compressed comment (dict size: 65536)
               0xd0-0xdf(?): operating systemm specific information
               0xfc: encapsulation (another opinion)
               0xfe: extended attribute - platform information(another opinion)
               0xff: extended attribute - permission, owner-id and timestamp
                     (level 3 on UNLHA32) */
            
            skip_bytes(header_size - n);
            break;
        }

        if (hcrc)
            *hcrc = calccrc(*hcrc, (unsigned char*)data, header_size);

        if (hdr->size_field_length == 2)
            whole_size += header_size = get_word();
        else
            whole_size += header_size = get_longword();
    }

    /* concatenate dirname and filename */
    if (dir_length) 
	{
        if (name_length + dir_length >= sizeof(hdr->name))
		{
            name_length = sizeof(hdr->name) - dir_length - 1;
            hdr->name[name_length] = 0;
        }
        strcat(dirname, hdr->name); /* ok */
        strcpy(hdr->name, dirname); /* ok */
        name_length += dir_length;
    }

    return whole_size;	
}

#define I_HEADER_SIZE           0               /* level 0,1,2   */
#define I_HEADER_CHECKSUM       1               /* level 0,1     */
#define I_METHOD                2               /* level 0,1,2,3 */
#define I_PACKED_SIZE           7               /* level 0,1,2,3 */
#define I_ATTRIBUTE             19              /* level 0,1,2,3 */
#define I_HEADER_LEVEL          20              /* level 0,1,2,3 */

#define COMMON_HEADER_SIZE      21      /* size of common part */

#define I_GENERIC_HEADER_SIZE 24 /* + name_length */
#define I_LEVEL0_HEADER_SIZE  36 /* + name_length (unix extended) */
#define I_LEVEL1_HEADER_SIZE  27 /* + name_length */
#define I_LEVEL2_HEADER_SIZE  26 /* + padding */
#define I_LEVEL3_HEADER_SIZE  32

#define EXTEND_GENERIC          0
#define EXTEND_UNIX             'U'
#define EXTEND_MSDOS            'M'
#define EXTEND_MACOS            'm'
#define EXTEND_OS9              '9'
#define EXTEND_OS2              '2'
#define EXTEND_OS68K            'K'
#define EXTEND_OS386            '3' /* OS-9000??? */
#define EXTEND_HUMAN            'H'
#define EXTEND_CPM              'C'
#define EXTEND_FLEX             'F'
#define EXTEND_RUNSER           'R'

#define UNIX_FILE_TYPEMASK      0170000
#define UNIX_FILE_REGULAR       0100000
#define UNIX_FILE_DIRECTORY     0040000
#define UNIX_FILE_SYMLINK       0120000
#define UNIX_SETUID             0004000
#define UNIX_SETGID             0002000
#define UNIX_STICKYBIT          0001000
#define UNIX_OWNER_READ_PERM    0000400
#define UNIX_OWNER_WRITE_PERM   0000200
#define UNIX_OWNER_EXEC_PERM    0000100
#define UNIX_GROUP_READ_PERM    0000040
#define UNIX_GROUP_WRITE_PERM   0000020
#define UNIX_GROUP_EXEC_PERM    0000010
#define UNIX_OTHER_READ_PERM    0000004
#define UNIX_OTHER_WRITE_PERM   0000002
#define UNIX_OTHER_EXEC_PERM    0000001
#define UNIX_RW_RW_RW           0000666

/*
 * level 0 header
 *
 *
 * offset  size  field name
 * ----------------------------------
 *     0      1  header size    [*1]
 *     1      1  header sum
 *            ---------------------------------------
 *     2      5  method ID                         ^
 *     7      4  packed size    [*2]               |
 *    11      4  original size                     |
 *    15      2  time                              |
 *    17      2  date                              |
 *    19      1  attribute                         | [*1] header size (X+Y+22)
 *    20      1  level (0x00 fixed)                |
 *    21      1  name length                       |
 *    22      X  pathname                          |
 * X +22      2  file crc (CRC-16)                 |
 * X +24      Y  ext-header(old style)             v
 * -------------------------------------------------
 * X+Y+24        data                              ^
 *                 :                               | [*2] packed size
 *                 :                               v
 * -------------------------------------------------
 *
 * ext-header(old style)
 *     0      1  ext-type ('U')
 *     1      1  minor version
 *     2      4  UNIX time
 *     6      2  mode
 *     8      2  uid
 *    10      2  gid
 *
 * attribute (MS-DOS)
 *    bit1  read only
 *    bit2  hidden
 *    bit3  system
 *    bit4  volume label
 *    bit5  directory
 *    bit6  archive bit (need to backup)
 *
 */

bool LHAPack::get_header_level0(LHAHeader *hdr, char *data)
{
	int i;
	int checksum;
    int name_length;    
	int extend_size; 

	size_t  header_size;    
	
	header_size = get_byte();
	checksum    = get_byte();

    hdr->size_field_length = 2; /* in bytes */
    hdr->header_size       = header_size;    
	
	memcpy(data + COMMON_HEADER_SIZE,mem_ptr,header_size + 2 - COMMON_HEADER_SIZE);
	mem_ptr += header_size + 2 - COMMON_HEADER_SIZE;
	
	if (calc_sum(data + I_METHOD, header_size) != checksum) 
	{
		
	}
	
    get_bytes(hdr->method, 5, sizeof(hdr->method));
	
    hdr->packed_size              = get_longword();
    hdr->original_size            = get_longword();
    hdr->unix_last_modified_stamp = generic_to_unix_stamp(get_longword());
    hdr->attribute                = get_byte(); /* MS-DOS attribute */
    hdr->header_level             = get_byte();
    name_length                   = get_byte();
	
    i = get_bytes(hdr->name, name_length, sizeof(hdr->name)-1);
    hdr->name[i] = '\0';
	
    /* defaults for other type */
    hdr->unix_mode = UNIX_FILE_REGULAR | UNIX_RW_RW_RW;
    hdr->unix_gid  = 0;
    hdr->unix_uid  = 0;
	
    extend_size = header_size+2 - name_length - 24;
	
	if (extend_size < 0) 
	{
        if (extend_size == -2) 
		{
            /* CRC field is not given */
            hdr->extend_type = EXTEND_GENERIC;
            hdr->has_crc = FALSE;
			
            return true;
        } 
        return false;
    }
	
    hdr->has_crc = TRUE;
    hdr->crc     = get_word();
	
    if (extend_size == 0)
        return true;
	
    hdr->extend_type = get_byte();
    extend_size--;
	
    if (hdr->extend_type == EXTEND_UNIX) 
	{
        if (extend_size >= 11) 
		{
            hdr->minor_version            = get_byte();
            hdr->unix_last_modified_stamp = (time_t) get_longword();
            hdr->unix_mode                = get_word();
            hdr->unix_uid                 = get_word();
            hdr->unix_gid                 = get_word();
			
            extend_size -= 11;
        }
		else 
		{
            hdr->extend_type = EXTEND_GENERIC;
        }
    }

    if (extend_size > 0) skip_bytes(extend_size);
	
    hdr->header_size += 2;
    return true;
}

/*
 * level 1 header
 *
 *
 * offset   size  field name
 * -----------------------------------
 *     0       1  header size   [*1]
 *     1       1  header sum
 *             -------------------------------------
 *     2       5  method ID                        ^
 *     7       4  skip size     [*2]               |
 *    11       4  original size                    |
 *    15       2  time                             |
 *    17       2  date                             |
 *    19       1  attribute (0x20 fixed)           | [*1] header size (X+Y+25)
 *    20       1  level (0x01 fixed)               |
 *    21       1  name length                      |
 *    22       X  filename                         |
 * X+ 22       2  file crc (CRC-16)                |
 * X+ 24       1  OS ID                            |
 * X +25       Y  ???                              |
 * X+Y+25      2  next-header size                 v
 * -------------------------------------------------
 * X+Y+27      Z  ext-header                       ^
 *                 :                               |
 * -----------------------------------             | [*2] skip size
 * X+Y+Z+27       data                             |
 *                 :                               v
 * -------------------------------------------------
 *
 */

bool LHAPack::get_header_level1(LHAHeader *hdr, char *data)
{
	int i, dummy;
	int checksum;
	int name_length;
    int extend_size;

	size_t header_size;
	
	header_size = get_byte();
	checksum    = get_byte();

    hdr->size_field_length = 2; /* in bytes */
    hdr->header_size       = header_size;    
	
	memcpy(data + COMMON_HEADER_SIZE,mem_ptr,header_size + 2 - COMMON_HEADER_SIZE);
	mem_ptr += header_size + 2 - COMMON_HEADER_SIZE;
	
    if (calc_sum(data + I_METHOD, header_size) != checksum) {
        //return false;
    }
	
    get_bytes(hdr->method, 5, sizeof(hdr->method));
    hdr->packed_size              = get_longword(); /* skip size */
    hdr->original_size            = get_longword();
    hdr->unix_last_modified_stamp = generic_to_unix_stamp(get_longword());
    hdr->attribute                = get_byte(); /* 0x20 fixed */
    hdr->header_level             = get_byte();
	
    name_length = get_byte();
    i = get_bytes(hdr->name, name_length, sizeof(hdr->name)-1);
    hdr->name[i] = '\0';
	
    /* defaults for other type */
    hdr->unix_mode   = UNIX_FILE_REGULAR | UNIX_RW_RW_RW;
    hdr->unix_gid    = 0;
    hdr->unix_uid    = 0;	
    hdr->has_crc     = TRUE;
    hdr->crc         = get_word();
    hdr->extend_type = get_byte();
	
    dummy = header_size+2 - name_length - I_LEVEL1_HEADER_SIZE;
    if (dummy > 0)
        skip_bytes(dummy); /* skip old style extend header */
	
    extend_size = get_word();
    extend_size = get_extended_header(hdr, extend_size, 0);
    if (extend_size == -1)
        return false;
	
    /* On level 1 header, size fields should be adjusted. */
    /* the `packed_size' field contains the extended header size. */
    /* the `header_size' field does not. */
    hdr->packed_size -= extend_size;
    hdr->header_size += extend_size + 2;
    
	return true;
}

/*
 * level 2 header
 *
 *
 * offset   size  field name
 * --------------------------------------------------
 *     0       2  total header size [*1]           ^
 *             -----------------------             |
 *     2       5  method ID                        |
 *     7       4  packed size       [*2]           |
 *    11       4  original size                    |
 *    15       4  time                             |
 *    19       1  RESERVED (0x20 fixed)            | [*1] total header size
 *    20       1  level (0x02 fixed)               |      (X+26+(1))
 *    21       2  file crc (CRC-16)                |
 *    23       1  OS ID                            |
 *    24       2  next-header size                 |
 * -----------------------------------             |
 *    26       X  ext-header                       |
 *                 :                               |
 * -----------------------------------             |
 * X +26      (1) padding                          v
 * -------------------------------------------------
 * X +26+(1)      data                             ^
 *                 :                               | [*2] packed size
 *                 :                               v
 * -------------------------------------------------
 *
 */
bool LHAPack::get_header_level2(LHAHeader *hdr, char *data)
{    
	int padding;
    int extend_size;    
    unsigned int hcrc;

	size_t header_size;
	
	header_size = get_word();

    hdr->size_field_length = 2; /* in bytes */
    hdr->header_size       = header_size;
	
	//验证该内存区域是否可读
	if(IsBadReadPtr(mem_ptr,I_LEVEL2_HEADER_SIZE - COMMON_HEADER_SIZE) != 0)
	{
		return false;
	}
	
	memcpy(data + COMMON_HEADER_SIZE,mem_ptr,I_LEVEL2_HEADER_SIZE - COMMON_HEADER_SIZE);   
	mem_ptr += I_LEVEL2_HEADER_SIZE - COMMON_HEADER_SIZE;
	
    get_bytes(hdr->method, 5, sizeof(hdr->method));
    hdr->packed_size              = get_longword();
    hdr->original_size            = get_longword();
    hdr->unix_last_modified_stamp = get_longword();
    hdr->attribute                = get_byte(); /* reserved */
    hdr->header_level             = get_byte();
	
    /* defaults for other type */
    hdr->unix_mode                = UNIX_FILE_REGULAR | UNIX_RW_RW_RW;
    hdr->unix_gid                 = 0;
    hdr->unix_uid                 = 0;
	
    hdr->has_crc                  = TRUE;
    hdr->crc                      = get_word();
    hdr->extend_type              = get_byte();
    extend_size                   = get_word();
	
    INITIALIZE_CRC(hcrc);
    hcrc = calccrc(hcrc, (unsigned char*)data, get_ptr - data);
	
    extend_size = get_extended_header(hdr, extend_size, &hcrc);
    if (extend_size == -1)
        return false;
	
    padding = header_size - I_LEVEL2_HEADER_SIZE - extend_size;
    while (padding--)           /* padding should be 0 or 1 */
        hcrc = UPDATE_CRC(hcrc, *mem_ptr++);
	
    if (hdr->header_crc != hcrc)
	{

	}
	
	return true;
}

/*
 * level 3 header
 *
 *
 * offset   size  field name
 * --------------------------------------------------
 *     0       2  size field length (4 fixed)      ^
 *     2       5  method ID                        |
 *     7       4  packed size       [*2]           |
 *    11       4  original size                    |
 *    15       4  time                             |
 *    19       1  RESERVED (0x20 fixed)            | [*1] total header size
 *    20       1  level (0x03 fixed)               |      (X+32)
 *    21       2  file crc (CRC-16)                |
 *    23       1  OS ID                            |
 *    24       4  total header size [*1]           |
 *    28       4  next-header size                 |
 * -----------------------------------             |
 *    32       X  ext-header                       |
 *                 :                               v
 * -------------------------------------------------
 * X +32          data                             ^
 *                 :                               | [*2] packed size
 *                 :                               v
 * -------------------------------------------------
 *
 */
bool LHAPack::get_header_level3(LHAHeader *hdr, char *data)
{
    int extend_size;
    int padding;
    unsigned int hcrc;

	size_t header_size;
	
    hdr->size_field_length = get_word();
	
	memcpy(data + COMMON_HEADER_SIZE,mem_ptr,I_LEVEL3_HEADER_SIZE - COMMON_HEADER_SIZE); 
	mem_ptr += I_LEVEL3_HEADER_SIZE - COMMON_HEADER_SIZE;
	
    get_bytes(hdr->method, 5, sizeof(hdr->method));
    hdr->packed_size              = get_longword();
    hdr->original_size            = get_longword();
    hdr->unix_last_modified_stamp = get_longword();
    hdr->attribute                = get_byte(); /* reserved */
    hdr->header_level             = get_byte();
	
    /* defaults for other type */
    hdr->unix_mode   = UNIX_FILE_REGULAR | UNIX_RW_RW_RW;
    hdr->unix_gid    = 0;
    hdr->unix_uid    = 0;
	
    hdr->has_crc     = TRUE;
    hdr->crc         = get_word();
    hdr->extend_type = get_byte();
    hdr->header_size = header_size = get_longword();
    extend_size      = get_longword();
	
    INITIALIZE_CRC(hcrc);
    hcrc = calccrc(hcrc, (unsigned char*)data, get_ptr - data);
	
    extend_size = get_extended_header(hdr, extend_size, &hcrc);
    if (extend_size == -1)
        return false;
	
    padding = header_size - I_LEVEL3_HEADER_SIZE - extend_size;
    while (padding--)           /* padding should be 0 */
        hcrc = UPDATE_CRC(hcrc, *mem_ptr++);
	
    if (hdr->header_crc != hcrc)
    {

	}
	
	return true;
}

bool LHAPack::get_header(const char *pMem, LHAHeader *hdr)
{
	if(NULL==pMem)	return false;
	mem_ptr = (char*)pMem;

	//读取LZH Pack文件头
	int   end_mark;	
	char  data[LZHEADER_STORAGE] = {0};   
	
	setup_get(data);	
    memset(hdr, 0, sizeof(LHAHeader));    	

    if ((end_mark = *mem_ptr++) == 0) 
	{
        return false;           /* finish */
    }
	
    data[0] = end_mark;

	//验证该内存区域是否可读
	if(IsBadReadPtr(mem_ptr,COMMON_HEADER_SIZE - 1) != 0)
	{
		return false;
	}

	memcpy(data+1,mem_ptr,COMMON_HEADER_SIZE - 1);
	mem_ptr += COMMON_HEADER_SIZE - 1;
  
    switch (data[I_HEADER_LEVEL]) 
	{
    case 0:
        if (get_header_level0(hdr, data) == FALSE)
            return false;
        break;
    case 1:
        if (get_header_level1(hdr, data) == FALSE)
            return false;
        break;
    case 2:
        if (get_header_level2(hdr, data) == FALSE)
            return false;
        break;
    case 3:
        if (get_header_level3(hdr, data) == FALSE)
            return false;
        break;
    default:
        //error("Unknown level header (level %d)", data[I_HEADER_LEVEL]);
        return false;
    }

	/* unix time stamp conversion */
	win32_systemtime = unix_to_win32_systemtime(hdr->unix_last_modified_stamp);
	dataoffset       = mem_ptr - pMem;

    return true;
}

#define CURRENT_UNIX_MINOR_VERSION      0x00
#define LHA_PATHSEP                     0xff    /* path separator of the
                                                filename in lha header.
												it should compare with
												'unsigned char' or 'int',
                                                that is not '\xff', but 0xff. */

void LHAPack::write_unix_info(LHAHeader *hdr)
{
    /* UNIX specific informations */
	
    put_word(5);            /* size */
    put_byte(0x50);         /* permission */
    put_word(hdr->unix_mode);
	
    put_word(7);            /* size */
    put_byte(0x51);         /* gid and uid */
    put_word(hdr->unix_gid);
    put_word(hdr->unix_uid);
	
    if (hdr->group[0]) 
	{
        int len = strlen(hdr->group);
        put_word(len + 3);  /* size */
        put_byte(0x52);     /* group name */
        put_bytes(hdr->group, len);
    }
	
    if (hdr->user[0])
	{
        int len = strlen(hdr->user);
        put_word(len + 3);  /* size */
        put_byte(0x53);     /* user name */
        put_bytes(hdr->user, len);
    }
	
    if (hdr->header_level == 1) 
	{
        put_word(7);        /* size */
        put_byte(0x54);     /* time stamp */
        put_longword(hdr->unix_last_modified_stamp);
    }
}

size_t LHAPack::write_header_level0(LHAHeader *hdr, char* data, char* pathname)
{
    int    limit;
    int    name_length;
    size_t header_size;
	
    setup_put(data);
    memset(data, 0, LZHEADER_STORAGE);
	
    put_byte(0x00);             /* header size */
    put_byte(0x00);             /* check sum */
    put_bytes(hdr->method, 5);
    put_longword(hdr->packed_size);
    put_longword(hdr->original_size);
    put_longword(unix_to_generic_stamp(hdr->unix_last_modified_stamp));
    put_byte(hdr->attribute);
    put_byte(hdr->header_level); /* level 0 */
	
    /* write pathname (level 0 header contains the directory part) */
    name_length = strlen(pathname);
    if (generic_format)
        limit = 255 - I_GENERIC_HEADER_SIZE + 2;
    else
        limit = 255 - I_LEVEL0_HEADER_SIZE  + 2;
	
    if (name_length > limit) 
	{
        name_length = limit;
    }
    put_byte(name_length);
    put_bytes(pathname, name_length);
    put_word(hdr->crc);
	
    if (generic_format) 
	{
        header_size = I_GENERIC_HEADER_SIZE + name_length - 2;
        data[I_HEADER_SIZE] = header_size;
        data[I_HEADER_CHECKSUM] = calc_sum(data + I_METHOD, header_size);
    } 
	else
	{
        /* write old-style extend header */
        put_byte(EXTEND_UNIX);
        put_byte(CURRENT_UNIX_MINOR_VERSION);
        put_longword(hdr->unix_last_modified_stamp);
        put_word(hdr->unix_mode);
        put_word(hdr->unix_uid);
        put_word(hdr->unix_gid);
		
        /* size of extended header is 12 */
        header_size = I_LEVEL0_HEADER_SIZE + name_length - 2;
        data[I_HEADER_SIZE] = header_size;
        data[I_HEADER_CHECKSUM] = calc_sum(data + I_METHOD, header_size);
    }
	
    return header_size + 2;
}

size_t LHAPack::write_header_level1(LHAHeader *hdr, char* data, char* pathname)
{
    int    name_length, dir_length, limit;
    char   *basename, *dirname;
    size_t header_size;
    char   *extend_header_top;
    size_t extend_header_size;
	
    basename = strrchr(pathname, LHA_PATHSEP);
    if (basename) 
	{
        basename++;
        name_length = strlen(basename);
        dirname = pathname;
        dir_length = basename - dirname;
    }
    else
	{
        basename = pathname;
        name_length = strlen(basename);
        dirname = "";
        dir_length = 0;
    }
	
    setup_put(data);
    memset(data, 0, LZHEADER_STORAGE);
	
    put_byte(0x00);             /* header size */
    put_byte(0x00);             /* check sum */
    put_bytes(hdr->method, 5);
    put_longword(hdr->packed_size);
    put_longword(hdr->original_size);
    put_longword(unix_to_generic_stamp(hdr->unix_last_modified_stamp));
    put_byte(0x20);
    put_byte(hdr->header_level); /* level 1 */
	
    /* level 1 header: write filename (basename only) */
    limit = 255 - I_LEVEL1_HEADER_SIZE + 2;
    if (name_length > limit)
	{
        put_byte(0);            /* name length */
    }
    else 
	{
        put_byte(name_length);
        put_bytes(basename, name_length);
    }
	
    put_word(hdr->crc);
	
    if (generic_format)
        put_byte(0x00);
    else
        put_byte(EXTEND_UNIX);
	
    /* write extend header from here. */
	
    extend_header_top = put_ptr+2; /* +2 for the field `next header size' */
    header_size = extend_header_top - data - 2;
	
    /* write filename and dirname */
	
    if (name_length > limit) 
	{
        put_word(name_length + 3); /* size */
        put_byte(0x01);         /* filename */
        put_bytes(basename, name_length);
    }
	
    if (dir_length > 0) 
	{
        put_word(dir_length + 3); /* size */
        put_byte(0x02);         /* dirname */
        put_bytes(dirname, dir_length);
    }
	
    if (!generic_format)
        write_unix_info(hdr);
	
    put_word(0x0000);           /* next header size */
	
    extend_header_size = put_ptr - extend_header_top;
    /* On level 1 header, the packed size field is contains the ext-header */
    hdr->packed_size += put_ptr - extend_header_top;
	
    /* put 'skip size' */
    setup_put(data + I_PACKED_SIZE);
    put_longword(hdr->packed_size);
	
    data[I_HEADER_SIZE] = header_size;
    data[I_HEADER_CHECKSUM] = calc_sum(data + I_METHOD, header_size);
	
    return header_size + extend_header_size + 2;
}

size_t LHAPack::write_header_level2(LHAHeader *hdr, char* data, char* pathname)
{
    int    name_length, dir_length;
    char   *basename, *dirname;
    size_t header_size;
    char   *extend_header_top;
    char   *headercrc_ptr;

    unsigned int hcrc;
	
    basename = strrchr(pathname, LHA_PATHSEP);
    if (basename)
	{
        basename++;
        name_length = strlen(basename);
        dirname = pathname;
        dir_length = basename - dirname;
    }
    else 
	{
        basename = pathname;
        name_length = strlen(basename);
        dirname = "";
        dir_length = 0;
    }
	
    setup_put(data);
    memset(data, 0, LZHEADER_STORAGE);
	
    put_word(0x0000);           /* header size */
    put_bytes(hdr->method, 5);
    put_longword(hdr->packed_size);
    put_longword(hdr->original_size);
    put_longword(hdr->unix_last_modified_stamp);
    put_byte(0x20);
    put_byte(hdr->header_level); /* level 2 */
	
    put_word(hdr->crc);
	
    if (generic_format)
        put_byte(0x00);
    else
        put_byte(EXTEND_UNIX);
	
    /* write extend header from here. */
	
    extend_header_top = put_ptr+2; /* +2 for the field `next header size' */
	
    /* write common header */
    put_word(5);
    put_byte(0x00);
    headercrc_ptr = put_ptr;
    put_word(0x0000);           /* header CRC */
	
    /* write filename and dirname */
    /* must have this header, even if the name_length is 0. */
    put_word(name_length + 3);  /* size */
    put_byte(0x01);             /* filename */
    put_bytes(basename, name_length);
	
    if (dir_length > 0)
	{
        put_word(dir_length + 3); /* size */
        put_byte(0x02);         /* dirname */
        put_bytes(dirname, dir_length);
    }
	
    if (!generic_format)
        write_unix_info(hdr);
	
    put_word(0x0000);           /* next header size */
	
    header_size = put_ptr - data;
    if ((header_size & 0xff) == 0) 
	{
        /* cannot put zero at the first byte on level 2 header. */
        /* adjust header size. */
        put_byte(0);            /* padding */
        header_size++;
    }
	
    /* put header size */
    setup_put(data + I_HEADER_SIZE);
    put_word(header_size);
	
    /* put header CRC in extended header */
    INITIALIZE_CRC(hcrc);
    hcrc = calccrc(hcrc, (unsigned char*)data, (unsigned int) header_size);
    setup_put(headercrc_ptr);
    put_word(hcrc);
	
    return header_size;
}
