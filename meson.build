project('person_demo',
        'cpp',
        license: ['GPL2+', 'LGPL2.1+'],
        version: files('./VERSION'),
        meson_version: '>=1.3.0',
        default_options: [
            'buildtype=debugoptimized',
            'b_lundef=false',
            'c_std=c11',
            'cpp_std=c++20',
            'cpp_eh=default',
            'warning_level=2',
        ]
)

build_root = meson.project_build_root()
source_root = meson.project_source_root()
python = find_program('python3')

# wtl
wtl = dependency('wtl', required: true)

# win32xx
win32xx = dependency('win32xx', required: true)

# spdlog
spdlog = dependency('spdlog', required: true)

# ffmpeg
libavcodec = dependency('libavcodec', version: '>= 60.31.102')
libavfilter = dependency('libavfilter', version: '>= 9.12.100')
libavformat = dependency('libavformat', version: '>= 60.16.100')
libavutil = dependency('libavutil', version: '>= 58.29.100')
libswresample = dependency('libswresample', version: '>= 4.12.100')
libswscale = dependency('libswscale', version: '>= 7.5.100')

# 定义 UNICODE 宏（Windows 专用）
if host_machine.system() == 'windows'
  add_project_arguments('-DUNICODE', language: 'cpp')
  add_project_arguments('-D_UNICODE', language: 'cpp')
endif

subdir('src')
