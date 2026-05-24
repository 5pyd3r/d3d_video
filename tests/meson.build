gtest_dep = dependency('gtest', main: true, fallback: ['gtest', 'gtest_main_dep'])

test('StreamUtils',
    executable('test_stream_utils',
        'platform/StreamUtils_test.cpp',
        '../src/platform/StreamUtils.cpp',
        dependencies: [gtest_dep]))

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
        dependencies: [gtest_dep, libavcodec, libavformat, libavutil, libavutil, spdlog]))

test('VideoDecoder',
    executable('test_video_decoder',
        'decode/VideoDecoder_test.cpp',
        '../src/source/MediaSource.cpp',
        '../src/decode/VideoDecoder.cpp',
        '../src/platform/Logger.cpp',
        dependencies: [gtest_dep, libavcodec, libavformat, libavutil, libavutil, spdlog]))

test('PlaybackPipeline',
    executable('test_playback_pipeline',
        'playback/PlaybackController_test.cpp',
        '../src/source/MediaSource.cpp',
        '../src/decode/VideoDecoder.cpp',
        '../src/platform/Logger.cpp',
        dependencies: [gtest_dep, libavcodec, libavformat, libavutil, libavutil, spdlog]))
