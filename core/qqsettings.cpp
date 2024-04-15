#include "qqsettings.h"

#include "qqbouchot.h"
#include "ui/qqpinipede.h"

#include <QtDebug>
#include <QFile>
#include <QList>
#include <QPair>
#include <QMessageBox>

//constexpr int DEFAULT_TOTOZ_MODE = QQSettings::Bald_Mode;

QQSettings::QQSettings(QObject * parent) :
    QSettings(parent)
{
	fillStaticPalmiShortcuts();
	if(! contains(SETTINGS_PALMI_SHORCUTS))
		m_userPalmiShortcuts = defaultUserPalmiShortcuts();
	else
		m_userPalmiShortcuts = savedUserPalmiShortcuts();
}

QQSettings::~QQSettings() = default;

void QQSettings::removeBouchot(const QString &name)
{
	beginGroup(QString::fromLatin1("bouchot"));
	beginGroup(name);

	remove(SETTINGS_BOUCHOT_COLOR);
	remove(SETTINGS_BOUCHOT_ALIASES);
	remove(SETTINGS_BOUCHOT_REFRESH);
	remove(SETTINGS_BOUCHOT_LOGIN);
	remove(SETTINGS_BOUCHOT_COOKIES);
	remove(SETTINGS_BOUCHOT_UA);
	remove(SETTINGS_BOUCHOT_BACKENDURL);
	remove(SETTINGS_BOUCHOT_POSTURL);
	remove(SETTINGS_BOUCHOT_POSTDATA);
	remove(SETTINGS_BOUCHOT_GROUP);
	remove(SETTINGS_BOUCHOT_SLIPTYPE);

	endGroup();
	endGroup();

	QStringList listBouchotsNames = listBouchots();
	listBouchotsNames.removeAll(name);
	setValue(SETTINGS_LIST_BOUCHOTS, QVariant(listBouchotsNames.join(QChar::fromLatin1(BOUCHOTS_SPLIT_CHAR))));
}

QStringList QQSettings::listTotozSrvPresets()
{
	QDir dir(SETTINGS_TOTOZ_PRESETS_PATH);
	return dir.entryList();
}

QQTotozSrvPreset QQSettings::getTotozSrvPreset(const QString& totozSrvPreset, bool labelOnly)
{
	QFile file(QString::fromLatin1(SETTINGS_TOTOZ_PRESETS_PATH) + "/" + totozSrvPreset);
	QQTotozSrvPreset preset;

	if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return preset;

	while (!file.atEnd())
	{
		QString line = QString::fromUtf8(file.readLine()).trimmed();
		int pos = line.indexOf('=');
		QString key = line.left(pos);
		QString value = line.right(line.length() - (pos + 1));

		if(key == "label")
		{
			preset.label = value;
			if(labelOnly)
				break;
		}
		else if(key == "url")
			preset.url = value;
		else if(key == "base_img_dir")
			preset.baseImgDir = value;
		else if(key == "img_suffix")
			preset.imgSuffix = value;
		else if(key == "search_pattern")
			preset.searchPattern = value;
		else if(key == "info_pattern")
			preset.infoPattern = value;
		else
			qWarning() << "Unknow totoz Server preset key :" << key;
	}
	return preset;
}

QList<QQEmojiCat> QQSettings::listEmojis()
{
	QFile file(QString::fromLatin1(SETTINGS_EMOJI_LIST_FILE));
	QList<QQEmojiCat> emojis;

	if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return emojis;

	QQEmojiCat cat;

	while (!file.atEnd())
	{
		QString line = QString::fromUtf8(file.readLine()).trimmed();
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
		QStringList fields = line.split("\t", Qt::SkipEmptyParts);
#else
		QStringList fields = line.split("\t", QString::SkipEmptyParts);
#endif

		if(fields.length() == 3)
		{
			if(fields.at(0) == "group")
			{
				if(cat.emojis.length() > 0) // Nouvelle categorie
					emojis.append(cat);

				cat.emojis.clear();
				cat.symbol = fields.at(1);
				cat.name = fields.at(2);
				cat.type = CAT;
			}
			else if(fields.at(0) == "emoji")
			{
				QQEmojiDef emoji;
				emoji.symbol = fields.at(1);
				emoji.name = fields.at(2);
				emoji.type = EMOJI;
				cat.emojis.append(emoji);
			}
		}
	}
	if(cat.emojis.length() > 0) // Derniere categorie
		emojis.append(cat);

	return emojis;
}

void QQSettings::saveBouchot(const QString &name, const QQBouchot::QQBouchotSettings &bouchotSettings)
{
	beginGroup(QString::fromLatin1("bouchot"));
	beginGroup(name);

	setValue(SETTINGS_BOUCHOT_COLOR, bouchotSettings.colorToString());
	setValue(SETTINGS_BOUCHOT_ALIASES, bouchotSettings.aliasesToString());
	setValue(SETTINGS_BOUCHOT_REFRESH, bouchotSettings.refreshToString());
	setValue(SETTINGS_BOUCHOT_LOGIN, bouchotSettings.login());
	setValue(SETTINGS_BOUCHOT_COOKIES, bouchotSettings.cookies());
	setValue(SETTINGS_BOUCHOT_UA, bouchotSettings.ua());
	setValue(SETTINGS_BOUCHOT_BACKENDURL, bouchotSettings.backendUrl());
	setValue(SETTINGS_BOUCHOT_POSTURL, bouchotSettings.postUrl());
	setValue(SETTINGS_BOUCHOT_POSTDATA, bouchotSettings.postData());
	setValue(SETTINGS_BOUCHOT_GROUP, bouchotSettings.group());
	setValue(SETTINGS_BOUCHOT_SLIPTYPE, bouchotSettings.slipType());
	setValue(SETTINGS_BOUCHOT_STRICTHTTPSCERTIF, bouchotSettings.isStrictHttpsCertif());

	endGroup();
	endGroup();

	QStringList listBouchotsNames = listBouchots();
	if(! listBouchotsNames.contains(name, Qt::CaseInsensitive))
		listBouchotsNames.append(name);
	setValue(SETTINGS_LIST_BOUCHOTS, QVariant(listBouchotsNames.join(QChar::fromLatin1(BOUCHOTS_SPLIT_CHAR))));
}

QQBouchot * QQSettings::loadBouchot(const QString &name)
{
	QQBouchot::QQBouchotSettings newBouchotSettings;

	beginGroup(QString::fromLatin1("bouchot"));
	beginGroup(name);

	// Pas d'url de backend , pas de bouchot
	if(! contains(SETTINGS_BOUCHOT_BACKENDURL))
		return nullptr;

	newBouchotSettings.setColorFromString(value(SETTINGS_BOUCHOT_COLOR, "").toString());
	newBouchotSettings.setAliasesFromString(value(SETTINGS_BOUCHOT_ALIASES, "").toString());
	newBouchotSettings.setRefresh(value(SETTINGS_BOUCHOT_REFRESH, "").toInt());
	newBouchotSettings.setLogin(value(SETTINGS_BOUCHOT_LOGIN, "").toString());
	newBouchotSettings.setCookies(value(SETTINGS_BOUCHOT_COOKIES, "").toString());
	newBouchotSettings.setUa(value(SETTINGS_BOUCHOT_UA, "").toString());
	newBouchotSettings.setBackendUrl(value(SETTINGS_BOUCHOT_BACKENDURL, "").toString());
	newBouchotSettings.setPostUrl(value(SETTINGS_BOUCHOT_POSTURL, "").toString());
	newBouchotSettings.setPostData(value(SETTINGS_BOUCHOT_POSTDATA, "").toString());
	newBouchotSettings.setGroup(value(SETTINGS_BOUCHOT_GROUP, "").toString());
	newBouchotSettings.setSlipType((QQBouchot::TypeSlip) value(SETTINGS_BOUCHOT_SLIPTYPE, "").toInt());
	newBouchotSettings.setStrictHttpsCertif(value(SETTINGS_BOUCHOT_STRICTHTTPSCERTIF, true).toBool());

	endGroup(); //beginGroup(name);
	endGroup(); //beginGroup(QString::fromLatin1("bouchot"));


	auto newBouchot = new QQBouchot(name, nullptr);
	newBouchot->setSettings(newBouchotSettings);

	return newBouchot;
}

QStringList QQSettings::listBouchots()
{
	QString bouchots = value(SETTINGS_LIST_BOUCHOTS, "").toString();
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	return bouchots.split(QChar::fromLatin1(BOUCHOTS_SPLIT_CHAR),
	                      Qt::SkipEmptyParts);
#else
	return bouchots.split(QChar::fromLatin1(BOUCHOTS_SPLIT_CHAR),
	                      QString::SkipEmptyParts);
#endif
}

//////////////////////////////////////////////////////////////
/// \brief QQSettings::setValueWithDefault
/// \param key
/// \param newValue
/// \param defaultValue
/// \return true if the stored value changed
///
bool QQSettings::setValueWithDefault(const QString &key, const QVariant &newValue, const QVariant &defaultValue)
{
	QVariant oldValue = value(key, defaultValue);

	if(newValue == defaultValue)
		remove(key);
	else
		setValue(key, newValue);

	return (oldValue != newValue);
}

void QQSettings::setUserPalmiShorcuts(const QList< QPair<QChar, QString> >& userPalmiShortcuts)
{
	m_userPalmiShortcuts = userPalmiShortcuts;
	if(userPalmiShortcuts != defaultUserPalmiShortcuts())
	{
		QStringList listShortcuts;
		for (const auto &shortcut : std::as_const(m_userPalmiShortcuts))
			listShortcuts.append(QString(shortcut.first).append(shortcut.second));

		setValue(SETTINGS_PALMI_SHORCUTS, listShortcuts);
	}
	else
		remove(SETTINGS_PALMI_SHORCUTS);
}

void QQSettings::fillStaticPalmiShortcuts()
{
	m_staticPalmiShortcuts.append(qMakePair(QChar('b'), QString("<b>%s</b>")));
	m_staticPalmiShortcuts.append(qMakePair(QChar('i'), QString("<i>%s</i>")));
	m_staticPalmiShortcuts.append(qMakePair(QChar('m'), QString("====> <b>Moment %s</b> <====")));
	m_staticPalmiShortcuts.append(qMakePair(QChar('o'), QString("_o/* <b>BLAM</b>! ")));
	m_staticPalmiShortcuts.append(qMakePair(QChar('p'), QString("_o/* <b>paf!</b> ")));
	m_staticPalmiShortcuts.append(qMakePair(QChar('s'), QString("<s>%s</s>")));
	m_staticPalmiShortcuts.append(qMakePair(QChar('u'), QString("<u>%s</u>")));
}

QList< QPair<QChar, QString> > QQSettings::defaultUserPalmiShortcuts()
{
	QList< QPair<QChar, QString> > shortcuts;
	shortcuts.append(qMakePair(QChar('j'), QString("\\o/")));
	shortcuts.append(qMakePair(QChar('k'), QString("/o\\")));
	return shortcuts;
}

QList< QPair<QChar, QString> > QQSettings::savedUserPalmiShortcuts() const
{
	QList< QPair<QChar, QString> > res;

	QStringList listShortcuts = value(SETTINGS_PALMI_SHORCUTS, QStringList()).toStringList();

	for(int i = 0; i < listShortcuts.size(); i++)
	{
		QString val = listShortcuts.at(i);
		QChar key = val.at(0);
		QString str = val.remove(0, 1);
		res.append(qMakePair(key, str));
	}

	return res;
}
