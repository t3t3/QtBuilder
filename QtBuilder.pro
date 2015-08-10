TARGET	 = QtBuilder
VERSION	 = 0.3
TEMPLATE = app
RC_FILE	 = appinfo.rc
QT      += core gui

CONFIG	+= 3dnow mmx stl sse sse2 \
	embed_manifest_exe

SOURCES += main.cpp \
	qtbuilder.cpp \
	helpers.cpp \
	guitools.cpp \
	buildsteps.cpp \
	processes.cpp \
    guimain.cpp

HEADERS += \
	qtbuilder.h \
	definitions.h \
	helpers.h \
	appinfo.h

RESOURCES += \
	resources.qrc

OTHER_FILES += \
	appinfo.rc

QMAKE_LFLAGS += \
	/MANIFESTUAC:"level='requireAdministrator'uiAccess='false'"

CONFIG(debug, debug|release):{
	D		 = d
	DEFINES += __DEBUG
}
CONFIG(release, debug|release):{
	D		 =
	DESTDIR  = $$PWD/../../DEPLOY
	QMAKE_CFLAGS_RELEASE += -GL -MP
	QMAKE_LFLAGS_RELEASE += /LTCG
}
