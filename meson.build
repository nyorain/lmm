project('lmm', ['c', 'cpp'],
	version: '0.1.0',
	default_options: [
		'werror=true',
		'warning_level=3',
		'cpp_std=c++20',
	]
)

src = files(
	'lmm.cpp',
	'linalloc.cpp',
)

lib = library('lmm', src)
