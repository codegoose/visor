import builtins
import os, subprocess

dir = os.path.abspath('.')
is_rel = dir.endswith('\\build_rel')
build_type = 'RelWithDebInfo' if is_rel else 'Debug'

commands = [
    ['conan', 'install', '..', '-s', 'build_type=%s' % build_type, '--build=missing'],
    ['cmake', '..', '-DCMAKE_BUILD_TYPE=%s' % build_type],
    ['cmake', '--build', '.', '--config', build_type]
]

for command_parts in commands:
    print('$', command_parts)
    try:
        install_output = subprocess.call(command_parts, shell=True)
    except subprocess.CalledProcessError as grepexc:
        quit()