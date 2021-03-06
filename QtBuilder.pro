TARGET	 = QtBuilder
VERSION	 = 0.3
TEMPLATE = app
RC_FILE	 = appinfo.rc
QT      += core gui

CONFIG	+= 3dnow mmx stl sse sse2 \
	embed_manifest_exe

QMAKE_LFLAGS_WINDOWS += \
	/MANIFESTUAC:"level='requireAdministrator'uiAccess='true'"

SOURCES += main.cpp \
	qtbuilder.cpp \
	qtcompile.cpp \
	processes.cpp \
	methods.cpp \
	helpers.cpp \
	guimain.cpp \
	guilogs.cpp \
	guitools.cpp \
	guislider.cpp \
	guiprogress.cpp

HEADERS += \
	qtbuilder.h \
	definitions.h \
	helpers.h \
	appinfo.h

RESOURCES += \
	resources.qrc

OTHER_FILES += \
	appinfo.rc

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
