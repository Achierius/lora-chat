bcp_sources = [
  'session.cpp',
  'lora_interface.cpp',
  'packet.cpp',
]

bcp_unittests = [
  { 'test' : 'session_unittest.cpp', 'source' : 'session.cpp' },
  { 'test' : 'packet_unittest.cpp', 'source' : 'packet.cpp' },
]

libbcp = shared_library('bcp',
  bcp_sources,
  # TODO use declare_dependency instead of copying around sx1276_include everywhere
  include_directories : [bcp_include, sx1276_include],
  link_with : libsx1276,
  install : true)

foreach s : bcp_unittests
  testf = s['test']
  sourcef = s['source']
  s_e = executable(fs.name(testf) + '_gtest', [testf, sourcef], dependencies : gtest)
  test(fs.name(testf), s_e, protocol : 'gtest')
endforeach
