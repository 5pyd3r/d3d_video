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

test('VideoController',
    executable('test_video_controller',
        'playback/VideoController_test.cpp',
        dependencies: [gtest_dep]))

test('IVideoSourceContract',
    executable('test_ivideo_source_contract',
        'source/IVideoSource_contract_test.cpp',
        dependencies: [gtest_dep]))

test('VideoControllerIntegration',
    executable('test_video_controller_integration',
        'playback/VideoControllerIntegration_test.cpp',
        '../src/playback/VideoController.cpp',
        '../src/playback/TextureUpdater.cpp',
        '../src/render/VideoQuad.cpp',
        '../src/render/SwapChainManager.cpp',
        vshader, pshader, cpshader,
        include_directories: include_directories('../src'),
        dependencies: [gtest_dep, libavcodec, libavformat, libavutil, spdlog]))
