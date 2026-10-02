# Author: Marc Comino 2020. Modified by Imanol Munoz-Pandiella 2022.

QT       += opengl widgets openglwidgets

TARGET = ViewerPBS
TEMPLATE = app

CONFIG += c++14
CONFIG(release, release|debug):QMAKE_CXXFLAGS += -Wall -O2

CONFIG(release, release|debug):DESTDIR = $$PWD/release/
CONFIG(release, release|debug):OBJECTS_DIR = $$PWD/release/
CONFIG(release, release|debug):MOC_DIR = $$PWD/release/
CONFIG(release, release|debug):UI_DIR = $$PWD/release/

CONFIG(debug, release|debug):DESTDIR = $$PWD/debug/
CONFIG(debug, release|debug):OBJECTS_DIR = $$PWD/debug/
CONFIG(debug, release|debug):MOC_DIR = $$PWD/debug/
CONFIG(debug, release|debug):UI_DIR = $$PWD/debug/

win32{
    LIBS += -lOpenGL32
    INCLUDEPATH += "C:\Program Files (x86)\glm\include"
}


SOURCES += \
    triangle_mesh.cc \
    mesh_io.cc \
    main.cc \
    main_window.cc \
    glwidget.cc \
    camera.cc \
    tiny_obj_loader.cc

HEADERS  += \
    triangle_mesh.h \
    mesh_io.h \
    main_window.h \
    glwidget.h \
    camera.h \
    tiny_obj_loader.h

FORMS    += \
    main_window.ui

OTHER_FILES +=

DISTFILES += \
    shaders/README.md \
    shaders/ibl-pbs.frag \
    shaders/ibl-pbs.vert \
    shaders/pbs.frag \
    shaders/pbs.vert \
    shaders/reflection.frag \
    shaders/reflection.vert \
    shaders/sky.frag \
    shaders/sky.vert \
    shaders/phong.frag \
    shaders/phong.vert \
    shaders/texMap.frag \
    shaders/texMap.vert \
    shaders/ao-norm-alb.vert \
    shaders/ao-norm-alb.frag \
    shaders/ao-depth.vert \
    shaders/ao-depth.frag \
    shaders/ao-texWrite.vert \
    shaders/ao-texWrite.frag \
    shaders/ao-compute.vert \
    shaders/ao-compute.frag \
    shaders/ao-show.vert \
    shaders/ao-show.frag \
    shaders/ao-filter.vert \
    shaders/ao-filter.frag \
    shaders/ao-IBL.vert \
    shaders/ao-IBL.frag
