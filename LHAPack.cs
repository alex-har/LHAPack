using System;
using System.Collections.Generic;
using System.IO;

namespace South.Utility
{
    /// <summary>
    /// LHA文件头和数据
    /// </summary>
    public struct LHAHeader
    {
        public int header_size;
        public int size_field_length;
        public byte[] method;
        public int packed_size;
        public int original_size;
        public int attribute;
        public int header_level;
        public string realname;
        public int crc;
        public int has_crc;
        public int header_crc;
        public int extend_type;
        public int minor_version;
        public ulong unix_last_modified_stamp;
        public int unix_mode;
        public int unix_uid;
        public int unix_gid;
        public string user;
        public string group;

        public int data_offset;

        public string name; // 文件名
        public byte[] data_content; // 文件数据
    };

    /// <summary>
    /// LHA解包类
    /// </summary>
    public class LHAPack
    {
        /// <summary>
        /// 
        /// </summary>
        public LHAPack()
        {
            make_crctable();
        }

        /// <summary>
        /// 完整数据
        /// </summary>
        byte[] fileContent = null;

        /// <summary>
        /// 校验表
        /// </summary>
        int[] crctable = new int[256];

        /// <summary>
        /// 所有的文件头和数据
        /// </summary>
        List<LHAHeader> headers = new List<LHAHeader>();
        public List<LHAHeader> Headers
        {
            get
            {
                return headers;
            }
        }

        /// <summary>
        /// 打开文件
        /// </summary>
        /// <param name="fileName"></param>
        /// <returns></returns>
        public int OpenFile(string fileName)
        {
            FileStream fs = null;
            try
            {
                fs = new FileStream(fileName, FileMode.Open, FileAccess.Read);
                long length = fs.Length;
                if (fs.Length == 0)
                {
                    return 0;
                }

                fileContent = new byte[length];
                fs.Read(fileContent, 0, (int)length);

                return ParseContent();
            }
            catch (Exception)
            {
                return -1;
            }
            finally
            {
                if (fs != null)
                {
                    fs.Close();
                }
            }
        }

        /// <summary>
        /// 直接指定数据
        /// </summary>
        /// <param name="content"></param>
        /// <returns></returns>
        public int SetContent(byte[] content)
        {
            if (content == null)
            {
                return 0;
            }
            if (content.Length == 0)
            {
                return 0;
            }

            fileContent = new byte[content.Length];

            content.CopyTo(fileContent, 0);

            return ParseContent();
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="stream"></param>
        /// <returns></returns>
        public int OpenFromStream(Stream stream)
        {
            if (stream == null)
            {
                return 0;
            }
            if (stream.Length == 0)
            {
                return 0;
            }

            fileContent = new byte[stream.Length];

            stream.Read(fileContent, 0, (int)stream.Length);

            return ParseContent();
        }

        /// <summary>
        /// 分析文件内容
        /// </summary>
        /// <returns></returns>
        int ParseContent()
        {
            headers.Clear();

            if (fileContent == null)
            {
                return -1;
            }
            if (fileContent.Length == 0)
            {
                return 0;
            }

            int data_index = 0;
            LHAHeader header;
            while (get_heaer(data_index, out header))
            {
                data_index += header.data_offset;

                header.data_content = new byte[header.packed_size];

                copy_data(header.data_content, data_index, 0, header.packed_size);

                data_index += header.packed_size;
                headers.Add(header);
            }

            return headers.Count;
        }

        /// <summary>
        /// 初始化校验表
        /// </summary>
        void make_crctable()
        {
            int i, j, r;
            for (i = 0; i <= 255; i++)
            {
                r = i;
                for (j = 0; j < 8; j++)
                    if ((r & 1) != 0)
                        r = (r >> 1) ^ 0xA001;
                    else
                        r >>= 1;
                crctable[i] = r;
            }
        }

        /// <summary>
        /// 读一个字节并移动读取位置
        /// </summary>
        /// <param name="data"></param>
        /// <param name="get_ptr"></param>
        /// <returns></returns>
        int get_byte(byte[] data, ref int get_ptr)
        {
            return data[get_ptr++] & 0xFF;
        }

        /// <summary>
        /// 读多个字节到指定数组中指定的位置
        /// </summary>
        /// <param name="data"></param>
        /// <param name="dest"></param>
        /// <param name="get_ptr"></param>
        /// <param name="len"></param>
        /// <param name="size"></param>
        /// <returns></returns>
        int get_bytes(byte[] data, byte[] dest, ref int get_ptr, int len, int size)
        {
            int i = 0;
            for (i = 0; i < len && i<size; i++)
            {
                dest[i] = data[get_ptr+i];
            }
            get_ptr += len;
            return i;
        }

        /// <summary>
        /// 读两个字节并移动读取位置
        /// </summary>
        /// <param name="data"></param>
        /// <param name="get_ptr"></param>
        /// <returns></returns>
        int get_word(byte[] data, ref int get_ptr)
        {
            int n0 = get_byte(data, ref get_ptr);
            int n1 = get_byte(data, ref get_ptr);
            return (n1 << 8) + n0;
        }

        /// <summary>
        /// 读四个字节并移动读取位置
        /// </summary>
        /// <param name="data"></param>
        /// <param name="get_ptr"></param>
        /// <returns></returns>
        int get_longword(byte[] data, ref int get_ptr)
        {
	        int b0, b1, b2, b3;
            int l;
            b0 = get_byte(data, ref get_ptr);
            b1 = get_byte(data, ref get_ptr);
            b2 = get_byte(data, ref get_ptr);
            b3 = get_byte(data, ref get_ptr);
            l = (b3 << 24) + (b2 << 16) + (b1 << 8) + b0;	
            return l;
        }

        /// <summary>
        /// 复制指定字节数到目标数组的指定位置
        /// </summary>
        /// <param name="data"></param>
        /// <param name="mem_ptr"></param>
        /// <param name="dataindex"></param>
        /// <param name="len"></param>
        void copy_data(byte[] data, int mem_ptr, int dataindex, int len)
        {
            for (int i = 0; i < len; i++)
            {
                data[dataindex + i] = fileContent[mem_ptr + i];
            }
        }

        /// <summary>
        /// 更新校验
        /// </summary>
        /// <param name="crc"></param>
        /// <param name="c"></param>
        /// <returns></returns>
        int update_crc(int crc, int c)
        {
            return (crctable[((crc) ^ (c)) & 0xFF] ^ ((crc) >> 8));
        }

        /// <summary>
        /// 计算校验
        /// </summary>
        /// <param name="crc"></param>
        /// <param name="p"></param>
        /// <param name="n"></param>
        /// <returns></returns>
        int calccrc(int crc, byte[] p, int n)
        {
            for (int i = 0; i < n; i++)
            {
                crc = update_crc(crc, p[i]);
            }
            return crc;
        }

        /// <summary>
        /// 字节数组转成字符串
        /// </summary>
        /// <param name="b"></param>
        /// <param name="len"></param>
        /// <returns></returns>
        string byte_to_string(byte[] b, int len)
        {
            string s = string.Empty;
            for (int i = 0; i < len; i++)
            {
                s += Convert.ToChar(b[i]);
            }
            return s;
        }

        /// <summary>
        /// 计算数组中的指定位置和个数的数字和
        /// </summary>
        /// <param name="data"></param>
        /// <param name="n"></param>
        /// <param name="len"></param>
        /// <returns></returns>
        int calc_sum(byte[] data, int n, int len)
        {
            int sum = 0;
            for (int i = 0; i < len; i++)
            {
                sum += data[n+i];
            }
            return sum & 0xff;
        }

        /// <summary>
        /// 时间格式转换：暂时未实现
        /// </summary>
        /// <param name="t"></param>
        /// <returns></returns>
        ulong generic_to_unix_stamp(int t)
        {
            return 0;
        }

        /// <summary>
        /// 时间格式转换：win->unix
        /// </summary>
        /// <param name="t1"></param>
        /// <param name="t2"></param>
        /// <returns></returns>
        ulong wintime_to_unix_stamp(int t1, int t2)
        {
            UInt64 t;
            UInt64 epoch = ((UInt64)0x019db1de << 32) + 0xd53e8000;
            /* 0x019db1ded53e8000ULL: 1970-01-01 00:00:00 (UTC) */

            t = (ulong)t1;
            t |= (UInt64)(ulong)t2 << 32;
            t = (t - epoch) / 10000000;
            return (ulong)t;
        }

        /// <summary>
        /// 读取一种格式的文件头内容
        /// </summary>
        /// <param name="data"></param>
        /// <param name="mem_ptr"></param>
        /// <param name="header"></param>
        /// <returns></returns>
        bool get_header_level0(byte[] data, ref int mem_ptr, ref LHAHeader header)
        {
            int i;
            int checksum;
            int name_length;
            int extend_size;

            int header_size;
            int get_ptr = 0;
            header_size = get_byte(data, ref get_ptr);
            checksum = get_byte(data, ref get_ptr);

            header.size_field_length = 2; /* in bytes */
            header.header_size = header_size;

            // 检查内存范围
            if (mem_ptr + header_size + 2 - 21 > fileContent.Length)
            {
                return false;
            }

            copy_data(data, mem_ptr, 21, header_size + 2 - 21);
            mem_ptr += header_size + 2 - 21;

            if (calc_sum(data, 2, header_size) != checksum)
            {

            }

            get_bytes(data, header.method, ref get_ptr, 5, 5);

            header.packed_size = get_longword(data, ref get_ptr);
            header.original_size = get_longword(data, ref get_ptr);
            header.unix_last_modified_stamp = generic_to_unix_stamp(get_longword(data, ref get_ptr));
            header.attribute = get_byte(data, ref get_ptr); /* MS-DOS attribute */
            header.header_level = get_byte(data, ref get_ptr);
            name_length = get_byte(data, ref get_ptr);

            byte[] name = new byte[1024];
            i = get_bytes(data, name, ref get_ptr, name_length, name.Length - 1);
            name[i] = 0;
            header.name = byte_to_string(name, i);

            /* defaults for other type */
            header.unix_mode = 0100000 | 0000666;
            header.unix_gid = 0;
            header.unix_uid = 0;

            extend_size = header_size + 2 - name_length - 24;

            if (extend_size < 0)
            {
                if (extend_size == -2)
                {
                    /* CRC field is not given */
                    header.extend_type = 0;
                    header.has_crc = 0;

                    return true;
                }
                return false;
            }

            header.has_crc = 1;
            header.crc = get_word(data, ref get_ptr);

            if (extend_size == 0)
                return true;

            header.extend_type = get_byte(data, ref get_ptr);
            extend_size--;

            if (header.extend_type == 'U')
            {
                if (extend_size >= 11)
                {
                    header.minor_version = get_byte(data, ref get_ptr);
                    header.unix_last_modified_stamp = (ulong)get_longword(data, ref get_ptr);
                    header.unix_mode = get_word(data, ref get_ptr);
                    header.unix_uid = get_word(data, ref get_ptr);
                    header.unix_gid = get_word(data, ref get_ptr);

                    extend_size -= 11;
                }
                else
                {
                    header.extend_type = 0;
                }
            }

            if (extend_size > 0) get_ptr += extend_size;

            header.header_size += 2;
            return true;
        }

        /// <summary>
        /// 读取一种格式的文件头内容
        /// </summary>
        /// <param name="data"></param>
        /// <param name="mem_ptr"></param>
        /// <param name="header"></param>
        /// <returns></returns>
        bool get_header_level1(byte[] data, ref int mem_ptr, ref LHAHeader header)
        {
            int i, dummy;
            int checksum;
            int name_length;
            int extend_size;
            int get_ptr = 0;
            int header_size;

            header_size = get_byte(data, ref get_ptr);
            checksum = get_byte(data, ref get_ptr);

            header.size_field_length = 2; /* in bytes */
            header.header_size = header_size;

            // 检查内存范围
            if (mem_ptr + header_size + 2 - 21 > fileContent.Length)
            {
                return false;
            }

            copy_data(data, mem_ptr, 21, header_size + 2 - 21);
            mem_ptr += header_size + 2 - 21;

            if (calc_sum(data, 2, header_size) != checksum)
            {
                //return false;
            }

            get_bytes(data, header.method, ref get_ptr, 5, 5);
            header.packed_size = get_longword(data, ref get_ptr); /* skip size */
            header.original_size = get_longword(data, ref get_ptr);
            header.unix_last_modified_stamp = generic_to_unix_stamp(get_longword(data, ref get_ptr));
            header.attribute = get_byte(data, ref get_ptr); /* 0x20 fixed */
            header.header_level = get_byte(data, ref get_ptr);

            name_length = get_byte(data, ref get_ptr);
            byte[] name = new byte[1024];
            i = get_bytes(data, name, ref get_ptr, name_length, name.Length - 1);
            name[i] = 0;
            header.name = byte_to_string(name, i);

            /* defaults for other type */
            header.unix_mode = 0100000 | 0000666;
            header.unix_gid = 0;
            header.unix_uid = 0;
            header.has_crc = 1;
            header.crc = get_word(data, ref get_ptr);
            header.extend_type = get_byte(data, ref get_ptr);

            dummy = header_size + 2 - name_length - 27;
            if (dummy > 0)
                get_ptr += dummy; /* skip old style extend header */

            extend_size = get_word(data, ref get_ptr);
            int hcrc = 0;
            extend_size = get_extended_header(ref mem_ptr, ref header, extend_size, ref hcrc);
            if (extend_size == -1)
                return false;

            /* On level 1 header, size fields should be adjusted. */
            /* the `packed_size' field contains the extended header size. */
            /* the `header_size' field does not. */
            header.packed_size -= extend_size;
            header.header_size += extend_size + 2;

            return true;
        }

        /// <summary>
        /// 读取一种格式的文件头内容
        /// </summary>
        /// <param name="data"></param>
        /// <param name="mem_ptr"></param>
        /// <param name="header"></param>
        /// <returns></returns>
        bool get_header_level2(byte[] data, ref int mem_ptr, ref LHAHeader header)
        {
            int get_ptr = 0;
            int header_size = get_word(data, ref get_ptr);

            header.header_size = header_size;
            header.size_field_length = 2;

            // 检查内存范围
            if (mem_ptr + 5 > fileContent.Length)
            {
                return false;
            }

            copy_data(data, mem_ptr, 21, 5);
            mem_ptr += 5;

            get_bytes(data, header.method, ref get_ptr, 5, 5);
            header.packed_size = get_longword(data, ref get_ptr);
            header.original_size = get_longword(data, ref get_ptr);
            header.unix_last_modified_stamp = (ulong)get_longword(data, ref get_ptr);
            header.attribute = get_byte(data, ref get_ptr); /* reserved */
            header.header_level = get_byte(data, ref get_ptr);

            /* defaults for other type */
            header.unix_mode = 0100000 | 0000666;
            header.unix_gid = 0;
            header.unix_uid = 0;

            header.has_crc = 1;
            header.crc = get_word(data, ref get_ptr);
            header.extend_type = get_byte(data, ref get_ptr);
            int extend_size = get_word(data, ref get_ptr);

            int hcrc = 0;
            hcrc = calccrc(hcrc, data, get_ptr);

            extend_size = get_extended_header(ref mem_ptr, ref header, extend_size, ref hcrc);
            if (extend_size == -1)
            {
                return false;
            }

            int padding = header_size - 26 - extend_size;
            while (padding-- > 0)           /* padding should be 0 or 1 */
            {
                hcrc = update_crc(hcrc, fileContent[mem_ptr++]);
            }

            return true;
        }

        /// <summary>
        /// 读取一种格式的文件头内容
        /// </summary>
        /// <param name="data"></param>
        /// <param name="mem_ptr"></param>
        /// <param name="header"></param>
        /// <returns></returns>
        bool get_header_level3(byte[] data, ref int mem_ptr, ref LHAHeader header)
        {
            int extend_size;
            int padding;
            int hcrc;

            int header_size;
            int get_ptr = 0;
            header.size_field_length = get_word(data, ref get_ptr);

            // 检查内存范围
            if (mem_ptr + 32 - 21 > fileContent.Length)
            {
                return false;
            }

            copy_data(data, mem_ptr, 21, 32 - 21);

            mem_ptr += 32 - 21;

            get_bytes(data, header.method, ref get_ptr, 5, 5);
            header.packed_size = get_longword(data, ref get_ptr);
            header.original_size = get_longword(data, ref get_ptr);
            header.unix_last_modified_stamp = (ulong)get_longword(data, ref get_ptr);
            header.attribute = get_byte(data, ref get_ptr); /* reserved */
            header.header_level = get_byte(data, ref get_ptr);

            /* defaults for other type */
            header.unix_mode = 0100000 | 0000666;
            header.unix_gid = 0;
            header.unix_uid = 0;

            header.has_crc = 1;
            header.crc = get_word(data, ref get_ptr);
            header.extend_type = get_byte(data, ref get_ptr);
            header.header_size = header_size = get_longword(data, ref get_ptr);
            extend_size = get_longword(data, ref get_ptr);

            hcrc = 0;
            hcrc = calccrc(hcrc, data, get_ptr);

            extend_size = get_extended_header(ref mem_ptr, ref header, extend_size, ref hcrc);
            if (extend_size == -1)
                return false;

            padding = header_size - 32 - extend_size;
            while (padding-- > 0)           /* padding should be 0 */
            {
                hcrc = update_crc(hcrc, fileContent[mem_ptr++]);
            }
            if (header.header_crc != hcrc)
            {

            }

            return true;
        }

        /// <summary>
        /// 读取文件头的扩展信息
        /// </summary>
        /// <param name="mem_ptr"></param>
        /// <param name="header"></param>
        /// <param name="header_size"></param>
        /// <param name="hcrc"></param>
        /// <returns></returns>
        int get_extended_header(ref int mem_ptr, ref LHAHeader header, int header_size, ref int hcrc)
        {
            byte[] data = new byte[4096];
            byte[] dirname = new byte[1024];

            int i;
            int ext_type;
            int name_length;
            int dir_length = 0;
            int n = 1 + header.size_field_length; /* `ext-type' + `next-header size' */

            int whole_size = header_size;

            if (header.header_level == 0)
                return 0;

            name_length = header.name.Length;

            while (header_size != 0)
            {
                int get_ptr = 0;
                if (data.Length < header_size)
                {
                    return 1;
                }

                byte[] name = new byte[1024];
                byte[] realname = new byte[1024];
                byte[] user = new byte[256];
                byte[] group = new byte[256];

                // 检查内存
                if (mem_ptr + header_size > fileContent.Length)
                {
                    return 1;
                }

                copy_data(data, mem_ptr, 0, header_size);

                mem_ptr += header_size;

                ext_type = get_byte(data, ref get_ptr);
                switch (ext_type)
                {
                    case 0:
                        /* header crc (CRC-16) */
                        header.header_crc = get_word(data, ref get_ptr);

                        /* clear buffer for CRC calculation. */
                        data[1] = data[2] = 0;
                        get_ptr += header_size - n - 2;
                        break;
                    case 1:
                        /* filename */
                        name_length = get_bytes(data, name, ref get_ptr, header_size - n, name.Length - 1);
                        name[name_length] = 0;
                        header.name = byte_to_string(name, name_length);
                        break;
                    case 2:
                        /* directory */
                        dir_length = get_bytes(data, dirname, ref get_ptr, header_size - n, dirname.Length - 1);
                        dirname[dir_length] = 0;
                        break;
                    case 0x40:
                        /* MS-DOS attribute */
                        header.attribute = get_word(data, ref get_ptr);
                        break;
                    case 0x41:
                        /* Windows time stamp (FILETIME structure) */
                        /* it is time in 100 nano seconds since 1601-01-01 00:00:00 */
                        get_ptr += 8; /* create time is ignored */

                        /* set last modified time */
                        if (header.header_level >= 2)
                            get_ptr += 8;  /* time_t has been already set */
                        else
                            header.unix_last_modified_stamp = wintime_to_unix_stamp(get_longword(data, ref get_ptr), get_longword(data, ref get_ptr));

                        get_ptr += 8; /* last access time is ignored */

                        break;
                    case 0x50:
                        /* UNIX permission */
                        header.unix_mode = get_word(data, ref get_ptr);
                        break;
                    case 0x51:
                        /* UNIX gid and uid */
                        header.unix_gid = get_word(data, ref get_ptr);
                        header.unix_uid = get_word(data, ref get_ptr);
                        break;
                    case 0x52:
                        /* UNIX group name */
                        i = get_bytes(data, group, ref get_ptr, header_size - n, group.Length - 1);
                        group[i] = 0;
                        header.group = byte_to_string(group, i);
                        break;
                    case 0x53:
                        /* UNIX user name */
                        i = get_bytes(data, user, ref get_ptr, header_size - n, user.Length - 1);
                        user[i] = 0;
                        header.user = byte_to_string(user, i);
                        break;
                    case 0x54:
                        /* UNIX last modified time */
                        header.unix_last_modified_stamp = 0;
                        break;
                    default:
                        get_ptr += (header_size - n);
                        break;
                }

                if (hcrc != 0)
                {
                    hcrc = calccrc(hcrc, data, header_size);
                }

                if (header.size_field_length == 2)
                    whole_size += header_size = get_word(data, ref get_ptr);
                else
                    whole_size += header_size = get_longword(data, ref get_ptr);
            }

            return whole_size;
        }

        /// <summary>
        /// 读取一个文件头
        /// </summary>
        /// <param name="data_index"></param>
        /// <param name="header"></param>
        /// <returns></returns>
        bool get_heaer(int data_index, out LHAHeader header)
        {
            header = new LHAHeader();
            header.method = new byte[5];
            header.name = string.Empty;
            header.realname = string.Empty;
            header.user = string.Empty;
            header.group = string.Empty;

            int mem_ptr = data_index;

            byte[] data = new byte[4096];

            if (fileContent[mem_ptr] == 0)
            {
                return false;
            }

            // 检查内存
            if (mem_ptr + 21 > fileContent.Length)
            {
                return false;
            }

            copy_data(data, mem_ptr, 0, 21);
            mem_ptr += 21;

            switch (data[20])
            {
                case 0:
                    if (!get_header_level0(data, ref mem_ptr, ref header))
                    {
                        return false;
                    }
                    break;
                case 1:
                    if (!get_header_level1(data, ref mem_ptr, ref header))
                    {
                        return false;
                    }
                    break;
                case 2:
                    if (!get_header_level2(data, ref mem_ptr, ref header))
                    {
                        return false;
                    }
                    break;
                case 3:
                    if (!get_header_level3(data, ref mem_ptr, ref header))
                    {
                        return false;
                    }
                    break;
                default:
                    return false;
            }

            header.data_offset = mem_ptr - data_index;

            return true;
        }
    }
}
