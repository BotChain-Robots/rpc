from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeToolchain, CMakeDeps
from conan.tools.files import copy
import os

class MyLibraryConan(ConanFile):
    name = "librpc"
    version = "1.1.6"

    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    exports_sources = "CMakeLists.txt", "src/*", "include/*"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["rpc"]
        self.cpp_info.includedirs = ["include"]

    def requirements(self):
        self.requires("flatbuffers/24.12.23")
        self.requires("spdlog/1.16.0")

    def configure(self):
        if self.settings.os == "Linux":
            self.options.fPIC = True
