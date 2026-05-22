cmake_minimum_required(VERSION 3.22)
project(NightClubMode)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Output shared library name ──────────────────────────────────────────────
set(MOD_ID "NightClubMode")

# ── Android NDK toolchain must be set externally via -DCMAKE_TOOLCHAIN_FILE ─
if(NOT DEFINED ANDROID_ABI)
  set(ANDROID_ABI arm64-v8a)
endif()

# ── Source files ─────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCE_FILES
  src/*.cpp
)

add_library(${MOD_ID} SHARED ${SOURCE_FILES})

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(${MOD_ID} PRIVATE
  include
  extern/includes
  extern/includes/beatsaber-hook/shared
  extern/includes/codegen/shared
  extern/includes/paper/shared
)

# ── Compiler flags ────────────────────────────────────────────────────────────
target_compile_options(${MOD_ID} PRIVATE
  -frtti
  -fexceptions
  -O3
  -DMOD_ID=\"${MOD_ID}\"
  -DVERSION=\"1.0.0\"
)

# ── Link against Android system libs ─────────────────────────────────────────
target_link_libraries(${MOD_ID}
  log
  android
)
