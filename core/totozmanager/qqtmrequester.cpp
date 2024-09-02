#include "qqtmrequester.h"

#include "core/qqsettings.h"

#include <QtDebug>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QXmlStreamAttributes>

QQTMRequester::QQTMRequester(QObject * parent) :
	QQNetworkAccessor(parent)
{
	m_currKey.clear();
	m_netReply = nullptr;
}

QQTMRequester::~QQTMRequester()
{
	if(m_netReply != nullptr)
		m_netReply->abort();
}

void QQTMRequester::requestFinishedSlot(QNetworkReply * reply)
{
	if(reply->error() == QNetworkReply::NoError)
	{
		QByteArray data = reply->readAll();

		QXmlStreamReader xmlReader(data);

		int numResults = 0;
		QStringList totozes;
		while (xmlReader.readNextStartElement())
		{
            if(xmlReader.name().compare(QString::fromUtf8("totozes")) == 0)
			{
				auto attributes = xmlReader.attributes();
				if (attributes.hasAttribute("results"))
				{
					bool ok = false;
					numResults = attributes.value("results").toInt(&ok, 10);
					if (!ok)
						numResults = 0;
				}
			}

            if(xmlReader.name().compare(QString::fromUtf8("name")) == 0)
			{
				totozes.append(xmlReader.readElementText(QXmlStreamReader::SkipChildElements));
				xmlReader.skipCurrentElement(); // next <totoz>....</totoz>
			}
		}
		if (xmlReader.hasError()) {
			qDebug() << Q_FUNC_INFO << "xmlReader.hasError() : " << xmlReader.error() << ", "  << xmlReader.errorString();
		}
		else
		{
			if(numResults > 0 &&
			        totozes.size() < numResults) //Paging
				searchTotoz(m_currKey, m_totozes.size());
			else
			{
				m_totozes = totozes;
				emit requestFinished();
			}
		}
	}
	else
	{
		qDebug() << Q_FUNC_INFO << "reply->error() : " << reply->error() << ", " << reply->errorString();
		emit requestFinished();
	}

	m_netReply = nullptr;
	reply->deleteLater();
}

void QQTMRequester::searchTotoz(const QString & key)
{
	cancel();
	m_totozes.clear();
	m_currKey = key;
	searchTotoz(key, 0);
}

void QQTMRequester::cancel()
{
	if(m_netReply != nullptr)
		m_netReply->abort();
}

void QQTMRequester::searchTotoz(const QString & key, int offset)
{
	QQSettings settings;

	QString searchPattern = settings.value(SETTINGS_TOTOZ_SERVER_QUERY_PATTERN,
										   DEFAULT_TOTOZ_SERVER_QUERY_PATTERN).toString();
	searchPattern.replace("%t", key.toLatin1().toPercentEncoding());
	searchPattern.replace("%o", QString::number(offset));


	QString urlString = settings.value(SETTINGS_TOTOZ_SERVER_URL, DEFAULT_TOTOZ_SERVER_URL).toString();
	urlString.append("/").append(searchPattern);

	QUrl url(urlString);

	QNetworkRequest request(url);
	request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
						 QNetworkRequest::PreferCache);

	m_netReply = httpGet(request);
}
