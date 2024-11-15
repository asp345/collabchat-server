project('collabchat-server', 'cpp', default_options: ['cpp_std=c++17'])

boost_dep = dependency('boost', modules: ['system', 'json'])
sqlite_dep = dependency('sqlite3', method: 'auto')
src_files = files('base64.cpp', 'database.cpp', 'http_connection.cpp', 'main.cpp')

executable('server', src_files, dependencies: [boost_dep, sqlite_dep])
