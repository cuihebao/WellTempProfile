QT       += core gui widgets datavisualization concurrent
CONFIG   += c++11
TARGET    = WellTempProfile
TEMPLATE  = app

# 源码为 UTF-8（含中文字符串），MSVC 需要显式指定
msvc: QMAKE_CXXFLAGS += /utf-8

# 研发阶段用模拟数据。接入真实数据时注释掉下面这一行（否则模拟器和真实数据会同时往缓冲里写）
DEFINES  += USE_SIMULATOR

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    SurfaceView.cpp \
    TimeAxisFormatter.cpp \
    FrameBuffer.cpp \
    Resampler.cpp \
    DataExporter.cpp \
    ColorBar.cpp \
    HeatmapView.cpp \
    SettingsDialog.cpp \
    ConfigStore.cpp \
    DataIngest.cpp \
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
    ProfileView.h \
    HeatmapView.h \
    SettingsDialog.h \
    ConfigStore.h \
    ColorMap.h \
    DataIngest.h \
    DataSimulator.h
