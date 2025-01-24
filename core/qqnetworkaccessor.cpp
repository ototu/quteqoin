#include "qqnetworkaccessor.h"
#include "ui/qqproxyauthdialog.h"

#include "qqsettings.h"

#include <QApplication>
#include <QAuthenticator>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkProxyFactory>
#include <QTimeZone>
#include <QTimer>
#include <QtDebug>

constexpr int NETWORK_REQUEST_TIMEOUT_MS = 60000;

QQNetworkAccessor::QQNetworkAccessor(QObject *parent) :
    QObject(parent)
{
    clearNetworkBackend();
}

void QQNetworkAccessor::updateProxySettings()
{
	QQSettings settings;

	auto nMode = settings.value(SETTINGS_NETWORK_MODE, DEFAULT_NETWORK_MODE).toString();
	if(! SETTINGS_NETWORK_MODES.contains(nMode))
	{
		nMode = DEFAULT_NETWORK_MODE;
		settings.remove(SETTINGS_NETWORK_MODE);
	}

	if(nMode == SETTINGS_NETWORK_MODE_SYSTEM)
	{
		QNetworkProxyFactory::setUseSystemConfiguration(true);
	}
	else
	{
		QNetworkProxyFactory::setUseSystemConfiguration(false);
		QNetworkProxy p;
		if(nMode == SETTINGS_NETWORK_MODE_DIRECT)
		{
			p.setType(QNetworkProxy::NoProxy);
		}
		else //if(Mode == SETTINGS_NETWORK_MODE_MANUAL)
		{
			auto host = settings.value(SETTINGS_NETWORK_PROXY_HOST, DEFAULT_NETWORK_PROXY_HOST).toString();
			auto port = settings.value(SETTINGS_NETWORK_PROXY_PORT, DEFAULT_NETWORK_PROXY_PORT).toUInt();
			auto isHttp = settings.value(SETTINGS_NETWORK_PROXY_IS_HTTP, DEFAULT_NETWORK_PROXY_IS_HTTP).toBool();

			p.setType(isHttp ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy);
			p.setHostName(host);
			p.setPort(port);

		}
		QNetworkProxy::setApplicationProxy(p);
	}
}

// Fast month string to int conversion. This code
// assumes that the Month name is correct and that
// the string is at least three chars long.
int QQNetworkAccessor::name_to_month(QByteArray month_str)
{
	switch (month_str[0]) {
	case 'J':
		switch (month_str[1]) {
		case 'a':
			return 1;
			break;
		case 'u':
			switch (month_str[2] ) {
			case 'n':
				return 6;
				break;
			case 'l':
				return 7;
				break;
			}
		}
		break;
	case 'F':
		return 2;
		break;
	case 'M':
		switch (month_str[2] ) {
		case 'r':
			return 3;
			break;
		case 'y':
			return 5;
			break;
		}
		break;
	case 'A':
		switch (month_str[1]) {
		case 'p':
			return 4;
			break;
		case 'u':
			return 8;
			break;
		}
		break;
	case 'O':
		return 10;
		break;
	case 'S':
		return 9;
		break;
	case 'N':
		return 11;
		break;
	case 'D':
		return 12;
		break;
	}
	return 0;
}

// Parse : "Tue, 09 Apr 2013 11:22:22 +0200"
QDateTime QQNetworkAccessor::parseRC822(const QString& string)
{
	auto pos = string.indexOf(',') + 1;
	auto minString = string.right(string.length() - pos);
	auto fields = minString.split(" ", Qt::SkipEmptyParts);
	QDate date(fields[2].toInt(), name_to_month(fields[1].toLatin1()), fields[0].toInt());
	auto time = QTime::fromString(fields[3], "hh:mm:ss");
	if(fields[4].startsWith('+'))
	{
		auto offset = QStringView{fields[4]}.right(4);
	    time = time.addSecs(offset.first(2).toInt() * 3600 + offset.right(2).toInt() * 60);
	}
	else if(fields[4].startsWith('-'))
	{
	    auto offset = QStringView{fields[4]}.right(4);
        time = time.addSecs(0 - (offset.first(2).toInt() * 3600 + offset.right(2).toInt() * 60));
	}
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
	QDateTime datetime(date, time, Qt::UTC);
#else
	QDateTime datetime(date, time, QTimeZone(QTimeZone::UTC));
#endif
	return datetime;
}

QNetworkReply * QQNetworkAccessor::httpGet(const QNetworkRequest &request)
{
	auto replyTimer = new QTimer(this);
	replyTimer->setSingleShot(true);

	auto reply = m_qnam->get(request);
	connect(replyTimer, SIGNAL(timeout()), this, SLOT(onRequestTimeout()));
	connect(reply, SIGNAL(finished()), replyTimer, SLOT(stop()));
	connect(reply, SIGNAL(downloadProgress(qint64,qint64)), replyTimer, SLOT(start()));
	connect(reply, SIGNAL(destroyed(QObject*)), this, SLOT(onRemoveTimer(QObject*)));
	replyTimer->start(NETWORK_REQUEST_TIMEOUT_MS);
	m_replyTimers.insert(reply, replyTimer);

	return reply;
}

QNetworkReply * QQNetworkAccessor::httpHead(const QNetworkRequest &request)
{
	return m_qnam->head(request);
}

QNetworkReply * QQNetworkAccessor::httpPost(const QNetworkRequest &request, const QByteArray &postData)
{
	return m_qnam->post(request, postData);
}

QNetworkReply * QQNetworkAccessor::httpPost(const QNetworkRequest &request, QHttpMultiPart *multiPart)
{
	return m_qnam->post(request, multiPart);
}

QNetworkReply * QQNetworkAccessor::httpPut(const QNetworkRequest &request, QIODevice *data)
{
	return m_qnam->put(request, data);
}

// Gestion des cookies
/**
 * @brief QQNetworkAccessor::setCookiesforUrl
 * @param cookieList
 * @param url
 * @return
 */
void QQNetworkAccessor::clearCookiesForUrl(const QUrl &url)
{
	auto cj = m_qnam->cookieJar();
	if(cj == nullptr)
		return;

	foreach (QNetworkCookie c, cj->cookiesForUrl(url)) {
		cj->deleteCookie(c);
	}
}

/**
 * @brief clearNetworkBackend
 */
void QQNetworkAccessor::clearNetworkBackend()
{
    if (! m_qnam.isNull())
        m_qnam->deleteLater();

	m_qnam = new QNetworkAccessManager(this);

	m_qnam->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);

	connect(m_qnam, &QNetworkAccessManager::finished, this, &QQNetworkAccessor::requestFinishedSlot);

	connect(m_qnam,
	        &QNetworkAccessManager::proxyAuthenticationRequired,
	        this,
	        &QQNetworkAccessor::onProxyAuthenticationRequired);
}

/**
 * @brief QQNetworkAccessor::setCookieforUrl
 * @param cookieList
 * @param url
 * @return
 */
bool QQNetworkAccessor::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
	auto cj = m_qnam->cookieJar();
	if(cj == nullptr)
		return false;

	return cj->setCookiesFromUrl(cookieList, url);
}

// Gestion du proxy
QMutex QQNetworkAccessor::m_proxyPopupMutex;
QString QQNetworkAccessor::m_proxyUser;
QString QQNetworkAccessor::m_proxyPasswd;

void QQNetworkAccessor::onProxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator)
{
	Q_UNUSED(proxy)

	//Premier echec
	if(QQNetworkAccessor::m_proxyUser.size() != 0 &&
	        authenticator->user() != QQNetworkAccessor::m_proxyUser)
	{
		authenticator->setUser(QQNetworkAccessor::m_proxyUser);
		authenticator->setPassword(QQNetworkAccessor::m_proxyPasswd);
	}
	else // Echec ulterieur, ou pas de conf proxy, on redemande
	{
		if(QQNetworkAccessor::m_proxyPopupMutex.tryLock())
		{
			auto proxyDialog = new QQProxyAuthDialog();
			proxyDialog->setLogin(QQNetworkAccessor::m_proxyUser);
			proxyDialog->setPasswd(QQNetworkAccessor::m_proxyPasswd);
			if(proxyDialog->exec() == QDialog::Accepted)
			{
				QQNetworkAccessor::m_proxyUser = proxyDialog->login();
				QQNetworkAccessor::m_proxyPasswd = proxyDialog->passwd();

				authenticator->setUser(QQNetworkAccessor::m_proxyUser);
				authenticator->setPassword(QQNetworkAccessor::m_proxyPasswd);
			}
			QQNetworkAccessor::m_proxyPopupMutex.unlock();
			//startBouchots();
		}
		else
		{
			QApplication::processEvents();
		}
	}
}

void QQNetworkAccessor::onRemoveTimer(QObject *obj)
{
	auto timer = m_replyTimers.take(static_cast<QNetworkReply *>(obj));

	delete timer;
}

void QQNetworkAccessor::onRequestTimeout()
{
	auto replyTimersKeys = m_replyTimers.keys();
	for (auto reply : std::as_const(replyTimersKeys)) {
		auto timer = m_replyTimers.value(reply);
		if(! timer->isActive())
		{
			reply->abort();
			qDebug() << Q_FUNC_INFO << reply->url() << "aborted";
			m_replyTimers.remove(reply);
			reply->deleteLater();
			timer->deleteLater();
		}
	}
}


