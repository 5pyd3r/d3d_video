gtest_dep = dependency('gtest', main: true, fallback: ['gtest', 'gtest_main_dep'])

test('StringUtils',
    executable('test_string_utils',
        'platform/StringUtils_test.cpp',
        '../src/platform/StringUtils.cpp',
        dependencies: [gtest_dep]))

test('MediaSource',
    executable('test_media_source',
        'source/MediaSource_test.cpp',
        '../src/source/MediaSource.cpp',
        '../src/platform/Logger.cpp',
        dependencies: [gtest_dep, libavcodec, libavformat, libavutil, libswscale, spdlog]))
