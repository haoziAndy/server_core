//
// request.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef Z_NET_HTTP_REQUEST_H
#define Z_NET_HTTP_REQUEST_H

#include "z/net/http/http_request.h"

namespace z {
namespace net {
namespace http {

struct Header
{
    std::string name;
    std::string value;
};

/// A request received from a client.
struct Request
{
    Request()
        : http_version_major(0)
        , http_version_minor(0)
        , content_length(0)
    {}
    std::string method;
    std::string uri;
    int http_version_major;
    int http_version_minor;
    std::vector<Header> headers;
    std::string content;
    int content_length;
};

struct QueryParam
{
    std::string name;
    std::string value;
};

struct RequestUri
{
    std::string query_api;
    std::vector<QueryParam> query_params;
    std::string post_data;
};

} // namespace http
} // namespace net
} // namespace z

#endif // Z_NET_HTTP_REQUEST_H