bcp_sources = [
  'session.cpp',
  'lora_interface.cpp',
  'packet.cpp',
]

bcp_unittests = [
  { 'test' : 'session_unittest.cpp', 'sources' : ['session.cpp', 'packet.cpp'] },
  { 'test' : 'packet_unittest.cpp', 'sources' : ['packet.cpp'] },
]

libbcp = shared_library('bcp',
  bcp_sources,
  # TODO use declare_dependency instead of copying around sx1276_include everywhere
  include_directories : [bcp_include, sx1276_include],
  link_with : libsx1276,
  install : true)

foreach s : bcp_unittests
  testf = s['test']
  sources = s['sources']
  s_e = executable(fs.name(testf) + '_gtest', [testf] + sources, dependencies : gtest)
  test(fs.name(testf), s_e, protocol : 'gtest')
endforeach
