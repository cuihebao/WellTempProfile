QT       += core gui widgets datavisualization concurrent
CONFIG   += c++11
TARGET    = WellTempProfile
TEMPLATE  = app

# 源码为 UTF-8（含中文字符串），MSVC 需要显式指定
msvc: QMAKE_CXXFLAGS += /utf-8

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    SurfaceView.cpp \
    TimeAxisFormatter.cpp \
    FrameBuffer.cpp \
    Resampler.cpp \
    DataExporter.cpp \
    ColorBar.cpp \
    DataSimulator.cpp

HEADERS += \
    Config.h \
    MainWindow.h \
    SurfaceView.h \
    TimeAxisFormatter.h \
    FrameBuffer.h \
    Resampler.h \
    DataExporter.h \
    ColorBar.h \
    ColorMap.h \
    DataSimulator.h
