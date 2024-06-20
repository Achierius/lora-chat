project('lora-chat', 'cpp',
  version : '0.0.1',
  license : 'bsd-3-clause',
  default_options : [
    'cpp_std=c++20',
  ],
)

fs = import('fs')
gtest = dependency('gtest', main: true)
gtest_nomain = dependency('gtest', main : false) # Should this be method: 'system' ?

subdir('libsx1276')
subdir('libbcp')
subdir('tools')
