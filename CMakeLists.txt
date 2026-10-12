cmake_minimum_required(VERSION 3.21)

project(AirMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Qml
    Quick
    QuickControls2
)

qt_standard_project_setup(REQUIRES 6.5)

set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(air_monitor
    src/main.cpp
    src/AirMonitorController.cpp
    src/AirMonitorController.h
)

set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS Theme.qml
)
set_source_files_properties(qml/main.qml PROPERTIES QT_RESOURCE_ALIAS main.qml)
set_source_files_properties(qml/HeaderBar.qml PROPERTIES QT_RESOURCE_ALIAS HeaderBar.qml)
set_source_files_properties(qml/FooterBar.qml PROPERTIES QT_RESOURCE_ALIAS FooterBar.qml)
set_source_files_properties(qml/Card.qml PROPERTIES QT_RESOURCE_ALIAS Card.qml)
set_source_files_properties(qml/PlantCard.qml PROPERTIES QT_RESOURCE_ALIAS PlantCard.qml)
set_source_files_properties(qml/MetricBlock.qml PROPERTIES QT_RESOURCE_ALIAS MetricBlock.qml)
set_source_files_properties(qml/Sparkline.qml PROPERTIES QT_RESOURCE_ALIAS Sparkline.qml)
set_source_files_properties(qml/SystemCard.qml PROPERTIES QT_RESOURCE_ALIAS SystemCard.qml)
set_source_files_properties(qml/EventsCard.qml PROPERTIES QT_RESOURCE_ALIAS EventsCard.qml)
set_source_files_properties(qml/HomePage.qml PROPERTIES QT_RESOURCE_ALIAS HomePage.qml)
set_source_files_properties(qml/CalPage.qml PROPERTIES QT_RESOURCE_ALIAS CalPage.qml)
set_source_files_properties(qml/AlarmPage.qml PROPERTIES QT_RESOURCE_ALIAS AlarmPage.qml)
set_source_files_properties(qml/EventOverlay.qml PROPERTIES QT_RESOURCE_ALIAS EventOverlay.qml)
set_source_files_properties(qml/AckDialog.qml PROPERTIES QT_RESOURCE_ALIAS AckDialog.qml)

qt_add_qml_module(air_monitor
    URI AirMonitor
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/Theme.qml
        qml/HeaderBar.qml
        qml/FooterBar.qml
        qml/Card.qml
        qml/PlantCard.qml
        qml/MetricBlock.qml
        qml/Sparkline.qml
        qml/SystemCard.qml
        qml/EventsCard.qml
        qml/HomePage.qml
        qml/CalPage.qml
        qml/AlarmPage.qml
        qml/EventOverlay.qml
        qml/AckDialog.qml
)

target_include_directories(air_monitor PRIVATE src)

target_link_libraries(air_monitor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(air_monitor PROPERTIES
    OUTPUT_NAME air_monitor
    BUILD_RPATH "$<TARGET_FILE_DIR:Qt6::Core>"
    INSTALL_RPATH "$<TARGET_FILE_DIR:Qt6::Core>"
)

add_custom_command(TARGET air_monitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/qtquickcontrols2.conf
        $<TARGET_FILE_DIR:air_monitor>/qtquickcontrols2.conf
)
