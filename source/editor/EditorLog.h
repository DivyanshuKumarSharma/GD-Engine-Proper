#pragma once

#include <string>
#include <vector>

namespace editor
{
    enum class LogLevel
    {
        Info,
        Success,
        Warning,
        Error
    };

    struct LogEntry
    {
        LogLevel level = LogLevel::Info;
        std::string category;
        std::string message;
        double time = 0.0;   // seconds since editor start
        int count = 1;       // consecutive identical entries are collapsed
    };

    // Process-wide diagnostics sink. The Diagnostics panel renders Entries();
    // actionable messages (missing asset, invalid config) should be logged here
    // with a category so the user can filter.
    class EditorLog
    {
    public:
        static void Info(const std::string& category, const std::string& message);
        static void Success(const std::string& category, const std::string& message);
        static void Warn(const std::string& category, const std::string& message);
        static void Error(const std::string& category, const std::string& message);
        static void Add(LogLevel level, const std::string& category, const std::string& message);

        static const std::vector<LogEntry>& Entries();
        static void Clear();
        static int WarningCount();
        static int ErrorCount();

        // Advances the clock used for entry timestamps.
        static void Tick(double deltaSeconds);

        // Routes anything written to std::cerr / std::clog into the log (one
        // entry per line, category "engine"). Call once at editor start-up.
        static void CaptureStandardError();
        static void ReleaseStandardError();
    };
}
