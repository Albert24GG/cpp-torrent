add_rules("mode.debug", "mode.release")

set_languages("c++23")

local packages = { 
    "cpptrace", 
    "openssl3",
    "cpr",
    "spdlog",
    "asio",
    "indicators",
    "argparse",
    "catch2",
    "cpp-httplib"
}

for _, pkg in ipairs(packages) do
    add_requires(pkg)
end

rule("copy_bin")
    after_build(function (target)
        build_path = "$(buildir)/$(os)/$(arch)/$(mode)/"
        bin_path = "bin/$(mode)/"    
        os.mkdir(bin_path)    
        os.cp(build_path .. target:name(), bin_path)
    end)

rule("config_pkgs")
    on_load(function(target)
        for _, pkg in ipairs(packages) do
           target:add("packages", pkg) 
        end
    end)

-- Create a static library for common source files
target("common_lib")
    add_rules("config_pkgs")
    set_kind("static")
    add_files("src/*.cpp|main.cpp|ProgressBar.cpp")
    add_includedirs("src")

-- Add the main target
target("cpp-torrent")
    if is_mode("debug") then
        add_defines("DEBUG")
    end
    set_kind("binary")
    add_deps("common_lib")
    add_files("src/main.cpp","src/ProgressBar.cpp")
    add_rules("copy_bin", "config_pkgs")

-- Add the unit test targets
for _, file in ipairs(os.files("test/*_test.cpp")) do
    local name = path.basename(file)
    target(name)
        set_kind("binary")
        set_default(false)
        add_files("test/" .. name .. ".cpp")
        if name == "Tracker_test" then
            add_files("test/MockUdpTracker.cpp", "test/MockHttpTracker.cpp")
        end

        add_includedirs("src")
        add_deps("common_lib")
        add_rules("config_pkgs")
        add_tests("default")
end
