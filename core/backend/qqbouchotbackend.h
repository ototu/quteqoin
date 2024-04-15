#ifndef QQBOUCHOTBACKEND_H
#define QQBOUCHOTBACKEND_H

#include "core/qqpost.h"
#include "core/qqtypes.h"
#include "core/qqmussel.h"
#include "qqbackend.h"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

class QNetworkReply;
class QQSettings;


class QQBouchotBackend : public QQBackend
{
	Q_OBJECT

public:
	enum TypeSlip { SlipTagsEncoded = 0, SlipTagsRaw = 1 };
	enum TypeRequest { UnknownRequest = 0, BackendRequest = 1, PostRequest = 2 };

	class QQBouchotSettings
	{
	public:
		QStringList aliases() const { return m_aliases; }
		void setAliases(const QStringList &newAliases) { m_aliases = newAliases; }
		QString aliasesToString() const { return this->m_aliases.join(QChar::fromLatin1(Separator)); }
		void setAliasesFromString(const QString &newAliasesString) { m_aliases = newAliasesString.split(QChar::fromLatin1(Separator)); }
		bool containsAlias(const QString & alias) { return m_aliases.contains(alias); }

		QString backendUrl() const { return m_backendUrl; }
		void setBackendUrl(const QString &newBackendUrl) { m_backendUrl = newBackendUrl; }

		QColor color() const { return m_color; }
		QColor colorLight() const { return m_color.lighter(110); }
		void setColor(const QColor &newColor) { m_color = newColor; }
		QString colorToString() const { return m_color.name(); }
		void setColorFromString(const QString &newColorString)
		{
			m_color = QColor::fromString(newColorString);
		}

		QString cookie() const { return m_cookie; }
		void setCookie(const QString &newCookie) { m_cookie = newCookie; }

		QString login() const { return m_login; }
		void setLogin(const QString &newLogin) { m_login = newLogin; }

		QString postData() const { return m_postData; }
		void setPostData(const QString &newPostData) { m_postData = newPostData; }

		QString postUrl() const { return m_postUrl; }
		void setPostUrl(const QString &newPostUrl) { m_postUrl = newPostUrl; }

		int refresh() const { return m_refresh; }
		void setRefresh(int newRefresh) { m_refresh = newRefresh; }
		QString refreshToString() const { return QString::number(this->m_refresh); }
		void setRefreshFromString(const QString &newRefreshString) { m_refresh = newRefreshString.toInt(); }

		QQBouchotBackend::TypeSlip slipType() const { return m_slipType; }
		void setSlipType(QQBouchotBackend::TypeSlip newSlipType) { m_slipType = newSlipType; }

		QString ua() const { return m_ua; }
		void setUa(const QString &newUA) { m_ua = newUA; }

		QString group() const { return m_group; }
		void setGroup(const QString & newGroup) { m_group = newGroup; }

		bool isStrictHttpsCertif() const { return m_strictHttpsCertif; }
		void setStrictHttpsCertif(bool strictHttpsCertif) { m_strictHttpsCertif = strictHttpsCertif; }

	private:
		QStringList m_aliases;
		QString m_backendUrl;
		QColor m_color;
		QString m_cookie;
		QString m_login;
		QString m_postData;
		QString m_postUrl;
		int m_refresh;
		TypeSlip m_slipType;
		QString m_ua;
		QString m_group;
		bool m_strictHttpsCertif;

		static const char Separator = ',';
	};

	QQBouchotBackend(const QString &name, QObject *parent = 0);
	~QQBouchotBackend();

	QString name() const { return m_name; }
	QList<QQMussel> lastPosters() const { return m_lastPosters; }

	void parseBackend(const QByteArray &data, const QString &contentType);
	void parseBackendTSV(const QByteArray &data);
	void parseBackendXML(const QByteArray &data);
	void postMessage(const QString &message);

	QQBouchotSettings settings() { return m_bSettings; }
	void setSettings(QQBouchotBackend bouchotRef) { setSettings(bouchotRef.settings()); }
	void setSettings(const QQBouchotSettings &newSettings);

	void startRefresh();
	void stopRefresh();
	int currentRefreshInterval();

	QQListPostPtr takeNewPosts();
	QQListPostPtr postsHistory() { return m_history; }
	void setNewPostsFromHistory();

	void addToBak(const QString &name, const bool isAuth);
	bool isBaked(const QString &name, const bool isAuth) const;
	void removeFromBak(const QString &name, const bool isAuth);
	void addToPlopify(const QString &name, const bool isAuth);
	bool isPlopified(const QString &name, const bool isAuth) const;
	void removeFromPlopify(const QString &name, const bool isAuth);

	QuteQoin::QQBoardStates boardState();
	void resetStatus();
	void setHasNewResponse();
	void setHasBigorno();

	bool isReadOnly();

	void registerForEventNotification(QObject *receiver, QQBouchotEvents events);

	virtual bool event(QEvent *e);

	virtual QQBackend::BackendType backendType() { return QQBackend::Bouchot; }

	//static
	static QQBouchotSettings getBouchotDef(const QString &bouchotName);
	static QStringList getBouchotDefNameList();
	static QQBouchotBackend * bouchot(const QString &bouchotName);
	static QList<QQBouchotBackend *> listBouchots();
	static QList<QQBouchotBackend *> listBouchotsGroup(const QString &groupName);
	static QStringList listGroups();

public slots:
	void slotSslErrors(const QList<QSslError> &errors);
	void unregisterForEventNotification(QObject *receiver);

signals:
	void destroyed(QQBouchotBackend *bouchot);
	void groupChanged(QQBouchotBackend *bouchot, QString oldGroupName);
	void lastPostersUpdated();
	void refreshStarted();
	void refreshOK();
	void refreshError(QString &errMsg);
	void removed(QString name, QString groupName);


protected slots:
	void fetchBackend();
	void requestFinishedSlot(QNetworkReply *reply);
	void insertNewPost(QQPost &newPost);
	void parsingFinished();

private:
	void checkBackendUrlModified(const QString &oldBackendUrl);
	void checkGroupModified(const QString &oldGroupName);
	void updateLastUsers();
	void sendBouchotEvents();
	float currentRefreshRatio();

	QQListPostPtr m_history;
	bool m_hasXPostId; //false = unknown
	QStringList m_xPostIds;
	qlonglong m_lastId;
	QQListPostPtr m_newPostHistory;
	QQBouchotSettings m_bSettings;
	QTimer m_timer;

	int m_deltaTimeH;
	QList<QQMussel> m_lastPosters;

	QList< QPair<QString, bool> > m_bakList;
	QList< QPair<QString, bool> > m_plopifyList;

	QuteQoin::QQBoardStates m_state;

	//static
	static QHash<QString, QQBouchotBackend *> s_hashBouchots;

	int m_refreshRatioIndex;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQBouchotBackend::QQBouchotEvents)

#endif // QQBOUCHOTBACKEND_H
