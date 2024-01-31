project('luajit', 'c', version: '2.1.0', license: 'MIT')

cc = meson.get_compiler('c')
host_system = host_machine.system()

system_deps = [
    cc.find_library('dl', required: false),
    cc.find_library('m', required: false)
]

# get architecture id for the host machine so it can be set when compiling buildvm natively
lj_target_id = cc.get_define('LUAJIT_TARGET', prefix: '#include "@0@/src/lj_arch.h"'.format(meson.current_source_dir()))
add_project_arguments('-DLUAJIT_TARGET=@0@'.format(lj_target_id), language: 'c', native: true)

# compat flag is needed for both the buildvm code generator (compiled natively) and luajit itself
add_project_arguments('-DLUAJIT_ENABLE_LUA52COMPAT', '-DENDIAN_LE', language: 'c', native: true)
add_project_arguments('-DLUAJIT_ENABLE_LUA52COMPAT', '-DENDIAN_LE', language: 'c', native: false)
if host_system == 'linux'
    add_project_arguments('-DLUAJIT_OS=LUAJIT_OS_LINUX', language: 'c', native: true)
    add_project_arguments('-DLUAJIT_UNWIND_EXTERNAL', language: 'c', native: false)
    ljvm_mode = 'elfasm'
    ljvm_bout = 'lj_vm.s'
elif host_system == 'darwin'
    add_project_arguments('-DLUAJIT_OS=LUAJIT_OS_OSX', language: 'c', native: true)
    add_project_arguments('-DLUAJIT_UNWIND_EXTERNAL', language: 'c', native: false)
    ljvm_mode = 'machasm'
    ljvm_bout = 'lj_vm.s'
elif host_system == 'windows'
    add_project_arguments('-DLUAJIT_OS=LUAJIT_OS_WINDOWS', language: 'c', native: true)
    if cc.get_id() != 'msvc'
        add_project_arguments('-malign-double', language: 'c', native: true)
    endif
    ljvm_mode = 'peobj'
    ljvm_bout = 'lj_vm.o'
else
    error('Unsupported platform')
endif

subdir('src')

incl_dirs = [src_inc]
if host_system == 'darwin'
    incl_dirs += include_directories('/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/')
endif

luajit_dep = declare_dependency(link_with: luajit_lib, include_directories: incl_dirs)
