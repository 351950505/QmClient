# 所有客户端平台和 Android Java 绑定使用同一份 SDL 源码。
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_ANDROID_JAR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
  URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
  URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
  DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/downloads/sdl3"
)
FetchContent_MakeAvailable(SDL3)
# 每次配置都幂等核对补丁，已有或离线下载缓存也必须使用同一修复。
execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DSDL3_SOURCE_DIR=${sdl3_SOURCE_DIR}" -P "${CMAKE_CURRENT_LIST_DIR}/PatchSDL3X11.cmake"
  RESULT_VARIABLE QM_SDL3_PATCH_RESULT
)
if(NOT "${QM_SDL3_PATCH_RESULT}" STREQUAL "0")
  message(FATAL_ERROR "Failed to apply the pinned SDL3 X11 allocation patch")
endif()
set_target_properties(SDL3-static PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
  # 项目已有 uuid 工具目标，显式链接系统库，避免 CMake 将 SDL 依赖解析成工具。
  if(MSVC)
    # 与客户端和 Rust 静态库保持相同 CRT，避免 SDL 引用 DLL CRT 导入符号。
    set_property(TARGET SDL3-static PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(QM_SDL_UUID_LIBRARY uuid.lib)
  else()
    set(QM_SDL_UUID_LIBRARY -luuid)
  endif()
  foreach(QM_SDL_LINK_PROPERTY LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(QM_SDL_LINKS SDL3-static ${QM_SDL_LINK_PROPERTY})
    string(REPLACE "$<LINK_ONLY:uuid>" "$<LINK_ONLY:${QM_SDL_UUID_LIBRARY}>" QM_SDL_LINKS "${QM_SDL_LINKS}")
    list(TRANSFORM QM_SDL_LINKS REPLACE "^uuid$" "${QM_SDL_UUID_LIBRARY}")
    set_property(TARGET SDL3-static PROPERTY ${QM_SDL_LINK_PROPERTY} "${QM_SDL_LINKS}")
  endforeach()
endif()

set(SDL3_FOUND TRUE)
set(SDL3_LIBRARIES SDL3::SDL3)
set(SDL3_INCLUDE_DIRS "${sdl3_SOURCE_DIR}/include")
set(SDL3_VERSION "3.4.18")
if(ANDROID)
  file(WRITE "${CMAKE_BINARY_DIR}/qm_sdl3_java_dir.txt"
    "${sdl3_SOURCE_DIR}/android-project/app/src/main/java\n")
endif()
