message("Adding Auterion Plugin")

#-- Version control
#   Major and minor versions are defined here (manually)

AUTERION_QGC_VER_MAJOR = 0
AUTERION_QGC_VER_MINOR = 0
AUTERION_QGC_VER_FIRST_BUILD = 0

#   Build number is automatic

AUTERION_QGC_VER_BUILD = $$system(git --git-dir ../.git rev-list master --first-parent --count)
win32 {
    AUTERION_QGC_VER_BUILD = $$system("set /a $$AUTERION_QGC_VER_BUILD - $$AUTERION_QGC_VER_FIRST_BUILD")
} else {
    AUTERION_QGC_VER_BUILD = $$system("echo $(($$AUTERION_QGC_VER_BUILD - $$AUTERION_QGC_VER_FIRST_BUILD))")
}
AUTERION_QGC_VERSION = $${AUTERION_QGC_VER_MAJOR}.$${AUTERION_QGC_VER_MINOR}.$${AUTERION_QGC_VER_BUILD}

DEFINES -= GIT_VERSION=\"\\\"$$GIT_VERSION\\\"\"
DEFINES += GIT_VERSION=\"\\\"$$AUTERION_QGC_VERSION\\\"\"

message(Auterion GS Version: $${AUTERION_QGC_VERSION})

#   Disable APM support

MAVLINK_CONF = common
CONFIG  += QGC_DISABLE_APM_MAVLINK

#   Branding

DEFINES += CUSTOMHEADER=\"\\\"AuterionPlugin.h\\\"\"
DEFINES += CUSTOMCLASS=AuterionPlugin

CONFIG  += QGC_DISABLE_APM_PLUGIN QGC_DISABLE_APM_PLUGIN_FACTORY

TARGET   = AuterionGS
DEFINES += QGC_APPLICATION_NAME=\"\\\"AuterionGS\\\"\"

DEFINES += QGC_ORG_NAME=\"\\\"auterion.com\\\"\"
DEFINES += QGC_ORG_DOMAIN=\"\\\"com.auterion\\\"\"

RESOURCES += \
    $$QGCROOT/custom/auterion.qrc

QGC_APP_NAME        = "Auterion GS"
QGC_ORG_NAME        = "Auterion"
QGC_ORG_DOMAIN      = "com.auterion"
QGC_APP_DESCRIPTION = "Auterion Ground Station"
QGC_APP_COPYRIGHT   = "Copyright (C) 2018 Auterion AG. All rights reserved."

MacBuild {
    QMAKE_INFO_PLIST    = $$PWD/macOS/Info.plist
    ICON                = $$PWD/macOS/icon.icns
    OTHER_FILES        -= $$QGCROOT/Custom-Info.plist
    OTHER_FILES        += $$PWD/macOS/Info.plist
}

WindowsBuild {
    VERSION             = $${AUTERION_QGC_VERSION}.0
    QGCWINROOT          = $$replace(QGCROOT, "/", "\\")
    RC_ICONS            = $$QGCWINROOT\\custom\\Windows\\icon.ico
    QGC_INSTALLER_ICON          = $$QGCWINROOT\\custom\\Windows\\icon.ico
    QGC_INSTALLER_HEADER_BITMAP = $$QGCWINROOT\\custom\\Windows\\banner.bmp
    ReleaseBuild {
        QMAKE_CFLAGS_RELEASE   += /Gy /Ox
        QMAKE_CXXFLAGS_RELEASE += /Gy /Ox
        # Eliminate duplicate COMDATs
        QMAKE_LFLAGS_RELEASE   += /OPT:ICF /LTCG
    }
}

QML_IMPORT_PATH += \
    $$QGCROOT/custom/res

SOURCES += \
    $$PWD/src/AuterionPlugin.cc \
    $$PWD/src/AuterionQuickInterface.cc

HEADERS += \
    $$PWD/src/AuterionPlugin.h \
    $$PWD/src/AuterionQuickInterface.h

INCLUDEPATH += \
    $$PWD/src \

#equals(QT_MAJOR_VERSION, 5) {
#    greaterThan(QT_MINOR_VERSION, 5) {
#        ReleaseBuild {
#            QT      += qml-private
#            CONFIG  += qtquickcompiler
#            message("Using Qt Quick Compiler")
#        }
#    }
#}

#-------------------------------------------------------------------------------------
# Android

AndroidBuild {
    CONFIG += DISABLE_BUILTIN_ANDROID
    ANDROID_EXTRA_LIBS += $${PLUGIN_SOURCE}
    include($$QGCROOT/libs/qtandroidserialport/src/qtandroidserialport.pri)
    message("Adding Custom Serial Java Classes")
    QT += androidextras
    ANDROID_PACKAGE_SOURCE_DIR = $$QGCROOT/custom/android
    OTHER_FILES += \
        $$QGCROOT/custom/android/AndroidManifest.xml \
        $$QGCROOT/custom/android/res/xml/device_filter.xml \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/CdcAcmSerialDriver.java \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/CommonUsbSerialDriver.java \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/Cp2102SerialDriver.java \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/FtdiSerialDriver.java \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/ProlificSerialDriver.java \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/UsbId.java \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/UsbSerialDriver.java \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/UsbSerialProber.java \
        $$QGCROOT/custom/android/src/com/hoho/android/usbserial/driver/UsbSerialRuntimeException.java \
        $$QGCROOT/custom/android/src/org/mavlink/qgroundcontrol/QGCActivity.java \
        $$QGCROOT/custom/android/src/org/mavlink/qgroundcontrol/UsbIoManager.java

    DISTFILES += \
        $$QGCROOT/custom/android/gradle/wrapper/gradle-wrapper.jar \
        $$QGCROOT/custom/android/gradlew \
        $$QGCROOT/custom/android/res/values/libs.xml \
        $$QGCROOT/custom/android/build.gradle \
        $$QGCROOT/custom/android/gradle/wrapper/gradle-wrapper.properties \
        $$QGCROOT/custom/android/gradlew.bat
}

