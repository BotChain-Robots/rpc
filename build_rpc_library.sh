#!/bin/bash
set -e

function usage() {
	echo "Usage:"
	echo "${SCRIPT_NAME} [-b <build type>] [-h]"
	echo "	-b | --build-type       - The build type (ie. Release, Debug, RelWithDebInfo)"
	echo "	-h | --help             - Print usage"
	echo "Example:"
	echo "${SCRIPT_NAME} -b Release"
	exit 1
}

function parse_args() {
	while [ -n "${1}" ]; do
		case "${1}" in
		-h | --help)
			usage
			;;
		-b | --build-type)
			[ -n "${2}" ] || usage || echo "ERROR: Not enough parameters"
			build_type="${2}"
			shift 2
			;;
		-d | --disable-format)
		    disable_format=true
			shift 1
			;;
		*)
			echo "ERROR: Invalid parameter. Exiting..."
			usage
			exit 1
			;;
		esac
	done
}

function check_pre_req() {
    if [ "${build_type}" != "Debug" ] && [ "${build_type}" != "Release" ] && [ "${build_type}" != "RelWithDebInfo" ]; then
        usage
        echo "ERROR: Build type must be one of: Release, Debug, RelWithDebInfo"
	fi
}

SCRIPT_NAME="$(basename "${0}")"
ROOT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

build_type=""
disable_format=false
parse_args "${@}"
check_pre_req

if [ "$disable_format" != "true" ]; then
    echo "Formatting with clang-format..."
    find "${ROOT_DIR}" -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i -style=file
fi

echo "Building..."
conan install "${ROOT_DIR}" --build=missing --output-folder="${ROOT_DIR}" -s build_type="${build_type}"
cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build/${build_type}" -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/build/${build_type}/generators/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE="${build_type}"
cmake --build "${ROOT_DIR}/build/${build_type}" --config "${build_type}"
conan create .
