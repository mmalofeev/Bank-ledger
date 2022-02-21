cmake_minimum_required(VERSION 3.10)

project(bank CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Do not specify /O2 because there are Debug/Release modes.
if (MSVC)
    add_compile_options(/W4 /WX /D_CRT_SECURE_NO_WARNINGS)  # Disable warnings about VS-only _s functions (e.g. scanf_s).
else()
    if (NOT CMAKE_BUILD_TYPE)
      set(CMAKE_BUILD_TYPE Debug CACHE STRING "Default build type from CMakeLists.txt" FORCE)  # Add debug information
    endif (NOT CMAKE_BUILD_TYPE)

    add_compile_options(-Wall -Wextra -Werror)
    if (UNIX AND NOT CMAKE_CXX_FLAGS)  # Do not add if -DCMAKE_CXX_FLAGS is passed
      add_compile_options(-fsanitize=address -fsanitize=undefined)
      add_link_options(-fsanitize=address -fsanitize=undefined)
    endif (UNIX AND NOT CMAKE_CXX_FLAGS)
endif (MSVC)

find_package(Threads)
find_package(Boost 1.67 REQUIRED system)

set(NETWORKING_LIBS)
if (WIN32 AND NOT MSVC AND NOT BORLAND)
    # Boost::Asio automatically links with necessary
    # libraries when using Visual C++ or Borland C++,
    # see BOOST_ASIO_NO_DEFAULT_LINKED_LIBS.
    set(NETWORKING_LIBS "ws2_32")
endif ()

add_executable(bank-test doctest_main.cpp bank_test.cpp bank.cpp)
target_link_libraries(bank-test ${CMAKE_THREAD_LIBS_INIT})

add_executable(bank-server bank_server.cpp bank.cpp)
target_include_directories(bank-server PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(bank-server ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ${NETWORKING_LIBS})
