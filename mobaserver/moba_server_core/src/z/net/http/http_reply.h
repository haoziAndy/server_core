//
// reply.hpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef Z_NET_HTTP_REPLY_H
#define Z_NET_HTTP_REPLY_H

#include "z/net/http/http_request.h"

namespace z {
namespace net {
namespace http {

/// A reply to be sent to a client.
struct Reply
{
    Reply()
    {
    }
    /// The status of the reply.
    enum status_type
    {
        ok = 200,
        created = 201,
        accepted = 202,
        no_content = 204,
        multiple_choices = 300,
        moved_permanently = 301,
        moved_temporarily = 302,
        not_modified = 304,
        bad_request = 400,
        unauthorized = 401,
        forbidden = 403,
        not_found = 404,
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503
    } status;

    const std::string& content() const { return content_;}
    
    void SetContent(const std::string& _content, Reply::status_type _status = Reply::ok);

    void AddHeader(const std::string& header_name, const std::string& value);

    /// Convert the reply into a vector of buffers. The buffers do not own the
    /// underlying memory blocks, therefore the reply object must remain valid and
    /// not be changed until the write operation has completed.
    std::vector<boost::asio::const_buffer> to_buffers() const;

    /// Get a stock reply.
    static Reply StockReply(status_type status);

private:
    /// The headers to be included in the reply.
    std::vector<Header> headers_;

    /// The content to be sent in the reply.
    std::string content_;
};

} // namespace http
} // namespace net
} // namespace z

#endif // Z_NET_HTTP_REPLY_H