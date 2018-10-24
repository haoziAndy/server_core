#include "stdafx.h"
#include "public_function.h"
#include "logger.h"
#include "snappy.h"
#include "google/protobuf/util/json_util.h"

namespace z {
namespace util {


google::protobuf::Message* CreateMessageByTypeName( const std::string& type_name )
{
    auto descriptor = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
    if (descriptor)
    {
        auto prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype)
        {
            return prototype->New();
        }
        else
        {
            return nullptr;
        }
    }
    return nullptr;
}

const std::string ConvertMessageToJson(const google::protobuf::Message *m) {
	std::string str;
	if( m == nullptr) {
		return str;
	}
	google::protobuf::util::MessageToJsonString(*m, &str);
	return str;
}
/*

std::string MD5( const std::string& content, bool to_upper /*= true)
{
static CryptoPP::Weak::MD5 hash;
static byte digest[CryptoPP::Weak::MD5::DIGESTSIZE];
CryptoPP::HexEncoder encoder(nullptr, to_upper);

hash.CalculateDigest(digest, (byte*)content.c_str(), content.length());
std::string output;
encoder.Attach(new CryptoPP::StringSink(output));
encoder.Put(digest, sizeof(digest));
encoder.MessageEnd();
encoder.Detach();

return output;
}


*/

std::string Base64Encode( const std::string& content )
{
    static CryptoPP::Base64Encoder encoder(nullptr, false);
    std::string encoded;
    encoder.Attach(new CryptoPP::StringSink(encoded));
    encoder.Put((byte*)content.data(), content.size());
    encoder.MessageEnd();
    encoder.Detach();

    return encoded;
}

std::string Base64Decode( const std::string& content )
{
    static CryptoPP::Base64Decoder decoder;
    std::string decoded;
    decoder.Attach(new CryptoPP::StringSink(decoded));
    decoder.Put((byte*)content.data(), content.size());
    decoder.MessageEnd();
    decoder.Detach();

    return decoded;
}

#ifdef Release
std::string Compress(const std::string& str)
{
	CryptoPP::Gzip compressor;
	// put data into the compressor
	compressor.PutMessageEnd(reinterpret_cast<const byte*>(str.data()), str.size());
	// flush it to mark the end of a zlib message
	bool bFlushRet = compressor.Flush(true);
	// get size of compressed data
	size_t outsize =
		static_cast<size_t>(compressor.MaxRetrievable());

	void* out_buff = alloca(outsize);
	compressor.Get(reinterpret_cast<byte*>(out_buff), outsize);

	return std::string(static_cast<char*>(out_buff), outsize);
}

std::string Decompress(const std::string& str)
{
	CryptoPP::Gunzip decompressor;
	size_t putSize = decompressor.Put(reinterpret_cast<const byte*>(str.data()), str.size());
	// tell filter we want to process whatever we have received
	bool bFlushRet = decompressor.Flush(true);
	size_t outsize =
		static_cast<size_t>(decompressor.MaxRetrievable());
	void* out_buff = alloca(outsize);
	decompressor.Get(reinterpret_cast<byte*>(out_buff), outsize);

	return std::string(static_cast<char*>(out_buff), outsize);
}

// snappy compress
std::string FastCompress(const char* data, int32 length)
{
	std::string compressed_str;
	snappy::Compress(data, length, &compressed_str);
	return compressed_str;
}

std::string FastCompress(const std::string& str)
{
	std::string compressed_str;
	snappy::Compress(str.c_str(), str.size(), &compressed_str);
	return compressed_str;
}

// snappy uncompress
std::string FastUncompress(const char* data, int32 length)
{
	std::string uncompressed_str;
	snappy::Uncompress(data, length, &uncompressed_str);
	return uncompressed_str;
}

std::string FastUncompress(const std::string& str)
{
	std::string uncompressed_str;
	snappy::Uncompress(str.c_str(), str.size(), &uncompressed_str);
	return uncompressed_str;
}


#endif // 

std::string UrlEncode(const std::string& str)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = str.begin(), n = str.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << '%' << std::setw(2) << int((unsigned char) c);
    }

    return escaped.str();
}

std::string UrlDecode(const std::string& sSrc)
{
    static const char HEX2DEC[256] = 
    {
        /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
        /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

        /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

        /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

        /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
    };


    // Note from RFC1630:  "Sequences which start with a percent sign
    // but are not followed by two hexadecimal characters (0-9, A-F) are reserved
    // for future extension"

    const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
    const int SRC_LEN = sSrc.length();
    const unsigned char * const SRC_END = pSrc + SRC_LEN;
    const unsigned char * const SRC_LAST_DEC = SRC_END - 2;   // last decodable '%' 

    char * const pStart = new char[SRC_LEN];
    char * pEnd = pStart;

    while (pSrc < SRC_LAST_DEC)
    {
        if (*pSrc == '%')
        {
            char dec1, dec2;
            if (-1 != (dec1 = HEX2DEC[*(pSrc + 1)])
                && -1 != (dec2 = HEX2DEC[*(pSrc + 2)]))
            {
                *pEnd++ = (dec1 << 4) + dec2;
                pSrc += 3;
                continue;
            }
        }
        else if (*pSrc == '+')
        {
            *pEnd++ = ' ';
            pSrc++;
            continue;
        }
        *pEnd++ = *pSrc++;
    }

    // the last 2- chars
    while (pSrc < SRC_END)
        *pEnd++ = *pSrc++;

    std::string sResult(pStart, pEnd);
    delete [] pStart;
    return sResult;

}

void ApplyProtobufDefaults(::google::protobuf::Message *m, bool apply_no_default)
{
    const ::google::protobuf::Descriptor *d = m->GetDescriptor();
    const ::google::protobuf::Reflection *r = m->GetReflection();

    for (int i = 0; i < d->field_count(); ++i) 
    {
        // Retrieve field info
        const ::google::protobuf::FieldDescriptor *fd = d->field(i);
        const ::google::protobuf::FieldDescriptor::CppType fdt = fd->cpp_type();

        if (::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE == fdt) 
        {
            // Recursively apply defaults if a nested message/group
            // is encountered
            if (!fd->is_repeated())
            {
                ApplyProtobufDefaults(r->MutableMessage(m, fd), apply_no_default);
            }
            else
            {
                r->AddMessage(m, fd);
                ApplyProtobufDefaults(r->MutableRepeatedMessage(m, fd, 0));
            }
        } 
        else if ((apply_no_default || fd->has_default_value()))
        {
            if (!fd->is_repeated())
            {
                if (!r->HasField(*m, fd))
                {
                    // Field has default value and has not yet been set -> apply default
                    switch(fdt) 
                    {
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_INT32 :
                        r->SetInt32(m, fd, fd->default_value_int32());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_INT64 :
                        r->SetInt64(m, fd, fd->default_value_int64());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT32 :
                        r->SetUInt32(m, fd, fd->default_value_uint32());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT64 :
                        r->SetUInt64(m, fd, fd->default_value_uint64());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE :
                        r->SetDouble(m, fd, fd->default_value_double());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_FLOAT :
                        r->SetFloat(m, fd, fd->default_value_float());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_BOOL :
                        r->SetBool(m, fd, fd->default_value_bool());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM :
                        r->SetEnum(m, fd, fd ->default_value_enum());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_STRING :
                        r->SetString(m, fd, fd->default_value_string());
                        break;
                    } // switch
                }
            }
            else // is_repeated
            {
                if (r->FieldSize(*m, fd) == 0)
                {
                    // Field has default value and has not yet been set -> apply default
                    switch(fdt) 
                    {
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_INT32 :
                        r->AddInt32(m, fd, fd->default_value_int32());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_INT64 :
                        r->AddInt64(m, fd, fd->default_value_int64());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT32 :
                        r->AddUInt32(m, fd, fd->default_value_uint32());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT64 :
                        r->AddUInt64(m, fd, fd->default_value_uint64());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE :
                        r->AddDouble(m, fd, fd->default_value_double());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_FLOAT :
                        r->AddFloat(m, fd, fd->default_value_float());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_BOOL :
                        r->AddBool(m, fd, fd->default_value_bool());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM :
                        r->AddEnum(m, fd, fd ->default_value_enum());
                        break;
                    case ::google::protobuf::FieldDescriptor::CPPTYPE_STRING :
                        r->AddString(m, fd, fd->default_value_string());
                        break;
                    } // switch
                }
            }           
        } // if
    } // for
} // apply_protobuf_defaults



}
}
