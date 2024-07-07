add_rules("mode.debug", "mode.release")

set_languages("c++23")
set_policy("build.c++.modules", true)

target("test")
    set_kind("binary")
    add_files("src/*.cpp", "src/*.mpp")
