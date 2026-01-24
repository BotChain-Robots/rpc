# RPC Library
The RPC library provides an interface to interact directly with the BotChain devices. The library is managed by the [conan](https://conan.io) package manager, and is consumed by BotChain's higher level libraries. This library provides the following features:
- mDNS discovery of modules
- TCP connection to modules
- UDP connection to modules
- An MPI like messaging interface

The latest releases of the RPC library can be found in our [artifactory](http://jslightham.com:8082), or on [Jenkins](https://jenkins.jslightham.com/job/Botchain/job/librpc/).

## Platform Support
- MacOS (Apple silicon)
- MacoS (x86)
- Ubuntu (x86)
- Windows (x86)

## Setup
### MacOS
Install xcode command line tools (if you do not already have them)
```
xcode-select --install
```

Install conan and dependencies
```
brew install conan cmake ninja
```

Generate a conan profile
```
conan profile detect --force
```

### Ubuntu
On newer versions of Ubuntu, the package manager is responsible for managing python packages. We use `pipx` to create a virtual environment.

Install `pipx` and dependencies
```
sudo apt install pipx cmake ninja-build
```

Install conan with pipx
```
pipx install conan
```

Generate a conan profile
```
conan profile detect --force
```

### Artifactory Setup (optional)
These instructions should only be followed after you have completed all setup steps for your platform.

This is an optional section that is only required if you plan on uploading releases to the artifactory manually.
Releases tagged with new versions in `conanfile.py` that are merged into the main branch are automatically uploaded to the artifactory by [Jenkins](https://jenkins.jslightham.com/job/Botchain/job/librpc/).

Add the artifactory
```
conan remote add artifactory http://jslightham.com:8081/artifactory/api/conan/botchain
```

Add credentials to connect to the remote artifactory
```
conan remote login artifactory <username> -p <password>
```

Contact Johnathon to get login credentials for the artifactory.

## Development
```
# On macos or Linux, you can run
./build_rpc_library

# Building manually
build_type=Release # change to the build type you want (ex. Debug, RelWithDebInfo).
conan install . --build=missing --output-folder=. -s build_type="${build_type}"
cmake -S . -B "build/${build_type}" -DCMAKE_TOOLCHAIN_FILE="$build/${build_type}/generators/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE="${build_type}"
cmake --build "build/${build_type}" --config "${build_type}"
conan create .
```

## Building For Release
Bump the version in `conanfile.py`.

Create the package
```
conan create .
```

Upload to the artifactory
```
conan upload librpc -r=artifactory
```
