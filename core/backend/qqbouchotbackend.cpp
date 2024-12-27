#include "qqbouchotbackend.h"

#include "core/qqbackendupdatedevent.h"
#include "core/qqboardstatechangeevent.h"
#include "core/qqpurgebouchothistoevent.h"
#include "core/qqsettings.h"
#include "core/parsers/qqbackendparser.h"
#include "core/parsers/qqtsvparser.h"
#include "core/parsers/qqcustomxmlparser.h"

#include <QApplication>
#include <QBuffer>
#include <QDateTime>
#include <QtDebug>
#include <QNetworkCookie>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSslError>

#define X_POST_ID_HEADER "X-Post-Id"

typedef struct QQBouchotDef
{
    char name[16];
    char getUrl[64];
    char postUrl[64];
    char postData[64];
    char color[16];
    char alias[64];
    char cookieProto[64];
    QQBouchotBackend::TypeSlip typeSlip;
} QQBouchotDef;

//Définition des bouchots préconfigurés
// tiré d'olcc by Chrisix
#define BOUCHOTS_DEF_SIZE 12
QQBouchotDef bouchotsDef[] =
{
    { "dlfp", "http://linuxfr.org/board/index.xml", "http://linuxfr.org/board", "board[message]=%m",
      "#dac0de", "linuxfr,beyrouth,passite,dapassite", "linuxfr.org_session=", QQBouchotBackend::SlipTagsEncoded },
    { "batavie", "http://batavie.leguyader.eu/remote.xml", "http://batavie.leguyader.eu/index.php/add", "message=%m",
      "#ffccaa", "llg", "", QQBouchotBackend::SlipTagsRaw },
    { "euromussels", "http://euromussels.eu/?q=tribune.xml&last_id=%i", "http://euromussels.eu/?q=tribune/post", "message=%m",
      "#d0d0ff", "euro,euroxers", "", QQBouchotBackend::SlipTagsEncoded },
    { "finss", "http://finss.fr/drupal/node/95/xml", "http://finss.fr/drupal/node/95/post", "message=%m",
      "#d0ffd0", "finss", "", QQBouchotBackend::SlipTagsRaw },
    { "sveetch", "http://sveetch.net/tribune/remote/xml/?last=%i", "http://sveetch.net/tribune/post/xml", "content=%m",
      "#ededdb", "shoop,dax", "", QQBouchotBackend::SlipTagsRaw },
    { "moules", "http://moules.org/board/last.php?backend=tsv&id=%i&order=desc", "http://moules.org/board/add.php", "message=%m",
      "#ffe3c9", "", "", QQBouchotBackend::SlipTagsRaw },
    { "hadoken", "http://hadoken.free.fr/board/remote.php", "http://hadoken.free.fr/board/post.php", "message=%m",
      "#77aadd", "axel,waf", "", QQBouchotBackend::SlipTagsEncoded },
    { "gabuzomeu", "http://gabuzomeu.fr/tribune.xml", "http://gabuzomeu.fr/tribune/post", "message=%m",
      "#aaffbb", "", "", QQBouchotBackend::SlipTagsRaw },
    { "see", "http://tout.essaye.sauf.ca/tribune.xml", "http://tout.essaye.sauf.ca/tribune/post", "message=%m",
      "#ffd0d0", "schee,seeschloss", "", QQBouchotBackend::SlipTagsRaw },
    { "devnewton", "http://b3.bci.im/legacy/xml", "http://b3.bci.im/legacy/post", "message=%m",
      "#666666", "", "", QQBouchotBackend::SlipTagsEncoded },
    { "faab", "http://ratatouille.leguyader.eu/data/backend.xml", "http://ratatouille.leguyader.eu/add.php", "message=%m",
      "#C5D068", "ratatouille", "", QQBouchotBackend::SlipTagsRaw },
    { "sauf.ca", "http://sauf.ca/feeds/all.tsv", "", "",
      "#4aff47", "", "", QQBouchotBackend::SlipTagsRaw }
};

#define REFRESH_RATIOS_SIZE 15
#define REFRESH_RATIOS_MID 7
float refreshRatiosArr[] = {
    0.1, 0.3, 0.5, 0.7, 0.8, 0.9,
    1.0, 1.0, 1.0,
    1.5, 2.0, 3.0, 5.0, 7.0, 10.0
};
#define REFRESH_NEW_POSTS_THRESHOLD 5
#define QQ_MIN_REFRESH_DELAY_S 3

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::QQBouchot
/// \param name
/// \param parent
///
QQBouchotBackend::QQBouchotBackend(const QString &name, QObject *parent) :
    QQBackend(name, parent),
    m_hasXPostId(false), // unknown
    m_lastId(-1),
    m_deltaTimeH(-1) // unknown
{
    m_bSettings.setRefresh(DEFAULT_BOUCHOT_REFRESH);

    m_state.hasBigorno = false;
    m_state.hasError = false;
    m_state.hasNewPosts = false;
    m_state.hasResponse = false;

    QQBouchotBackend::s_hashBouchots.insert(m_name, this);

    m_refreshRatioIndex = REFRESH_RATIOS_MID;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::~QQBouchot
///
QQBouchotBackend::~QQBouchotBackend()
{
    QQBouchotBackend::s_hashBouchots.remove(m_name);
    emit destroyed(this);
    delete m_parser;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::postMessage
/// \param message
///
void QQBouchotBackend::postMessage(const QString &message)
{
    //Si l'on poste on remet un refresh "normal" si en mode lent
    if(currentRefreshRatio() > 1.0)
        m_refreshRatioIndex = REFRESH_RATIOS_MID;

    QString url = m_bSettings.postUrl();
    QByteArray postData = m_bSettings.postData().toLatin1();
    QByteArray mark("%m");

    if(postData.contains(mark))
        postData.replace(mark, QUrl::toPercentEncoding(message));
    else
        postData=QUrl::toPercentEncoding(message);

    QNetworkRequest request(QUrl::fromUserInput(url));
    request.setAttribute(QNetworkRequest::User, QQBouchotBackend::PostRequest);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded; charset=UTF-8");

    QString ua = m_bSettings.ua();
    if(ua.isEmpty())
    {
        QQSettings settings;
        ua = settings.value(SETTINGS_GENERAL_DEFAULT_UA, DEFAULT_GENERAL_DEFAULT_UA).toString();
        if(ua.isEmpty())
            ua=QString(DEFAULT_GENERAL_DEFAULT_UA);
    }

    request.setRawHeader("User-Agent", ua.toLatin1());

    if(! m_bSettings.cookie().isEmpty())
        request.setRawHeader("Cookie", m_bSettings.cookie().toLatin1());

    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Accept-Encoding","gzip, deflate");
    request.setRawHeader("Referer", request.url().toString().toLatin1());
    httpPost(request, postData);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::setSettings
/// \param newSettings
///
void QQBouchotBackend::setSettings(const QQBouchotSettings &newSettings)
{
    QString oldGroup = m_bSettings.group();
    QString oldBackendURL = m_bSettings.backendUrl();
    m_bSettings = newSettings;
    checkGroupModified(oldGroup);
    checkBackendUrlModified(oldBackendURL);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::startRefresh
///
void QQBouchotBackend::startRefresh()
{
    if(m_bSettings.refresh() <= 0)
        return;

    //Connection du signal
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(fetchBackend()));

    //Première récuperation
    fetchBackend();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::stopRefresh
///
void QQBouchotBackend::stopRefresh()
{
    m_timer.disconnect();
    m_timer.stop();

    m_refreshRatioIndex = REFRESH_RATIOS_MID;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::currentRefreshInterval
/// \return
///
int QQBouchotBackend::currentRefreshInterval()
{
    return qMax(int(m_bSettings.refresh() * 1000 * currentRefreshRatio()), QQ_MIN_REFRESH_DELAY_S);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::takeNewPosts
/// \return
///
QQListPostPtr QQBouchotBackend::takeNewPosts()
{
    QQListPostPtr list(m_newPostHistory);
    m_newPostHistory.clear();
    return list;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::setNewPostsFromHistory
///
void QQBouchotBackend::setNewPostsFromHistory()
{
    int index = m_history.length();
    if(! m_newPostHistory.isEmpty())
    {
        QQPostPtr firstNew = m_newPostHistory.first();
        index = m_history.indexOf(firstNew);
    }
    while (--index >= 0)
        m_newPostHistory.prepend(m_history.at(index));

    m_state.hasNewPosts = true;
    sendBouchotEvents();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::addToBak
/// \param name
/// \param isAuth
///
void QQBouchotBackend::addToBak(const QString &name, const bool isAuth)
{
    QPair<QString, bool> newItem = qMakePair(name, isAuth);
    if(! m_bakList.contains(newItem))
        m_bakList.append(newItem);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::isBaked
/// \param name
/// \param isAuth
///
bool QQBouchotBackend::isBaked(const QString &name, const bool isAuth) const
{
    QPair<QString, bool> newItem = qMakePair(name, isAuth);
    return m_bakList.contains(newItem);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::removeFromBak
/// \param name
/// \param isAuth
///
void QQBouchotBackend::removeFromBak( const QString &name, const bool isAuth)
{
    QPair<QString, bool> newItem = qMakePair(name, isAuth);
    m_bakList.removeAll(newItem);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::addToPlopify
/// \param name
/// \param isAuth
///
void QQBouchotBackend::addToPlopify(const QString &name, const bool isAuth)
{
    QPair<QString, bool> newItem = qMakePair(name, isAuth);
    if(! m_plopifyList.contains(newItem))
        m_plopifyList.append(newItem);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::isPlopified
/// \param name
/// \param isAuth
///
bool QQBouchotBackend::isPlopified(const QString &name, const bool isAuth) const
{
    QPair<QString, bool> newItem = qMakePair(name, isAuth);
    return m_plopifyList.contains(newItem);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::removeFromPlopify
/// \param name
/// \param isAuth
///
void QQBouchotBackend::removeFromPlopify(const QString &name, const bool isAuth)
{
    QPair<QString, bool> newItem = qMakePair(name, isAuth);
    m_plopifyList.removeAll(newItem);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::boardState
/// \return
///
QuteQoin::QQBoardStates QQBouchotBackend::boardState()
{
    return m_state;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::resetStatus
///
void QQBouchotBackend::resetStatus()
{
    m_state.hasNewPosts = false;
    m_state.hasBigorno = false;
    m_state.hasResponse = false;
    sendBouchotEvents();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::setHasNewResponse
///
void QQBouchotBackend::setHasNewResponse()
{
    m_state.hasResponse = true;
    sendBouchotEvents();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::setHasBigorno
///
void QQBouchotBackend::setHasBigorno()
{
    m_state.hasBigorno = true;
    sendBouchotEvents();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::isReadOnly
/// \return
///
bool QQBouchotBackend::isReadOnly()
{
    return m_bSettings.postUrl().isEmpty();
}

//////////////////////////////////////////////////////////////
/// \brief registerForEventNotification
/// \param receiver
/// \param events
///
void QQBouchotBackend::registerForEventNotification(QObject *receiver, QQBouchotEvents events)
{
    if(receiver)
    {
        connect(receiver, SIGNAL(destroyed(QObject *)),
                this, SLOT(unregisterForEventNotification(QObject*)));
        unregisterForEventNotification(receiver);
        QQBouchotBackend::EventReceiver evRcv;
        evRcv.acceptedEvents = events;
        evRcv.receiver = receiver;
        m_listEventReceivers.append(evRcv);
    }
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::event
/// \param e
/// \return
///
bool QQBouchotBackend::event(QEvent *e)
{
    if(e->type() == QQPurgeBouchotHistoEvent::PURGE_BOUCHOT_HISTO)
    {
        QMutableListIterator<QQPostPtr> i(m_history);
        while (i.hasNext())
        {
            QQPostPtr post = i.next();
            if(post.isNull())
                i.remove();
            else
                break;
        }
    }
    else
        QObject::event(e);

    return true;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::fetchBackend
///
static QRegularExpression regFetchBackend("&?\\w+=%i", QRegularExpression::NoPatternOption);
void QQBouchotBackend::fetchBackend()
{
    QString url = m_bSettings.backendUrl();
    QString lastId;

    //on bloque le timer
    m_timer.stop();

    if(m_lastId < 0)
    {
        //1° appel au backend, on supprime le last/last_id/... de l'url
        url.replace(regFetchBackend, "");

        //Nettoyage de l'url finale
        regFetchBackend.setPattern("\\?$");
        url.replace(regFetchBackend, "");

        url.replace("?&", "?");
    }
    else
    {
        lastId.setNum(m_lastId);
        url.replace(QString::fromLatin1("%i"), lastId);
    }

    QNetworkRequest request(QUrl::fromUserInput(url));
    request.setAttribute(QNetworkRequest::User, QQBouchotBackend::BackendRequest);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);

    request.setRawHeader(QString::fromLatin1("User-Agent").toLatin1(), QString(DEFAULT_GENERAL_DEFAULT_UA).toLatin1());

    if(m_bSettings.cookie().isEmpty() == false)
        request.setRawHeader(QString::fromLatin1("Cookie").toLatin1(), m_bSettings.cookie().toLatin1());

    QNetworkReply *reply = httpGet(request);
    connect(reply, SIGNAL(sslErrors(const QList<QSslError>&)), this, SLOT(slotSslErrors(const QList<QSslError>&)));

    emit refreshStarted();
    m_timer.setInterval(currentRefreshInterval());
    qDebug() << Q_FUNC_INFO << m_name << currentRefreshRatio() << currentRefreshInterval();
    m_timer.start();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::slotSslErrors
/// \param errors
///
void QQBouchotBackend::slotSslErrors(const QList<QSslError> &errors)
{
    QString msgs;
    foreach(QSslError err, errors)
    {
        switch (err.error()) {
            case QSslError::SelfSignedCertificate:
            case QSslError::SelfSignedCertificateInChain:
            case QSslError::UnableToGetLocalIssuerCertificate:
            case QSslError::UnableToVerifyFirstCertificate:
            case QSslError::CertificateUntrusted:
                if(m_bSettings.isStrictHttpsCertif() == true)
                    msgs.append(err.errorString()).append("\n");
                break;
            case QSslError::NoError:
                break;
            default:
                msgs.append(err.errorString()).append("\n");
                break;
        }
    }
    if(msgs.length() > 0)
        emit refreshError(msgs);
    else
        ((QNetworkReply *)sender())->ignoreSslErrors();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::unregisterForEventNotification
/// \param receiver
///
void QQBouchotBackend::unregisterForEventNotification(QObject *receiver)
{
    if(receiver)
    {
        QMutableListIterator<EventReceiver> i(m_listEventReceivers);
        while(i.hasNext())
        {
            if(i.next().receiver == receiver)
            {
                disconnect(receiver, SIGNAL(destroyed(QObject *)),
                           this, SLOT(unregisterForEventNotification(QObject*)));
                i.remove();
            }
        }
    }
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::requestFinishedSlot
/// \param reply
///
void QQBouchotBackend::requestFinishedSlot(QNetworkReply *reply)
{
    if(reply->error() != QNetworkReply::NoError)
    {
        QString errMsg = reply->errorString();
        m_state.hasError = true;
        qWarning() << Q_FUNC_INFO
                   << "Bouchot :" << m_name
                   << "error :" << reply->error()
                   << "msg :" << reply->errorString();
        switch(reply->request().attribute(QNetworkRequest::User, QQBouchotBackend::UnknownRequest).toInt(0))
        {
            case QQBouchotBackend::BackendRequest:
                emit refreshError(errMsg);
                break;
            default:
                qWarning() << Q_FUNC_INFO
                           << "Bouchot :" << m_name
                           << "reply->request().attribute(QNetworkRequest::User).toInt() unknown :"
                           << reply->request().attribute(QNetworkRequest::User, QQBouchotBackend::UnknownRequest).toInt(0);
        }
    }
    else
    {
        m_state.hasError = false;

        QVariant cookieVar = reply->header(QNetworkRequest::SetCookieHeader);
        if(cookieVar.isValid())
        {
            QList<QNetworkCookie> listCookies = cookieVar.value<QList<QNetworkCookie> >();
            foreach (QNetworkCookie c, listCookies)
            {
                QString s = QString("%1=%2")
                        .arg(QString::fromLatin1(c.name()), QString::fromLatin1(c.value()));

                m_bSettings.setCookie(s);
            }
        }

        switch(reply->request().attribute(QNetworkRequest::User, QQBouchotBackend::UnknownRequest).toInt(0))
        {
            case QQBouchotBackend::PostRequest:
                if(reply->hasRawHeader(X_POST_ID_HEADER))
                {
                    m_hasXPostId = true;
                    m_xPostIds.append(QString(reply->rawHeader(X_POST_ID_HEADER)));
                }

                fetchBackend();
                break;
            case QQBouchotBackend::BackendRequest:
                qDebug() << Q_FUNC_INFO << reply->url();
                QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
                parseBackend(reply->readAll(), contentType);
                emit refreshOK();
                break;
        }
    }
    reply->deleteLater();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::parseBackend
/// \param data
///
static QRegularExpression xmlReg("^<\\w+ ");
static QRegularExpression tsvReg("^\\d+\t");
void QQBouchotBackend::parseBackend(const QByteArray &data, const QString &contentType)
{
    if(contentType.startsWith("text/xml") ||
            contentType.startsWith("application/xml"))
        parseBackendXML(data);
    else if(contentType.startsWith("text/tab-separated-values"))
        parseBackendTSV(data);
    else
    {
        QBuffer b;
        b.setData(data);
        b.open(QIODevice::ReadOnly);

        bool parserFound = false;
        if(b.canReadLine())
        {
            QString l = b.readLine();
            qDebug() << Q_FUNC_INFO << l;
            if(l.startsWith("<?xml ") ||
                    l.contains(xmlReg))
            {
                qDebug() << Q_FUNC_INFO << "XML found";
                parseBackendXML(data);
                parserFound = true;
            }
            else if(l.contains(tsvReg))
            {
                qDebug() << Q_FUNC_INFO << "TSV found";
                parseBackendTSV(data);
                parserFound = true;
            }
        }
        b.close();

        if(! parserFound)
            qWarning() << Q_FUNC_INFO
                       << "Bouchot :" << m_name
                       << "Unsupported backend format (yet) :"
                       << contentType;
    }
}

void QQBouchotBackend::parseBackendTSV(const QByteArray &data)
{
    QQTsvParser *p = NULL;
    if(m_parser == NULL)
    {
        p = new QQTsvParser(this);

        connect(p, SIGNAL(newPostReady(QQPost&)), this, SLOT(insertNewPost(QQPost&)));
        connect(p, SIGNAL(finished()), this, SLOT(parsingFinished()));

        m_parser=p;
    }
    else
        p = qobject_cast<QQTsvParser *>(m_parser);

    p->setLastId(m_lastId);
    p->parseBackend(data);
}

void QQBouchotBackend::parseBackendXML(const QByteArray &data)
{
    QQCustomXmlParser *p = NULL;
    if(m_parser == NULL)
    {
        p = new QQCustomXmlParser(this);

        connect(p, SIGNAL(newPostReady(QQPost&)), this, SLOT(insertNewPost(QQPost&)));
        connect(p, SIGNAL(finished()), this, SLOT(parsingFinished()));

        m_parser=p;
    }
    else
        p = qobject_cast<QQCustomXmlParser *>(m_parser);

    p->setLastId(m_lastId);
    p->parseBackend(data);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::insertNewPost
/// \param newPost
///
void QQBouchotBackend::insertNewPost(QQPost &newPost)
{
    QQPost * tmpNewPost = new QQPost(newPost);
    tmpNewPost->setParent(this);

    QString postId = tmpNewPost->id();
    if(m_xPostIds.contains(postId))
    {
        tmpNewPost->setSelfPost();
        m_xPostIds.removeOne(postId);
    }
    //Si la tribune gere le X-Post-Id mais que l'id du post ne fait pas
    // partie de la liste alors il est d'une autre moule

    //Pas du tout efface, il peut s'agir d'un post provenant d'un autre canard
    //else if(m_hasXPostId)
    //	tmpNewPost->setSelfPost(false);

    m_newPostHistory.prepend(tmpNewPost);
}

#define MAX_TIME_USER_ACTIVE_S (30 * 60)
//////////////////////////////////////////////////////////////
/// \brief QQBouchot::parsingFinished
///
void QQBouchotBackend::parsingFinished()
{
    if(m_newPostHistory.size() > 0)
    {
        if(m_newPostHistory.size() >= REFRESH_NEW_POSTS_THRESHOLD
                && m_refreshRatioIndex > 0)
        {
            m_refreshRatioIndex --; // faster
            if(currentRefreshRatio() > 1.0)
                m_refreshRatioIndex = REFRESH_RATIOS_MID;
        }

        if(m_deltaTimeH == -1 &&
                m_history.size() > 0) //Ne peut-etre fait sur le 1° backend recupere
        {
            //Le delta de TZ ne peut etre determine efficacement que lors d'un
            // refresh de backend (pas lors du chargement initial).
            QQPost *last = m_newPostHistory.last();
            QDateTime postDateTime = QDateTime::fromString(last->norloge(), "yyyyMMddHHmmss");
            m_deltaTimeH = postDateTime.secsTo(QDateTime::currentDateTime()) / 3600; //Secondes vers Heures
        }

        m_history.append(m_newPostHistory);
        m_lastId = m_parser->maxId();
        m_state.hasNewPosts = true;
        sendBouchotEvents();
    }
    else if(m_refreshRatioIndex < REFRESH_RATIOS_SIZE - 1)
        m_refreshRatioIndex ++; // slower

    updateLastUsers();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::checkBackendUrlModified
/// \param oldBackendUrl
///
void QQBouchotBackend::checkBackendUrlModified(const QString &oldBackendUrl)
{
    if(m_bSettings.backendUrl() != oldBackendUrl)
    {
        if(m_parser != NULL)
        {
            delete m_parser;
            m_parser = NULL;
        }
    }
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::checkGroupModified
/// \param oldGroupName
///
void QQBouchotBackend::checkGroupModified(const QString &oldGroupName)
{
    if(m_bSettings.group() != oldGroupName)
        emit groupChanged(this, oldGroupName);
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::updateLastUsers
///
void QQBouchotBackend::updateLastUsers()
{
    m_lastPosters.clear();

    int deltaTimeH = m_deltaTimeH;
    // Si on ne sait pas on considere que c'est 0 en attendant d'en savoir plus
    if(deltaTimeH < 0)
        deltaTimeH = 0;

    QDateTime currentDateTime = QDateTime::currentDateTime();
    currentDateTime = currentDateTime.addSecs( deltaTimeH * 3600 ); // on se met sur le meme TZ que le bouchot
    for(int i = m_history.size() - 1; i >= 0; i--)
    {
        QQPost *post = m_history.at(i);
        QDateTime postDateTime = QDateTime::fromString(post->norloge(), "yyyyMMddHHmmss");
        if(qAbs(currentDateTime.secsTo(postDateTime)) < MAX_TIME_USER_ACTIVE_S)
        {
            QString name;
            bool isAuth = false;
            if(post->login().size() > 0)
            {
                name = post->login();
                isAuth = true;
            }
            else if((post->UA().size() > 0) && ! post->UA().contains('/'))
                name = post->UA();

            if(name.length() > 0)
            {
                QQMussel mussel(name, m_name, isAuth);
                if(!m_lastPosters.contains(mussel))
                    m_lastPosters.append(mussel);
            }
        }
        else
            break;
    }
    emit lastPostersUpdated();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::postStateChangedEvent
///
void QQBouchotBackend::sendBouchotEvents()
{
    foreach (EventReceiver evRcv, m_listEventReceivers)
    {
        if(evRcv.acceptedEvents.testFlag(NewPostsAvailable) && m_state.hasNewPosts)
        {
            QApplication::postEvent(evRcv.receiver,
                                    new QQBackendUpdatedEvent(
                                        QQBackendUpdatedEvent::BACKEND_UPDATED,
                                        m_bSettings.group()),
                                    Qt::LowEventPriority
                                    );
        }
        if(evRcv.acceptedEvents.testFlag(StateChanged))
        {
            QApplication::postEvent(evRcv.receiver,
                                    new QQBoardStateChangeEvent(m_name),
                                    Qt::LowEventPriority
                                    );
        }
    }
}

//////////////////////////////////////////////////////////////
/// \brief currentRefreshRatio
/// \return
///
float QQBouchotBackend::currentRefreshRatio()
{
    if(m_refreshRatioIndex >= 0 && m_refreshRatioIndex < REFRESH_RATIOS_SIZE)
        return refreshRatiosArr[m_refreshRatioIndex];
    else
    {
        m_refreshRatioIndex = REFRESH_RATIOS_MID;
        return refreshRatiosArr[REFRESH_RATIOS_MID];
    }
}


/////////////////////////
// Static
/////////////////////////

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::getBouchotDef
/// \param bouchotName
/// \return
///
QQBouchotBackend::QQBouchotSettings QQBouchotBackend::getBouchotDef(const QString &bouchotName)
{

    QQBouchotBackend::QQBouchotSettings settings;

    int i = 0;
    for(; i < BOUCHOTS_DEF_SIZE; i++)
        if(QString::compare(bouchotName, QLatin1String(bouchotsDef[i].name)) == 0)
            break;

    if(i < BOUCHOTS_DEF_SIZE)
    {
        settings.setAliasesFromString(bouchotsDef[i].alias);
        settings.setBackendUrl(bouchotsDef[i].getUrl);
        settings.setColorFromString(bouchotsDef[i].color);
        settings.setPostData(bouchotsDef[i].postData);
        settings.setPostUrl(bouchotsDef[i].postUrl);
        settings.setRefresh(DEFAULT_BOUCHOT_REFRESH);
        settings.setSlipType(bouchotsDef[i].typeSlip);
        settings.setCookie(bouchotsDef[i].cookieProto);
    }

    return settings;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::getBouchotDefNameList
/// \return
///
QStringList QQBouchotBackend::getBouchotDefNameList()
{
    int i = 0;
    QStringList res;
    for(i = 0; i < BOUCHOTS_DEF_SIZE; i++)
        res.append(QString::fromLatin1(bouchotsDef[i].name));

    return res;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::s_hashBouchots
///
QHash<QString, QQBouchotBackend *> QQBouchotBackend::s_hashBouchots;

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::bouchot
/// \param bouchotName
/// \return
///
QQBouchotBackend * QQBouchotBackend::bouchot(const QString &bouchotName)
{
    QQBouchotBackend * ret = NULL;
    if(s_hashBouchots.contains(bouchotName))
        ret = s_hashBouchots.value(bouchotName);
    else
    {
        foreach (QQBouchotBackend *bouchot, listBouchots())
        {
            if(bouchot->settings().aliases().contains(bouchotName))
            {
                ret = bouchot;
                break;
            }
        }
    }

    return ret;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::listBouchots
/// \return
///
QList<QQBouchotBackend *> QQBouchotBackend::listBouchots()
{
    return s_hashBouchots.values();
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::listBouchotsGroup
/// \param groupName
/// \return
///
QList<QQBouchotBackend *> QQBouchotBackend::listBouchotsGroup(const QString &groupName)
{
    QHashIterator<QString, QQBouchotBackend *> i(s_hashBouchots);
    QQBouchotBackend *bouchot;
    QList<QQBouchotBackend *> res;

    while (i.hasNext()) {
        bouchot = i.next().value();
        if(bouchot->settings().group() == groupName)
            res.append(bouchot);
    }
    return res;
}

//////////////////////////////////////////////////////////////
/// \brief QQBouchot::listGroups
/// \return
///
QStringList QQBouchotBackend::listGroups()
{
    QStringList listGroups;
    auto bouchots = s_hashBouchots.values();
    foreach (auto bouchot, bouchots)
    {
        QString group = bouchot->settings().group();
        if(! listGroups.contains(group))
            listGroups.append(group);
    }
    return listGroups;
}
