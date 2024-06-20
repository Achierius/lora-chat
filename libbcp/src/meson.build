bcp_sources = [
  'session.cpp',
]

bcp_unittests = [
  { 'test' : 'session_unittest.cpp', 'source' : 'session.cpp' },
]

libbcp = shared_library('bcp',
  bcp_sources,
  include_directories : [bcp_include, sx1276_include],
  link_with : libsx1276,
  install : true)

foreach s : bcp_unittests
  testf = s['test']
  sourcef = s['source']
  s_e = executable(fs.name(testf) + '_gtest', [testf, sourcef], dependencies : gtest)
  test(fs.name(testf), s_e, protocol : 'gtest')
endforeach
