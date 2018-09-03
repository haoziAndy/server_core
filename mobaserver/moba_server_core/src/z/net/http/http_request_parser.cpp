//
// request_parser.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "stdafx.h"
#include "z/net/http/http_request.h"
#include "z/net/http/http_request_parser.h"


namespace z {
namespace net {
namespace http {

RequestParser::RequestParser()
  : state_(method_start)
{
}

void RequestParser::Reset()
{
  state_ = method_start;
}

RequestParser::result_type RequestParser::Consume(Request& req, char input)
{
  switch (state_)
  {
  case method_start:
    if (!is_char(input) || is_ctl(input) || is_tspecial(input))
    {
      return bad;
    }
    else
    {
      state_ = method;
      req.method.push_back(input);
      return indeterminate;
    }
  case method:
    if (input == ' ')
    {
      state_ = uri;
      return indeterminate;
    }
    else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
    {
      return bad;
    }
    else
    {
      req.method.push_back(input);
      return indeterminate;
    }
  case uri:
    if (input == ' ')
    {
      state_ = http_version_h;
      return indeterminate;
    }
    else if (is_ctl(input))
    {
      return bad;
    }
    else
    {
      req.uri.push_back(input);
      return indeterminate;
    }
  case http_version_h:
    if (input == 'H')
    {
      state_ = http_version_t_1;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_t_1:
    if (input == 'T')
    {
      state_ = http_version_t_2;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_t_2:
    if (input == 'T')
    {
      state_ = http_version_p;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_p:
    if (input == 'P')
    {
      state_ = http_version_slash;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_slash:
    if (input == '/')
    {
      req.http_version_major = 0;
      req.http_version_minor = 0;
      state_ = http_version_major_start;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_major_start:
    if (is_digit(input))
    {
      req.http_version_major = req.http_version_major * 10 + input - '0';
      state_ = http_version_major;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_major:
    if (input == '.')
    {
      state_ = http_version_minor_start;
      return indeterminate;
    }
    else if (is_digit(input))
    {
      req.http_version_major = req.http_version_major * 10 + input - '0';
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_minor_start:
    if (is_digit(input))
    {
      req.http_version_minor = req.http_version_minor * 10 + input - '0';
      state_ = http_version_minor;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_minor:
    if (input == '\r')
    {
      state_ = expecting_newline_1;
      return indeterminate;
    }
    else if (is_digit(input))
    {
      req.http_version_minor = req.http_version_minor * 10 + input - '0';
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case expecting_newline_1:
    if (input == '\n')
    {
      state_ = header_line_start;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case header_line_start:
    if (input == '\r')
    {
      state_ = expecting_newline_3;
      return indeterminate;
    }
    else if (!req.headers.empty() && (input == ' ' || input == '\t'))
    {
      state_ = header_lws;
      return indeterminate;
    }
    else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
    {
      return bad;
    }
    else
    {
      req.headers.push_back(Header());
      req.headers.back().name.push_back(input);
      state_ = header_name;
      return indeterminate;
    }
  case header_lws:
    if (input == '\r')
    {
      state_ = expecting_newline_2;
      return indeterminate;
    }
    else if (input == ' ' || input == '\t')
    {
      return indeterminate;
    }
    else if (is_ctl(input))
    {
      return bad;
    }
    else
    {
      state_ = header_value;
      req.headers.back().value.push_back(input);
      return indeterminate;
    }
  case header_name:
    if (input == ':')
    {
      state_ = space_before_header_value;
      return indeterminate;
    }
    else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
    {
      return bad;
    }
    else
    {
      req.headers.back().name.push_back(input);
      return indeterminate;
    }
  case space_before_header_value:
    if (input == ' ')
    {
      state_ = header_value;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case header_value:
    if (input == '\r')
    {
      state_ = expecting_newline_2;
      if (req.headers.back().name == "Content-Length")
      {
          try
          {
              req.content_length =
                  boost::lexical_cast<std::size_t>(req.headers.back().value);
          }
          catch (boost::bad_lexical_cast&)
          {
              return bad;
          }
      }
      return indeterminate;
    }
    else if (is_ctl(input))
    {
      return bad;
    }
    else
    {
      req.headers.back().value.push_back(input);
      return indeterminate;
    }
  case expecting_newline_2:
    if (input == '\n')
    {
      state_ = header_line_start;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case expecting_newline_3:
      if (input == '\n')
      {
          if (req.method == "POST")
          {
              if (req.content_length > 0)
              {
                  state_ = option_post_data;
                  return indeterminate;
              }
              else
              {
                  return good;
              }
          }
          else
              return good;
      }
      else
          return bad;
  case option_post_data:
      {
          req.content.push_back(input);
          if (req.content.size() < req.content_length)
              return indeterminate;
          else
              return good;
      }
  default:
    return bad;
  }
}

bool RequestParser::is_char(int c)
{
  return c >= 0 && c <= 127;
}

bool RequestParser::is_ctl(int c)
{
  return (c >= 0 && c <= 31) || (c == 127);
}

bool RequestParser::is_tspecial(int c)
{
  switch (c)
  {
  case '(': case ')': case '<': case '>': case '@':
  case ',': case ';': case ':': case '\\': case '"':
  case '/': case '[': case ']': case '?': case '=':
  case '{': case '}': case ' ': case '\t':
    return true;
  default:
    return false;
  }
}

bool RequestParser::is_digit(int c)
{
  return c >= '0' && c <= '9';
}

std::string RequestParser::UrlDecode(const std::string& sSrc)
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

bool RequestParser::ParseRequestUri( const std::string& uri_str, RequestUri* req_uri )
{
    if (uri_str.empty())
        return false;

    if (!boost::algorithm::starts_with(uri_str, "/"))
        return false;

    std::vector<std::string> api_split_strings;
    boost::algorithm::split(api_split_strings, uri_str, boost::is_any_of("?"));

    if (api_split_strings.size() > 2 || api_split_strings.size() <= 0)
        return false;
    
    if (boost::algorithm::contains(api_split_strings.at(0), ".."))
        return false;

    if (api_split_strings.size() == 1)
    {
        req_uri->query_api.swap(api_split_strings.at(0));
        return true;
    }

    auto& query_params_str = api_split_strings.at(1);

    std::vector<std::string> params_str_v;
    boost::algorithm::split(params_str_v, query_params_str, boost::is_any_of("&"));

    std::vector<QueryParam> params_vec;
    params_vec.reserve(params_str_v.size());
    for (auto it = params_str_v.begin(); it != params_str_v.end(); ++it)
    {
        auto& param_str = *it;
        std::vector<std::string> param_str_v;
        boost::algorithm::split(param_str_v, param_str, boost::is_any_of("="));
        if (param_str_v.size() != 2)
            return false;
        auto cur_index = params_vec.size();
        params_vec.resize(cur_index + 1);
        auto& param = params_vec.at(cur_index);
        param.name = UrlDecode(param_str_v.at(0));
        param.value = UrlDecode(param_str_v.at(1));
    }
    req_uri->query_api.swap(api_split_strings.at(0));
    req_uri->query_params.swap(params_vec);

    return true;
}

bool RequestParser::ParseRequestParam( const std::string& query_params_str, std::vector<QueryParam>* query_params )
{
    std::vector<std::string> params_str_v;
    boost::algorithm::split(params_str_v, query_params_str, boost::is_any_of("&"));

    std::vector<QueryParam> params_vec;
    query_params->reserve(params_str_v.size());
    for (auto it = params_str_v.begin(); it != params_str_v.end(); ++it)
    {
        auto& param_str = *it;
        std::vector<std::string> param_str_v;
        boost::algorithm::split(param_str_v, param_str, boost::is_any_of("="));
        if (param_str_v.size() != 2)
            return false;
        auto cur_index = params_vec.size();
        params_vec.resize(cur_index + 1);
        auto& param = params_vec.at(cur_index);
        param.name = UrlDecode(param_str_v.at(0));
        param.value = UrlDecode(param_str_v.at(1));
    }
    query_params->swap(params_vec);

    return true;
}

} // namespace http
} // namespace net
} // namespace z
