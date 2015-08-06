TARGET	 = QtBuilder

QT      += core gui

TEMPLATE = app

CONFIG	+= 3dnow mmx stl sse sse2 \
	embed_manifest_exe \
	mobility

MOBILITY += systeminfo

SOURCES += main.cpp \
	qtbuilder.cpp \
	helpers.cpp

HEADERS += \
	qtbuilder.h \
	definitions.h \
	helpers.h
