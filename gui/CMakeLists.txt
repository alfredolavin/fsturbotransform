# fsturbotransform-gui: the QML front end (Qt 6 Quick, KDE's Kirigami and org.kde.desktop style
# at run time). It compiles the same header-only engine as the command line.

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(fsturbotransform-gui
    main.cpp
    change_model.cpp
    change_model.hpp
    console_format.cpp
    console_format.hpp
    icon_provider.hpp
    regex_highlighter.cpp
    regex_highlighter.hpp
    transform_controller.cpp
    transform_controller.hpp
)

set(FSTURBO_QML_FILES
    qml/Main.qml
    qml/Card.qml
    qml/ChangeDelegate.qml
    qml/FileList.qml
    qml/GradientProgress.qml
    qml/IconButton.qml
    qml/IconCheckBox.qml
    qml/IconComboBox.qml
    qml/IconTextField.qml
    qml/PatternList.qml
    qml/PropertyIcon.qml
    qml/RegexEditor.qml
    qml/StatTile.qml
    qml/AuthorSignature.qml
    qml/ConsoleFormatDialog.qml
    qml/ConsoleView.qml
)
# Served from the module's root (qrc:/qt/qml/FsTurbo/Main.qml), next to icons/.
foreach(qml_file IN LISTS FSTURBO_QML_FILES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

file(GLOB FSTURBO_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS icons/*.svg icons/*.png)

# The shared modules of ~/.insync/trabajo/proyectos/plasmoids/shared, linked into qml/ by its sync.sh
# (`sync.sh fsturbotransform`, then `tools/gen_icons.py gui/qml` for the Tango icons): plain resources,
# not module types, served without the qml/ prefix so their relative imports ("../controls") work.
file(GLOB FSTURBO_SHARED RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
    qml/controls/*.qml qml/controls/*.js qml/controls/icons/*.svg qml/controls/icons/bundled.js
    qml/common/*.qml qml/common/*.js
    qml/colorspec/*.qml qml/colorspec/*.js
    qml/gradientpicker/code/*.js
    qml/gpu/*.qml qml/gpu/*.js qml/gpu/qsb/*.qsb)
foreach(f ${FSTURBO_SHARED})
    string(REGEX REPLACE "^qml/" "" alias "${f}")
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS "${alias}")
endforeach()

qt_add_qml_module(fsturbotransform-gui
    URI FsTurbo
    VERSION 1.0
    QML_FILES ${FSTURBO_QML_FILES}
    RESOURCES ${FSTURBO_ICONS} ${FSTURBO_SHARED}
)

# Qt is linked dynamically and loads the QML plugins at run time: nothing to link statically
# (the scan only warns that the distribution's plugin targets are not defined).
set_target_properties(fsturbotransform-gui PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)

target_include_directories(fsturbotransform-gui PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
# QT_NO_KEYWORDS: the engine has functions named emit (Qt's `emit` macro would rewrite them).
target_compile_definitions(fsturbotransform-gui PRIVATE QT_NO_KEYWORDS FSTURBO_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib/fsturbotransform")
# No -ffast-math (the engine's NaN checks) and no static libstdc++ (Qt links it dynamically).
target_compile_options(fsturbotransform-gui PRIVATE -O2 -Wall -Wextra)
# #embed is not tracked by every dependency scanner: rebuild when the font changes.
set_source_files_properties(main.cpp PROPERTIES OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/src/FiraCode-Regular.ttf")
target_link_libraries(fsturbotransform-gui PRIVATE
    Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Svg KF6::I18n KF6::I18nQml KF6::ColorScheme KF6::ConfigCore
    PkgConfig::PCRE2 ${CMAKE_DL_LIBS})
if(TARGET fsturbo_js)
    add_dependencies(fsturbotransform-gui fsturbo_js)
endif()

install(TARGETS fsturbotransform-gui RUNTIME DESTINATION bin)
install(FILES fsturbotransform-gui.desktop DESTINATION share/applications)
install(FILES icons/app.png DESTINATION share/icons/hicolor/256x256/apps RENAME fsturbotransform.png)

# Integration test of TransformController (the engine on a worker thread, no window)
add_executable(gui_controller_test ${PROJECT_SOURCE_DIR}/tests/gui_controller_test.cpp transform_controller.cpp transform_controller.hpp change_model.cpp change_model.hpp)
set_target_properties(gui_controller_test PROPERTIES AUTOMOC ON)
target_include_directories(gui_controller_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(gui_controller_test PRIVATE QT_NO_KEYWORDS)
target_link_libraries(gui_controller_test PRIVATE Qt6::Core Qt6::Qml PkgConfig::PCRE2 ${CMAKE_DL_LIBS})
add_test(NAME gui_controller COMMAND gui_controller_test)
