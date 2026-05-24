platform_src = files(
    'platform/StringUtils.cpp',
    'platform/Logger.cpp',
    'platform/CrashHandler.cpp',
    'platform/StreamUtils.cpp',
    'platform/ScreenCapture.cpp',
)

source_src = files(
    'source/MediaSource.cpp',
    'source/FileSource.cpp',
    'source/CaptureSource.cpp',
    'source/PlaylistSource.cpp',
)

decode_src = files(
    'decode/VideoDecoder.cpp',
)

render_src = files(
    'render/VideoQuad.cpp',
    'render/SwapChainManager.cpp',
)

playback_src = files(
    'playback/VideoController.cpp',
    'playback/TextureUpdater.cpp',
)

app_src = files(
    'app/Application.cpp',
)

d3d_video_src = files(
    'main.cpp',
)

fxc = find_program('fxc.exe', required: true)

vshader = custom_target(
  'vshader-compile',
  input: 'VertexShader.hlsl',
  output: 'VertexShader.h',
  command: [
    fxc,
    '/T', 'vs_5_0',
    '/E', 'main',
    '/Vn', 'g_vs',
    '/Fh', '@OUTPUT@',
    '@INPUT@'
  ],
  build_by_default: true
)

pshader = custom_target(
  'pshader-compile',
  input: 'PixelShader.hlsl',
  output: 'PixelShader.h',
  command: [
    fxc,
    '/T', 'ps_5_0',
    '/E', 'main',
    '/Vn', 'g_ps',
    '/Fh', '@OUTPUT@',
    '@INPUT@'
  ],
  build_by_default: true
)

cpshader = custom_target(
  'cpshader-compile',
  input: 'CapturePixelShader.hlsl',
  output: 'CapturePixelShader.h',
  command: [
    fxc,
    '/T', 'ps_5_0',
    '/E', 'main',
    '/Vn', 'g_cps',
    '/Fh', '@OUTPUT@',
    '@INPUT@'
  ],
  build_by_default: true
)

dbghelp = meson.get_compiler('cpp').find_library('dbghelp', required: true)
psapi = meson.get_compiler('cpp').find_library('psapi', required: true)
windowsapp = meson.get_compiler('cpp').find_library('windowsapp', required: true)

executable('d3d_video', [d3d_video_src, platform_src, source_src, decode_src, render_src, playback_src, app_src, vshader, pshader, cpshader],
    dependencies: [libavcodec, libavformat, libavutil, spdlog, dbghelp, psapi, windowsapp],
    win_subsystem: 'windows')
