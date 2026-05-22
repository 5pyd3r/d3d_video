gtest_dep = dependency('gtest', main: true, fallback: ['gtest', 'gtest_main_dep'])

test('StringUtils',
    executable('test_string_utils',
        'platform/StringUtils_test.cpp',
        '../src/platform/StringUtils.cpp',
        dependencies: [gtest_dep]))
