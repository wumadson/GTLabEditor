#############################################################################
##
## Copyright (C) 2007~2010 Colin Willcocks.
## Copyright (C) 2005~2007 Uco Mesdag. 
## All rights reserved.
##
## This file is part of "GT-10 Fx FloorBoard".
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program; if not, write to the Free Software Foundation, Inc.,
## 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
##
#############################################################################

TEMPLATE = app
#ifdef Q_OS_MAC
CONFIG += release
TARGET = "GT-10FxFloorBoard"
DESTDIR = ./packager
#else 
CONFIG += release
TARGET = "GT-10FxFloorBoard"
DESTDIR = ./packager
#endif
	OBJECTS_DIR += release
	UI_DIR += ./generatedfiles
	MOC_DIR += ./generatedfiles/release
INCLUDEPATH += ./generatedfiles \
    ./generatedfiles/release \
    .
    
TRANSLATIONS = language_fr.ts \
               language_ge.ts \
               language_ch.ts
               
CODECFORTR = UTF-8

DEPENDPATH += .
QT += core gui widgets xml printsupport

#Platform dependent file(s)
win32 {
        exists("C:/Progra~1/MS_SDKs/Windows/v6.1/Lib/WinMM.Lib") {	# <-- Change the path to WinMM.Lib here!
                LIBS += C:/Progra~1/MS_SDKs/Windows/v6.1/Lib/WinMM.Lib	# <-- Change the path here also!
    } else { 
        exists("c:/PROGRA~1/MICROS~3/VC/PLATFO~1/Lib/WinMM.Lib") { # Path vs2005 (Vista)
        	LIBS += c:/PROGRA~1/MICROS~3/VC/PLATFO~1/Lib/WinMM.Lib
        } else { 
            LIBS += .\WinMM.Lib
            message("WINMM.LIB IS REQUIRED. IF NOT INSTALLED THEN")
            message("PLEASE DOWNLOAD AND INSTALL THE LATEST PLATFORM SDK")
            message("FROM MICROSOFT.COM AND AFTER INSTALLATION")
            message("CHANGE THE CORRECT (DOS) PATH TO WinMM.lib")
            message("IN THIS (GT-10FxFloorBoard.pro) FILE WHERE INDICATED")
        }
	}
	 HEADERS += 
	 SOURCES += ./windows/RtMidi.cpp                        
	 INCLUDEPATH += ./windows
	message(Including Windows specific headers and sources...)
}
linux-g++ {
        LIBS += -lasound
	message("ALSA LIBRARIES SHOULD BE INSTALLED or ERROR will Occur") 
	message("Please install the ALSA Audio System packages if not present") 	
 
	 HEADERS += 
	 SOURCES += ./linux/RtMidi.cpp 
	 INCLUDEPATH += ./linux
	message(Including Linux specific headers and sources...)
}
linux-g++-64 {
        LIBS += -lasound
	message("ALSA LIBRARIES SHOULD BE INSTALLED or ERROR will Occur") 
	message("Please install the ALSA Audio System packages if not present") 	
 
	 HEADERS += 
	 SOURCES += ./linux/RtMidi.cpp 
	 INCLUDEPATH += ./linux
	message(Including Linux specific headers and sources...)
}
macx {
	LIBS += -framework CoreMidi -framework CoreAudio -framework CoreFoundation
	message("X-Code LIBRARIES SHOULD BE INSTALLED or ERROR will Occur") 
	message("Please install the X-Code Audio System packages if not present") 
	 HEADERS += 
	 SOURCES += ./macosx/RtMidi.cpp 
	INCLUDEPATH += ./macosx
	ICON = GT-10FxFloorBoard.icns
	message(Including Mac OS X specific headers and sources...)
}


#Include file(s)
include(GT-10FxFloorBoard.pri)

#Windows resource file
win32:RC_FILE = GT-10FxFloorBoard.rc

# Qt4 -> Qt5 baseline compatibility
HEADERS += qt4compat.h
QMAKE_CXXFLAGS += -include $$PWD/qt4compat.h

# GT-10 FxFloorBoard Modern UI
SOURCES += modernFloorBoard.cpp modernTheme.cpp modernWidgets.cpp modernSignalChainModel.cpp \
           effectArtworkWidget.cpp effectModelBrowser.cpp parameterBar.cpp \
           modernPatchListModel.cpp patchSidebar.cpp modernEqGraph.cpp modernFxEditor.cpp
HEADERS += modernFloorBoard.h modernTheme.h modernWidgets.h modernSignalChainModel.h \
           effectArtworkWidget.h effectModelBrowser.h parameterBar.h \
           modernPatchListModel.h patchSidebar.h modernEqGraph.h modernFxEditor.h
