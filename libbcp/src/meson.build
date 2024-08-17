bcp_sources = [
  'session.cpp',
  'lora_interface.cpp',
  'packet.cpp',
  'protocol_agent.cpp',
]

bcp_unittests = [
  { 'test' : 'session_unittest.cpp' },
  { 'test' : 'packet_unittest.cpp' },
  { 'test' : 'protocol_agent_unittest.cpp' },
]

libbcp = shared_library('bcp',
  bcp_sources,
  # TODO use declare_dependency instead of copying around sx1276_include everywhere
  include_directories : [bcp_include, sx1276_include],
  link_with : libsx1276,
  install : true)

libbcp_dep = declare_dependency(
  include_directories : [bcp_include, sx1276_include],
  link_with : libbcp)
 
foreach s : bcp_unittests
  testf = s['test']
  s_e = executable(fs.name(testf) + '_gtest', [testf], dependencies : [gtest, libbcp_dep])
  test(fs.name(testf), s_e, protocol : 'gtest')
endforeach
