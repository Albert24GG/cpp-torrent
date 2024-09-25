add_rules("mode.debug", "mode.release")

includes("test")

set_languages("c++23")

add_requires("cpptrace", "openssl3", "cpr", "spdlog", "asio", "indicators", "argparse")

rule("copy_bin")
    after_build(function (target)
        build_path = "$(buildir)/$(os)/$(arch)/$(mode)/"
        bin_path = "bin/$(mode)/"    
        os.mkdir(bin_path)    
        os.cp(build_path .. target:name(), bin_path)
    end)

target("cpp-torrent")
    if is_mode("debug") then
        add_defines("DEBUG")
    end
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("cpptrace", "openssl3", "cpr", "spdlog", "asio", "indicators", "argparse")
    add_rules("copy_bin")
