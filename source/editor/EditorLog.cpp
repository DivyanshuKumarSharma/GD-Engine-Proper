#include "editor/EditorLog.h"

#include <iostream>
#include <mutex>
#include <streambuf>

namespace editor
{
    namespace
    {
        std::vector<LogEntry> g_entries;
        std::mutex g_mutex;
        double g_clock = 0.0;
        int g_warnings = 0;
        int g_errors = 0;
        constexpr std::size_t kMaxEntries = 4000;

        // A streambuf that forwards to the original cerr buffer and also splits
        // completed lines into the editor log.
        class TeeBuf : public std::streambuf
        {
        public:
            explicit TeeBuf(std::streambuf* downstream) : m_downstream(downstream) {}

        protected:
            int overflow(int ch) override
            {
                if (ch == EOF)
                {
                    return ch;
                }
                if (m_downstream)
                {
                    m_downstream->sputc(static_cast<char>(ch));
                }
                if (ch == '\n')
                {
                    Flush();
                }
                else
                {
                    m_line += static_cast<char>(ch);
                    if (m_line.size() > 2000)
                    {
                        Flush();
                    }
                }
                return ch;
            }

            int sync() override
            {
                if (m_downstream)
                {
                    m_downstream->pubsync();
                }
                return 0;
            }

        private:
            void Flush()
            {
                if (!m_line.empty())
                {
                    LogLevel level = LogLevel::Info;
                    if (m_line.find("ERROR") != std::string::npos || m_line.find("error") != std::string::npos ||
                        m_line.find("Failed") != std::string::npos || m_line.find("failed") != std::string::npos)
                    {
                        level = LogLevel::Error;
                    }
                    else if (m_line.find("WARN") != std::string::npos || m_line.find("warning") != std::string::npos)
                    {
                        level = LogLevel::Warning;
                    }
                    EditorLog::Add(level, "engine", m_line);
                    m_line.clear();
                }
            }

            std::streambuf* m_downstream = nullptr;
            std::string m_line;
        };

        TeeBuf* g_tee = nullptr;
        std::streambuf* g_originalCerr = nullptr;
        std::streambuf* g_originalClog = nullptr;
    }

    void EditorLog::Add(LogLevel level, const std::string& category, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (!g_entries.empty())
        {
            LogEntry& last = g_entries.back();
            if (last.level == level && last.category == category && last.message == message)
            {
                ++last.count;
                last.time = g_clock;
                return;
            }
        }

        LogEntry entry;
        entry.level = level;
        entry.category = category;
        entry.message = message;
        entry.time = g_clock;
        g_entries.push_back(std::move(entry));

        if (level == LogLevel::Warning) ++g_warnings;
        if (level == LogLevel::Error) ++g_errors;

        if (g_entries.size() > kMaxEntries)
        {
            g_entries.erase(g_entries.begin(), g_entries.begin() + (kMaxEntries / 4));
        }
    }

    void EditorLog::Info(const std::string& c, const std::string& m) { Add(LogLevel::Info, c, m); }
    void EditorLog::Success(const std::string& c, const std::string& m) { Add(LogLevel::Success, c, m); }
    void EditorLog::Warn(const std::string& c, const std::string& m) { Add(LogLevel::Warning, c, m); }
    void EditorLog::Error(const std::string& c, const std::string& m) { Add(LogLevel::Error, c, m); }

    const std::vector<LogEntry>& EditorLog::Entries()
    {
        return g_entries;
    }

    void EditorLog::Clear()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_entries.clear();
        g_warnings = 0;
        g_errors = 0;
    }

    int EditorLog::WarningCount() { return g_warnings; }
    int EditorLog::ErrorCount() { return g_errors; }

    void EditorLog::Tick(double deltaSeconds)
    {
        g_clock += deltaSeconds;
    }

    void EditorLog::CaptureStandardError()
    {
        if (g_tee)
        {
            return;
        }
        g_originalCerr = std::cerr.rdbuf();
        g_tee = new TeeBuf(g_originalCerr);
        std::cerr.rdbuf(g_tee);
        g_originalClog = std::clog.rdbuf();
        std::clog.rdbuf(g_tee);
    }

    void EditorLog::ReleaseStandardError()
    {
        if (!g_tee)
        {
            return;
        }
        std::cerr.rdbuf(g_originalCerr);
        std::clog.rdbuf(g_originalClog);
        delete g_tee;
        g_tee = nullptr;
    }
}
