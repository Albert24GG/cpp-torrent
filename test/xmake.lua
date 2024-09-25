add_requires("catch2", "cpp-httplib")

-- Create a static library for common source files
target("common_lib")
    set_kind("static")
    add_files("../src/*.cpp|main.cpp|ProgressBar.cpp")
    add_includedirs("../src")
    add_packages("cpptrace","openssl3", "cpr", "spdlog", "asio")

-- Create test targets
for _, file in ipairs(os.files("*_test.cpp")) do
    local name = path.basename(file)
    target(name)
        set_kind("binary")
        set_default(false)
        add_files(name .. ".cpp")
        if name == "Tracker_test" then
            add_packages("cpp-httplib")
            add_files("MockUdpTracker.cpp", "MockHttpTracker.cpp")
        end

        add_includedirs("../src")
        add_packages("catch2", "asio")
        add_deps("common_lib")
        add_tests("default")
end
