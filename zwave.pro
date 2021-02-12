isEmpty(PLUGIN_PRI) {
  exists($$[QT_INSTALL_PREFIX]/include/nymea/plugin.pri) {
    include($$[QT_INSTALL_PREFIX]/include/nymea/plugin.pri)
  } else {
    message("plugin.pri not found. Either install libnymea1-dev or use the PLUGIN_PRI argument to point to it.")
    message("For building this project without nymea installed system-wide, you will want to export those variables in addition:")
    message("PKG_CONFIG_PATH=/path/to/build-nymea/libnymea/pkgconfig/")
    message("CPATH=/path/to/nymea/libnymea/")
    message("LIBRARY_PATH=/path/to/build-nymea/libnymea/")
    message("PATH=/path/to/build-nymea/tools/nymea-plugininfocompiler:$PATH")
    message("LD_LIBRARY_PATH=/path/to/build-nymea/libnymea/")
    error("plugin.pri not found. Cannot continue")
  }
} else {
#  message("Using $$PLUGIN_PRI")
  include($$PLUGIN_PRI)
}

QT += serialport

LIBS += -L/usr/local/lib/ -lopenzwave
INCLUDEPATH += /usr/include/openzwave/

CONFIG_PATH=/etc/openzwave/
DEFINES += CONFIG_PATH=\\\"$${CONFIG_PATH}\\\"

#QMAKE_CXXFLAGS += -isystem "$${INCLUDEPATH}"

message(============================================)
message(Qt version: $$[QT_VERSION])
message("Building $$deviceplugin$${TARGET}.so")
message("Open z-wave configurations: $${CONFIG_PATH}")

SOURCES += \
    integrationpluginzwave.cpp \
    zwavemanager.cpp \
    zwavenode.cpp \
    zwavevalue.cpp

HEADERS += \
    integrationpluginzwave.h \
    zwavemanager.h \
    zwavenode.h \
    zwavevalue.h
