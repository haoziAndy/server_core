#ifndef GATE_STDAFX_H
#define GATE_STDAFX_H

#include <stdarg.h>

#include <zmq.h>

#include <google/protobuf/message.h>

#include <boost/timer/timer.hpp>
#include <boost/timer/progress_display.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/singleton_pool.hpp>
#include <boost/format.hpp>
#include <boost/random.hpp>

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

#include <z/common/types.h>
#include <z/common/singleton.h>


#ifdef _WIN32
#define snprintf sprintf_s
#define localtime_r(a,b) localtime_s(b, a)
#define gmtime_r(a,b) gmtime_s(b, a)
#endif // _WIN32

#endif //GATE_STDAFX_H
