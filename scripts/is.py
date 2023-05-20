#!/usr/bin/env python3

# Importing necessary modules
import os, json

# Checking if the directory ends with '_rel'
is_rel = os.path.abspath('.').endswith('_rel')

# Path to the C/CPP properties file
props_path = '../.vscode/c_cpp_properties.json'

# Checking if the C/CPP properties file exists
props_exists = os.path.exists(props_path)

# Printing whether C/CPP properties exist and the path of the file
print('C/CPP properties exists:', props_exists, '("%s")' % props_path)

# Quitting if C/CPP properties file does not exist
if not props_exists:
    quit()

# Path to the Conan build info file
info_path = 'conanbuildinfo.txt'

# Checking if the Conan build info file exists
info_exists = os.path.exists(info_path)

# Printing whether Conan build info exists and the path of the file
print('Conan build info exists:', info_exists, '("%s")' % info_path)

# Quitting if Conan build info file does not exist
if not info_exists:
    quit()

# Reading the contents of the Conan build info file
info = [line.strip() for line in open(info_path).readlines()]

# Function to extract a specific section from the build info
def grab_section(list, name):
    location = list.index('[%s]' % name)
    short = list[location+1:]
    short = short[:short.index('')]
    print(name, '->', ', '.join(short))
    return short

# Extracting 'includedirs' and 'defines' sections from the build info
includes = grab_section(info, 'includedirs')
defines = grab_section(info, 'defines')

# Loading the JSON document from the C/CPP properties file
doc = json.loads(open(props_path).read())

# Checking if 'configurations' key exists and if there are any configurations present
if not 'configurations' in doc or len(doc['configurations']) == 0:
    print('No configurations found in C/CPP properties file.')
    quit()

# Extracting the 'includePath' from the first configuration
doc_includes = doc['configurations'][0]['includePath']
workspace_doc_includes = []

# Filtering the 'includePath' to preserve only workspace folder includes
for include in doc_includes:
    if include.startswith('${workspaceFolder}'):
        workspace_doc_includes.append(include)
        print('Preserving: ', include)

# Updating 'includePath' with workspace folder includes
doc_includes = workspace_doc_includes

# Adding additional includes from the build info to 'includePath'
for include in includes:
    if include in doc_includes:
        print('Skipping:', include)
        continue
    print('Adding to includes:', include)
    doc_includes.append(include)

# Updating 'includePath' in the first configuration with the modified list
doc['configurations'][0]['includePath'] = doc_includes

# Extracting the 'defines' from the first configuration
doc_defines = doc['configurations'][0]['defines']

# Adding additional defines from the build info to 'defines'
for define in defines:
    if define in doc_defines:
        print('Skipping:', define)
        continue
    print('Adding to defines:', define)
    doc_defines.append(define)

# Modifying 'defines' based on the build type
if is_rel:
    if 'NDEBUG' not in doc_defines:
        doc_defines.append('NDEBUG')
        print('Added NDEBUG.')
else:
    if 'NDEBUG' in doc_defines:
        doc_defines.remove('NDEBUG')
        print('Removed NDEBUG.')

# Writing the modified JSON document back to the C/CPP properties file
open(props_path, 'w').write(json.dumps(doc, indent=4))
