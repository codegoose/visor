# Building

Use [**Build Tools for Visual Studio 2019**](https://visualstudio.microsoft.com/downloads/). (*All Downloads > Tools for Visual Studio 2019*)

You must also have [**Conan**](https://conan.io/) and [**CMake**](https://cmake.org/) installed.

```
mkdir build
cd build
conan install .. -s build_type=Debug --build=missing
cmake ..
cmake --build . --config Debug
```

Note: that's ```Debug``` mode. Possible options are:

```
None
Debug
Release
RelWithDebInfo
MinSizeRel
```

Use ```RelWithDebInfo``` for release builds that implement Sentry.

```
conan install .. -s build_type=RelWithDebInfo --build=missing
cmake ..
cmake --build . --config RelWithDebInfo
```

## Update Intellisense Paths

VSCode's C++ extension ([*ms-vscode.cpptools*](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)) needs to know where the include paths for all the external libraries are and preprocessor defintions to assume. These are kept within:

```
.vscode/c_cpp_properties.json
```

Adding this info to that file automatically is managed by a [**Python**](https://www.python.org/) script. From the root of the project run:

```
python sync-intellisense.py
```

## Debug in Visual Studio

```
&"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.exe" /debugexe .\bin\visor.exe
```

This will open Visual Studio and upon hitting the Start button it'll run the executable and break if there are any problems. I recommend to keep it open and run the program in there instead of writing ```.\bin\visor.exe```.