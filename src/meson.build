d3d_video_src = files(
	'main.cpp',
	'VideoQuad.cpp',
  'VideoCtx.cpp',
  'RenderParam.cpp',
	)
	
# Find the HLSL compiler
fxc = find_program('fxc.exe', required: true)

# Compile HLSL to .cso
vshader = custom_target(
  'vshader-compile',
  input: 'VertexShader.hlsl',
  output: 'VertexShader.h',
  command: [
    fxc,
    '/T', 'vs_5_0',       # Shader type and model (e.g., vertex shader 5.0)
    '/E', 'main',         # Entry point function
	'/Vn', 'g_vs',        # Variable name in generated header file
    '/Fh', '@OUTPUT@',    # Output file
    '@INPUT@'             # Input file
  ],
  build_by_default: true  # Always build this target
)

pshader = custom_target(
  'pshader-compile',
  input: 'PixelShader.hlsl',
  output: 'PixelShader.h',
  command: [
    fxc,
    '/T', 'ps_5_0',       # Shader type and model (e.g., vertex shader 5.0)
    '/E', 'main',         # Entry point function
	'/Vn', 'g_ps',        # Variable name in generated header file
    '/Fh', '@OUTPUT@',    # Output file
    '@INPUT@'             # Input file
  ],
  build_by_default: true  # Always build this target
)

dbghelp = meson.get_compiler('cpp').find_library('dbghelp', required: true)

executable('d3d_video', [d3d_video_src, vshader, pshader], dependencies: [libavcodec, libavformat, spdlog, dbghelp], win_subsystem: 'windows')