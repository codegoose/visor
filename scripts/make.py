#!/usr/bin/env python3

# Importing necessary modules
import builtins
import os, subprocess
from sys import platform

# Getting the absolute path of the current directory
dir = os.path.abspath('.')

# Checking if the directory ends with '_rel'
is_rel = dir.endswith('_rel')

# Setting the build type based on whether the directory ends with '_rel'
build_type = 'RelWithDebInfo' if is_rel else 'Debug'

# List of commands to be executed
commands = [
    ['conan', 'install', '..', '-s', 'build_type=%s' % build_type, '--build=missing'],
    ['cmake', '-G', 'Ninja', '..', '-DCMAKE_BUILD_TYPE=%s' % build_type],
    ['cmake', '--build', '.', '--config', build_type]
]

# Looping through the commands
for command_parts in commands:
    # Printing the command
    print('$', command_parts)
    try:
        # Executing the command
        return_code = subprocess.call([' '.join(command_parts)] if platform == 'linux' else command_parts, shell=True)
        
        # Checking the return code of the command
        if return_code != 0:
            quit()
    except subprocess.CalledProcessError as grepexc:
        quit()
