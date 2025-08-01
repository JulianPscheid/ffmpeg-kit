#include "ffmpeg_kit_flutter_plugin.h"
#include "ffmpeg_session_manager.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/encodable_value.h>

#include <memory>
#include <sstream>
#include <algorithm>

namespace ffmpeg_kit_flutter {

// static
void FFmpegKitFlutterPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "flutter.arthenica.com/ffmpeg_kit",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<FFmpegKitFlutterPlugin>();

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

FFmpegKitFlutterPlugin::FFmpegKitFlutterPlugin() {}

FFmpegKitFlutterPlugin::~FFmpegKitFlutterPlugin() {}

void FFmpegKitFlutterPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  
  const std::string method_name = method_call.method_name();
  const auto* arguments = std::get_if<flutter::EncodableMap>(method_call.arguments());
  
  try {
    // Platform and Configuration Methods
    if (method_name == "getPlatform") {
      result->Success(flutter::EncodableValue("windows"));
    }
    else if (method_name == "getArch") {
      result->Success(flutter::EncodableValue("x86_64"));
    }
    else if (method_name == "getLogLevel") {
      result->Success(flutter::EncodableValue(static_cast<int>(ffmpeg_kit_flutter::LogLevel::INFO)));
    }
    else if (method_name == "setLogLevel") {
      result->Success(flutter::EncodableValue());
    }
    else if (method_name == "enableRedirection" || method_name == "disableRedirection") {
      result->Success(flutter::EncodableValue());
    }
    else if (method_name == "enableStatistics" || method_name == "disableStatistics") {
      result->Success(flutter::EncodableValue());
    }
    
    // Session Creation Methods
    else if (method_name == "ffmpegSession") {
      HandleFFmpegSession(arguments, std::move(result));
    }
    else if (method_name == "ffprobeSession") {
      HandleFFprobeSession(arguments, std::move(result));
    }
    else if (method_name == "mediaInformationSession") {
      HandleMediaInformationSession(arguments, std::move(result));
    }
    
    // Session Execution Methods
    else if (method_name == "ffmpegSessionExecute") {
      HandleSessionExecute(arguments, std::move(result));
    }
    else if (method_name == "asyncFFmpegSessionExecute") {
      HandleAsyncSessionExecute(arguments, std::move(result));
    }
    
    // Session State Methods
    else if (method_name == "abstractSessionGetState") {
      HandleGetSessionState(arguments, std::move(result));
    }
    else if (method_name == "abstractSessionGetReturnCode") {
      HandleGetReturnCode(arguments, std::move(result));
    }
    else if (method_name == "abstractSessionGetLogs") {
      HandleGetLogs(arguments, std::move(result));
    }
    else if (method_name == "abstractSessionGetAllLogs") {
      HandleGetAllLogs(arguments, std::move(result));
    }
    else if (method_name == "abstractSessionGetFailStackTrace") {
      HandleGetFailStackTrace(arguments, std::move(result));
    }
    
    // Statistics Methods (FFmpeg only)
    else if (method_name == "ffmpegSessionGetStatistics") {
      HandleGetStatistics(arguments, std::move(result));
    }
    
    // Session Control Methods
    else if (method_name == "cancel" || method_name == "cancelExecution" || method_name == "cancelSession") {
      HandleCancelSession(arguments, std::move(result));
    }
    
    else {
      result->NotImplemented();
    }
  }
  catch (const std::exception& e) {
    result->Error("PLUGIN_ERROR", "Error handling method call: " + std::string(e.what()), flutter::EncodableValue());
  }
}

// Session Creation Handlers
void FFmpegKitFlutterPlugin::HandleFFmpegSession(const flutter::EncodableMap* arguments,
                                                std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  auto args = ExtractArgumentsFromMap(arguments);
  auto session = SessionManager::GetInstance().CreateSession(args, ffmpeg_kit_flutter::SessionType::FFMPEG);
  result->Success(session->ToEncodableValue());
}

void FFmpegKitFlutterPlugin::HandleFFprobeSession(const flutter::EncodableMap* arguments,
                                                 std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  auto args = ExtractArgumentsFromMap(arguments);
  auto session = SessionManager::GetInstance().CreateSession(args, ffmpeg_kit_flutter::SessionType::FFPROBE);
  result->Success(session->ToEncodableValue());
}

void FFmpegKitFlutterPlugin::HandleMediaInformationSession(const flutter::EncodableMap* arguments,
                                                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  auto args = ExtractArgumentsFromMap(arguments);
  auto session = SessionManager::GetInstance().CreateSession(args, ffmpeg_kit_flutter::SessionType::MEDIA_INFORMATION);
  result->Success(session->ToEncodableValue());
}

// Session Execution Handlers
void FFmpegKitFlutterPlugin::HandleSessionExecute(const flutter::EncodableMap* arguments,
                                                 std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  int64_t sessionId = ExtractSessionIdFromMap(arguments);
  if (sessionId <= 0) {
    result->Error("INVALID_SESSION_ID", "Invalid session ID", flutter::EncodableValue());
    return;
  }

  int returnCode = SessionManager::GetInstance().ExecuteSession(sessionId);
  result->Success(flutter::EncodableValue(returnCode));
}

void FFmpegKitFlutterPlugin::HandleAsyncSessionExecute(const flutter::EncodableMap* arguments,
                                                      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  int64_t sessionId = ExtractSessionIdFromMap(arguments);
  if (sessionId <= 0) {
    result->Error("INVALID_SESSION_ID", "Invalid session ID", flutter::EncodableValue());
    return;
  }

  SessionManager::GetInstance().ExecuteSessionAsync(sessionId);
  result->Success(flutter::EncodableValue());
}

// Session State Handlers
void FFmpegKitFlutterPlugin::HandleGetSessionState(const flutter::EncodableMap* arguments,
                                                  std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  int64_t sessionId = ExtractSessionIdFromMap(arguments);
  auto session = SessionManager::GetInstance().GetSession(sessionId);
  
  if (!session) {
    result->Error("SESSION_NOT_FOUND", "Session not found", flutter::EncodableValue());
    return;
  }

  result->Success(flutter::EncodableValue(static_cast<int>(session->GetState())));
}

void FFmpegKitFlutterPlugin::HandleGetReturnCode(const flutter::EncodableMap* arguments,
                                                std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  int64_t sessionId = ExtractSessionIdFromMap(arguments);
  auto session = SessionManager::GetInstance().GetSession(sessionId);
  
  if (!session) {
    result->Error("SESSION_NOT_FOUND", "Session not found", flutter::EncodableValue());
    return;
  }

  int returnCode = session->GetReturnCode();
  if (returnCode == -1) {
    result->Success(flutter::EncodableValue()); // null
  } else {
    result->Success(flutter::EncodableValue(returnCode));
  }
}

void FFmpegKitFlutterPlugin::HandleGetLogs(const flutter::EncodableMap* arguments,
                                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  int64_t sessionId = ExtractSessionIdFromMap(arguments);
  auto session = SessionManager::GetInstance().GetSession(sessionId);
  
  if (!session) {
    result->Error("SESSION_NOT_FOUND", "Session not found", flutter::EncodableValue());
    return;
  }

  result->Success(session->GetLogsAsEncodableList());
}

void FFmpegKitFlutterPlugin::HandleGetAllLogs(const flutter::EncodableMap* arguments,
                                             std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  // For now, same as GetLogs - could implement timeout logic later
  HandleGetLogs(arguments, std::move(result));
}

void FFmpegKitFlutterPlugin::HandleGetFailStackTrace(const flutter::EncodableMap* arguments,
                                                    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  int64_t sessionId = ExtractSessionIdFromMap(arguments);
  auto session = SessionManager::GetInstance().GetSession(sessionId);
  
  if (!session) {
    result->Error("SESSION_NOT_FOUND", "Session not found", flutter::EncodableValue());
    return;
  }

  // Return empty string for now - could implement actual stack trace collection
  result->Success(flutter::EncodableValue(""));
}

// Statistics Handler
void FFmpegKitFlutterPlugin::HandleGetStatistics(const flutter::EncodableMap* arguments,
                                                std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    result->Error("INVALID_ARGUMENTS", "Arguments cannot be null", flutter::EncodableValue());
    return;
  }

  int64_t sessionId = ExtractSessionIdFromMap(arguments);
  auto session = SessionManager::GetInstance().GetSession(sessionId);
  
  if (!session) {
    result->Error("SESSION_NOT_FOUND", "Session not found", flutter::EncodableValue());
    return;
  }

  if (session->GetType() != ffmpeg_kit_flutter::SessionType::FFMPEG) {
    result->Success(flutter::EncodableValue(flutter::EncodableList())); // Empty list for non-FFmpeg sessions
    return;
  }

  result->Success(session->GetStatisticsAsEncodableList());
}

// Session Control Handler
void FFmpegKitFlutterPlugin::HandleCancelSession(const flutter::EncodableMap* arguments,
                                                std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!arguments) {
    // Cancel all sessions if no specific session ID
    SessionManager::GetInstance().CancelAllSessions();
    result->Success(flutter::EncodableValue());
    return;
  }

  int64_t sessionId = ExtractSessionIdFromMap(arguments);
  auto session = SessionManager::GetInstance().GetSession(sessionId);
  
  if (!session) {
    result->Error("SESSION_NOT_FOUND", "Session not found", flutter::EncodableValue());
    return;
  }

  session->Cancel();
  result->Success(flutter::EncodableValue());
}

// Helper Methods
std::vector<std::string> FFmpegKitFlutterPlugin::ExtractArgumentsFromMap(const flutter::EncodableMap* arguments) {
  std::vector<std::string> result;
  
  if (!arguments) return result;

  auto argumentsIt = arguments->find(flutter::EncodableValue("arguments"));
  if (argumentsIt != arguments->end()) {
    const auto* argsList = std::get_if<flutter::EncodableList>(&argumentsIt->second);
    if (argsList) {
      for (const auto& arg : *argsList) {
        const auto* argStr = std::get_if<std::string>(&arg);
        if (argStr) {
          // Basic input validation
          if (argStr->length() > 32768) { // Max argument length
            continue; // Skip overly long arguments
          }
          
          // Check for potentially dangerous characters/patterns
          std::string safeArg = *argStr;
          
          // Remove null bytes
          safeArg.erase(std::remove(safeArg.begin(), safeArg.end(), '\0'), safeArg.end());
          
          // Skip empty arguments
          if (safeArg.empty()) {
            continue;
          }
          
          result.push_back(safeArg);
        }
      }
    }
  }
  
  // Limit total number of arguments
  if (result.size() > 1000) {
    result.resize(1000);
  }
  
  return result;
}

int64_t FFmpegKitFlutterPlugin::ExtractSessionIdFromMap(const flutter::EncodableMap* arguments) {
  if (!arguments) return 0;

  auto sessionIdIt = arguments->find(flutter::EncodableValue("sessionId"));
  if (sessionIdIt != arguments->end()) {
    const auto* sessionIdPtr = std::get_if<int64_t>(&sessionIdIt->second);
    if (sessionIdPtr) {
      return *sessionIdPtr;
    }
    // Try as int32 in case it comes as regular int
    const auto* sessionIdIntPtr = std::get_if<int>(&sessionIdIt->second);
    if (sessionIdIntPtr) {
      return static_cast<int64_t>(*sessionIdIntPtr);
    }
  }
  
  return 0;
}

}  // namespace ffmpeg_kit_flutter

extern "C" {

void FFmpegKitFlutterPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  auto registrar_windows = 
      reinterpret_cast<flutter::PluginRegistrarWindows*>(registrar);
  ffmpeg_kit_flutter::FFmpegKitFlutterPlugin::RegisterWithRegistrar(registrar_windows);
}

void FFmpegKitFlutterPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  auto registrar_windows = 
      reinterpret_cast<flutter::PluginRegistrarWindows*>(registrar);
  ffmpeg_kit_flutter::FFmpegKitFlutterPlugin::RegisterWithRegistrar(registrar_windows);
}

}  // extern "C"