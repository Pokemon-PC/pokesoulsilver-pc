# Compile settings for game / NitroSDK / NitroSystem code built for the host.
#
#   phg_game_flags      INTERFACE target: include paths, defines and options
#                       mirroring the DS build (common.mk / config.mk).
#   phg_low_address_link(<target>)
#                       links an executable so its code and static data live
#                       below 4 GiB (see src/platform_pc/hw_pc.h for why).

set(PHG_GAME_VERSION "SOULSILVER" CACHE STRING "HEARTGOLD or SOULSILVER")   # pokesoulsilver-pc: SoulSilver by default
set_property(CACHE PHG_GAME_VERSION PROPERTY STRINGS HEARTGOLD SOULSILVER)
set(PHG_GAME_LANGUAGE "ENGLISH" CACHE STRING "Game language (only ENGLISH is supported by the decomp)")
option(PHG_WARN_PTR_CASTS "Report pointer<->int size-mismatch casts in game code (64-bit audit)" OFF)

# --- generated headers ------------------------------------------------------
set(PHG_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)

add_executable(phg_gen_fx_consts
    ${PROJECT_SOURCE_DIR}/tools/gen_fx_consts/main.c
    ${PROJECT_SOURCE_DIR}/tools/gen_fx_consts/fx_encode.c
    ${PROJECT_SOURCE_DIR}/tools/gen_fx_consts/fx_data.c)
target_compile_definitions(phg_gen_fx_consts PRIVATE
    NDEBUG SOURCE_DIR="${PROJECT_SOURCE_DIR}/tools/gen_fx_consts")
if(NOT MSVC)
    target_link_libraries(phg_gen_fx_consts PRIVATE m)
endif()
set_target_properties(phg_gen_fx_consts PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/hosttools)

add_custom_command(
    OUTPUT ${PHG_GENERATED_DIR}/nitro/fx/fx_const.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PHG_GENERATED_DIR}/nitro/fx
    COMMAND phg_gen_fx_consts nitro/fx/fx_const.h
    WORKING_DIRECTORY ${PHG_GENERATED_DIR}
    DEPENDS phg_gen_fx_consts ${PROJECT_SOURCE_DIR}/tools/gen_fx_consts/fx_const.csv
    COMMENT "Generating nitro/fx/fx_const.h")
# Host tools from the repository used to generate headers (no Metrowerks).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
enable_language(CXX)
# PHG_PREBUILT_HOSTTOOLS: reuse msgenc/jsonproc/csv2bin from another build
# (e.g. build-clang/hosttools when building the other game version). Also a
# workaround when an antivirus holds freshly built tools on their first run.
set(PHG_PREBUILT_HOSTTOOLS "" CACHE PATH "Folder with msgenc/jsonproc/csv2bin to use instead of building them")
if(PHG_PREBUILT_HOSTTOOLS)
    set(PHG_HOSTTOOLS_DIR ${PHG_PREBUILT_HOSTTOOLS})
    set(_phg_ext "${CMAKE_EXECUTABLE_SUFFIX}")
    set(_phg_dep_msgenc ${PHG_HOSTTOOLS_DIR}/msgenc${_phg_ext})
    set(_phg_dep_jsonproc ${PHG_HOSTTOOLS_DIR}/jsonproc${_phg_ext})
    set(_phg_dep_csv2bin ${PHG_HOSTTOOLS_DIR}/csv2bin${_phg_ext})
    set(_phg_tools_to_build)
else()
    set(PHG_HOSTTOOLS_DIR ${CMAKE_BINARY_DIR}/hosttools)
    set(_phg_dep_msgenc phg_tool_msgenc)
    set(_phg_dep_jsonproc phg_tool_jsonproc)
    set(_phg_dep_csv2bin phg_tool_csv2bin)
    set(_phg_tools_to_build msgenc jsonproc csv2bin)
endif()
foreach(_tool ${_phg_tools_to_build})
    file(GLOB _tool_srcs ${PROJECT_SOURCE_DIR}/tools/${_tool}/*.cpp)
    add_executable(phg_tool_${_tool} ${_tool_srcs})
    target_include_directories(phg_tool_${_tool} PRIVATE ${PROJECT_SOURCE_DIR}/tools/${_tool})
    target_compile_features(phg_tool_${_tool} PRIVATE cxx_std_17)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(phg_tool_${_tool} PRIVATE -w)
    endif()
    if(MINGW)
        target_link_options(phg_tool_${_tool} PRIVATE -static)
    endif()
    set_target_properties(phg_tool_${_tool} PROPERTIES
        OUTPUT_NAME ${_tool} RUNTIME_OUTPUT_DIRECTORY ${PHG_HOSTTOOLS_DIR})
endforeach()

# ROM file system the NARC indices are validated against (and that provides
# member counts for o2narc-numbered archives). Extract it from your own
# cartridge dump with tools/pc_port/nitrofs.py (see PORTING_ROADMAP.md).
set(PHG_ROM_DATA_DIR "${PROJECT_SOURCE_DIR}/pc_data/fr" CACHE PATH
    "Extracted NitroFS of your own HeartGold ROM")
if(NOT EXISTS ${PHG_ROM_DATA_DIR}/a)
    message(WARNING "PHG_ROM_DATA_DIR (${PHG_ROM_DATA_DIR}) does not contain an extracted ROM: "
                    "a few NARC indices cannot be generated. Run: "
                    "python tools/pc_port/nitrofs.py extract <your-rom.nds> pc_data/fr")
endif()

file(GLOB _phg_gmms CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/files/msgdata/msg/*.gmm)
set(PHG_EN_MSG_DIR ${CMAKE_BINARY_DIR}/datagen/en_msg)

add_custom_command(
    OUTPUT ${PHG_GENERATED_DIR}/msg.stamp
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/pc_port/gen_msg.py
            ${PHG_GENERATED_DIR} ${PHG_EN_MSG_DIR} --hosttools ${PHG_HOSTTOOLS_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${PHG_GENERATED_DIR}/msg.stamp
    DEPENDS ${_phg_dep_msgenc} ${_phg_dep_jsonproc} ${_phg_gmms}
            ${PROJECT_SOURCE_DIR}/tools/pc_port/gen_msg.py
            ${PROJECT_SOURCE_DIR}/files/msgdata/msg.mk
            ${PROJECT_SOURCE_DIR}/files/poketool/trainer/trainers.json
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Compiling game text (msgenc) -> msg_XXXX.h"
    VERBATIM)

add_custom_command(
    OUTPUT ${PHG_GENERATED_DIR}/naix.stamp
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/pc_port/gen_naix.py
            ${PHG_GENERATED_DIR} --all --rom-dir ${PHG_ROM_DATA_DIR}
            --hosttools ${PHG_HOSTTOOLS_DIR} --stamp ${PHG_GENERATED_DIR}/naix.stamp
    DEPENDS ${_phg_dep_jsonproc} ${_phg_dep_csv2bin}
            ${PROJECT_SOURCE_DIR}/tools/pc_port/gen_naix.py
            ${PROJECT_SOURCE_DIR}/filesystem.mk
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Generating NARC indices (.naix)"
    VERBATIM)

add_custom_command(
    OUTPUT ${PHG_GENERATED_DIR}/pc_overlay_ids.h ${PHG_GENERATED_DIR}/pc_overlay_ids.c
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/pc_port/gen_overlay_ids.py
            ${PHG_GENERATED_DIR}/pc_overlay_ids.h
    DEPENDS ${PROJECT_SOURCE_DIR}/main.lsf ${PROJECT_SOURCE_DIR}/tools/pc_port/gen_overlay_ids.py
    COMMENT "Generating overlay IDs from main.lsf"
    VERBATIM)

# English layer (language choice): the repository's English text and files,
# next to the data extracted from the player's ROM (tools/pc_port/build_lang_en.py).
if(PHG_GAME_VERSION STREQUAL "SOULSILVER")
    set(PHG_DATA_DIR_NAME pc_data_ss)
else()
    set(PHG_DATA_DIR_NAME pc_data)
endif()
set(PHG_LANG_TEMPLATE "${PROJECT_SOURCE_DIR}/${PHG_DATA_DIR_NAME}/fr" CACHE PATH
    "Data extracted from your ROM, used as the template of the English layer")
set(PHG_LANG_EN_DIR ${PROJECT_SOURCE_DIR}/${PHG_DATA_DIR_NAME}/en)
if(EXISTS ${PHG_LANG_TEMPLATE}/a/0/2/7)
    add_custom_command(
        OUTPUT ${PHG_LANG_EN_DIR}/.built
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/pc_port/build_lang_en.py
                --template ${PHG_LANG_TEMPLATE} --en-msg ${PHG_EN_MSG_DIR}
                --files ${PROJECT_SOURCE_DIR}/files --out ${PHG_LANG_EN_DIR}
                --stamp ${PHG_LANG_EN_DIR}/.built
        DEPENDS ${PHG_GENERATED_DIR}/msg.stamp ${PROJECT_SOURCE_DIR}/tools/pc_port/build_lang_en.py
        COMMENT "English data layer -> ${PHG_DATA_DIR_NAME}/en"
        VERBATIM)
    add_custom_target(phg_lang_en ALL DEPENDS ${PHG_LANG_EN_DIR}/.built)
else()
    message(STATUS "No extracted ROM in ${PHG_LANG_TEMPLATE}: English layer not built")
endif()

add_custom_target(phg_generated_headers DEPENDS
    ${PHG_GENERATED_DIR}/nitro/fx/fx_const.h
    ${PHG_GENERATED_DIR}/msg.stamp
    ${PHG_GENERATED_DIR}/naix.stamp
    ${PHG_GENERATED_DIR}/pc_overlay_ids.h)

# --- flags ----------------------------------------------------------------
add_library(phg_game_flags INTERFACE)
add_dependencies(phg_game_flags phg_generated_headers)

# Same order as MWCFLAGS in common.mk. Marked SYSTEM so warnings coming from
# SDK headers written for Metrowerks do not drown the port's own warnings.
target_include_directories(phg_game_flags SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/src/platform_pc/nitro_compat
    ${PHG_GENERATED_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/library
    ${PROJECT_SOURCE_DIR}/files
    ${PROJECT_SOURCE_DIR}/lib/include
    ${PROJECT_SOURCE_DIR}/lib/NitroSDK/include
    ${PROJECT_SOURCE_DIR}/lib/NitroSDK/include-mw
    ${PROJECT_SOURCE_DIR}/lib/NitroSDK/include-pokediamond)

target_compile_definitions(phg_game_flags INTERFACE
    PLATFORM_PC=1
    ${PHG_GAME_VERSION}
    GAME_REMASTER=0
    ${PHG_GAME_LANGUAGE}
    PM_KEEP_ASSERTS
    SDK_ARM9 SDK_CODE_ARM SDK_FINALROM)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(phg_game_flags INTERFACE
        -include platform_pc_prefix.h
        -fsigned-char                 # MWCC: -char signed
        -fms-extensions
        -fno-strict-aliasing          # DS code type-puns freely
        -fwrapv                       # and relies on 2's complement wrap-around
        -Wno-unknown-pragmas          # #pragma force_active/opt_*/unused
        -Wno-multichar)
    if(NOT PHG_WARN_PTR_CASTS)
        target_compile_options(phg_game_flags INTERFACE
            -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast)
    endif()
elseif(MSVC)
    target_compile_options(phg_game_flags INTERFACE /FIplatform_pc_prefix.h /J- /wd4068)
endif()

# --- low-address executables ---------------------------------------------
function(phg_low_address_link target)
    if(WIN32 AND (MINGW OR CMAKE_C_COMPILER_ID MATCHES "GNU|Clang") AND NOT MSVC)
        target_link_options(${target} PRIVATE
            -Wl,--image-base=0x10000000
            -Wl,--disable-dynamicbase
            -Wl,--disable-high-entropy-va)
    elseif(MSVC)
        target_link_options(${target} PRIVATE /BASE:0x10000000 /DYNAMICBASE:NO /HIGHENTROPYVA:NO)
    elseif(APPLE)
        # Default __PAGEZERO covers the whole low 4 GiB on 64-bit macOS.
        target_link_options(${target} PRIVATE -Wl,-pagezero_size,0x1000 -Wl,-image_base,0x10000000 -Wl,-no_pie
            -Wl,-no_fixup_chains)   # chained fixups ignore the fixed image base
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
    else()
        # Linux / BSD: a non-PIE executable placed at 0x10000000, above the
        # DS memory map (0x01FF0000-0x07010000) and below 4 GiB, like on
        # Windows (the game stores code and data addresses in 32-bit words).
        set(PHG_LD_TTEXT_SEGMENT ON)   # GNU ld and lld both accept it
        if(NOT CMAKE_VERSION VERSION_LESS 3.18)
            include(CheckLinkerFlag)
            check_linker_flag(C "-Wl,-Ttext-segment=0x10000000" PHG_LD_TTEXT_SEGMENT)
        endif()
        target_link_options(${target} PRIVATE -no-pie)
        if(PHG_LD_TTEXT_SEGMENT)
            target_link_options(${target} PRIVATE -Wl,-Ttext-segment=0x10000000)
        else()
            target_link_options(${target} PRIVATE -Wl,--image-base=0x10000000)
        endif()
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
    endif()
endfunction()
