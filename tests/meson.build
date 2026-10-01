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

test('IVideoSourceContract',
    executable('test_ivideo_source_contract',
        'source/IVideoSource_contract_test.cpp',
        dependencies: [gtest_dep]))

test('VideoProcSource',
    executable('test_videoproc_source',
        'detect/VideoProcSource_test.cpp',
        '../src/detect/VideoProcSource.cpp',
        dependencies: [gtest_dep]))

test('VideoControllerIntegration',
    executable('test_video_controller_integration',
        'playback/VideoControllerIntegration_test.cpp',
        '../src/playback/VideoController.cpp',
        '../src/playback/TextureUpdater.cpp',
        '../src/render/VideoQuad.cpp',
        '../src/render/SwapChainManager.cpp',
        '../src/platform/Logger.cpp',
        vshader, pshader, cpshader,
        include_directories: include_directories('../src'),
        dependencies: [gtest_dep, libavcodec, libavformat, libavutil, spdlog]))

test('VideoControllerPauseReasons',
    executable('test_video_controller_pause_reasons',
        'playback/VideoControllerPauseReasons_test.cpp',
        '../src/playback/VideoController.cpp',
        '../src/playback/TextureUpdater.cpp',
        '../src/render/VideoQuad.cpp',
        '../src/render/SwapChainManager.cpp',
        '../src/platform/Logger.cpp',
        vshader, pshader, cpshader,
        include_directories: include_directories('../src'),
        dependencies: [gtest_dep, libavcodec, libavformat, libavutil, spdlog]))

test('TextureUpdater',
    executable('test_texture_updater',
        'playback/TextureUpdater_test.cpp',
        '../src/playback/TextureUpdater.cpp',
        '../src/render/VideoQuad.cpp',
        '../src/platform/Logger.cpp',
        vshader, pshader, cpshader,
        include_directories: include_directories('../src'),
        dependencies: [gtest_dep, libavcodec, libavformat, libavutil, spdlog]))
test('SwapChainManager',
    executable('test_swapchain_manager',
        'render/SwapChainManager_test.cpp',
        '../src/render/SwapChainManager.cpp',
        '../src/platform/Logger.cpp',
        include_directories: include_directories('../src'),
        # D3D11CreateDeviceAndSwapChain lives in d3d11.lib. The rest of the project
        # picks it up through VideoQuad.h's #pragma comment(lib, ...); this target
        # does not include that header, so the import libraries are declared here.
        link_args: ['-ld3d11', '-ldxgi'],
        dependencies: [gtest_dep, spdlog]))