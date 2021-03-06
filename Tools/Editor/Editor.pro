QT       += core gui opengl widgets

TARGET = Editor
CONFIG   += console
CONFIG += c++11

TEMPLATE = app
RC_ICONS = glider.ico
ICON = Editor.icns
QMAKE_INFO_PLIST = Info.plist
DEFINES += RENDERER_OPENGL=1

INCLUDEPATH += $$PWD/../../Engine/Include
DEPENDPATH += $$PWD/../../../aether3d_build

win32 {
    PWD_WIN = $${PWD}
    DESTDIR_WIN = $${OUT_PWD}
    PWD_WIN ~= s,/,\\,g
    DESTDIR_WIN ~= s,/,\\,g
# Use the following line if you compiled with Visual Studio
    LIBS += -L$$PWD/../../../aether3d_build/ -lAether3D_GL_d -lUser32 -lGdi32 -lOpenGL32

    LIBS += -L$$PWD/../../Engine/ThirdParty/lib/ -llibOpenAL32

#win32-msvc:LIBS += -L$$PWD/../../../aether3d_build/ -lAether3D_GL_d
#win32-g++:LIBS += -L$$PWD/../../../aether3d_build/ -laether3d_gl_win

    copyfiles.commands = $$quote(cmd /c xcopy /S /I /y $${PWD_WIN}\copy_to_output $${DESTDIR_WIN})
    #PRE_TARGETDEPS += $$PWD/../../../aether3d_build/libaether3d_gl_win.a
}
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
    #QMAKE_MAC_SDK = macosx10.11
    QMAKE_CXXFLAGS += -std=c++11 -stdlib=libc++ -Wall -Wextra
    LIBS += -L$$PWD/../../../aether3d_build/ -laether3d_osx
    LIBS += -framework CoreFoundation -framework OpenAL -framework QuartzCore -framework IOKit -framework Cocoa
    copyfiles.commands = cp -r $$PWD/copy_to_output/* $$OUT_PWD
    PRE_TARGETDEPS += $$PWD/../../../aether3d_build/libaether3d_osx.a
}
linux {
    QMAKE_CXXFLAGS += -std=c++11
    LIBS += -L$$PWD/../../../aether3d_build -laether3d_gl_linux
    LIBS += -ldl -fPIC -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lX11-xcb -lX11 -lGL -lopenal
    copyfiles.commands = cp -r $$PWD/copy_to_output/* $$OUT_PWD
}

QMAKE_EXTRA_TARGETS += copyfiles
POST_TARGETDEPS += copyfiles

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    SceneWidget.cpp \
    WindowMenu.cpp \
    CreateCameraCommand.cpp \
    CommandManager.cpp \
    CreateGoCommand.cpp \
    TransformInspector.cpp \
    ModifyTransformCommand.cpp \
    CameraInspector.cpp \
    ModifyCameraCommand.cpp \
    MeshRendererInspector.cpp \
    DirectionalLightInspector.cpp \
    CreateLightCommand.cpp \
    SpotLightInspector.cpp \
    CreateMeshRendererCommand.cpp \
    LightingInspector.cpp \
    AudioSourceInspector.cpp \
    CreateAudioSourceCommand.cpp \
    SpriteRendererInspector.cpp \
    CreateSpriteRendererCommand.cpp \
    PointLightInspector.cpp \
    RemoveComponentCommand.cpp \
    ModifySpriteRendererCommand.cpp \
    RenameGameObjectCommand.cpp \
    DeleteGameObjectCommand.cpp

HEADERS += \
    MainWindow.hpp \
    SceneWidget.hpp \
    WindowMenu.hpp \
    Command.hpp \
    CreateCameraCommand.hpp \
    CommandManager.hpp \
    CreateGoCommand.hpp \
    TransformInspector.hpp \
    ModifyTransformCommand.hpp \
    CameraInspector.hpp \
    ModifyCameraCommand.hpp \
    MeshRendererInspector.hpp \
    DirectionalLightInspector.hpp \
    CreateLightCommand.hpp \
    SpotLightInspector.hpp \
    CreateMeshRendererCommand.hpp \
    LightingInspector.hpp \
    AudioSourceInspector.hpp \
    CreateAudioSourceCommand.hpp \
    SpriteRendererInspector.hpp \
    CreateSpriteRendererCommand.hpp \
    PointLightInspector.hpp \
    RemoveComponentCommand.hpp \
    ModifySpriteRendererCommand.hpp \
    RenameGameObjectCommand.hpp \
    DeleteGameObjectCommand.hpp
