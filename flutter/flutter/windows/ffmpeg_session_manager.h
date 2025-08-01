#ifndef FFMPEG_SESSION_MANAGER_H_
#define FFMPEG_SESSION_MANAGER_H_

#include <flutter/encodable_value.h>
#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>

namespace ffmpeg_kit_flutter {

enum class SessionState {
    CREATED = 0,
    RUNNING = 1,
    FAILED = 2,
    COMPLETED = 3
};

enum class SessionType {
    FFMPEG = 1,
    FFPROBE = 2,
    MEDIA_INFORMATION = 3
};

enum class LogLevel {
    TRACE = 56,
    DEBUG = 48,
    VERBOSE = 40,
    INFO = 32,
    WARNING = 24,
    ERROR = 16,
    FATAL = 8,
    PANIC = 0
};

struct LogEntry {
    int64_t sessionId;
    LogLevel level;
    std::string message;
    int64_t timestamp;

    flutter::EncodableValue ToEncodableValue() const {
        return flutter::EncodableValue(flutter::EncodableMap{
            {"sessionId", flutter::EncodableValue(sessionId)},
            {"level", flutter::EncodableValue(static_cast<int>(level))},
            {"message", flutter::EncodableValue(message)},
            {"timestamp", flutter::EncodableValue(timestamp)}
        });
    }
};

struct StatisticsEntry {
    int64_t sessionId;
    int videoFrameNumber = 0;
    double videoFps = 0.0;
    double videoQuality = 0.0;
    int64_t size = 0;
    int time = 0;
    double bitrate = 0.0;
    double speed = 0.0;

    flutter::EncodableValue ToEncodableValue() const {
        return flutter::EncodableValue(flutter::EncodableMap{
            {"sessionId", flutter::EncodableValue(sessionId)},
            {"videoFrameNumber", flutter::EncodableValue(videoFrameNumber)},
            {"videoFps", flutter::EncodableValue(videoFps)},
            {"videoQuality", flutter::EncodableValue(videoQuality)},
            {"size", flutter::EncodableValue(size)},
            {"time", flutter::EncodableValue(time)},
            {"bitrate", flutter::EncodableValue(bitrate)},
            {"speed", flutter::EncodableValue(speed)}
        });
    }
};

class FFmpegSession {
public:
    FFmpegSession(int64_t sessionId, const std::vector<std::string>& arguments, SessionType type);
    ~FFmpegSession();

    // Basic accessors
    int64_t GetSessionId() const { return sessionId_; }
    SessionState GetState() const { return state_; }
    int GetReturnCode() const { return returnCode_; }
    SessionType GetType() const { return type_; }
    std::string GetCommand() const { return command_; }
    const std::vector<std::string>& GetArguments() const { return arguments_; }
    int64_t GetCreateTime() const { return createTime_; }
    int64_t GetStartTime() const { return startTime_; }
    int64_t GetEndTime() const { return endTime_; }

    // Session data as Flutter map
    flutter::EncodableValue ToEncodableValue() const;

    // Log management
    void AddLog(LogLevel level, const std::string& message);
    std::vector<LogEntry> GetLogs() const;
    flutter::EncodableValue GetLogsAsEncodableList() const;

    // Statistics management (FFmpeg only)
    void AddStatistics(const StatisticsEntry& stats);
    std::vector<StatisticsEntry> GetStatistics() const;
    flutter::EncodableValue GetStatisticsAsEncodableList() const;

    // Execution control
    void SetState(SessionState state);
    void SetReturnCode(int code);
    void SetStartTime();
    void SetEndTime();
    void Cancel();
    bool IsCancelled() const { return cancelled_; }

    // Async execution support
    void SetProcessHandle(HANDLE processHandle, HANDLE threadHandle);
    HANDLE GetProcessHandle() const { return processHandle_; }

private:
    int64_t sessionId_;
    std::vector<std::string> arguments_;
    std::string command_;
    SessionType type_;
    std::atomic<SessionState> state_;
    std::atomic<int> returnCode_;
    int64_t createTime_;
    int64_t startTime_;
    int64_t endTime_;
    std::atomic<bool> cancelled_;
    
    mutable std::mutex logsMutex_;
    std::vector<LogEntry> logs_;
    
    mutable std::mutex statsMutex_;
    std::vector<StatisticsEntry> statistics_;
    
    // Process management
    HANDLE processHandle_;
    HANDLE threadHandle_;
    mutable std::mutex processMutex_;

    int64_t GetCurrentTimeMillis() const;
    std::string ArgumentsToCommand(const std::vector<std::string>& args) const;
};

class SessionManager {
public:
    static SessionManager& GetInstance();
    
    // Session lifecycle
    std::shared_ptr<FFmpegSession> CreateSession(const std::vector<std::string>& arguments, SessionType type);
    std::shared_ptr<FFmpegSession> GetSession(int64_t sessionId);
    void RemoveSession(int64_t sessionId);
    
    // Session execution
    int ExecuteSession(int64_t sessionId);
    void ExecuteSessionAsync(int64_t sessionId);
    
    // Cleanup
    void CancelAllSessions();
    void CleanupCompletedSessions();

private:
    SessionManager() = default;
    ~SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    mutable std::mutex sessionsMutex_;
    std::map<int64_t, std::shared_ptr<FFmpegSession>> sessions_;
    std::atomic<int64_t> nextSessionId_{1};

    int64_t GenerateSessionId();
    int ExecuteFFmpegCommand(std::shared_ptr<FFmpegSession> session);
    std::string FindFFmpegExecutable();
};

} // namespace ffmpeg_kit_flutter

#endif // FFMPEG_SESSION_MANAGER_H_