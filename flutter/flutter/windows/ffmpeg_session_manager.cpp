#include "ffmpeg_session_manager.h"
#include <sstream>
#include <thread>
#include <iostream>
#include <fstream>
#include <regex>

namespace ffmpeg_kit_flutter {

// FFmpegSession Implementation
FFmpegSession::FFmpegSession(int64_t sessionId, const std::vector<std::string>& arguments, SessionType type)
    : sessionId_(sessionId)
    , arguments_(arguments)
    , command_(ArgumentsToCommand(arguments))
    , type_(type)
    , state_(SessionState::CREATED)
    , returnCode_(-1)
    , createTime_(GetCurrentTimeMillis())
    , startTime_(0)
    , endTime_(0)
    , cancelled_(false)
    , processHandle_(nullptr)
{
}

FFmpegSession::~FFmpegSession() {
    if (processHandle_ && processHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(processHandle_);
    }
}

flutter::EncodableValue FFmpegSession::ToEncodableValue() const {
    return flutter::EncodableValue(flutter::EncodableMap{
        {"sessionId", flutter::EncodableValue(sessionId_)},
        {"createTime", flutter::EncodableValue(createTime_)},
        {"startTime", flutter::EncodableValue(startTime_)},
        {"command", flutter::EncodableValue(command_)},
        {"type", flutter::EncodableValue(static_cast<int>(type_))}
    });
}

void FFmpegSession::AddLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logsMutex_);
    LogEntry entry;
    entry.sessionId = sessionId_;
    entry.level = level;
    entry.message = message;
    entry.timestamp = GetCurrentTimeMillis();
    logs_.push_back(entry);
}

std::vector<LogEntry> FFmpegSession::GetLogs() const {
    std::lock_guard<std::mutex> lock(logsMutex_);
    return logs_;
}

flutter::EncodableValue FFmpegSession::GetLogsAsEncodableList() const {
    std::lock_guard<std::mutex> lock(logsMutex_);
    flutter::EncodableList logsList;
    for (const auto& log : logs_) {
        logsList.push_back(log.ToEncodableValue());
    }
    return flutter::EncodableValue(logsList);
}

void FFmpegSession::AddStatistics(const StatisticsEntry& stats) {
    if (type_ != SessionType::FFMPEG) return; // Only FFmpeg sessions have statistics
    
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_.push_back(stats);
}

std::vector<StatisticsEntry> FFmpegSession::GetStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return statistics_;
}

flutter::EncodableValue FFmpegSession::GetStatisticsAsEncodableList() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    flutter::EncodableList statsList;
    for (const auto& stat : statistics_) {
        statsList.push_back(stat.ToEncodableValue());
    }
    return flutter::EncodableValue(statsList);
}

void FFmpegSession::SetState(SessionState state) {
    state_ = state;
}

void FFmpegSession::SetReturnCode(int code) {
    returnCode_ = code;
}

void FFmpegSession::SetStartTime() {
    startTime_ = GetCurrentTimeMillis();
}

void FFmpegSession::SetEndTime() {
    endTime_ = GetCurrentTimeMillis();
}

void FFmpegSession::Cancel() {
    cancelled_ = true;
    if (processHandle_ && processHandle_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(processHandle_, 1);
    }
}

int64_t FFmpegSession::GetCurrentTimeMillis() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string FFmpegSession::ArgumentsToCommand(const std::vector<std::string>& args) const {
    std::stringstream ss;
    ss << "ffmpeg";
    for (const auto& arg : args) {
        ss << " ";
        if (arg.find(' ') != std::string::npos) {
            ss << "\"" << arg << "\"";
        } else {
            ss << arg;
        }
    }
    return ss.str();
}

// SessionManager Implementation
SessionManager& SessionManager::GetInstance() {
    static SessionManager instance;
    return instance;
}

std::shared_ptr<FFmpegSession> SessionManager::CreateSession(const std::vector<std::string>& arguments, SessionType type) {
    int64_t sessionId = GenerateSessionId();
    auto session = std::make_shared<FFmpegSession>(sessionId, arguments, type);
    
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        sessions_[sessionId] = session;
    }
    
    session->AddLog(LogLevel::INFO, "Session created with id " + std::to_string(sessionId));
    return session;
}

std::shared_ptr<FFmpegSession> SessionManager::GetSession(int64_t sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(sessionId);
    return (it != sessions_.end()) ? it->second : nullptr;
}

void SessionManager::RemoveSession(int64_t sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_.erase(sessionId);
}

int SessionManager::ExecuteSession(int64_t sessionId) {
    auto session = GetSession(sessionId);
    if (!session) {
        return -1; // Session not found
    }
    
    return ExecuteFFmpegCommand(session);
}

void SessionManager::ExecuteSessionAsync(int64_t sessionId) {
    auto session = GetSession(sessionId);
    if (!session) {
        return;
    }
    
    // Execute in a separate thread
    std::thread([this, session]() {
        ExecuteFFmpegCommand(session);
    }).detach();
}

void SessionManager::CancelAllSessions() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (auto& pair : sessions_) {
        pair.second->Cancel();
    }
}

void SessionManager::CleanupCompletedSessions() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        auto state = it->second->GetState();
        if (state == SessionState::COMPLETED || state == SessionState::FAILED) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

int64_t SessionManager::GenerateSessionId() {
    return nextSessionId_++;
}

int SessionManager::ExecuteFFmpegCommand(std::shared_ptr<FFmpegSession> session) {
    session->SetState(SessionState::RUNNING);
    session->SetStartTime();
    session->AddLog(LogLevel::INFO, "Starting FFmpeg execution");
    
    // Build command line
    std::string ffmpegPath = FindFFmpegExecutable();
    if (ffmpegPath.empty()) {
        session->AddLog(LogLevel::ERROR, "FFmpeg executable not found");
        session->SetState(SessionState::FAILED);
        session->SetReturnCode(-1);
        session->SetEndTime();
        return -1;
    }
    
    // Prepare command line arguments
    std::stringstream cmdLine;
    cmdLine << "\"" << ffmpegPath << "\"";
    
    // Add session arguments
    for (const auto& arg : session->GetArguments()) {
        cmdLine << " ";
        if (arg.find(' ') != std::string::npos) {
            cmdLine << "\"" << arg << "\"";
        } else {
            cmdLine << arg;
        }
    }
    
    session->AddLog(LogLevel::DEBUG, "Command: " + cmdLine.str());
    
    // Create process
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    
    // Create pipes for stdout/stderr capture
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) ||
        !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        session->AddLog(LogLevel::ERROR, "Failed to create pipes for process output");
        session->SetState(SessionState::FAILED);
        session->SetReturnCode(-1);
        session->SetEndTime();
        return -1;
    }
    
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    
    // Convert command to mutable string for CreateProcessA
    std::string cmdStr = cmdLine.str();
    std::vector<char> cmdBuffer(cmdStr.begin(), cmdStr.end());
    cmdBuffer.push_back('\0');
    
    BOOL success = CreateProcessA(
        NULL,                       // lpApplicationName
        cmdBuffer.data(),           // lpCommandLine
        NULL,                       // lpProcessAttributes
        NULL,                       // lpThreadAttributes
        TRUE,                       // bInheritHandles
        CREATE_NO_WINDOW,           // dwCreationFlags
        NULL,                       // lpEnvironment
        NULL,                       // lpCurrentDirectory
        &si,                        // lpStartupInfo
        &pi                         // lpProcessInformation
    );
    
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    
    if (!success) {
        DWORD error = GetLastError();
        session->AddLog(LogLevel::ERROR, "Failed to create FFmpeg process. Error: " + std::to_string(error));
        session->SetState(SessionState::FAILED);
        session->SetReturnCode(-1);
        session->SetEndTime();
        
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        return -1;
    }
    
    session->SetProcessHandle(pi.hProcess);
    
    // Read output in separate threads
    std::thread stdoutThread([session, hStdoutRead]() {
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string output(buffer);
            session->AddLog(LogLevel::INFO, output);
        }
        CloseHandle(hStdoutRead);
    });
    
    std::thread stderrThread([session, hStderrRead]() {
        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hStderrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string output(buffer);
            session->AddLog(LogLevel::WARNING, output);
        }
        CloseHandle(hStderrRead);
    });
    
    // Wait for process completion
    DWORD waitResult = WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    // Wait for output threads to complete
    stdoutThread.join();
    stderrThread.join();
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    // Update session state
    session->SetReturnCode(static_cast<int>(exitCode));
    session->SetEndTime();
    
    if (session->IsCancelled()) {
        session->AddLog(LogLevel::INFO, "Session was cancelled");
        session->SetState(SessionState::FAILED);
    } else if (exitCode == 0) {
        session->AddLog(LogLevel::INFO, "Session completed successfully");
        session->SetState(SessionState::COMPLETED);
    } else {
        session->AddLog(LogLevel::ERROR, "Session failed with exit code: " + std::to_string(exitCode));
        session->SetState(SessionState::FAILED);
    }
    
    return static_cast<int>(exitCode);
}

std::string SessionManager::FindFFmpegExecutable() {
    // First, try to find ffmpeg.exe in the same directory as the DLLs
    // This would be bundled with the Flutter app
    
    // Get the current module path
    char modulePath[MAX_PATH];
    HMODULE hModule = GetModuleHandleA("ffmpeg_kit_flutter_plugin.dll");
    if (hModule && GetModuleFileNameA(hModule, modulePath, MAX_PATH)) {
        std::string moduleDir(modulePath);
        size_t lastSlash = moduleDir.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            moduleDir = moduleDir.substr(0, lastSlash + 1);
            std::string ffmpegPath = moduleDir + "ffmpeg.exe";
            
            // Check if file exists
            DWORD attributes = GetFileAttributesA(ffmpegPath.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                return ffmpegPath;
            }
        }
    }
    
    // Fallback: try to find ffmpeg in system PATH
    return "ffmpeg"; // Let CreateProcess search in PATH
}

} // namespace ffmpeg_kit_flutter