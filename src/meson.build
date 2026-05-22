platform_src = files(
    'platform/StringUtils.cpp',
    'platform/Logger.cpp',
    'platform/CrashHandler.cpp',
)

source_src = files(
    'source/MediaSource.cpp',
)

d3d_video_src = files(
    'main.cpp',
    'VideoQuad.cpp',
    'VideoCtx.cpp',
    'RenderParam.cpp',
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

dbghelp = meson.get_compiler('cpp').find_library('dbghelp', required: true)

executable('d3d_video', [d3d_video_src, platform_src, source_src, vshader, pshader],
    dependencies: [libavcodec, libavformat, libavutil, spdlog, dbghelp],
    win_subsystem: 'windows')
