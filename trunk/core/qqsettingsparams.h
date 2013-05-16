#ifndef QQSETTINGSPARAMS_H
#define QQSETTINGSPARAMS_H

#define MAX_CACHE_AGE_DAYS 60

#define SETTINGS_TOTOZ_PRESETS_PATH ":/totoz"

// MAIN WINDOW
#define SETTINGS_MAINWINDOW_STATE "mainwindow_state"
#define SETTINGS_MAINWINDOW_GEOMETRY "mainwindow_geometry"

// GENERAL
#define SETTINGS_GENERAL_MAX_HISTLEN "max_hist_len"
#define DEFAULT_GENERAL_MAX_HISTLEN 500

#define SETTINGS_GENERAL_DEFAULT_UA "default_ua"
#define DEFAULT_GENERAL_DEFAULT_UA "quteqoin crash edition"

#define SETTINGS_GENERAL_DEFAULT_LOGIN "default_login"
#define DEFAULT_GENERAL_DEFAULT_LOGIN ""

#define SETTINGS_GENERAL_WEBSEARCH_URL "default_websearch_url"
#define DEFAULT_GENERAL_WEBSEARCH_URL "http://www.google.fr/search?q=%s&ie=utf-8&oe=utf-8"

#define SETTINGS_GENERAL_DEFAULT_FONT "default_font"
#define DEFAULT_GENERAL_DEFAULT_FONT QFont().toString()

// TOTOZ
#define SETTINGS_TOTOZ_BOOKMARKLIST "totoz_bookmarklist"

#define SETTINGS_TOTOZ_SERVER_URL "totoz_server_url"
#define DEFAULT_TOTOZ_SERVER_URL "http://totoz.eu"

#define SETTINGS_TOTOZ_SERVER_BASE_IMG "totoz_server_base_img"
#define DEFAULT_TOTOZ_SERVER_BASE_IMG "img"

#define SETTINGS_TOTOZ_SERVER_NAME_SUFFIX "totoz_server_name_suffix"
#define DEFAULT_TOTOZ_SERVER_NAME_SUFFIX ""

#define SETTINGS_TOTOZ_SERVER_ALLOW_SEARCH "totoz_serveur_allow_search"
#define DEFAULT_TOTOZ_SERVER_ALLOW_SEARCH true

#define SETTINGS_TOTOZ_SERVER_QUERY_PATTERN "totoz_query_pattern"
#define DEFAULT_TOTOZ_SERVER_QUERY_PATTERN "search.xml?terms=%t&offset=%o"

#define SETTINGS_TOTOZ_VISUAL_MODE "totoz_visual_mode"
#define TOTOZ_VISUAL_MODE_DISABLED "Disabled"
#define TOTOZ_VISUAL_MODE_INLINE "Inline"
#define TOTOZ_VISUAL_MODE_ON_HOVER "On hover"
//#define DEFAULT_TOTOZ_VISUAL_MODES (QStringList() << TOTOZ_VISUAL_MODE_DISABLED << TOTOZ_VISUAL_MODE_INLINE << TOTOZ_VISUAL_MODE_ON_HOVER)
#define DEFAULT_TOTOZ_VISUAL_MODES (QStringList() << TOTOZ_VISUAL_MODE_DISABLED << TOTOZ_VISUAL_MODE_ON_HOVER)
#define DEFAULT_TOTOZ_VISUAL_MODE "On hover"

// BOUCHOTS
#define BOUCHOTS_SPLIT_CHAR ';'

#define SETTINGS_LIST_BOUCHOTS "bouchots"

#define SETTINGS_BOUCHOT_COLOR "color"
#define SETTINGS_BOUCHOT_ALIASES "aliases"
#define SETTINGS_BOUCHOT_REFRESH "refresh"
#define SETTINGS_BOUCHOT_LOGIN "login"
#define SETTINGS_BOUCHOT_COOKIES "cookies"
#define SETTINGS_BOUCHOT_UA "ua"
#define SETTINGS_BOUCHOT_BACKENDURL "backendUrl"
#define SETTINGS_BOUCHOT_POSTURL "postUrl"
#define SETTINGS_BOUCHOT_POSTDATA "postData"
#define SETTINGS_BOUCHOT_GROUP "group"
#define SETTINGS_BOUCHOT_SLIPTYPE "slipType"

// PALMI
#define SETTINGS_PALMI_MINI "palmi_minimized"
#define DEFAULT_PALMI_MINI false
#define SETTINGS_PALMI_HIDDEN "palmi_hidden"
#define DEFAULT_PALMI_HIDDEN false
#define SETTINGS_PALMI_SHORCUTS "palmi_shortcuts"
#define SETTINGS_PALMI_SHORTCUTS_MIN_KEY Qt::Key_Space
#define SETTINGS_PALMI_SHORTCUTS_MAX_KEY Qt::Key_AsciiTilde


#endif // QQSETTINGSPARAMS_H
