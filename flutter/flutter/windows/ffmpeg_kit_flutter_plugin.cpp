#include "ffmpeg_kit_flutter_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <sstream>

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
  
  // Handle initialization methods that are causing the MissingPluginException
  if (method_name == "getLogLevel") {
    // Return default log level (INFO = 20)
    result->Success(flutter::EncodableValue(20));
  }
  else if (method_name == "setLogLevel") {
    // Accept the log level but don't do anything yet
    result->Success(flutter::EncodableValue());
  }
  else if (method_name == "getPlatform") {
    result->Success(flutter::EncodableValue("windows"));
  }
  else if (method_name == "getArch") {
    result->Success(flutter::EncodableValue("x86_64"));
  }
  else if (method_name == "enableRedirection") {
    // Accept but don't implement yet
    result->Success(flutter::EncodableValue());
  }
  else if (method_name == "disableRedirection") {
    // Accept but don't implement yet
    result->Success(flutter::EncodableValue());
  }
  else if (method_name == "enableStatistics") {
    // Accept but don't implement yet
    result->Success(flutter::EncodableValue());
  }
  else if (method_name == "disableStatistics") {
    // Accept but don't implement yet
    result->Success(flutter::EncodableValue());
  }
  // FFmpeg execution methods - return not implemented for now
  else if (method_name == "ffmpegSession" || 
           method_name == "ffmpegSessionExecute" ||
           method_name == "ffprobeSession" ||
           method_name == "ffprobeSessionExecute" ||
           method_name == "mediaInformationSession") {
    result->NotImplemented();
  }
  // Session management methods - return defaults
  else if (method_name == "abstractSessionGetState") {
    // Return default state (1 = CREATED)
    result->Success(flutter::EncodableValue(1));
  }
  else if (method_name == "abstractSessionGetReturnCode") {
    // Return default return code (0 = success)
    result->Success(flutter::EncodableValue(0));
  }
  else if (method_name == "abstractSessionGetLogs") {
    // Return empty logs array
    result->Success(flutter::EncodableValue(flutter::EncodableList()));
  }
  else if (method_name == "abstractSessionGetFailStackTrace") {
    // Return empty stack trace
    result->Success(flutter::EncodableValue(""));
  }
  else {
    result->NotImplemented();
  }
}

}  // namespace ffmpeg_kit_flutter

void FFmpegKitFlutterPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  ffmpeg_kit_flutter::FFmpegKitFlutterPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarWindows::GetFromRegistrar(registrar));
}