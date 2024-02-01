from conan import ConanFile, tools

class CommentsServiceConan(ConanFile):
    name = "comments-service"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "boost/1.84.0",
        "nlohmann_json/3.11.3",
        "cassandra-cpp-driver/2.17.1"
    )
    tool_requires = "cmake/3.28.1"
    generators = "CMakeDeps", "CMakeToolchain"

    def configure(self):
        if self.settings.compiler == "gcc":
            self.settings.compiler.version = "10"
            self.settings.compiler.libcxx = "libstdc++11"

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
