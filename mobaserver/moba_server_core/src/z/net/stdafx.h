#ifndef NET_STDAFX_H
#define NET_STDAFX_H

#include <boost/timer/timer.hpp>
#include <boost/timer/progress_display.hpp>
#include <boost/date_time.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/singleton_pool.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/filesystem.hpp>
#include <boost/random.hpp>
#include <boost/random/random_device.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>


#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <zmq.h>

#include <google/protobuf/message.h>

#include <map>
#include <list>
#include <vector>
#include <set>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <iostream>
#include <time.h>
#include <string>

#include "log4cplus/logger.h"
#include "log4cplus/appender.h"
#include "log4cplus/fileappender.h"
#include "log4cplus/consoleappender.h"
#include "log4cplus/loglevel.h"
#include "log4cplus/loggingmacros.h"
#include "log4cplus/configurator.h"
#include "log4cplus/initializer.h"
#include <z/common/types.h>
#include <z/common/singleton.h>
#include <z/common/memory_pool.h>
#include <z/common/logger.h>
#include <z/common/error.h>
#include <z/common/public_function.h>


#include "asio_kcp/server.hpp"
#include "udp_server.h"
#include "u_connection.h"

#ifdef _WIN32
#define snprintf sprintf_s
#define localtime_r(a,b) localtime_s(b, a)
#define gmtime_r(a,b) gmtime_s(b, a)
#endif // _WIN32

#endif //NET_STDAFX_H
