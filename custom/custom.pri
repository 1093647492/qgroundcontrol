message("Adding Yuneec Typhoon H520 plugin")

linux : android-g++ {
    DEFINES += NO_SERIAL_LINK
    CONFIG  += DISABLE_BUILTIN_ANDROID
    CONFIG  += MobileBuild
    CONFIG  += NoSerialBuild
    equals(ANDROID_TARGET_ARCH, x86)  {
        message("Using ST16 specific Android interface")
    } else {
        message("Unsuported Android toolchain, limited functionality for development only")
    }
} else {
    SimulatedMobile {
        message("Simulated mobile build")
        DEFINES += __mobile__
        DEFINES += NO_SERIAL_LINK
        CONFIG  += MobileBuild
        CONFIG  += NoSerialBuild
    } else {
        message("Desktop build")
    }
}

TARGET   = DataPilot

DEFINES += CUSTOMHEADER=\"\\\"TyphoonHPlugin.h\\\"\"
DEFINES += CUSTOMCLASS=TyphoonHPlugin

CONFIG  += QGC_DISABLE_APM_PLUGIN QGC_DISABLE_APM_PLUGIN_FACTORY QGC_DISABLE_PX4_PLUGIN_FACTORY
CONFIG  += DISABLE_VIDEORECORDING
CONFIG  += QGC_DISABLE_BUILD_SETUP

DEFINES += NO_UDP_VIDEO
DEFINES += QGC_DISABLE_BLUETOOTH
DEFINES += QGC_DISABLE_UVC
DEFINES += DISABLE_ZEROCONF

DEFINES += QGC_APPLICATION_NAME=\"\\\"DataPilot\\\"\"
DEFINES += QGC_ORG_NAME=\"\\\"Yuneec.com\\\"\"
DEFINES += QGC_ORG_DOMAIN=\"\\\"com.yuneec\\\"\"

RESOURCES += \
    $$QGCROOT/custom/typhoonh.qrc

MacBuild {
    QMAKE_INFO_PLIST    = $$PWD/macOS/Info.plist
    ICON                = $$PWD/macOS/icon.icns
    OTHER_FILES        -= $$QGCROOT/Custom-Info.plist
    OTHER_FILES        += $$PWD/macOS/Info.plist
}

WindowsBuild {
    RC_ICONS            = $$PWD/Windows/icon.ico
    VERSION             = 1.0.0.0
    ReleaseBuild {
        QMAKE_CFLAGS_RELEASE   += /Gy /Ox
        QMAKE_CXXFLAGS_RELEASE += /Gy /Ox
        # Eliminate duplicate COMDATs
        QMAKE_LFLAGS_RELEASE   += /OPT:ICF /LTCG
    }
}

SOURCES += \
    $$PWD/src/CameraControl.cc \
    $$PWD/src/m4serial.cc \
    $$PWD/src/m4util.cc \
    $$PWD/src/TyphoonHM4Interface.cc \
    $$PWD/src/TyphoonHPlugin.cc \
    $$PWD/src/TyphoonHQuickInterface.cc \

AndroidBuild {
    SOURCES += \
        $$PWD/src/TyphoonHJNI.cc \
}

HEADERS += \
    $$PWD/src/CameraControl.h \
    $$PWD/src/m4channeldata.h \
    $$PWD/src/m4def.h \
    $$PWD/src/m4serial.h \
    $$PWD/src/m4util.h \
    $$PWD/src/TyphoonHCommon.h \
    $$PWD/src/TyphoonHM4Interface.h \
    $$PWD/src/TyphoonHPlugin.h \
    $$PWD/src/TyphoonHQuickInterface.h \

INCLUDEPATH += \
    $$PWD/src \

equals(QT_MAJOR_VERSION, 5) {
    greaterThan(QT_MINOR_VERSION, 5) {
        ReleaseBuild {
            AndroidBuild {
                QT      += qml-private
                CONFIG  += qtquickcompiler
                message("Using Qt Quick Compiler")
            } else {
                CONFIG  -= qtquickcompiler
            }
        }
    }
}

QT += \
    multimedia

#-------------------------------------------------------------------------------------
# Firmware/AutoPilot Plugin

INCLUDEPATH += \
    $$PWD/src/FirmwarePlugin \
    $$PWD/src/AutoPilotPlugin

HEADERS+= \
    $$PWD/src/AutoPilotPlugin/YuneecAutoPilotPlugin.h \
    $$PWD/src/AutoPilotPlugin/GimbalComponent.h \
    $$PWD/src/AutoPilotPlugin/ChannelComponent.h \
    $$PWD/src/AutoPilotPlugin/HealthComponent.h \
    $$PWD/src/FirmwarePlugin/YuneecFirmwarePlugin.h \
    $$PWD/src/FirmwarePlugin/YuneecFirmwarePluginFactory.h

SOURCES += \
    $$PWD/src/AutoPilotPlugin/YuneecAutoPilotPlugin.cc \
    $$PWD/src/AutoPilotPlugin/GimbalComponent.cc \
    $$PWD/src/AutoPilotPlugin/ChannelComponent.cc \
    $$PWD/src/AutoPilotPlugin/HealthComponent.cc \
    $$PWD/src/FirmwarePlugin/YuneecFirmwarePlugin.cc \
    $$PWD/src/FirmwarePlugin/YuneecFirmwarePluginFactory.cc

#-------------------------------------------------------------------------------------
# Android

AndroidBuild {
    ANDROID_EXTRA_LIBS += $${PLUGIN_SOURCE}
    include($$PWD/AndroidTyphoonH.pri)
}

#-------------------------------------------------------------------------------------
# Desktop Distribution

DesktopInstall {

    DEFINES += QGC_INSTALL_RELEASE
    QMAKE_POST_LINK = echo Start post link

    MacBuild {
        message("Preparing GStreamer Framework")
        QMAKE_POST_LINK += && $$QGCROOT/tools/prepare_gstreamer_framework.sh $${OUT_PWD}/gstwork/ $${DESTDIR}/$${TARGET}.app $${TARGET}
        # Copy non-standard frameworks into app package
        QMAKE_POST_LINK += && rsync -a $$BASEDIR/libs/lib/Frameworks $$DESTDIR/$${TARGET}.app/Contents/
        # SDL2 Framework
        QMAKE_POST_LINK += && install_name_tool -change "@rpath/SDL2.framework/Versions/A/SDL2" "@executable_path/../Frameworks/SDL2.framework/Versions/A/SDL2" $$DESTDIR/$${TARGET}.app/Contents/MacOS/$${TARGET}
        # We cd to release directory so we can run macdeployqt without a path to the
        # qgroundcontrol.app file. If you specify a path to the .app file the symbolic
        # links to plugins will not be created correctly.
        QMAKE_POST_LINK += && mkdir -p $${DESTDIR}/package
        QMAKE_POST_LINK += && cd $${DESTDIR} && $$dirname(QMAKE_QMAKE)/macdeployqt $${TARGET}.app -appstore-compliant -verbose=2 -qmldir=$${BASEDIR}/src
        QMAKE_POST_LINK += && cd $${OUT_PWD}
        QMAKE_POST_LINK += && hdiutil create -verbose -stretch 4g -layout SPUD -srcfolder $${DESTDIR}/$${TARGET}.app -volname $${TARGET} $${DESTDIR}/package/$${TARGET}.dmg
    }

    WindowsBuild {
        BASEDIR_WIN = $$replace(BASEDIR, "/", "\\")
        DESTDIR_WIN = $$replace(DESTDIR, "/", "\\")
        QT_BIN_DIR  = $$dirname(QMAKE_QMAKE)

        # Copy dependencies
        DebugBuild: DLL_QT_DEBUGCHAR = "d"
        ReleaseBuild: DLL_QT_DEBUGCHAR = ""
        COPY_FILE_LIST = \
            $$BASEDIR\\libs\\lib\\sdl2\\msvc\\lib\\x86\\SDL2.dll \
            $$BASEDIR\\deploy\\libeay32.dll

        for(COPY_FILE, COPY_FILE_LIST) {
            QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"$$COPY_FILE\" \"$$DESTDIR_WIN\"
        }

        ReleaseBuild {
            # Copy Visual Studio DLLs
            # Note that this is only done for release because the debugging versions of these DLLs cannot be redistributed.
            win32-msvc2010 {
                QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"C:\\Windows\\System32\\msvcp100.dll\"  \"$$DESTDIR_WIN\"
                QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"C:\\Windows\\System32\\msvcr100.dll\"  \"$$DESTDIR_WIN\"

            } else:win32-msvc2012 {
                QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"C:\\Windows\\System32\\msvcp110.dll\"  \"$$DESTDIR_WIN\"
                QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"C:\\Windows\\System32\\msvcr110.dll\"  \"$$DESTDIR_WIN\"

            } else:win32-msvc2013 {
                QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"C:\\Windows\\System32\\msvcp120.dll\"  \"$$DESTDIR_WIN\"
                QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"C:\\Windows\\System32\\msvcr120.dll\"  \"$$DESTDIR_WIN\"

            } else:win32-msvc2015 {
                QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"C:\\Windows\\System32\\msvcp140.dll\"  \"$$DESTDIR_WIN\"
                QMAKE_POST_LINK += $$escape_expand(\\n) $$QMAKE_COPY \"C:\\Windows\\System32\\vcruntime140.dll\"  \"$$DESTDIR_WIN\"

            } else {
                error("Visual studio version not supported, installation cannot be completed.")
            }
        }

        DEPLOY_TARGET = $$shell_quote($$shell_path($$DESTDIR_WIN\\$${TARGET}.exe))
        QMAKE_POST_LINK += $$escape_expand(\\n) $$QT_BIN_DIR\\windeployqt --no-compiler-runtime --qmldir=$${BASEDIR_WIN}\\src $${DEPLOY_TARGET}
        #QMAKE_POST_LINK += $$escape_expand(\\n) cd $$BASEDIR_WIN && $$quote("\"C:\\Program Files \(x86\)\\NSIS\\makensis.exe\"" /NOCD "\"/XOutFile $${DESTDIR_WIN}\\$${TARGET}-installer.exe\"" "$$BASEDIR_WIN\\deploy\\qgroundcontrol_installer.nsi")
        #OTHER_FILES += deploy/$${TARGET}_installer.nsi
    }

    LinuxBuild {
        QMAKE_POST_LINK += && mkdir -p $$DESTDIR/Qt/libs && mkdir -p $$DESTDIR/Qt/plugins

        # QT_INSTALL_LIBS
        QT_LIB_LIST = \
            libQt5Core.so.5 \
            libQt5DBus.so.5 \
            libQt5Gui.so.5 \
            libQt5Location.so.5 \
            libQt5Multimedia.so.5 \
            libQt5MultimediaQuick_p.so.5 \
            libQt5Network.so.5 \
            libQt5OpenGL.so.5 \
            libQt5Positioning.so.5 \
            libQt5PrintSupport.so.5 \
            libQt5Qml.so.5 \
            libQt5Quick.so.5 \
            libQt5QuickWidgets.so.5 \
            libQt5SerialPort.so.5 \
            libQt5Sql.so.5 \
            libQt5Svg.so.5 \
            libQt5Test.so.5 \
            libQt5Widgets.so.5 \
            libQt5XcbQpa.so.5 \
            libicudata.so.56 \
            libicui18n.so.56 \
            libicuuc.so.56

        for(QT_LIB, QT_LIB_LIST) {
            QMAKE_POST_LINK += && $$QMAKE_COPY --dereference $$[QT_INSTALL_LIBS]/$$QT_LIB $$DESTDIR/Qt/libs/
        }

        # QT_INSTALL_PLUGINS
        QT_PLUGIN_LIST = \
            bearer \
            geoservices \
            iconengines \
            imageformats \
            platforminputcontexts \
            platforms \
            position \
            sqldrivers \
            xcbglintegrations

        for(QT_PLUGIN, QT_PLUGIN_LIST) {
            QMAKE_POST_LINK += && $$QMAKE_COPY --dereference --recursive $$[QT_INSTALL_PLUGINS]/$$QT_PLUGIN $$DESTDIR/Qt/plugins/
        }

        # QT_INSTALL_QML
        QMAKE_POST_LINK += && $$QMAKE_COPY --dereference --recursive $$[QT_INSTALL_QML] $$DESTDIR/Qt/
        # QGroundControl start script
        QMAKE_POST_LINK += && $$QMAKE_COPY $$BASEDIR/deploy/qgroundcontrol-start.sh $$DESTDIR
        QMAKE_POST_LINK += && $$QMAKE_COPY $$BASEDIR/deploy/qgroundcontrol.desktop $$DESTDIR
        QMAKE_POST_LINK += && $$QMAKE_COPY $$BASEDIR/resources/icons/qgroundcontrol.png $$DESTDIR
        #-- TODO: This uses hardcoded paths. It should use $${DESTDIR}
        QMAKE_POST_LINK += && mkdir -p release/package
        QMAKE_POST_LINK += && tar -cjf release/package/$${TARGET}.tar.bz2 release --exclude='package' --transform 's/release/$${TARGET}/'
    }
}
