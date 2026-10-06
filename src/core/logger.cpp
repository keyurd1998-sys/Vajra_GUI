#include "vajra/core/logger.hpp"

#include <cstdio>
#include <iostream>

namespace vajra::core {

namespace {
constexpr std::string_view ANSI_RESET = "\033[0m";
constexpr std::string_view ANSI_CYAN = "\033[1;36m";
constexpr std::string_view ANSI_YELLOW = "\033[1;33m";
constexpr std::string_view ANSI_RED = "\033[1;31m";
} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() = default;

Logger::~Logger() {
    close_transcript();
}

void Logger::init_transcript(const std::filesystem::path& log_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (transcript_.is_open()) {
        transcript_.close();
    }
    transcript_path_ = log_path;
    if (auto parent = transcript_path_.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    transcript_.open(transcript_path_, std::ios::out | std::ios::trunc);
}

void Logger::close_transcript() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (transcript_.is_open()) {
        transcript_.flush();
        transcript_.close();
    }
    transcript_path_.clear();
}

std::optional<std::filesystem::path> Logger::get_transcript_path() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (transcript_.is_open()) {
        return transcript_path_;
    }
    return std::nullopt;
}

void Logger::set_verbose(bool verbose) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    verbose_ = verbose;
}

void Logger::set_quiet(bool quiet) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    quiet_ = quiet;
}

void Logger::set_color_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    color_enabled_ = enabled;
}

bool Logger::is_verbose() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return verbose_;
}

bool Logger::is_quiet() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return quiet_;
}

bool Logger::is_color_enabled() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return color_enabled_;
}

void Logger::set_stream(std::ostream* stream) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    stream_ = stream ? stream : &std::cout;
}

std::ostream* Logger::get_stream() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return stream_;
}

void Logger::log_formatted(Severity sev, int code, std::string_view msg) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    std::string sev_tag;
    std::string_view color_code;

    switch (sev) {
    case Severity::INFO:
        sev_tag = "INFO";
        color_code = ANSI_CYAN;
        break;
    case Severity::WARNING:
        sev_tag = "WARN";
        color_code = ANSI_YELLOW;
        break;
    case Severity::ERROR:
        sev_tag = "ERR";
        color_code = ANSI_RED;
        break;
    case Severity::RAW:
        break;
    }

    char hdr_buf[64];
    std::snprintf(hdr_buf, sizeof(hdr_buf), "[VAJRA-%s-%03d]", sev_tag.c_str(), code);
    std::string header(hdr_buf);
    std::string plain_line = header + " " + std::string(msg) + "\n";

    // Mirror to transcript
    if (transcript_.is_open()) {
        transcript_ << plain_line;
        transcript_.flush();
    }

    // Suppress INFO messages if quiet mode is active
    if (sev == Severity::INFO && quiet_) {
        return;
    }

    if (stream_) {
        if (color_enabled_) {
            *stream_ << color_code << header << ANSI_RESET << " " << msg << "\n";
        } else {
            *stream_ << plain_line;
        }
        stream_->flush();
    }
}

void Logger::info(int code, std::string_view msg) {
    log_formatted(Severity::INFO, code, msg);
}

void Logger::warning(int code, std::string_view msg) {
    log_formatted(Severity::WARNING, code, msg);
}

void Logger::error(int code, std::string_view msg) {
    log_formatted(Severity::ERROR, code, msg);
}

void Logger::raw(std::string_view msg) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    std::string line{msg};
    if (line.empty() || line.back() != '\n') {
        line.push_back('\n');
    }

    if (transcript_.is_open()) {
        transcript_ << line;
        transcript_.flush();
    }

    if (stream_) {
        *stream_ << line;
        stream_->flush();
    }
}

OutputRedirectGuard::OutputRedirectGuard(Logger& logger, const std::filesystem::path& path, bool append)
    : logger_(logger) {
    if (auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    auto mode = std::ios::out;
    if (append) {
        mode |= std::ios::app;
    } else {
        mode |= std::ios::trunc;
    }
    file_.open(path, mode);
    if (file_.is_open()) {
        is_open_ = true;
        prev_stream_ = logger_.get_stream();
        prev_color_ = logger_.is_color_enabled();
        logger_.set_stream(&file_);
        logger_.set_color_enabled(false);
    }
}

OutputRedirectGuard::~OutputRedirectGuard() {
    if (is_open_) {
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
        logger_.set_stream(prev_stream_);
        logger_.set_color_enabled(prev_color_);
    }
}

bool OutputRedirectGuard::is_open() const {
    return is_open_;
}

} // namespace vajra::core
