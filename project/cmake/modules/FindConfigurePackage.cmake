﻿#.rst:
# FindConfigurePackage
# ----------------
#
# find package, and try to configure it when not found in system.
#
# FindConfigurePackage(
#     PACKAGE <name>
#     BUILD_WITH_CONFIGURE
#     BUILD_WITH_CMAKE
#     BUILD_WITH_SCONS
#     BUILD_WITH_CUSTOM_COMMAND
#     CONFIGURE_FLAGS [configure options...]
#     CMAKE_FLAGS [cmake options...]
#     CMAKE_INHIRT_BUILD_ENV
#     SCONS_FLAGS [scons options...]
#     CUSTOM_BUILD_COMMAND [custom build cmd...]
#     MAKE_FLAGS [make options...]
#     PREBUILD_COMMAND [run cmd before build ...]
#     AFTERBUILD_COMMAND [run cmd after build ...]
#     RESET_FIND_VARS [cmake vars]
#     WORKING_DIRECTORY <work directory>
#     BUILD_DIRECTORY <build directory>
#     PREFIX_DIRECTORY <prefix directory>
#     SRC_DIRECTORY_NAME <source directory name>
#     MSVC_CONFIGURE [Debug/Release/RelWithDebInfo/MinSizeRel]
#     INSTALL_TARGET <target name: install>
#     ZIP_URL <zip url>
#     TAR_URL <tar url>
#     SVN_URL <svn url>
#     GIT_URL <git url>
# )
#
# ::
#
#   <configure options>     - flags added to configure command
#   <cmake options>         - flags added to cmake command
#   <scons options>         - flags added to scons command
#   <custom build cmd>      - custom commands for build
#   <make options>          - flags added to make command
#   <pre build cmd>         - commands to run before build tool
#   <work directory>        - work directory
#   <build directory>       - where to execute configure and make
#   <prefix directory>      - prefix directory(default: <work directory>)
#   <source directory name> - source directory name(default detected by download url)
#   <zip url>               - from where to download zip when find package failed
#   <tar url>               - from where to download tar.* or tgz when find package failed
#   <svn url>               - from where to svn co when find package failed
#   <git url>               - from where to git clone when find package failed
#

#=============================================================================
# Copyright 2014-2015 OWenT.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

function(FindConfigurePackageDownloadFile from to)
    find_program (WGET_FULL_PATH wget)
    if(WGET_FULL_PATH)
        execute_process(COMMAND ${WGET_FULL_PATH} --no-check-certificate -v ${from} -O ${to})
    else()
        find_program (CURL_FULL_PATH curl)
        if(CURL_FULL_PATH)
            execute_process(COMMAND ${CURL_FULL_PATH} --insecure -L ${from} -o ${to})
        else()
            file(DOWNLOAD ${from} ${to} SHOW_PROGRESS)
        endif()
    endif()
endfunction()

function(FindConfigurePackageUnzip src work_dir)
    if (CMAKE_HOST_UNIX)
        find_program (FindConfigurePackage_UNZIP_BIN unzip)
        if (FindConfigurePackage_UNZIP_BIN)
            execute_process(COMMAND ${FindConfigurePackage_UNZIP_BIN} -uo ${src}
                WORKING_DIRECTORY ${work_dir}
            )
            return()
        endif ()
    endif ()
    # fallback
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xvf ${src}
        WORKING_DIRECTORY ${work_dir}
    )
endfunction()

function(FindConfigurePackageTarXV src work_dir)
    if (CMAKE_HOST_UNIX)
        find_program (FindConfigurePackage_TAR_BIN tar)
        if (FindConfigurePackage_TAR_BIN)
            execute_process(COMMAND ${FindConfigurePackage_TAR_BIN} -xvf ${src}
                WORKING_DIRECTORY ${work_dir}
            )
            return()
        endif ()
    endif ()
    # fallback
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xvf ${src}
        WORKING_DIRECTORY ${work_dir}
    )
endfunction()

function(FindConfigurePackageRemoveEmptyDir DIR)
    if (EXISTS ${DIR})
        file(GLOB RESULT "${DIR}/*")
        list(LENGTH RESULT RES_LEN)
        if(${RES_LEN} EQUAL 0)
            file(REMOVE_RECURSE ${DIR})
        endif()
    endif()
endfunction()

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
    include(ProcessorCount)
    ProcessorCount(CPU_CORE_NUM)
    set(FindConfigurePackageCMakeBuildMultiJobs "--parallel" ${CPU_CORE_NUM} CACHE INTERNAL "Build options for multi-jobs")
    unset(CPU_CORE_NUM)
elseif ( (CMAKE_MAKE_PROGRAM STREQUAL "make") OR (CMAKE_MAKE_PROGRAM STREQUAL "gmake") OR (CMAKE_MAKE_PROGRAM STREQUAL "ninja") )
    include(ProcessorCount)
    ProcessorCount(CPU_CORE_NUM)
    set(FindConfigurePackageCMakeBuildMultiJobs "--" "-j${CPU_CORE_NUM}" CACHE INTERNAL "Build options for multi-jobs")
    unset(CPU_CORE_NUM)
elseif ( CMAKE_MAKE_PROGRAM STREQUAL "xcodebuild" )
    include(ProcessorCount)
    ProcessorCount(CPU_CORE_NUM)
    set(FindConfigurePackageCMakeBuildMultiJobs "--" "-jobs" ${CPU_CORE_NUM} CACHE INTERNAL "Build options for multi-jobs")
    unset(CPU_CORE_NUM)
elseif (CMAKE_VS_MSBUILD_COMMAND)
    set(FindConfigurePackageCMakeBuildMultiJobs "--" "/m" CACHE INTERNAL "Build options for multi-jobs")
endif()

if (NOT FindConfigurePackageGitFetchDepth)
    set (FindConfigurePackageGitFetchDepth 100 CACHE STRING "Defalut depth of git clone/fetch")
endif ()

macro (FindConfigurePackage)
    include(CMakeParseArguments)
    set(optionArgs BUILD_WITH_CONFIGURE BUILD_WITH_CMAKE BUILD_WITH_SCONS BUILD_WITH_CUSTOM_COMMAND CMAKE_INHIRT_BUILD_ENV)
    set(oneValueArgs PACKAGE WORKING_DIRECTORY BUILD_DIRECTORY PREFIX_DIRECTORY SRC_DIRECTORY_NAME PROJECT_DIRECTORY MSVC_CONFIGURE ZIP_URL TAR_URL SVN_URL GIT_URL GIT_BRANCH INSTALL_TARGET)
    set(multiValueArgs CONFIGURE_CMD CONFIGURE_FLAGS CMAKE_FLAGS RESET_FIND_VARS SCONS_FLAGS MAKE_FLAGS CUSTOM_BUILD_COMMAND PREBUILD_COMMAND AFTERBUILD_COMMAND)
    cmake_parse_arguments(FindConfigurePackage "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

    if (NOT FindConfigurePackage_INSTALL_TARGET)
        set (FindConfigurePackage_INSTALL_TARGET "install")
    endif ()
    # some module is not match standard, using upper case but package name
    string(TOUPPER "${FindConfigurePackage_PACKAGE}_FOUND" FIND_CONFIGURE_PACKAGE_UPPER_NAME)

    # step 1. find using standard method
    find_package(${FindConfigurePackage_PACKAGE} QUIET)
    if(NOT ${FindConfigurePackage_PACKAGE}_FOUND AND NOT ${FIND_CONFIGURE_PACKAGE_UPPER_NAME})
        if(NOT FindConfigurePackage_PREFIX_DIRECTORY)
            # prefix
            set(FindConfigurePackage_PREFIX_DIRECTORY ${FindConfigurePackage_WORK_DIRECTORY})
        endif()

        list(APPEND CMAKE_FIND_ROOT_PATH ${FindConfigurePackage_PREFIX_DIRECTORY})

        # step 2. find in prefix
        find_package(${FindConfigurePackage_PACKAGE} QUIET)

        # step 3. build
        if(NOT ${FindConfigurePackage_PACKAGE}_FOUND AND NOT ${FIND_CONFIGURE_PACKAGE_UPPER_NAME})
            set (FindConfigurePackage_UNPACK_SOURCE NO)

            # tar package
            if(NOT FindConfigurePackage_UNPACK_SOURCE AND FindConfigurePackage_TAR_URL)
                get_filename_component(DOWNLOAD_FILENAME "${FindConfigurePackage_TAR_URL}" NAME)
                if(NOT FindConfigurePackage_SRC_DIRECTORY_NAME)
                    string(REGEX REPLACE "\\.tar\\.[A-Za-z0-9]+$" "" FindConfigurePackage_SRC_DIRECTORY_NAME "${DOWNLOAD_FILENAME}")
                endif()
                set(FindConfigurePackage_DOWNLOAD_SOURCE_DIR "${FindConfigurePackage_WORKING_DIRECTORY}/${FindConfigurePackage_SRC_DIRECTORY_NAME}")
                FindConfigurePackageRemoveEmptyDir(${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})

                if(NOT EXISTS "${FindConfigurePackage_WORKING_DIRECTORY}/${DOWNLOAD_FILENAME}")
                    message(STATUS "start to download ${DOWNLOAD_FILENAME} from ${FindConfigurePackage_TAR_URL}")
                    FindConfigurePackageDownloadFile("${FindConfigurePackage_TAR_URL}" "${FindConfigurePackage_WORKING_DIRECTORY}/${DOWNLOAD_FILENAME}")
                endif()

                FindConfigurePackageTarXV("${FindConfigurePackage_WORKING_DIRECTORY}/${DOWNLOAD_FILENAME}" ${FindConfigurePackage_WORKING_DIRECTORY})

                if (EXISTS ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                    set (FindConfigurePackage_UNPACK_SOURCE YES)
                endif()
            endif()

            # zip package
            if(NOT FindConfigurePackage_UNPACK_SOURCE AND FindConfigurePackage_ZIP_URL)
                get_filename_component(DOWNLOAD_FILENAME "${FindConfigurePackage_ZIP_URL}" NAME)
                if(NOT FindConfigurePackage_SRC_DIRECTORY_NAME)
                    string(REGEX REPLACE "\\.zip$" "" FindConfigurePackage_SRC_DIRECTORY_NAME "${DOWNLOAD_FILENAME}")
                endif()
                set(FindConfigurePackage_DOWNLOAD_SOURCE_DIR "${FindConfigurePackage_WORKING_DIRECTORY}/${FindConfigurePackage_SRC_DIRECTORY_NAME}")
                FindConfigurePackageRemoveEmptyDir(${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})

                if(NOT EXISTS "${FindConfigurePackage_WORKING_DIRECTORY}/${DOWNLOAD_FILENAME}")
                    message(STATUS "start to download ${DOWNLOAD_FILENAME} from ${FindConfigurePackage_ZIP_URL}")
                    FindConfigurePackageDownloadFile("${FindConfigurePackage_ZIP_URL}" "${FindConfigurePackage_WORKING_DIRECTORY}/${DOWNLOAD_FILENAME}")
                endif()

                if(NOT EXISTS ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                    FindConfigurePackageUnzip("${FindConfigurePackage_WORKING_DIRECTORY}/${DOWNLOAD_FILENAME}" ${FindConfigurePackage_WORKING_DIRECTORY})
                endif()

                if (EXISTS ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                    set (FindConfigurePackage_UNPACK_SOURCE YES)
                endif()
            endif()

            # git package
            if(NOT FindConfigurePackage_UNPACK_SOURCE AND FindConfigurePackage_GIT_URL)
                get_filename_component(DOWNLOAD_FILENAME "${FindConfigurePackage_GIT_URL}" NAME)
                if(NOT FindConfigurePackage_SRC_DIRECTORY_NAME)
                    get_filename_component(FindConfigurePackage_SRC_DIRECTORY_FULL_NAME "${DOWNLOAD_FILENAME}" NAME)
                    string(REGEX REPLACE "\\.git$" "" FindConfigurePackage_SRC_DIRECTORY_NAME "${FindConfigurePackage_SRC_DIRECTORY_FULL_NAME}")
                endif()
                set(FindConfigurePackage_DOWNLOAD_SOURCE_DIR "${FindConfigurePackage_WORKING_DIRECTORY}/${FindConfigurePackage_SRC_DIRECTORY_NAME}")
                FindConfigurePackageRemoveEmptyDir(${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})

                if(NOT EXISTS ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                    find_package(Git)
                    if(GIT_FOUND)
                        if (NOT FindConfigurePackage_GIT_BRANCH)
                            set(FindConfigurePackage_GIT_BRANCH master)
                        endif()
                        execute_process(COMMAND ${GIT_EXECUTABLE} clone "--depth=${FindConfigurePackageGitFetchDepth}" -b ${FindConfigurePackage_GIT_BRANCH} ${FindConfigurePackage_GIT_URL} ${FindConfigurePackage_SRC_DIRECTORY_NAME}
                            WORKING_DIRECTORY ${FindConfigurePackage_WORKING_DIRECTORY}
                        )
                    else()
                       message(STATUS "git not found, skip ${FindConfigurePackage_GIT_URL}")
                    endif()
                endif()

                if (EXISTS ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                    set (FindConfigurePackage_UNPACK_SOURCE YES)
                endif()
            endif()

            # svn package
            if(NOT FindConfigurePackage_UNPACK_SOURCE AND FindConfigurePackage_SVN_URL)
                get_filename_component(DOWNLOAD_FILENAME "${FindConfigurePackage_SVN_URL}" NAME)
                if(NOT FindConfigurePackage_SRC_DIRECTORY_NAME)
                    get_filename_component(FindConfigurePackage_SRC_DIRECTORY_NAME "${DOWNLOAD_FILENAME}" NAME)
                endif()
                set(FindConfigurePackage_DOWNLOAD_SOURCE_DIR "${FindConfigurePackage_WORKING_DIRECTORY}/${FindConfigurePackage_SRC_DIRECTORY_NAME}")
                FindConfigurePackageRemoveEmptyDir(${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})

                if(NOT EXISTS ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                    find_package(Subversion)
                    if(SUBVERSION_FOUND)
                        execute_process(COMMAND ${Subversion_SVN_EXECUTABLE} co "${FindConfigurePackage_SVN_URL}" "${FindConfigurePackage_SRC_DIRECTORY_NAME}"
                            WORKING_DIRECTORY "${FindConfigurePackage_WORKING_DIRECTORY}"
                        )
                    else()
                       message(STATUS "svn not found, skip ${FindConfigurePackage_SVN_URL}")
                    endif()
                endif()

                if (EXISTS ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                    set (FindConfigurePackage_UNPACK_SOURCE YES)
                endif()
            endif()

            if(NOT FindConfigurePackage_UNPACK_SOURCE)
                message(FATAL_ERROR "Can not download source for ${FindConfigurePackage_PACKAGE}")
            endif()

            # init build dir
            if (NOT FindConfigurePackage_BUILD_DIRECTORY)
                set(FindConfigurePackage_BUILD_DIRECTORY ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
            endif()
            if (NOT EXISTS ${FindConfigurePackage_BUILD_DIRECTORY})
                file(MAKE_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY})
            endif()

            # prebuild commands
            foreach(cmd ${FindConfigurePackage_PREBUILD_COMMAND})
                message(STATUS "FindConfigurePackage - Run: ${cmd} @ ${FindConfigurePackage_BUILD_DIRECTORY}")
                execute_process(
                    COMMAND ${cmd}
                    WORKING_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY}
                )
            endforeach()

            # build using configure and make
            if(FindConfigurePackage_BUILD_WITH_CONFIGURE)
                if(FindConfigurePackage_PROJECT_DIRECTORY)
                    file(RELATIVE_PATH CONFIGURE_EXEC_FILE ${FindConfigurePackage_BUILD_DIRECTORY} "${FindConfigurePackage_PROJECT_DIRECTORY}/configure")
                else()
                    file(RELATIVE_PATH CONFIGURE_EXEC_FILE ${FindConfigurePackage_BUILD_DIRECTORY} "${FindConfigurePackage_WORKING_DIRECTORY}/${FindConfigurePackage_SRC_DIRECTORY_NAME}/configure")
                endif()
                if ( ${CONFIGURE_EXEC_FILE} STREQUAL "configure")
                    set(CONFIGURE_EXEC_FILE "./configure")
                endif()
                message(STATUS "@${FindConfigurePackage_BUILD_DIRECTORY} Run: ${CONFIGURE_EXEC_FILE} --prefix=${FindConfigurePackage_PREFIX_DIRECTORY} ${FindConfigurePackage_CONFIGURE_FLAGS}")
                execute_process(
                    COMMAND ${CONFIGURE_EXEC_FILE} "--prefix=${FindConfigurePackage_PREFIX_DIRECTORY}" ${FindConfigurePackage_CONFIGURE_FLAGS}
                    WORKING_DIRECTORY "${FindConfigurePackage_BUILD_DIRECTORY}"
                )

                execute_process(
                    COMMAND "make" ${FindConfigurePackage_MAKE_FLAGS} ${FindConfigurePackage_INSTALL_TARGET}
                    WORKING_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY}
                )

            # build using cmake and make
            elseif(FindConfigurePackage_BUILD_WITH_CMAKE)
                if(FindConfigurePackage_PROJECT_DIRECTORY)
                    file(RELATIVE_PATH BUILD_WITH_CMAKE_PROJECT_DIR ${FindConfigurePackage_BUILD_DIRECTORY} ${FindConfigurePackage_PROJECT_DIRECTORY})
                else()
                    file(RELATIVE_PATH BUILD_WITH_CMAKE_PROJECT_DIR ${FindConfigurePackage_BUILD_DIRECTORY} ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                endif()
                if ( NOT BUILD_WITH_CMAKE_PROJECT_DIR)
                    set(BUILD_WITH_CMAKE_PROJECT_DIR ".")
                endif()
                message(STATUS "@${FindConfigurePackage_BUILD_DIRECTORY} Run: ${CMAKE_COMMAND} ${BUILD_WITH_CMAKE_PROJECT_DIR} -G '${CMAKE_GENERATOR}' -DCMAKE_INSTALL_PREFIX=${FindConfigurePackage_PREFIX_DIRECTORY} ${FindConfigurePackage_CMAKE_FLAGS}")
                set (FindConfigurePackage_BUILD_WITH_CMAKE_GENERATOR -G ${CMAKE_GENERATOR})
                if (CMAKE_GENERATOR_PLATFORM)
                    list (APPEND FindConfigurePackage_BUILD_WITH_CMAKE_GENERATOR -A ${CMAKE_GENERATOR_PLATFORM})
                endif ()
                list (APPEND FindConfigurePackage_BUILD_WITH_CMAKE_GENERATOR "-DCMAKE_INSTALL_PREFIX=${FindConfigurePackage_PREFIX_DIRECTORY}")

                if (FindConfigurePackage_CMAKE_INHIRT_BUILD_ENV)
                    set (FindConfigurePackage_CMAKE_INHIRT_BUILD_ENV OFF)
                    set (INHERIT_VARS 
                        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
                        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL
                        CMAKE_ASM_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_STATIC_LINKER_FLAGS
                        CMAKE_TOOLCHAIN_FILE CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_AR
                        CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER CMAKE_RANLIB CMAKE_SYSTEM_NAME 
                        CMAKE_SYSROOT CMAKE_SYSROOT_COMPILE # CMAKE_SYSTEM_LIBRARY_PATH # CMAKE_SYSTEM_LIBRARY_PATH ninja里解出的参数不对，原因未知
                        CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES 
                        ANDROID_TOOLCHAIN ANDROID_ABI ANDROID_STL ANDROID_PIE ANDROID_PLATFORM ANDROID_CPP_FEATURES
                        ANDROID_ALLOW_UNDEFINED_SYMBOLS ANDROID_ARM_MODE ANDROID_ARM_NEON ANDROID_DISABLE_NO_EXECUTE ANDROID_DISABLE_RELRO
                        ANDROID_DISABLE_FORMAT_STRING_CHECKS ANDROID_CCACHE
                    )
                
                    foreach (VAR_NAME IN LISTS INHERIT_VARS)
                        # message(STATUS "${VAR_NAME}=${${VAR_NAME}}")
                        if (DEFINED ${VAR_NAME})
                            # message(STATUS "DEFINED ${VAR_NAME}")
                            set(VAR_VALUE "${${VAR_NAME}}")
                            if (VAR_VALUE)
                                # message(STATUS "SET ${VAR_NAME}")
                                list (APPEND FindConfigurePackage_BUILD_WITH_CMAKE_GENERATOR "-D${VAR_NAME}=${VAR_VALUE}")
                            endif ()
                            unset(VAR_VALUE)
                        endif ()
                    endforeach ()

                    unset (INHERIT_VARS)
                endif ()

                execute_process(
                    COMMAND 
                        ${CMAKE_COMMAND} ${BUILD_WITH_CMAKE_PROJECT_DIR} 
                        ${FindConfigurePackage_BUILD_WITH_CMAKE_GENERATOR}
                        ${FindConfigurePackage_CMAKE_FLAGS}
                    WORKING_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY}
                )

                # cmake --build and install
                if(MSVC)
                    if (NOT FindConfigurePackage_MSVC_CONFIGURE)
                        set(FindConfigurePackage_MSVC_CONFIGURE RelWithDebInfo)
                    endif()
                    execute_process(
                        COMMAND ${CMAKE_COMMAND} --build . --target ${FindConfigurePackage_INSTALL_TARGET} --config ${FindConfigurePackage_MSVC_CONFIGURE}
                            ${FindConfigurePackageCMakeBuildMultiJobs}
                        WORKING_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY}
                    )
                else()
                    execute_process(
                        COMMAND ${CMAKE_COMMAND} --build . --target ${FindConfigurePackage_INSTALL_TARGET}
                            ${FindConfigurePackageCMakeBuildMultiJobs}
                        WORKING_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY}
                    )
                endif()

                # adaptor for new cmake module
                set ("${FindConfigurePackage_PACKAGE}_ROOT" ${FindConfigurePackage_PREFIX_DIRECTORY})

            # build using scons
            elseif(FindConfigurePackage_BUILD_WITH_SCONS)
                if(FindConfigurePackage_PROJECT_DIRECTORY)
                    file(RELATIVE_PATH BUILD_WITH_SCONS_PROJECT_DIR ${FindConfigurePackage_BUILD_DIRECTORY} ${FindConfigurePackage_PROJECT_DIRECTORY})
                else()
                    file(RELATIVE_PATH BUILD_WITH_SCONS_PROJECT_DIR ${FindConfigurePackage_BUILD_DIRECTORY} ${FindConfigurePackage_DOWNLOAD_SOURCE_DIR})
                endif()
                if ( NOT BUILD_WITH_SCONS_PROJECT_DIR)
                    set(BUILD_WITH_SCONS_PROJECT_DIR ".")
                endif()

                set(OLD_ENV_PREFIX $ENV{prefix})
                set(ENV{prefix} ${FindConfigurePackage_PREFIX_DIRECTORY})
                message(STATUS "@${FindConfigurePackage_BUILD_DIRECTORY} Run: scons ${FindConfigurePackage_SCONS_FLAGS} ${BUILD_WITH_SCONS_PROJECT_DIR}")
                execute_process(
                    COMMAND "scons" ${FindConfigurePackage_SCONS_FLAGS} ${BUILD_WITH_SCONS_PROJECT_DIR}
                    WORKING_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY}
                )
                set(ENV{prefix} ${OLD_ENV_PREFIX})

            # build using custom commands(such as gyp)
            elseif(FindConfigurePackage_BUILD_WITH_CUSTOM_COMMAND)
                foreach(cmd ${FindConfigurePackage_CUSTOM_BUILD_COMMAND})
                message(STATUS "@${FindConfigurePackage_BUILD_DIRECTORY} Run: ${cmd}")
                    execute_process(
                        COMMAND ${cmd}
                        WORKING_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY}
                    )
                endforeach()

            else()
                message(FATAL_ERROR "build type is required")
            endif()

            # afterbuild commands
            foreach(cmd ${FindConfigurePackage_AFTERBUILD_COMMAND})
                message(STATUS "FindConfigurePackage - Run: ${cmd} @ ${FindConfigurePackage_BUILD_DIRECTORY}")
                execute_process(
                    COMMAND ${cmd}
                    WORKING_DIRECTORY ${FindConfigurePackage_BUILD_DIRECTORY}
                )
            endforeach()

            # reset vars before retry to find package
            foreach (RESET_VAR ${FindConfigurePackage_RESET_FIND_VARS})
                unset (${RESET_VAR} CACHE)
            endforeach()
            find_package(${FindConfigurePackage_PACKAGE})
        endif()
    endif()
    unset (FindConfigurePackage_INSTALL_TARGET)
endmacro(FindConfigurePackage)


