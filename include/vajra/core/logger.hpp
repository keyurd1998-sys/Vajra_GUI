#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <optional>

namespace vajra::core {

enum class Severity { INFO, WARNING, ERROR, RAW };

class Logger {
public:
    static Logger& instance();

    void init_transcript(const std::filesystem::path& log_path = "vajra.log");
    void close_transcript();
    std::optional<std::filesystem::path> get_transcript_path() const;

    void info(int code, std::string_view msg);
    void warning(int code, std::string_view msg);
    void error(int code, std::string_view msg);
    void raw(std::string_view msg);

    void set_verbose(bool verbose);
    void set_quiet(bool quiet);
    void set_color_enabled(bool enabled);

    bool is_verbose() const;
    bool is_quiet() const;
    bool is_color_enabled() const;

    void set_stream(std::ostream* stream);
    std::ostream* get_stream();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log_formatted(Severity sev, int code, std::string_view msg);

    std::ofstream transcript_;
    std::filesystem::path transcript_path_;
    std::ostream* stream_{&std::cout};
    bool verbose_{false};
    bool quiet_{false};
    bool color_enabled_{true};
    mutable std::mutex log_mutex_;
};

class OutputRedirectGuard {
public:
    OutputRedirectGuard(Logger& logger, const std::filesystem::path& path, bool append);
    ~OutputRedirectGuard();

    OutputRedirectGuard(const OutputRedirectGuard&) = delete;
    OutputRedirectGuard& operator=(const OutputRedirectGuard&) = delete;

    bool is_open() const;

private:
    Logger& logger_;
    std::ofstream file_;
    std::ostream* prev_stream_{nullptr};
    bool prev_color_{true};
    bool is_open_{false};
};

} // namespace vajra::core
