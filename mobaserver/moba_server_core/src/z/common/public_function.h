#ifndef SRC_Z_COMMON_PUBLIC_FUNCTION_H
#define SRC_Z_COMMON_PUBLIC_FUNCTION_H

namespace z {
namespace util {

google::protobuf::Message* CreateMessageByTypeName(const std::string& type_name);

//std::string MD5(const std::string& content, bool to_upper = true);

//std::string Base64Encode(const std::string& content);

//std::string Base64Decode(const std::string& content);

std::string UrlEncode(const std::string& str);

std::string UrlDecode(const std::string& str);

// gzip compress
std::string Compress(const std::string& str);
// gzip uncompress
std::string Decompress(const std::string& str);

// snappy compress
std::string FastCompress(const char* data, int32 length);
std::string FastCompress(const std::string& str);
// snappy uncompress
std::string FastUncompress(const char* data, int32 length);
std::string FastUncompress(const std::string& str);

std::string AESEncryptStr(const std::string& key, const std::string& plain_text, const std::string& ci);
std::string AESDecryptStr(const std::string& key, const std::string& crypted_text, const std::string& ci);

// protobuf …Ë÷√Œ™default value
void ApplyProtobufDefaults(::google::protobuf::Message *m, bool apply_no_default = true);

const std::string ConvertMessageToJson(const google::protobuf::Message *m);
}
}

#endif // !SRC_Z_COMMON_PUBLIC_FUNCTION_H
