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
    , threadHandle_(nullptr)
{
}

FFmpegSession::~FFmpegSession() {
    std::lock_guard<std::mutex> lock(processMutex_);
    
    if (processHandle_ && processHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(processHandle_);
        processHandle_ = nullptr;
    }
    
    if (threadHandle_ && threadHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(threadHandle_);
        threadHandle_ = nullptr;
    }
}

flutter::EncodableValue FFmpegSession::ToEncodableValue() const {
    flutter::EncodableMap sessionMap{
        {"sessionId", flutter::EncodableValue(sessionId_)},
        {"createTime", flutter::EncodableValue(createTime_)},
        {"command", flutter::EncodableValue(command_)},
        {"type", flutter::EncodableValue(static_cast<int>(type_))}
    };
    
    // Handle null values for startTime and endTime when not set
    if (startTime_ > 0) {
        sessionMap["startTime"] = flutter::EncodableValue(startTime_);
    } else {
        sessionMap["startTime"] = flutter::EncodableValue(); // null
    }
    
    if (endTime_ > 0) {
        sessionMap["endTime"] = flutter::EncodableValue(endTime_);
    } else {
        sessionMap["endTime"] = flutter::EncodableValue(); // null
    }
    
    return flutter::EncodableValue(sessionMap);
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

void FFmpegSession::SetProcessHandle(HANDLE processHandle, HANDLE threadHandle) {
    std::lock_guard<std::mutex> lock(processMutex_);
    processHandle_ = processHandle;
    threadHandle_ = threadHandle;
}

void FFmpegSession::Cancel() {
    cancelled_ = true;
    
    std::lock_guard<std::mutex> lock(processMutex_);
    if (processHandle_ && processHandle_ != INVALID_HANDLE_VALUE) {
        // First try to terminate gracefully
        TerminateProcess(processHandle_, 1);
        
        // Wait up to 5 seconds for process to terminate
        DWORD waitResult = WaitForSingleObject(processHandle_, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            // Force kill if still running
            TerminateProcess(processHandle_, 9);
            WaitForSingleObject(processHandle_, 1000);
        }
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
    
    // Execute in a separate thread with proper resource management
    // Use a shared_ptr to ensure the session stays alive during execution
    std::thread([this, session]() {
        try {
            ExecuteFFmpegCommand(session);
        } catch (const std::exception& e) {
            session->AddLog(LogLevel::ERROR_LEVEL, "Exception during async execution: " + std::string(e.what()));
            session->SetState(SessionState::FAILED);
            session->SetReturnCode(-1);
            session->SetEndTime();
        } catch (...) {
            session->AddLog(LogLevel::ERROR_LEVEL, "Unknown exception during async execution");
            session->SetState(SessionState::FAILED);
            session->SetReturnCode(-1);
            session->SetEndTime();
        }
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
        session->AddLog(LogLevel::ERROR_LEVEL, "FFmpeg executable not found");
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
    
    // Create pipes for stdout/stderr capture with proper cleanup
    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    HANDLE hStderrRead = nullptr, hStderrWrite = nullptr;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    // Create stdout pipe
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        session->AddLog(LogLevel::ERROR_LEVEL, "Failed to create stdout pipe");
        session->SetState(SessionState::FAILED);
        session->SetReturnCode(-1);
        session->SetEndTime();
        return -1;
    }
    
    // Create stderr pipe
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
        session->AddLog(LogLevel::ERROR_LEVEL, "Failed to create stderr pipe");
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        session->SetState(SessionState::FAILED);
        session->SetReturnCode(-1);
        session->SetEndTime();
        return -1;
    }
    
    // Make sure the read ends of the pipes are not inherited
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    
    // Create process
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    
    // Convert command to mutable string for CreateProcessA
    std::string cmdStr = cmdLine.str();
    std::vector<char> cmdBuffer(cmdStr.begin(), cmdStr.end());
    cmdBuffer.push_back('\0');
    
    BOOL success = CreateProcessA(
        nullptr,                    // lpApplicationName
        cmdBuffer.data(),           // lpCommandLine
        nullptr,                    // lpProcessAttributes
        nullptr,                    // lpThreadAttributes
        TRUE,                       // bInheritHandles
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, // dwCreationFlags
        nullptr,                    // lpEnvironment
        nullptr,                    // lpCurrentDirectory
        &si,                        // lpStartupInfo
        &pi                         // lpProcessInformation
    );
    
    // Close write ends of pipes (child process owns them now)
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    
    if (!success) {
        DWORD error = GetLastError();
        session->AddLog(LogLevel::ERROR_LEVEL, "Failed to create FFmpeg process. Error: " + std::to_string(error));
        session->SetState(SessionState::FAILED);
        session->SetReturnCode(-1);
        session->SetEndTime();
        
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        return -1;
    }
    
    // Store process handles
    session->SetProcessHandle(pi.hProcess, pi.hThread);
    
    // Create shared state for output reading threads
    std::atomic<bool> outputComplete{false};
    std::mutex outputMutex;
    
    // Read output in separate threads with timeout handling
    std::thread stdoutThread([session, hStdoutRead, &outputComplete, &outputMutex]() {
        char buffer[4096];
        DWORD bytesRead;
        while (!outputComplete.load() && ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string output(buffer);
                // Remove newlines and empty messages
                if (!output.empty() && output != "\n" && output != "\r\n") {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    session->AddLog(LogLevel::INFO, output);
                }
            } else {
                break;
            }
        }
        CloseHandle(hStdoutRead);
    });
    
    std::thread stderrThread([session, hStderrRead, &outputComplete, &outputMutex]() {
        char buffer[4096];
        DWORD bytesRead;
        while (!outputComplete.load() && ReadFile(hStderrRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string output(buffer);
                // Remove newlines and empty messages
                if (!output.empty() && output != "\n" && output != "\r\n") {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    session->AddLog(LogLevel::WARNING, output);
                }
            } else {
                break;
            }
        }
        CloseHandle(hStderrRead);
    });
    
    // Wait for process completion
    DWORD waitResult = WaitForSingleObject(pi.hProcess, INFINITE);
    
    // Signal output threads to stop and wait for them
    outputComplete.store(true);
    
    // Give threads a chance to finish reading
    if (stdoutThread.joinable()) {
        stdoutThread.join();
    }
    if (stderrThread.joinable()) {
        stderrThread.join();
    }
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
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
        session->AddLog(LogLevel::ERROR_LEVEL, "Session failed with exit code: " + std::to_string(exitCode));
        session->SetState(SessionState::FAILED);
    }
    
    return static_cast<int>(exitCode);
}

std::string SessionManager::FindFFmpegExecutable() {
    // Method 1: Try to find ffmpeg.exe in the same directory as the current executable
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
        std::string exeDir(exePath);
        size_t lastSlash = exeDir.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            exeDir = exeDir.substr(0, lastSlash + 1);
            std::string ffmpegPath = exeDir + "ffmpeg.exe";
            
            // Check if file exists
            DWORD attributes = GetFileAttributesA(ffmpegPath.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                return ffmpegPath;
            }
        }
    }
    
    // Method 2: Try to find ffmpeg.exe in the plugin DLL directory
    char modulePath[MAX_PATH];
    HMODULE hModule = GetModuleHandleA(nullptr); // Get main executable handle first
    if (hModule) {
        // Try to get plugin DLL handle by enumerating modules
        HMODULE hPluginModule = GetModuleHandleA("ffmpeg_kit_flutter_plugin.dll");
        if (!hPluginModule) {
            // Try alternative names
            hPluginModule = GetModuleHandleA("libffmpeg_kit_flutter_plugin.dll");
        }
        
        if (hPluginModule && GetModuleFileNameA(hPluginModule, modulePath, MAX_PATH)) {
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
    }
    
    // Method 3: Check if ffmpeg is available in system PATH
    char pathBuffer[MAX_PATH];
    DWORD result = SearchPathA(nullptr, "ffmpeg", ".exe", MAX_PATH, pathBuffer, nullptr);
    if (result > 0 && result < MAX_PATH) {
        return std::string(pathBuffer);
    }
    
    // Fallback: return just "ffmpeg" and let CreateProcess search in PATH
    return "ffmpeg";
}

} // namespace ffmpeg_kit_flutter