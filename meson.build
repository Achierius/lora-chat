project('lora-chat', 'cpp',
  version : '0.0.1',
  license : 'bsd-3-clause',
  default_options : [
    'cpp_std=c++20',
  ],
)

# This shouldn't be necessary but Meson isn't adding the argument -- does it think that my GCC will compile @ c++20 by default?
add_project_arguments('-std=c++20', language : 'cpp')

fs = import('fs')
gtest = dependency('gtest', main: true)
gtest_nomain = dependency('gtest', main : false) # Should this be method: 'system' ?

subdir('libsx1276')
subdir('libbcp')
subdir('tools')
