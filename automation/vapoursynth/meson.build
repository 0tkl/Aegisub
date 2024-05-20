fs = import('fs')
# Copy files to build directory for testing purposes
vs_files = files(
    'aegisub_vs.py',
)

foreach f: vs_files
    fs.copyfile(f)
endforeach
