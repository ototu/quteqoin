cmake_minimum_required(VERSION 3.22)

project(quteqoin VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(Qt6 REQUIRED COMPONENTS DBus)
find_package(Qt6 REQUIRED COMPONENTS Multimedia)
find_package(Qt6 REQUIRED COMPONENTS MultimediaWidgets)
find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_VERBOSE_MAKEFILE ON)

qt_standard_project_setup()

execute_process(
	COMMAND git describe --always --tags
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
	OUTPUT_VARIABLE GIT_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
    )
add_compile_definitions(GIT_VERSION="${GIT_VERSION}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SOURCES
	"main.cpp"
	"mainwindow.cpp"
	"mainwindow.h"
	"mainwindow.ui"
	"core/qqbackendupdatedevent.cpp"
	"core/qqbackendupdatedevent.h"
	"core/qqbakdisplayfilter.cpp"
	"core/qqbakdisplayfilter.h"
	"core/qqbigornoitem.cpp"
	"core/qqbigornoitem.h"
	"core/qqboardstatechangeevent.cpp"
	"core/qqboardstatechangeevent.h"
	"core/qqbouchot.cpp"
	"core/qqbouchot.h"
	"core/qqmessagetransformfilter.cpp"
	"core/qqmessagetransformfilter.h"
	"core/qqmussel.cpp"
	"core/qqmussel.h"
	"core/qqnetworkaccessor.cpp"
	"core/qqnetworkaccessor.h"
	"core/qqnorloge.cpp"
	"core/qqnorloge.h"
	"core/qqnorlogeref.cpp"
	"core/qqnorlogeref.h"
	"core/qqpost.cpp"
	"core/qqpostdisplayfilter.h"
	"core/qqpost.h"
	"core/qqpurgebouchothistoevent.cpp"
	"core/qqpurgebouchothistoevent.h"
	"core/qqsettings.cpp"
	"core/qqsettings.h"
	"core/qqsettingsparams.h"
	"core/qqsimplepostdisplayfilter.cpp"
	"core/qqsimplepostdisplayfilter.h"
	"core/qqtotoz.cpp"
	"core/qqtotozdownloader.cpp"
	"core/qqtotozdownloader.h"
	"core/qqtotoz.h"
	"core/qqtypes.h"
	"core/qqwebdownloader.cpp"
	"core/qqwebdownloader.h"
	"core/qutetools.cpp"
	"core/qutetools.h"
	"core/backend/qqbackend.cpp"
	"core/backend/qqbackend.h"
	"core/backend/qqbouchotbackend.cpp"
	"core/backend/qqbouchotbackend.h"
	"core/parsers/qqbackendparser.cpp"
	"core/parsers/qqbackendparser.h"
	"core/parsers/qqcustomxmlparser.cpp"
	"core/parsers/qqcustomxmlparser.h"
	"core/parsers/qqtsvparser.cpp"
	"core/parsers/qqtsvparser.h"
	"core/totozmanager/qqtmrequester.cpp"
	"core/totozmanager/qqtmrequester.h"
	"ui/qqboardinfo.cpp"
	"ui/qqboardinfo.h"
	"ui/qqboardinfo.ui"
	"ui/qqboardsinfo.cpp"
	"ui/qqboardsinfo.h"
	"ui/qqboardsinfo.ui"
	"ui/qqcmdtoolbuttons.cpp"
	"ui/qqcmdtoolbuttons.h"
	"ui/qqcmdtoolbuttons.ui"
	"ui/qqimageviewer.cpp"
	"ui/qqimageviewer.h"
	"ui/qqmessageblockuserdata.cpp"
	"ui/qqmessageblockuserdata.h"
	"ui/qqmusselinfo.cpp"
	"ui/qqmusselinfo.h"
	"ui/qqpalmipede.cpp"
	"ui/qqpalmipede.h"
	"ui/qqpalmipede.ui"
	"ui/qqpinipede.cpp"
	"ui/qqpinipede.h"
	"ui/qqpinisearchwidget.cpp"
	"ui/qqpinisearchwidget.h"
	"ui/qqpinisearchwidget.ui"
	"ui/qqproxyauthdialog.cpp"
	"ui/qqproxyauthdialog.h"
	"ui/qqproxyauthdialog.ui"
	"ui/qqsettingsmanager.cpp"
	"ui/qqsettingsmanager.h"
	"ui/qqsettingsmanager.ui"
	"ui/qqtotozmanager.cpp"
	"ui/qqtotozmanager.h"
	"ui/qqtotozmanager.ui"
	"ui/qqtotozviewer.cpp"
	"ui/qqtotozviewer.h"
	"ui/boardinfo/qqboardinfoprogressbar.cpp"
	"ui/boardinfo/qqboardinfoprogressbar.h"
	"ui/palmipede/delimitedcompleter.cpp"
	"ui/palmipede/delimitedcompleter.h"
	"ui/palmipede/qqdockpalmi.cpp"
	"ui/palmipede/qqdockpalmi.h"
	"ui/palmipede/qqpalmifileposter.cpp"
	"ui/palmipede/qqpalmifileposter.h"
	"ui/palmipede/qqpalmilineedit.cpp"
	"ui/palmipede/qqpalmilineedit.h"
	"ui/palmipede/qqpalmilineeditint.cpp"
	"ui/palmipede/qqpalmilineeditint.h"
	"ui/pinipede/qqduckpixmapitem.cpp"
	"ui/pinipede/qqduckpixmapitem.h"
	"ui/pinipede/qqhuntingview.cpp"
	"ui/pinipede/qqhuntingview.h"
	"ui/pinipede/qqhuntpixmapitem.cpp"
	"ui/pinipede/qqhuntpixmapitem.h"
	"ui/pinipede/qqpinioverlay.cpp"
	"ui/pinipede/qqpinioverlay.h"
	"ui/pinipede/qqpiniurlhelper.cpp"
	"ui/pinipede/qqpiniurlhelper.h"
	"ui/pinipede/qqpostparser.cpp"
	"ui/pinipede/qqpostparser.h"
	"ui/pinipede/qqtextbrowser.cpp"
	"ui/pinipede/qqtextbrowser.h"
	"ui/pinipede/qqwebimageviewer.cpp"
	"ui/pinipede/qqwebimageviewer.h"
	"ui/postdisplayfilters/qqplopifydisplayfilter.cpp"
	"ui/postdisplayfilters/qqplopifydisplayfilter.h"
	"ui/settingsmanager/qqboardssettings.cpp"
	"ui/settingsmanager/qqboardssettings.h"
	"ui/settingsmanager/qqboardssettings.ui"
	"ui/settingsmanager/qqboardwizard.cpp"
	"ui/settingsmanager/qqboardwizard.h"
	"ui/settingsmanager/qqboardwizardnative.cpp"
	"ui/settingsmanager/qqboardwizardnative.h"
	"ui/settingsmanager/qqbouchotsettingsdialog.cpp"
	"ui/settingsmanager/qqbouchotsettingsdialog.h"
	"ui/settingsmanager/qqbouchotsettingsdialog.ui"
	"ui/settingsmanager/qqfiltersettings.cpp"
	"ui/settingsmanager/qqfiltersettings.h"
	"ui/settingsmanager/qqfiltersettings.ui"
	"ui/settingsmanager/qqgeneralsettings.cpp"
	"ui/settingsmanager/qqgeneralsettings.h"
	"ui/settingsmanager/qqgeneralsettings.ui"
	"ui/settingsmanager/qqhuntsettings.cpp"
	"ui/settingsmanager/qqhuntsettings.h"
	"ui/settingsmanager/qqhuntsettings.ui"
	"ui/settingsmanager/qqnetworksettings.cpp"
	"ui/settingsmanager/qqnetworksettings.h"
	"ui/settingsmanager/qqnetworksettings.ui"
	"ui/settingsmanager/qqpalmisettings.cpp"
	"ui/settingsmanager/qqpalmisettings.h"
	"ui/settingsmanager/qqpalmisettingstablewidget.cpp"
	"ui/settingsmanager/qqpalmisettingstablewidget.h"
	"ui/settingsmanager/qqpalmisettings.ui"
	"ui/settingsmanager/qqtotozsettings.cpp"
	"ui/settingsmanager/qqtotozsettings.h"
	"ui/settingsmanager/qqtotozsettings.ui"
	"ui/totozmanager/qqtotozmanagerheader.cpp"
	"ui/totozmanager/qqtotozmanagerheader.h"
	"ui/totozmanager/qqtotozmanagerheader.ui"
)

qt_add_resources(
	SOURCES
	"rc/quteqoin_anims.qrc"
	"rc/quteqoin_defs.qrc"
	"rc/quteqoin_img.qrc"
)

add_executable(quteqoin
	${SOURCES})

target_link_libraries(quteqoin PRIVATE Qt6::Core)
target_link_libraries(quteqoin PRIVATE Qt6::Gui)
target_link_libraries(quteqoin PRIVATE Qt6::DBus)
target_link_libraries(quteqoin PRIVATE Qt6::Multimedia)
target_link_libraries(quteqoin PRIVATE Qt6::MultimediaWidgets)
target_link_libraries(quteqoin PRIVATE Qt6::Network)
target_link_libraries(quteqoin PRIVATE Qt6::Widgets)

set_target_properties(quteqoin PROPERTIES
	WIN32_EXECUTABLE ON
	MACOSX_BUNDLE ON
)

INSTALL(PROGRAMS quteqoin DESTINATION bin)
INSTALL(FILES quteqoin.desktop DESTINATION share/applications/)
INSTALL(FILES rc/QuteQoin-Icon.ico DESTINATION share/icons/)

