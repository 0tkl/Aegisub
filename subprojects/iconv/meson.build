project('iconv', 'c', license: 'LGPL',
        meson_version: '>=1.0.0')

iconv_src = [
    'libcharset/localcharset.c',
    'libiconv/iconv.c'
]

iconv_headers = ['include/iconv.h']

iconv_incs = include_directories(
    'include',
    'libcharset',
    'libiconv'
)

libiconv = static_library('iconv', iconv_src, include_directories: iconv_incs)
libiconv_dep = declare_dependency(link_with: libiconv,
                                  include_directories: iconv_incs)

meson.override_dependency('iconv', libiconv_dep)
