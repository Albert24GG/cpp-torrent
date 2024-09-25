add_requires("catch2", "cpp-httplib")

for _, file in ipairs(os.files("*_test.cpp")) do
     local name = path.basename(file)
     target(name)
         set_kind("binary")
         set_default(false)
         add_files(name .. ".cpp")
         if name == "Tracker_test" then
             add_files("MockUdpTracker.cpp", "MockHttpTracker.cpp")
         end

         add_files("../src/*.cpp|main.cpp|ProgressBar.cpp")
         add_includedirs("../src")
         add_packages("catch2","cpptrace", "openssl3", "cpr", "cpp-httplib", "spdlog", "asio")
         add_tests("default")
end
