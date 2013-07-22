
PROJECT( be.shell )
cmake_minimum_required(VERSION 2.4)
FIND_PACKAGE(KDE4 REQUIRED)

INCLUDE_DIRECTORIES(
	${CMAKE_SOURCE_DIR}
	${KDE4_INCLUDE_DIR}
	${QT_INCLUDES}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${KDE4_INCLUDES} )

set(be.shell_SRCS
    be.shell.cpp
    be.plugged.cpp
    tasks.cpp
    systray.cpp
    clock.cpp
    desktop.cpp
    button.cpp
    panel.cpp
    mediatray.cpp
    pager.cpp
    runner.cpp
    session.cpp
    hmenubar.cpp
    vmenubar.cpp
    globalmenu.cpp
    infocenter.cpp
    flowlayout.cpp
    trash.cpp
    volume.cpp
    claws.cpp
    meter.cpp
    label.cpp
    battery.cpp
    touchwheel.cpp
 )

set(be.shell_MOC_HDRS be.shell.h dbus_shell.h dbus_desktop.h dbus_runner.h dbus_gmenu.h dbus_info.h dbus_claws.h dbus_button.h)
qt4_wrap_cpp(be.shell_MOC_SRCS ${be.shell_MOC_HDRS})

kde4_add_executable(be.shell ${be.shell_SRCS} ${be.shell_MOC_SRCS})

target_link_libraries(be.shell ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDE4_KFILE_LIBS} solid ${X11_LIBRARIES} ${X11_Xcomposite_LIB} ${X11_Xdamage_LIB} ${X11_Xrender_LIB} ${X11_Xext_LIB} ${X11_Xfixes_LIB} kcmutils)

install(TARGETS be.shell DESTINATION ${BIN_INSTALL_DIR} )

add_subdirectory(i18n)


# PROJECT( be.run )
# set(be.run_SRCS be.run.cpp runner.cpp)
# set(be.shell_MOC_HDRS runner_dbus.h)
# qt4_wrap_cpp(be.run_MOC_SRCS ${be.run_MOC_HDRS})

# kde4_add_executable(be.run ${be.run_SRCS} ${be.run_MOC_SRCS})

# target_link_libraries(be.run ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDE4_KFILE_LIBS} kworkspace )

# install(TARGETS be.run DESTINATION ${BIN_INSTALL_DIR} )

########### install files ###############

install( FILES be.shell.desktop  DESTINATION  ${AUTOSTART_INSTALL_DIR} )

