# The default platforms.toml and the emulator presets, relative to this folder.
list(TRANSFORM LIBRARY_RESOURCES PREPEND "../" OUTPUT_VARIABLE TEST_LIBRARY_RESOURCES)

find_package(Qt6 REQUIRED COMPONENTS Test Qml Gui Concurrent Network)

# Every source folder, as the app has them; no two headers share a name.
set(TEST_INCLUDES ../src/core ../src/input ../src/launch ../src/library ../src/metadata ../src/steam ../src/theme
    ../src/backup)

# A test <name> from <name>.cpp and SOURCES, linked to Qt Test and LIBS. PRESETS embeds the default
# platforms.toml and the emulator presets under the app's resource path; OFFSCREEN runs it without a
# display; BENCH builds it without adding it to ctest.
function(lattis_test name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "PRESETS;OFFSCREEN;BENCH" "" "SOURCES;LIBS;DEFINES")
    qt_add_executable(${name} ${name}.cpp ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE ${TEST_INCLUDES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    if(ARG_DEFINES)
        target_compile_definitions(${name} PRIVATE ${ARG_DEFINES})
    endif()
    if(ARG_PRESETS)
        qt_add_resources(${name} "${name}_presets"
            PREFIX "/qt/qml/Launcher/resources"
            BASE ../resources
            FILES ${TEST_LIBRARY_RESOURCES}
        )
    endif()
    if(NOT ARG_BENCH)
        add_test(NAME ${name} COMMAND ${name})
        if(ARG_OFFSCREEN)
            set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_XHR_ALLOW_FILE_READ=1")
        endif()
    endif()
endfunction()

# Source groups, each with what it needs: TOML parsing, Steam, the library's files, the whole library,
# the backups.
set(CORE_SOURCES
    ../src/core/suggestion.h ../src/core/suggestion.cpp
    ../src/core/tomlsection.h ../src/core/tomlsection.cpp
    ../src/library/parsed.h
)
set(STEAM_SOURCES
    ../src/steam/vdf.h ../src/steam/vdf.cpp
    ../src/steam/steamlocate.h ../src/steam/steamlocate.cpp
    ../src/steam/steampresets.h ../src/steam/steampresets.cpp
    ../src/launch/launchoptions.h ../src/launch/launchoptions.cpp
    ../src/steam/steamscan.h ../src/steam/steamscan.cpp
    ../src/steam/binaryvdf.h ../src/steam/binaryvdf.cpp
    ../src/steam/steamshortcuts.h ../src/steam/steamshortcuts.cpp
    ${CORE_SOURCES}
)
set(LIBRARY_FILE_SOURCES
    ../src/library/libraryfiles.h ../src/library/libraryfiles.cpp
    ../src/library/emulators.h ../src/library/emulators.cpp
    ../src/library/romscan.h ../src/library/romscan.cpp
    ../src/library/gamefile.h ../src/library/gamefile.cpp
    ../src/library/gameid.h ../src/library/gameid.cpp
    ${STEAM_SOURCES}
)
set(LIBRARY_SOURCES
    ../src/library/library.h ../src/library/library.cpp
    ../src/steam/steamdownloads.h ../src/steam/steamdownloads.cpp
    ../src/metadata/igdb.h ../src/metadata/igdb.cpp
    ../src/library/uninstall.h ../src/library/uninstall.cpp
    ../src/launch/launchcommand.h ../src/launch/launchcommand.cpp
    ../src/library/librarystore.h ../src/library/librarystore.cpp
    ../src/library/libraryundo.h ../src/library/libraryundo.cpp
    ../src/library/playhistory.h ../src/library/playhistory.cpp
    ../src/library/artimage.h ../src/library/artimage.cpp
    ../src/library/herocache.h ../src/library/herocache.cpp
    ../src/library/libraryversion.h ../src/library/libraryversion.cpp
    ../src/library/librarycache.h ../src/library/librarycache.cpp
    ../src/library/romfoldermatch.h ../src/library/romfoldermatch.cpp
    ../src/library/searchtext.h ../src/library/searchtext.cpp
    ../src/library/listsettings.h ../src/library/listsettings.cpp
    ../src/core/apppaths.h ../src/core/apppaths.cpp
    ../src/core/dateformat.h ../src/core/dateformat.cpp
    ../src/core/tomlpatch.h ../src/core/tomlpatch.cpp
    ../src/steam/steamsync.h ../src/steam/steamsync.cpp
    ../src/steam/steamart.h ../src/steam/steamart.cpp
    ${LIBRARY_FILE_SOURCES}
)
set(BACKUP_SOURCES
    ../src/backup/backupplan.h ../src/backup/backupplan.cpp
    ../src/backup/besiderom.h ../src/backup/besiderom.cpp
    ../src/backup/protonsaves.h ../src/backup/protonsaves.cpp
    ../src/backup/backuparchive.h ../src/backup/backuparchive.cpp
    ../src/metadata/credentials.h ../src/metadata/credentials.cpp
    ../src/core/tomlpatch.h ../src/core/tomlpatch.cpp
    ${LIBRARY_FILE_SOURCES}
)
set(LIBRARY_LIBS Qt6::Qml Qt6::Gui Qt6::Concurrent tomlplusplus::tomlplusplus)
set(STEAM_DATA "STEAM_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/data/steam\"")
set(DEMO_DATA "DEMO_DATA_DIR=\"${CMAKE_SOURCE_DIR}/tools/demo-data\"")
set(DOCS "DOCS_DIR=\"${CMAKE_SOURCE_DIR}/docs\"")
set(VDF_DATA "VDF_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/data/vdf\"")
set(HISTORY_DATA "HISTORY_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/data/history\"")

# Core
lattis_test(tomlpatch_test
    SOURCES ../src/core/tomlpatch.h ../src/core/tomlpatch.cpp
    LIBS tomlplusplus::tomlplusplus)
lattis_test(general_test OFFSCREEN
    SOURCES ../src/core/general.h ../src/core/general.cpp ../src/core/dateformat.h ../src/core/dateformat.cpp
            ../src/core/generalsettings.h ../src/core/generalsettings.cpp
            ../src/core/gamepagesettings.h ../src/core/gamepagesettings.cpp
            ../src/core/gamepagemigration.h ../src/core/gamepagemigration.cpp
            ../src/core/autostart.h ../src/core/autostart.cpp
            ../src/core/tomlpatch.h ../src/core/tomlpatch.cpp ${CORE_SOURCES}
    LIBS Qt6::Qml Qt6::Gui tomlplusplus::tomlplusplus)
lattis_test(windowstate_test OFFSCREEN
    SOURCES ../src/core/windowstate.h ../src/core/windowstate.cpp
    LIBS Qt6::Qml Qt6::Gui tomlplusplus::tomlplusplus)
lattis_test(autostart_test OFFSCREEN
    SOURCES ../src/core/autostart.h ../src/core/autostart.cpp
    LIBS Qt6::Gui)
lattis_test(singleinstance_test
    SOURCES ../src/core/singleinstance.h ../src/core/singleinstance.cpp
    LIBS Qt6::Network)
lattis_test(oldname_test
    SOURCES ../src/core/oldname.h ../src/core/oldname.cpp ../src/core/apppaths.h ../src/core/apppaths.cpp)

# Theme and input
lattis_test(theme_test OFFSCREEN
    SOURCES ../src/theme/theme.h ../src/theme/theme.cpp
            ../src/theme/themeschema.h ../src/theme/themeschema.cpp
            ../src/theme/schemamodel.h ../src/theme/schemamodel.cpp
            ../src/core/gamepagemigration.h ../src/core/gamepagemigration.cpp
            ../src/core/tomlpatch.h ../src/core/tomlpatch.cpp ${CORE_SOURCES}
            ../src/library/uninstall.h ../src/library/uninstall.cpp
    LIBS Qt6::Qml Qt6::Gui tomlplusplus::tomlplusplus)
# The built-in themes under the app's resource path, so Theme's isBuiltIn() and extends resolve the
# same as in lattis.
qt_add_resources(theme_test "theme_test_builtin_themes"
    PREFIX "/qt/qml/Launcher/resources"
    BASE ../resources
    FILES
        ../resources/themes/Catppuccin.toml
        ../resources/themes/Ember.toml
        ../resources/themes/Mythos.toml
        ../resources/themes/Moss.toml
        ../resources/themes/Steam.toml
        ../resources/themes/Gruvbox.toml
        "../resources/themes/Rose Pine.toml"
        ../resources/themes/Nord.toml
        ../resources/themes/colors/Catppuccin.toml
        ../resources/themes/colors/Ember.toml
        ../resources/themes/colors/Mythos.toml
        ../resources/themes/colors/Moss.toml
        ../resources/themes/colors/Steam.toml
        ../resources/themes/colors/Gruvbox.toml
        "../resources/themes/colors/Rose Pine.toml"
        ../resources/themes/colors/Nord.toml
        ../resources/themes/styles/Round.toml
        ../resources/themes/styles/Sharp.toml
        ../resources/themes/styles/Soft.toml
        ../resources/themes/styles/Steam.toml
)
lattis_test(actions_test OFFSCREEN
    SOURCES ../src/input/actions.h ../src/input/actions.cpp
            ../src/input/actionschema.h ../src/input/actionschema.cpp
            ../src/core/tomlpatch.h ../src/core/tomlpatch.cpp ${CORE_SOURCES}
    LIBS Qt6::Qml Qt6::Gui tomlplusplus::tomlplusplus)

# Library
lattis_test(herocache_test SOURCES ../src/library/herocache.h ../src/library/herocache.cpp
    ../src/library/artimage.h ../src/library/artimage.cpp LIBS Qt6::Gui)
lattis_test(gameid_test SOURCES ../src/library/gameid.h ../src/library/gameid.cpp)
lattis_test(romfoldermatch_test SOURCES ../src/library/romfoldermatch.h ../src/library/romfoldermatch.cpp)
lattis_test(romscan_test PRESETS
    SOURCES ${LIBRARY_FILE_SOURCES}
    LIBS tomlplusplus::tomlplusplus)
lattis_test(emulators_test PRESETS
    SOURCES ${LIBRARY_FILE_SOURCES}
    LIBS tomlplusplus::tomlplusplus
    DEFINES ${DOCS})
lattis_test(libraryformat_test PRESETS
    SOURCES ../src/library/game.h ${LIBRARY_FILE_SOURCES}
    LIBS tomlplusplus::tomlplusplus
    DEFINES ${DEMO_DATA} ${DOCS})
lattis_test(librarycache_test OFFSCREEN
    SOURCES ../src/library/librarycache.h ../src/library/librarycache.cpp
            ../src/core/apppaths.h ../src/core/apppaths.cpp
            ../src/steam/steamsync.h ../src/steam/steamsync.cpp
            ../src/steam/steamart.h ../src/steam/steamart.cpp
            ${LIBRARY_FILE_SOURCES}
    LIBS Qt6::Gui tomlplusplus::tomlplusplus)
# The whole library, compiled once for the tests below that use it.
qt_add_library(library_objects OBJECT ${LIBRARY_SOURCES})
target_include_directories(library_objects PUBLIC ${TEST_INCLUDES})
target_link_libraries(library_objects PUBLIC ${LIBRARY_LIBS})

# One test per area, on the fixture in librarytestbase.h.
foreach(area list edit tags rom emulators platforms version steamimport steamsync programs uninstall undo hero art)
    lattis_test(library_${area}_test PRESETS
        SOURCES librarytestbase.h librarytestbase.cpp
        LIBS library_objects
        DEFINES ${DEMO_DATA} ${STEAM_DATA})
endforeach()
lattis_test(play_history_test PRESETS
    SOURCES librarytestbase.h librarytestbase.cpp
    LIBS library_objects
    DEFINES ${DEMO_DATA} ${STEAM_DATA} ${HISTORY_DATA})
# Timings for a 1000-game library; run by hand (build/tests/library_bench), not by ctest.
lattis_test(library_bench BENCH PRESETS LIBS library_objects)

# Launching
lattis_test(launch_test PRESETS
    SOURCES ../src/launch/launchsettings.h ../src/launch/launchsettings.cpp
            ../src/core/restart.h ../src/core/restart.cpp
            ../src/launch/detachedprocess.h ../src/launch/detachedprocess.cpp
            ../src/launch/gamerunner.h ../src/launch/gamerunner.cpp
    LIBS library_objects
    DEFINES ${DEMO_DATA} ${STEAM_DATA})

# Steam
lattis_test(vdf_test
    SOURCES ../src/steam/vdf.h ../src/steam/vdf.cpp
    DEFINES ${VDF_DATA})
lattis_test(steam_test OFFSCREEN
    SOURCES ../src/steam/steamsync.h ../src/steam/steamsync.cpp
            ../src/steam/steamart.h ../src/steam/steamart.cpp
            ${STEAM_SOURCES}
    LIBS Qt6::Gui tomlplusplus::tomlplusplus
    DEFINES ${STEAM_DATA})
lattis_test(shortcuts_test
    SOURCES binaryvdfbuilder.h
            ../src/steam/binaryvdf.h ../src/steam/binaryvdf.cpp
            ../src/steam/steamshortcuts.h ../src/steam/steamshortcuts.cpp
    DEFINES ${STEAM_DATA} ${VDF_DATA})
lattis_test(steampresets_test
    SOURCES ../src/steam/steampresets.h ../src/steam/steampresets.cpp
            ../src/launch/launchoptions.h ../src/launch/launchoptions.cpp
            ${CORE_SOURCES}
    LIBS tomlplusplus::tomlplusplus)

# Metadata
lattis_test(metadata_test
    SOURCES fakehttpserver.h
            ../src/metadata/credentials.h ../src/metadata/credentials.cpp
            ../src/metadata/metadatahttp.h ../src/metadata/metadatahttp.cpp
            ../src/metadata/igdb.h ../src/metadata/igdb.cpp
            ../src/metadata/steamgriddb.h ../src/metadata/steamgriddb.cpp
            ../src/metadata/metadataclient.h ../src/metadata/metadataclient.cpp
            ../src/metadata/idlistmodel.h ../src/metadata/idlistmodel.cpp
            ../src/metadata/bulklookup.h ../src/metadata/bulklookup.cpp
            ../src/metadata/scanlookup.h ../src/metadata/scanlookup.cpp
            ../src/metadata/metadatasettings.h ../src/metadata/metadatasettings.cpp
            ../src/library/lookupgame.h
            ../src/core/apppaths.h ../src/core/apppaths.cpp
            ../src/library/searchtext.h ../src/library/searchtext.cpp
            ../src/library/gameid.h ../src/library/gameid.cpp
            ../src/library/gamefile.h ../src/library/gamefile.cpp
            ../src/core/tomlpatch.h ../src/core/tomlpatch.cpp
            ${CORE_SOURCES}
    LIBS Qt6::Qml Qt6::Gui Qt6::Network tomlplusplus::tomlplusplus
    DEFINES "METADATA_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/data/metadata\"")

# Backups
lattis_test(archive_test
    SOURCES ../src/backup/backuparchive.h ../src/backup/backuparchive.cpp
    LIBS LibArchive::LibArchive)
lattis_test(besiderom_test SOURCES ../src/backup/besiderom.h ../src/backup/besiderom.cpp)
lattis_test(protonsaves_test
    SOURCES ../src/backup/protonsaves.h ../src/backup/protonsaves.cpp
            ../src/backup/backuparchive.h ../src/backup/backuparchive.cpp
    LIBS LibArchive::LibArchive
    DEFINES ${STEAM_DATA})
lattis_test(movedaside_test
    SOURCES ../src/backup/movedaside.h ../src/backup/movedaside.cpp
            ../src/backup/backuparchive.h ../src/backup/backuparchive.cpp
    LIBS LibArchive::LibArchive tomlplusplus::tomlplusplus)
lattis_test(backupplan_test
    SOURCES ${BACKUP_SOURCES}
    LIBS LibArchive::LibArchive tomlplusplus::tomlplusplus
    DEFINES ${STEAM_DATA})
lattis_test(backup_test
    SOURCES ../src/backup/backup.h ../src/backup/backup.cpp ${BACKUP_SOURCES}
    LIBS Qt6::Concurrent LibArchive::LibArchive tomlplusplus::tomlplusplus
    DEFINES ${STEAM_DATA})
lattis_test(restore_test
    SOURCES ../src/backup/restore.h ../src/backup/restore.cpp
            ../src/backup/movedaside.h ../src/backup/movedaside.cpp
            ../src/backup/backups.h ../src/backup/backups.cpp
            ../src/core/dateformat.h ../src/core/dateformat.cpp
            ../src/backup/backup.h ../src/backup/backup.cpp
            ${BACKUP_SOURCES}
    LIBS Qt6::Qml Qt6::Concurrent LibArchive::LibArchive tomlplusplus::tomlplusplus
    DEFINES ${STEAM_DATA})

# QML and JavaScript tests (tests/qml/tst_*.qml), run from the source folder.
find_package(Qt6 REQUIRED COMPONENTS QuickTest)
qt_add_executable(qmltest qmltest.cpp)
target_compile_definitions(qmltest PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
target_link_libraries(qmltest PRIVATE Qt6::QuickTest Qt6::Quick)

add_test(NAME qmltest COMMAND qmltest)
set_tests_properties(qmltest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_XHR_ALLOW_FILE_READ=1")
