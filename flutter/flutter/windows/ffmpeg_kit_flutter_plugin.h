#ifndef FLUTTER_PLUGIN_FFMPEG_KIT_FLUTTER_PLUGIN_H_
#define FLUTTER_PLUGIN_FFMPEG_KIT_FLUTTER_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/encodable_value.h>

#include <memory>

namespace ffmpeg_kit_flutter {

class FFmpegKitFlutterPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  FFmpegKitFlutterPlugin();

  virtual ~FFmpegKitFlutterPlugin();

  // Disallow copy and assign.
  FFmpegKitFlutterPlugin(const FFmpegKitFlutterPlugin&) = delete;
  FFmpegKitFlutterPlugin& operator=(const FFmpegKitFlutterPlugin&) = delete;

 private:
  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  // Session creation handlers
  void HandleFFmpegSession(const flutter::EncodableMap* arguments,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void HandleFFprobeSession(const flutter::EncodableMap* arguments,
                           std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void HandleMediaInformationSession(const flutter::EncodableMap* arguments,
                                    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  // Session execution handlers
  void HandleSessionExecute(const flutter::EncodableMap* arguments,
                           std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void HandleAsyncSessionExecute(const flutter::EncodableMap* arguments,
                                std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  // Session state handlers
  void HandleGetSessionState(const flutter::EncodableMap* arguments,
                            std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void HandleGetReturnCode(const flutter::EncodableMap* arguments,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void HandleGetLogs(const flutter::EncodableMap* arguments,
                    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void HandleGetAllLogs(const flutter::EncodableMap* arguments,
                       std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void HandleGetFailStackTrace(const flutter::EncodableMap* arguments,
                              std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  // Statistics handler
  void HandleGetStatistics(const flutter::EncodableMap* arguments,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  // Session control handlers
  void HandleCancelSession(const flutter::EncodableMap* arguments,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  // Helper methods
  std::vector<std::string> ExtractArgumentsFromMap(const flutter::EncodableMap* arguments);
  int64_t ExtractSessionIdFromMap(const flutter::EncodableMap* arguments);
};

}  // namespace ffmpeg_kit_flutter

#endif  // FLUTTER_PLUGIN_FFMPEG_KIT_FLUTTER_PLUGIN_H_