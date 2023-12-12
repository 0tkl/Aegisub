project('csri', 'c')

csri_sources = [
    'lib/list.c',
    'lib/wrap.c',
    'subhelp/logging.c',
]

csri_inc = [include_directories('include')]

host_system = host_machine.system()
if host_system == 'windows'
    csri_inc += include_directories('lib/win32')
    csri_sources += ['lib/win32/enumerate.c', 'subhelp/win32/openfile.c']
else
    csri_inc += include_directories('lib/posix')
    csri_sources += ['lib/posix/enumerate.c', 'subhelp/posix/openfile.c']
endif

csri = library('csri', csri_sources, include_directories: csri_inc)
csri_dep = declare_dependency(link_with: csri, include_directories: csri_inc)
