file(GLOB_RECURSE PANS_SOURCES CONFIGURE_DEPENDS *.cpp *.cc *.c)
file(GLOB_RECURSE PANS_HEADERS CONFIGURE_DEPENDS *.h *.hpp)

add_library(pans STATIC ${PANS_SOURCES} ${PANS_HEADERS})

add_library(pans::pans ALIAS pans)

redefine_file_macro("${PANS_SOURCES}")

# 公共头文件使用 C++20；让下游目标通过 pans::pans 自动继承该要求。
target_compile_features(pans PUBLIC cxx_std_20)

target_include_directories(pans
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(pans PUBLIC pans_options) # 继承所有编译/链接选项

target_link_libraries(pans PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(pans PRIVATE dbghelp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(pans PRIVATE PkgConfig::PANS_LIBDW)
endif()

target_link_libraries(pans PUBLIC yaml-cpp::yaml-cpp)

# target_link_libraries(pans PRIVATE Boost::context)

add_dependencies(pans pans_third_party_dependencies)


target_compile_options(pans PRIVATE
    # 主库可能需要更严格的警告
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Werror          # 将警告视为错误（主库严格要求）
    >
)

install(TARGETS pans
    EXPORT pansTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS pans_options EXPORT pansTargets)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
