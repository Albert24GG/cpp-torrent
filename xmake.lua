add_rules("mode.debug", "mode.release")

set_languages("c++23")

add_requires("catch2", "cpptrace")

target("test")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("cpptrace")

rule("run_tests")
    after_build(function (target)
        os.exec("xmake run catch2-test")
    end)

target("catch2-test")
    set_kind("binary")
    add_files("test/*.cpp")
    add_files("src/*.cpp|main.cpp")
    add_packages("catch2","cpptrace")
    add_includedirs("src")
    add_rules("run_tests")
