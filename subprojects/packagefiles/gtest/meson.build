project('gtest', 'cpp', version: '1.14.0')

gtest_inc = include_directories('googletest/include')

gtest_lib = static_library('gtest',
    'googletest/src/gtest-all.cc',
    include_directories: gtest_inc)

gtest_dep = declare_dependency(
    include_directories: gtest_inc,
    link_with: gtest_lib)

gtest_main_lib = static_library('gtest_main',
    'googletest/src/gtest_main.cc',
    include_directories: gtest_inc)

gtest_main_dep = declare_dependency(
    include_directories: gtest_inc,
    link_with: [gtest_main_lib, gtest_lib])
