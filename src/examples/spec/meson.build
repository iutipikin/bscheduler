libz = dependency('zlib')

executable(
	'spec',
	sources: 'spec.cc',
	include_directories: [srcdir],
	dependencies: [threads, libz, unistdx, bscheduler_app]
)
