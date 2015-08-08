#ifndef APP_INFO_H
#define APP_INFO_H

#define APP_VERSION_MAJOR		0
#define APP_VERSION_MINOR		3
#define APP_VERSION_REVSN		1
#define APP_VERSION_BUILD		85

#define APP_INFO_NAME			"QtBuilder"
#define APP_INFO_LABEL			"QtBuilder"
#define APP_INFO_DOMAIN			"T3Win"
#define APP_INFO_ORG			"T3Tools"

#define APP_COPYRIGHT_YEAR		"2015"
#define APP_COPYRIGHT_INTN		"OcDS"

#define APP_COPYRIGHT_AUTH		"Gerald Gstaltner"
#define APP_COPYRIGHT_COMP		"Gerald Gstaltner"
#define APP_COPYRIGHT_PROD		"QtBuilder"
#define APP_COPYRIGHT_DESC		"Automated MSVC building of Qt libraries"

#define APP_COPYRIGHT_PAR1		"Copyright (C)2015-"
#define APP_COPYRIGHT_PAR2		" "

#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE	101
#define _APS_NEXT_COMMAND_VALUE		40001
#define _APS_NEXT_CONTROL_VALUE		1001
#define _APS_NEXT_SYMED_VALUE		101
#endif
#endif

#define STR(value) #value
#define STRINGIZE(value) STR(value)

#define APS_FILE_VERSION_STR \
	STRINGIZE(APP_VERSION_MAJOR) "." \
	STRINGIZE(APP_VERSION_MINOR) "." \
	STRINGIZE(APP_VERSION_REVSN) "." \
	STRINGIZE(APP_VERSION_BUILD)

#define APS_PROD_VERSION_STR \
	STRINGIZE(APP_VERSION_MAJOR) "." \
	STRINGIZE(APP_VERSION_MINOR) "."

#define APS_COPYRIGHTALL_STR \
	APP_COPYRIGHT_PAR1 \
	APP_COPYRIGHT_YEAR \
	APP_COPYRIGHT_PAR2 \
	APP_COPYRIGHT_COMP

#define APS_FILE_NAME_STR \
	APP_INFO_NAME

#endif // INFO_H