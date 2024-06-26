#=============================================================================
#  MuseScore
#  Music Composition & Notation
#
#  Copyright (C) 2011 Werner Schweer
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2
#  as published by the Free Software Foundation and appearing in
#  the file LICENSE.GPL
#=============================================================================

add_executable(
      ${TARGET}
      ${ui_headers}
      ${mocs}
      ${TARGET}.cpp
      )

if (MTEST_LINK_MSCOREAPP)
      target_link_libraries(${TARGET} testutils_mscoreapp mscoreapp)
else (MTEST_LINK_MSCOREAPP)
      target_link_libraries(${TARGET} testutils)
endif (MTEST_LINK_MSCOREAPP)

if (MSVC)
      set(_all_h_file "${PROJECT_SOURCE_DIR}/all.h")
      target_sources(${TARGET} PRIVATE
            ${_all_h_file}
            ${PCH}
            )
      target_link_libraries(
            ${TARGET}
            ${QT_QTTEST_LIBRARY}
            testResources
            libmscore
            audio
            qzip
            zlibstat
            )
else (MSVC)
      target_link_libraries(
            ${TARGET}
            ${QT_QTTEST_LIBRARY}
            testResources
            libmscore
            audio
            qzip
            z
            )
endif (MSVC)

if (MSVC)
      target_link_libraries(
            ${TARGET}
            ${QT_LIBRARIES}
            zlibstat
            )
else (MSVC)
      target_link_libraries(
            ${TARGET}
            ${QT_LIBRARIES}
            z
            )
endif (MSVC)

if (USE_SYSTEM_FREETYPE)
      target_link_libraries(${TARGET} freetype)
else (USE_SYSTEM_FREETYPE)
      target_link_libraries(${TARGET} mscore_freetype)
endif (USE_SYSTEM_FREETYPE)

if (NOT MINGW AND NOT APPLE AND NOT MSVC)
   target_link_libraries(${TARGET}
      dl
      pthread)
endif (NOT MINGW AND NOT APPLE AND NOT MSVC)

if (APPLE)
target_link_libraries(${TARGET}
      dl
      pthread
      )
set_target_properties (
      ${TARGET}
      PROPERTIES
      AUTOMOC true
      COMPILE_FLAGS "-include all.h -D QT_GUI_LIB -D TESTROOT=\\\"${PROJECT_SOURCE_DIR}\\\" -g -Wall -Wextra"
      LINK_FLAGS    "-g -stdlib=libc++"
      )
else(APPLE)
      if (MSVC)
            set_target_properties (
                  ${TARGET}
                  PROPERTIES
                  AUTOMOC true
                  COMPILE_FLAGS "/Zi ${PCH_INCLUDE} /D QT_GUI_LIB  /D TESTROOT=\\\"${PROJECT_SOURCE_DIR}\\\""
                  )
      else (MSVC)
            set_target_properties (
                  ${TARGET}
                  PROPERTIES
                  AUTOMOC true
                  COMPILE_FLAGS "-include all.h -D QT_GUI_LIB -D TESTROOT=\\\"${PROJECT_SOURCE_DIR}\\\" -g -Wall -Wextra"
                  LINK_FLAGS    "-g"
                  )
      endif (MSVC)
endif(APPLE)

if (APPLE AND (CMAKE_VERSION VERSION_LESS "3.5.0"))
set_target_properties (
      ${TARGET}
      PROPERTIES
      COMPILE_FLAGS "-include all.h -D QT_GUI_LIB -D TESTROOT=\\\\\"${PROJECT_SOURCE_DIR}\\\\\" -g -Wall -Wextra"
      )
endif (APPLE AND (CMAKE_VERSION VERSION_LESS "3.5.0"))

add_test(${TARGET} ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}  -xunitxml -o result.xml)

include_guard(DIRECTORY)

if (MSVC)
install(FILES
      ${PROJECT_SOURCE_DIR}/share/styles/chords_std.xml
      ${PROJECT_SOURCE_DIR}/share/styles/chords_jazz.xml
      ${PROJECT_SOURCE_DIR}/share/styles/chords.xml
      ${PROJECT_SOURCE_DIR}/share/styles/stdchords.xml
      ${PROJECT_SOURCE_DIR}/share/styles/jazzchords.xml
      ${PROJECT_SOURCE_DIR}/share/styles/cchords_muse.xml
      ${PROJECT_SOURCE_DIR}/share/styles/cchords_nrb.xml
      ${PROJECT_SOURCE_DIR}/share/styles/cchords_rb.xml
      ${PROJECT_SOURCE_DIR}/share/styles/cchords_sym.xml
      DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/styles
      )
endif (MSVC)
