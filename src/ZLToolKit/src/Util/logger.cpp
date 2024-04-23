#include "logger.h"
#include <string.h>
#include <sys/stat.h>

namespace toolkit {
    Logger* g_defaultLogger = &Logger::Instance();
#define CLEAR_COLOR "\033[0m"
static const char *LOG_CONST_TABLE[][3] = {
        {"\033[44;37m", "\033[34m", "T"},
        {"\033[42;37m", "\033[32m", "D"},
        {"\033[46;37m", "\033[36m", "I"},
        {"\033[43;37m", "\033[33m", "W"},
        {"\033[41;37m", "\033[31m", "E"}};

///////////////////Logger///////////////////
INSTANCE_IMP(Logger, exeName());

Logger::Logger(const string &loggerName) {
    _loggerName = loggerName;
}

Logger::~Logger() {
    _writer.reset();
    {
        LogContextCapturer(*this, LInfo, __FILE__, __FUNCTION__, __LINE__);
    }
    _channels.clear();
}

void Logger::add(const std::shared_ptr<LogChannel> &channel) {
    _channels[channel->name()] = channel;
}

void Logger::del(const string &name) {
    _channels.erase(name);
}

std::shared_ptr<LogChannel> Logger::get(const string &name) {
    auto it = _channels.find(name);
    if (it == _channels.end()) {
        return nullptr;
    }
    return it->second;
}

void Logger::setWriter(const std::shared_ptr<LogWriter> &writer) {
    _writer = writer;
}

void Logger::write(const LogContextPtr &ctx) {
    if (_writer) {
        _writer->write(ctx);
    } else {
        writeChannels(ctx);
    }
}

void Logger::setLevel(LogLevel level) {
    for (auto &chn : _channels) {
        chn.second->setLevel(level);
    }
}

void Logger::writeChannels(const LogContextPtr &ctx) {
    for (auto &chn : _channels) {
        chn.second->write(*this, ctx);
    }
}

const string &Logger::getName() const {
    return _loggerName;
}

///////////////////LogContext///////////////////
static inline const char *getFileName(const char *file) {
    auto pos = strrchr(file, '/');
    return pos ? pos + 1 : file;
}

static inline const char *getFunctionName(const char *func) {
    return func;
}

LogContext::LogContext(LogLevel level, const char *file, const char *function, int line) :
        _level(level),
        _line(line),
        _file(getFileName(file)),
        _function(getFunctionName(function)) {
    gettimeofday(&_tv, NULL);
}

///////////////////AsyncLogWriter///////////////////
LogContextCapturer::LogContextCapturer(Logger &logger, LogLevel level, const char *file, const char *function, int line) :
        _ctx(new LogContext(level, file, function, line)), _logger(logger) {
}

LogContextCapturer::LogContextCapturer(const LogContextCapturer &that) : _ctx(that._ctx), _logger(that._logger) {
    const_cast<LogContextPtr &>(that._ctx).reset();
}

LogContextCapturer::~LogContextCapturer() {
    *this << endl;
}

LogContextCapturer &LogContextCapturer::operator<<(ostream &(*f)(ostream &)) {
    if (!_ctx) {
        return *this;
    }
    _logger.write(_ctx);
    _ctx.reset();
    return *this;
}

void LogContextCapturer::clear() {
    _ctx.reset();
}

///////////////////AsyncLogWriter///////////////////
AsyncLogWriter::AsyncLogWriter(Logger &logger) : _exit_flag(false), _logger(logger) {
    _thread = std::make_shared<thread>([this]() { this->run(); });
}

AsyncLogWriter::~AsyncLogWriter() {
    _exit_flag = true;
    _sem.post();
    _thread->join();
    flushAll();
}

void AsyncLogWriter::write(const LogContextPtr &ctx) {
    {
        lock_guard<mutex> lock(_mutex);
        _pending.emplace_back(ctx);
    }
    _sem.post();
}

void AsyncLogWriter::run() {
    while (!_exit_flag) {
        _sem.wait();
        flushAll();
    }
}

void AsyncLogWriter::flushAll() {
    List<LogContextPtr> tmp;
    {
        lock_guard<mutex> lock(_mutex);
        tmp.swap(_pending);
    }

    tmp.for_each([&](const LogContextPtr &ctx) {
        _logger.writeChannels(ctx);
    });

}

///////////////////ConsoleChannel///////////////////

ConsoleChannel::ConsoleChannel(const string &name, LogLevel level) : LogChannel(name, level) {}
ConsoleChannel::~ConsoleChannel() {}

void ConsoleChannel::write(const Logger &logger, const LogContextPtr &ctx) {
    if (_level > ctx->_level) {
        return;
    }
    format(logger, std::cout, ctx);
}


///////////////////LogChannel///////////////////
LogChannel::LogChannel(const string &name, LogLevel level) : _name(name), _level(level) {}

LogChannel::~LogChannel() {}

const string &LogChannel::name() const { return _name; }

void LogChannel::setLevel(LogLevel level) { _level = level; }

std::string LogChannel::printTime(const timeval &tv) {
    time_t sec_tmp = tv.tv_sec;
    struct tm tm;
    localtime_r(&sec_tmp, &tm);
    char buf[128];
    snprintf(buf, sizeof(buf), "%02d%02d %02d:%02d:%02d.%03d",
             1 + tm.tm_mon,
             tm.tm_mday,
             tm.tm_hour,
             tm.tm_min,
             tm.tm_sec,
             (int) (tv.tv_usec / 1000));
    return buf;
}

void LogChannel::format(const Logger &logger, ostream &ost, const LogContextPtr &ctx, bool enableColor, bool enableDetail) {
    if (ctx->str().empty()) {
        return;
    }

    ost << LOG_CONST_TABLE[ctx->_level][1] 
        << LOG_CONST_TABLE[ctx->_level][2]
        << printTime(ctx->_tv) << " "
        << ctx->_file << ":" << ctx->_line << " " << ctx->_function << " | "
        << ctx->str()
        << CLEAR_COLOR
        << std::endl;
}

}
