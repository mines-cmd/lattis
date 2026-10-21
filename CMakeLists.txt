cmake_minimum_required(VERSION 3.21)

project(Lattis VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Concurrent Network ShaderTools)
find_package(tomlplusplus 3 REQUIRED)
# Backups (.tar.zst); zstd threads need 3.6.
find_package(LibArchive 3.6 REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

# The app icon at the hicolor sizes (tools/logo/icons.js): the window icon, the logo the
# rail and Welcome show, and the installed icon.
set(APP_ICON_SIZES 16 24 32 48 64 128 256 512)
set(APP_ICONS)
foreach(size IN LISTS APP_ICON_SIZES)
    list(APPEND APP_ICONS resources/logo/icons/lattis-${size}.png)
endforeach()

# The setup guide's cover wall: Steam's library covers by app ID, in the wall's order
# (qml/setup/SetupCoverWall.qml lists the same IDs). Swapping the covers means replacing
# these files and that list.
set(COVER_IDS
    271590 1245620 1145360 367520 504230 292030 620 220 1091500 1174180 413150 105600 374320 814380 782330
    1086940 632470 1145350 753640 1092790 588650 268910 1057090 264710 1850570 1151640 1593500 1817070
    1687950 582010 2050650 524220 870780 1649240 1328670 976730 584400 250760 1627720 1888160 1244090
    553420 2138710 1817230)
set(COVER_IMAGES)
foreach(id IN LISTS COVER_IDS)
    list(APPEND COVER_IMAGES resources/covers/${id}.webp)
endforeach()

# The default platforms.toml and tags.toml, and the built-in emulator presets (see
# docs/code/library.md); the tests load them too.
set(LIBRARY_RESOURCES
    resources/platforms.toml
    resources/tags.toml
    resources/emulators/azahar.toml
    resources/emulators/cemu.toml
    resources/emulators/dolphin.toml
    resources/emulators/duckstation.toml
    resources/emulators/melonds.toml
    resources/emulators/mgba.toml
    resources/emulators/pcsx2.toml
    resources/emulators/ppsspp.toml
    resources/emulators/rmg.toml
    resources/emulators/rpcs3.toml
    resources/emulators/ryubing.toml
    resources/emulators/shadps4.toml
)

qt_add_executable(lattis src/main.cpp)

set_source_files_properties(qml/components/Icons.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/components/Motion.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# licensed/ holds icons that may ship inside the app but not in the public repo; without it,
# the stand-ins in qml/components/LicensedIcons.qml build.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/licensed/LicensedIcons.qml)
    set(licensed_icons licensed/LicensedIcons.qml)
else()
    set(licensed_icons qml/components/LicensedIcons.qml)
endif()
set_source_files_properties(${licensed_icons} PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS qml/components/LicensedIcons.qml)

qt_add_qml_module(lattis
    URI Launcher
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        # Generic building blocks
        qml/components/ActionShortcuts.qml
        qml/components/AppLogo.qml
        qml/components/ArtPlaceholder.qml
        qml/components/FadeImage.qml
        qml/components/GameIcon.qml
        qml/components/Icon.qml
        qml/components/Stars.qml
        qml/components/IconButton.qml
        qml/components/Args.js
        qml/components/Icons.qml
        qml/components/Motion.qml
        qml/components/MotionColor.qml
        qml/components/MotionNumber.qml
        qml/components/DropdownEnter.qml
        qml/components/DropdownExit.qml
        qml/components/MotionPop.qml
        qml/components/PopLeave.qml
        qml/components/BoxBump.qml
        qml/components/MarkPop.qml
        qml/components/MotionSpring.qml
        qml/components/PopupEnter.qml
        qml/components/PopupExit.qml
        qml/components/MotionPopup.qml
        qml/components/MenuPopEnter.qml
        qml/components/MenuPopExit.qml
        qml/components/Unfold.qml
        qml/components/LogoScrim.qml
        qml/components/Scrim.qml
        qml/components/PopoverBackground.qml
        qml/components/ScreenSwap.qml
        ${licensed_icons}
        qml/components/LaunchButton.qml
        qml/components/NoticeBar.qml
        qml/components/CaptionLabel.qml
        qml/components/StatChip.qml
        qml/components/Surface.qml
        qml/components/SegmentedToggle.qml
        qml/components/TextLink.qml
        qml/components/TagChip.qml
        qml/components/FilterChip.qml
        qml/components/KeyedListModel.qml
        qml/components/TagPalettePopup.qml
        qml/components/Toast.qml
        qml/components/AboutContent.qml
        qml/components/TopToolbar.qml
        qml/components/WindowControls.qml
        qml/components/WindowFrame.qml
        qml/components/Wordmark.qml
        # Parts of the main window
        qml/views/Backdrop.qml
        qml/views/GameDetails.qml
        qml/views/ImportNotice.qml
        qml/views/DownloadBar.qml
        qml/views/DownloadPill.qml
        qml/views/GameEditor.qml
        qml/views/SetupGuide.qml
        qml/setup/SetupBackdrop.qml
        qml/setup/SetupCoverWall.qml
        qml/setup/SetupWelcome.qml
        qml/setup/SetupSteam.qml
        qml/setup/SetupEmulators.qml
        qml/setup/SetupMetadata.qml
        qml/setup/SetupLook.qml
        qml/setup/SetupDone.qml
        qml/setup/FindRomFoldersDialog.qml
        qml/views/SteamLaunchPrompt.qml
        qml/views/SteamUndoPrompt.qml
        qml/views/ForceClosePrompt.qml
        qml/views/PlayHistoryPopup.qml
        qml/views/StatsPage.qml
        qml/views/StatsPeriodPicker.qml
        qml/views/SteamUninstallPrompt.qml
        qml/views/ProtonUninstall.qml
        qml/views/LookupRow.qml
        qml/views/ArtBrowser.qml
        qml/views/DetailedLookup.qml
        qml/views/BulkLookup.qml
        qml/views/BulkOption.qml
        qml/views/BulkTagsPopup.qml
        qml/views/ArtEditor.qml
        qml/views/ArtThumb.qml
        qml/views/ImageFileDialog.qml
        qml/views/TagsEditor.qml
        qml/views/GameList.qml
        qml/views/GameMenu.qml
        qml/views/NewTagPrompt.qml
        qml/views/LogoAdjuster.qml
        qml/views/LogoMenu.qml
        qml/views/AboutDialog.qml
        qml/views/AppMenu.qml
        qml/views/PlatformMenu.qml
        qml/views/RailMenu.qml
        qml/views/FilterPanel.qml
        qml/views/Hero.qml
        qml/views/InfoPanel.qml
        qml/views/RatingEditor.qml
        qml/views/DateEditor.qml
        qml/views/StepField.qml
        qml/views/RomFileDialog.qml
        qml/views/Rail.qml
        qml/views/RailButton.qml
        # Settings window
        qml/settings/GeneralPage.qml
        qml/settings/SettingsWindow.qml
        qml/settings/AppearancePage.qml
        qml/settings/DeleteThemeDialog.qml
        qml/settings/RenameThemeDialog.qml
        qml/settings/EditDescriptionDialog.qml
        qml/settings/StyleScene.qml
        qml/settings/ThemeSettingsList.qml
        qml/settings/ThemePickColumn.qml
        qml/settings/NewThemeDialog.qml
        qml/settings/ResetThemeDialog.qml
        qml/settings/CustomizePage.qml
        qml/settings/KeyboardPage.qml
        qml/settings/ShortcutRow.qml
        qml/settings/KeyChip.qml
        qml/settings/EmulatorsPage.qml
        qml/settings/EmulatorRow.qml
        qml/settings/ProgramDialog.qml
        qml/settings/LibraryPage.qml
        qml/settings/MetadataPage.qml
        qml/settings/SteamPage.qml
        qml/settings/ShortcutPicker.qml
        qml/settings/SteamAccountBox.qml
        qml/settings/BackupDialog.qml
        qml/settings/ImportDialog.qml
        qml/settings/KeyRow.qml
        qml/settings/ConnectionRow.qml
        qml/settings/HelpSectionHeader.qml
        qml/settings/SettingsCheckBox.qml
        qml/settings/SettingsRadioButton.qml
        qml/settings/PathField.qml
        qml/settings/PlatformsPage.qml
        qml/settings/PlatformRow.qml
        qml/settings/Section.qml
        qml/settings/SectionTitle.qml
        qml/settings/ListRow.qml
        qml/settings/ListAction.qml
        qml/settings/ListButton.qml
        qml/settings/RomFolderDialog.qml
        qml/settings/ProblemsButton.qml
        qml/settings/SessionChangesPopup.qml
        qml/settings/SettingRow.qml
        qml/settings/SettingsButton.qml
        qml/settings/SettingsCard.qml
        qml/settings/SettingsNote.qml
        qml/settings/SettingsComboBox.qml
        qml/settings/SettingsTextField.qml
        qml/settings/SettingsSearchField.qml
        qml/settings/SettingsSlider.qml
        qml/settings/SettingsMenu.qml
        qml/settings/SettingsMenuItem.qml
        qml/settings/SettingsMenuSlider.qml
        qml/settings/SettingsMenuSeparator.qml
        qml/settings/TextContextMenu.qml
        qml/settings/SettingsSwitch.qml
        qml/settings/SectionJump.qml
        qml/settings/SettingsScrollBar.qml
        qml/settings/editors/ColorEditor.qml
        qml/settings/editors/NumberEditor.qml
        qml/settings/editors/BoolEditor.qml
        qml/settings/editors/StringEditor.qml
        qml/settings/editors/EnumEditor.qml
        qml/settings/editors/StringListEditor.qml
        qml/settings/editors/ColorListEditor.qml
        qml/settings/editors/ColorPickerPopup.qml
    SOURCES
        src/core/apppaths.h src/core/apppaths.cpp
        src/core/dateformat.h src/core/dateformat.cpp
        src/core/autostart.h src/core/autostart.cpp
        src/core/general.h src/core/general.cpp
        src/core/generalsettings.h src/core/generalsettings.cpp
        src/core/oldname.h src/core/oldname.cpp
        src/core/restart.h src/core/restart.cpp
        src/core/singleinstance.h src/core/singleinstance.cpp
        src/core/suggestion.h src/core/suggestion.cpp
        src/core/tomlsection.h src/core/tomlsection.cpp
        src/core/tomlpatch.h src/core/tomlpatch.cpp
        src/core/windowstate.h src/core/windowstate.cpp
        src/input/actionschema.h src/input/actionschema.cpp
        src/input/actions.h src/input/actions.cpp
        src/library/artimage.h src/library/artimage.cpp
        src/library/herocache.h src/library/herocache.cpp
        src/library/game.h
        src/library/lookupgame.h
        src/library/gameid.h src/library/gameid.cpp
        src/library/gamefile.h src/library/gamefile.cpp
        src/library/libraryfiles.h src/library/libraryfiles.cpp
        src/library/librarycache.h src/library/librarycache.cpp
        src/library/librarystore.h src/library/librarystore.cpp
        src/library/libraryundo.h src/library/libraryundo.cpp
        src/library/libraryversion.h src/library/libraryversion.cpp
        src/library/emulators.h src/library/emulators.cpp
        src/library/parsed.h
        src/library/romfoldermatch.h src/library/romfoldermatch.cpp
        src/library/romscan.h src/library/romscan.cpp
        src/library/searchtext.h src/library/searchtext.cpp
        src/library/listsettings.h src/library/listsettings.cpp
        src/library/library.h src/library/library.cpp
        src/library/playhistory.h src/library/playhistory.cpp
        src/library/uninstall.h src/library/uninstall.cpp
        src/launch/launchsettings.h src/launch/launchsettings.cpp
        src/launch/launchcommand.h src/launch/launchcommand.cpp
        src/launch/launchoptions.h src/launch/launchoptions.cpp
        src/launch/detachedprocess.h src/launch/detachedprocess.cpp
        src/launch/gamerunner.h src/launch/gamerunner.cpp
        src/metadata/credentials.h src/metadata/credentials.cpp
        src/metadata/metadatahttp.h src/metadata/metadatahttp.cpp
        src/metadata/igdb.h src/metadata/igdb.cpp
        src/metadata/steamgriddb.h src/metadata/steamgriddb.cpp
        src/metadata/metadataclient.h src/metadata/metadataclient.cpp
        src/metadata/thumbnailcache.h
        src/metadata/idlistmodel.h src/metadata/idlistmodel.cpp
        src/metadata/bulklookup.h src/metadata/bulklookup.cpp
        src/metadata/scanlookup.h src/metadata/scanlookup.cpp
        src/metadata/metadatasettings.h src/metadata/metadatasettings.cpp
        src/steam/vdf.h src/steam/vdf.cpp
        src/steam/steamlocate.h src/steam/steamlocate.cpp
        src/steam/steampresets.h src/steam/steampresets.cpp
        src/steam/steamscan.h src/steam/steamscan.cpp
        src/steam/steamdownloads.h src/steam/steamdownloads.cpp
        src/steam/binaryvdf.h src/steam/binaryvdf.cpp
        src/steam/steamshortcuts.h src/steam/steamshortcuts.cpp
        src/steam/steamsync.h src/steam/steamsync.cpp
        src/steam/steamart.h src/steam/steamart.cpp
        src/theme/theme.h src/theme/theme.cpp
        src/theme/themeschema.h src/theme/themeschema.cpp
        src/theme/schemamodel.h src/theme/schemamodel.cpp
        src/backup/backuparchive.h src/backup/backuparchive.cpp
        src/backup/besiderom.h src/backup/besiderom.cpp
        src/backup/protonsaves.h src/backup/protonsaves.cpp
        src/backup/backupplan.h src/backup/backupplan.cpp
        src/backup/backup.h src/backup/backup.cpp
        src/backup/backups.h src/backup/backups.cpp
        src/backup/restore.h src/backup/restore.cpp
        src/backup/movedaside.h src/backup/movedaside.cpp
    RESOURCES
        resources/themes/Catppuccin.toml
        resources/themes/Ember.toml
        resources/themes/Mythos.toml
        resources/themes/Moss.toml
        resources/themes/Steam.toml
        resources/themes/Gruvbox.toml
        "resources/themes/Rose Pine.toml"
        resources/themes/Nord.toml
        resources/themes/colors/Catppuccin.toml
        resources/themes/colors/Ember.toml
        resources/themes/colors/Mythos.toml
        resources/themes/colors/Moss.toml
        resources/themes/colors/Steam.toml
        resources/themes/colors/Gruvbox.toml
        "resources/themes/colors/Rose Pine.toml"
        resources/themes/colors/Nord.toml
        resources/themes/styles/Round.toml
        resources/themes/styles/Sharp.toml
        resources/themes/styles/Soft.toml
        resources/themes/styles/Steam.toml
        ${APP_ICONS}
        ${COVER_IMAGES}
        resources/fonts/InterVariable.ttf
        resources/setup/welcome.webp
        resources/logo/lattis-ember-simple.svg
        resources/logo/lattis-ember-horizontal.png
        resources/logo/lattis-ember-horizontal-light.png
        ${LIBRARY_RESOURCES}
)

qt_add_shaders(lattis "shaders"
    PREFIX "/qt/qml/Launcher"
    FILES
        qml/shaders/backdropfade.frag
        qml/shaders/coverround.frag
        qml/shaders/coverwall.frag
)

# The subfolders are listed too: Qt's generated type registration includes QML_ELEMENT
# headers by file name only.
target_include_directories(lattis PRIVATE src src/core src/input src/launch src/library src/metadata src/steam src/theme src/backup)
# Shown in About.
target_compile_definitions(lattis PRIVATE LATTIS_VERSION="${PROJECT_VERSION}")

target_link_libraries(lattis PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Network
    tomlplusplus::tomlplusplus
    LibArchive::LibArchive
)

enable_testing()
add_subdirectory(tests)

# For packaging (tools/appimage.sh): cmake --install puts these under a prefix.
include(GNUInstallDirs)
install(TARGETS lattis RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/lattis.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/lattis.appdata.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/lattis)
install(FILES resources/fonts/OFL.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/lattis RENAME OFL-Inter.txt)
foreach(size IN LISTS APP_ICON_SIZES)
    install(FILES resources/logo/icons/lattis-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps RENAME lattis.png)
endforeach()
