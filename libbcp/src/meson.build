bcp_sources = [
  'session.cpp',
]

libbcp = shared_library('bcp',
  bcp_sources,
  include_directories : [bcp_include, sx1276_include],
  link_with : libsx1276,
  install : true)
