# FFmpeg-Kit Windows Support

This fork adds comprehensive Windows platform support to the FFmpeg-Kit Flutter plugin.

## Features Implemented

### ✅ Complete Windows Platform Integration
- **Method Channel**: Full Windows implementation matching Android/iOS API
- **Session Management**: Complete session lifecycle with thread-safe operations
- **DLL Bundling**: Automatic FFmpeg DLL bundling with Flutter Windows builds
- **Async Execution**: Non-blocking FFmpeg operations with proper threading

### ✅ Supported FFmpeg-Kit Methods
- `ffmpegSession` - Create FFmpeg sessions
- `ffprobeSession` - Create FFprobe sessions  
- `mediaInformationSession` - Create media info sessions
- `ffmpegSessionExecute` - Execute sessions synchronously
- `asyncFFmpegSessionExecute` - Execute sessions asynchronously
- `abstractSessionGetState` - Get session state (CREATED, RUNNING, COMPLETED, FAILED)
- `abstractSessionGetReturnCode` - Get FFmpeg exit code
- `abstractSessionGetLogs` - Get session logs
- `ffmpegSessionGetStatistics` - Get FFmpeg statistics
- `cancel` / `cancelSession` - Cancel running sessions
- Platform info: `getPlatform`, `getArch`, `getLogLevel`

### ✅ Bundled FFmpeg Libraries (GPL Build)
- `avcodec-62.dll` (97.7MB) - Core codec library
- `avformat-62.dll` - Format handling
- `avutil-60.dll` - Utility functions
- `swresample-6.dll` - Audio resampling
- `swscale-9.dll` - Video scaling
- `avdevice-62.dll` - Device handling
- `avfilter-11.dll` - Filtering capabilities

## Usage

### 1. Update Your pubspec.yaml

```yaml
dependencies:
  ffmpeg_kit_flutter:
    git:
      url: https://github.com/JulianPscheid/ffmpeg-kit.git
      path: flutter/flutter
```

### 2. Install FFmpeg (Required)

The plugin requires `ffmpeg.exe` to be available in your system PATH or bundled with your app.

**Option A: Install FFmpeg system-wide**
- Download from https://www.gyan.dev/ffmpeg/builds/
- Add to Windows PATH

**Option B: Bundle with your app** (recommended for distribution)
- Place `ffmpeg.exe` in your Flutter app's build directory
- The plugin will automatically find it

### 3. Basic Usage Example

```dart
import 'package:ffmpeg_kit_flutter/ffmpeg_kit.dart';
import 'package:ffmpeg_kit_flutter/ffmpeg_session.dart';
import 'package:ffmpeg_kit_flutter/return_code.dart';

// Execute FFmpeg command
Future<void> convertVideo() async {
  final session = await FFmpegKit.execute('-i input.mp4 -c:v libx264 output.mp4');
  
  final returnCode = await session.getReturnCode();
  if (ReturnCode.isSuccess(returnCode)) {
    print('Conversion successful!');
  } else {
    print('Conversion failed with code: $returnCode');
  }
  
  // Get logs
  final logs = await session.getLogs();
  for (final log in logs) {
    print('FFmpeg: ${log.getMessage()}');
  }
}

// Async execution with callbacks
Future<void> convertVideoAsync() async {
  await FFmpegKit.executeAsync(
    '-i input.mp4 -c:v libx264 output.mp4',
    (session) async {
      final returnCode = await session.getReturnCode();
      print('Async conversion completed with code: $returnCode');
    }
  );
}
```

### 4. Advanced Usage

```dart
// Create session manually
final session = await FFmpegKit.createSession(['-i', 'input.mp4', 'output.mp4']);
await FFmpegKit.executeSession(session);

// Monitor progress
final session = await FFmpegKit.executeAsync(
  '-i input.mp4 -progress pipe:1 output.mp4',
  (session) => print('Completed'),
  (log) => print('Log: ${log.getMessage()}'),
  (statistics) => print('Progress: ${statistics.getTime()}ms')
);

// Cancel execution
await session.cancel();
```

## Implementation Details

### Architecture
- **SessionManager**: Singleton managing all FFmpeg sessions
- **FFmpegSession**: Individual session with state, logs, and statistics
- **Windows Process**: Native CreateProcess integration with stdout/stderr capture
- **Thread Safety**: Mutex-protected session registry and log collection

### Session States
- `CREATED (0)` - Session created, not yet started
- `RUNNING (1)` - FFmpeg process is executing
- `COMPLETED (3)` - Execution completed successfully
- `FAILED (2)` - Execution failed or was cancelled

### DLL Bundling
FFmpeg DLLs are automatically bundled with your Windows Flutter app during build. No additional setup required.

## Troubleshooting

### "FFmpeg not found" Error
- Ensure `ffmpeg.exe` is in your system PATH
- Or bundle `ffmpeg.exe` with your Flutter app

### Plugin Build Errors
- Ensure you have Visual Studio with C++ development tools
- CMake 3.14+ is required
- Clean and rebuild: `flutter clean && flutter build windows`

### Performance Issues
- Use async execution for long-running operations
- The bundled DLLs are GPL-licensed with full codec support
- Consider LGPL builds for smaller size if needed

## Limitations

1. **FFmpeg Executable**: Currently requires system-installed or bundled `ffmpeg.exe`
2. **GPL License**: Bundled DLLs use GPL license - ensure compliance for commercial use
3. **File Size**: Large DLL files (150MB+ total) increase app size

## Contributing

This is a community fork maintaining Windows support for the retired ffmpeg-kit project. 

- **Original Project**: https://github.com/arthenica/ffmpeg-kit (retired)
- **Windows Fork**: https://github.com/JulianPscheid/ffmpeg-kit
- **Issues**: Please report Windows-specific issues in the fork repository

## License

- **Plugin Code**: Same license as original ffmpeg-kit
- **FFmpeg DLLs**: GPL v3 (bundled builds include GPL-licensed codecs)
- **Commercial Use**: Ensure GPL compliance or use LGPL builds