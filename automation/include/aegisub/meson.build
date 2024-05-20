# Copy files to build directory for testing purposes
fs = import('fs')

lua_files = files(
    'argcheck.moon',
    'clipboard.lua',
    'ffi.moon',
    'lfs.moon',
    're.moon',
    'unicode.moon',
    'util.moon',
)

foreach f: lua_files
    fs.copyfile(f)
endforeach
