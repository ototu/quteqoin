#include "qqpiniurlhelper.h"

#include "core/qqtypes.h"
#include "core/qqsettings.h"

#include <QtDebug>
#include <QStringList>

#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextDocumentFragment>

constexpr char URL_HOST_REGEXP[] = R"((<a [^>]*href="(https?://[^/"]+)[^"]*"[^>]*>)\[(?:url|https?)\]</a>)"; //Raw string

constexpr int CONTENT_TYPE_CACHE_SIZE = 500;

constexpr char INITIAL_URL_PROPERTY[] = "INITIAL_URL";
constexpr char VIDEO_ID[] = "VIDEO_ID";

QHash<QUrl, QQPiniUrlHelper::CacheInfo *> QQPiniUrlHelper::m_cache;
QList<QUrl> QQPiniUrlHelper::m_cacheUrls;

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::QQPiniUrlHelper
/// \param parent
///
QQPiniUrlHelper::QQPiniUrlHelper(QObject *parent) :
    QQNetworkAccessor(parent)
{
    m_contentTypeReplies.clear();
}

//////////////////////////////////////////////////////////////
/// \brief cancel
///
void QQPiniUrlHelper::cancel()
{
    foreach (QNetworkReply * m_contentTypeReplies, m_contentTypeReplies) {
        m_contentTypeReplies->abort();
        m_contentTypeReplies->deleteLater();
    }
    m_contentTypeReplies.clear();
}

//////////////////////////////////////////////////////////////
/// \brief getUrlInfo
/// \param url
///
void QQPiniUrlHelper::getUrlInfo(QUrl &url)
{
    // Sans doute a ameliorer si la liste grandit
    if(url.host().endsWith("youtube.com") ||
            url.host().endsWith("youtu.be"))
        getYoutubeExtendedInfo(url);
    else if(url.host().endsWith("dailymotion.com"))
        getDailymotionExtendedInfo(url);
    else if(url.host().endsWith("vimeo.com"))
        getVimeoExtendedInfo(url);
    else if(url.host() == "sauf.ca")
        getSaufCaExtendedInfo(url);
    else
        getContentType(url);
}

//////////////////////////////////////////////////////////////
/// \brief transformMessage
/// \param post
/// \param message
///
void QQPiniUrlHelper::transformMessage(const QString &bouchot, QString &message)
{
    Q_UNUSED(bouchot);

    QQSettings settings;
    QuteQoin::QQSmartUrlFilerTransformType trType =
                (QuteQoin::QQSmartUrlFilerTransformType) settings.value(SETTINGS_FILTER_SMART_URL_TRANSFORM_TYPE, DEFAULT_FILTER_SMART_URL_TRANSFORM_TYPE).toInt();

    QRegularExpression reg(QString::fromUtf8(static_cast<const char *>(URL_HOST_REGEXP)), QRegularExpression::NoPatternOption);

    QRegularExpressionMatch match;
    auto index=0;
    while((match = reg.match(message, index)).hasMatch())
    {
        QUrl url(match.captured(2));
        QString host = url.host();

        QString s(match.captured(1));
        s.append("[");

        if (trType == QuteQoin::Full)
            s.append(host);
        else
        {
            QString tld = host.right( host.lastIndexOf('.'));
            QString fullDomain = host.left(host.length() - host.lastIndexOf('.'));

            QString baseDomain = fullDomain.right(fullDomain.length() - (fullDomain.lastIndexOf(".") + 1));

            //trType == QuteQoin::Shorter
            s.append(baseDomain);

            if (trType == QuteQoin::Short)
                s.append(tld);
        }

        s.append("]</a>");

        message.replace(match.capturedStart(), match.captured(0).length(), s);
        index += match.capturedEnd();
    }
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::requestFinishedSlot
/// \param reply
///
void QQPiniUrlHelper::requestFinishedSlot(QNetworkReply *reply)
{
    if(m_contentTypeReplies.contains(reply))
    {
        m_contentTypeReplies.removeAll(reply);
        QUrl sourceUrl = reply->request().url();

        // ou de l'url de destination si on a affaire a une redirection:
        QUrl redirectedURL = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

        if(!redirectedURL.isEmpty() &&
                redirectedURL != sourceUrl)
        {
            if(redirectedURL.host().isEmpty()) // Relative redirection
                redirectedURL=reply->request().url().resolved(redirectedURL);

            QNetworkRequest rq(reply->request());
            rq.setUrl(redirectedURL);
            QNetworkReply *newReply = httpHead(rq);

            if(reply->property(static_cast<const char *>(INITIAL_URL_PROPERTY)).isValid())
                newReply->setProperty(static_cast<const char *>(INITIAL_URL_PROPERTY), reply->property(static_cast<const char *>(INITIAL_URL_PROPERTY)));
            else
                newReply->setProperty(static_cast<const char *>(INITIAL_URL_PROPERTY), sourceUrl);

            m_contentTypeReplies.append(newReply);
        } // Une erreur HTTP est survenue
        else if(reply->error() != QNetworkReply::NoError)
        {
            qDebug() << Q_FUNC_INFO << reply->errorString();
            QString rep = tr("Unknown");
            emit contentTypeAvailable(sourceUrl, rep);
        }
        else
        {
            QNetworkRequest rq = reply->request();
            if(reply->property(static_cast<const char *>(INITIAL_URL_PROPERTY)).isValid())
                sourceUrl = reply->property(static_cast<const char *>(INITIAL_URL_PROPERTY)).toUrl();

            if(rq.attribute((QNetworkRequest::Attribute) RequestContentType, false).toBool())
                handleContentTypeResponse(reply->header(QNetworkRequest::ContentTypeHeader).toString(), sourceUrl);
            else if(rq.attribute((QNetworkRequest::Attribute) RequestDailyMotionInfo, false).toBool())
                handleDailymotionExtendedInfo(reply->readAll(), sourceUrl);
            else if(rq.attribute((QNetworkRequest::Attribute) RequestVimeoInfo, false).toBool())
                handleVimeoExtendedInfo(reply->readAll(), sourceUrl);
            else if(rq.attribute((QNetworkRequest::Attribute) RequestYoutubeInfo, false).toBool())
                handleYoutubeExtendedInfo(reply->readAll(), sourceUrl, reply->property(static_cast<const char *>(VIDEO_ID)).toString());
        }
    }
    reply->deleteLater();
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::getContentType
/// \param url
///
void QQPiniUrlHelper::getContentType(QUrl &url)
{
    QQPiniUrlHelper::CacheInfo *urlInfo = m_cache[url];
    if(urlInfo == nullptr)
    {
        QNetworkRequest r(url);
        r.setAttribute((QNetworkRequest::Attribute) RequestContentType, true);
        QNetworkReply *reply = httpHead(r);
        m_contentTypeReplies.append(reply);
    }
    else
    {
        QUrl reqUrl = url;

        emit contentTypeAvailable(reqUrl, urlInfo->contentType);

        if(urlInfo->contentType.startsWith("image/") ||
                urlInfo->contentType.startsWith("video/"))
            emit mmDataAvailable(url, urlInfo->contentType);
    }
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::handleContentTypeResponse
/// \param contentType
/// \param sourceUrl
///
void QQPiniUrlHelper::handleContentTypeResponse(const QString &contentType, QUrl &sourceUrl)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList contentTypeParts = contentType.split(";", Qt::SkipEmptyParts);
#else
    QStringList contentTypeParts = contentType.split(";", QString::SkipEmptyParts);
#endif
    QString rep = (contentTypeParts.isEmpty()) ? tr("Unknown") : contentTypeParts.at(0);

    auto info = new QQPiniUrlHelper::CacheInfo;
    info->contentType = rep;
    addToCache(sourceUrl, info);

    QString cT = contentType;
    if(contentType.startsWith("image/") ||
            contentType.startsWith("video/"))
        emit mmDataAvailable(sourceUrl, cT);

    emit contentTypeAvailable(sourceUrl, rep);
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::getDailymotionExtendedInfo
/// \param url
///
void QQPiniUrlHelper::getDailymotionExtendedInfo(QUrl &url)
{
    QQPiniUrlHelper::CacheInfo *urlInfo = m_cache[url];
    if(urlInfo == nullptr)
    {
        QStringList tmpList = url.path().split('/');
        if(tmpList.isEmpty())
            return;
        QString tmp = tmpList.last();

        tmpList = tmp.split('_');
        if(tmpList.isEmpty())
            return;
        QString vidId = tmpList.first();

        QUrl qUrl = QUrl("https://api.dailymotion.com");
        QString postData(QString(R"({"call": "GET /video/%1","args": {"fields": ["title","thumbnail_url"]}})").arg(vidId));

        QNetworkRequest r(qUrl);
        r.setHeader(QNetworkRequest::ContentTypeHeader, "Application/json");

        r.setAttribute((QNetworkRequest::Attribute) RequestDailyMotionInfo, true);
        QNetworkReply *reply = httpPost(r, postData.toLatin1());
        reply->setProperty(static_cast<const char *>(INITIAL_URL_PROPERTY), url);
        reply->setProperty(static_cast<const char *>(VIDEO_ID), vidId);

        m_contentTypeReplies.append(reply);
    }
    else
    {
        QString cT = "image/";
        emit mmDataAvailable(urlInfo->videoThumbnailUrl, cT);
        emit videoTitleAvailable(url, urlInfo->videoTitle);
    }
}

//////////////////////////////////////////////////////////////
/// \brief handleDailymotionExtendedInfo
/// \param jsonInfo
/// \param sourceUrl
///
///
void QQPiniUrlHelper::handleDailymotionExtendedInfo(const QByteArray &jsonInfo, QUrl &sourceUrl)
{
    QString thumbnailUrl;
    QString title;

    auto info = new QQPiniUrlHelper::CacheInfo;
    //JSON seulement supporté par Qt 5
    QJsonDocument d = QJsonDocument::fromJson(jsonInfo);
    QJsonObject o = d.object();
    QJsonObject o2 = o["result"].toObject();

    thumbnailUrl = o2["thumbnail_url"].toString();
    title = o2["title"].toString();

    auto cached = false;
    if(! thumbnailUrl.isEmpty())
    {
        if(! title.isEmpty())
        {
            info->videoThumbnailUrl = thumbnailUrl;
            info->videoTitle = title;
            addToCache(sourceUrl, info);
            cached = true;
        }
        QUrl tUrl(thumbnailUrl);
        QString cT = "image/";
        emit mmDataAvailable(tUrl, cT);
    }
    if(!cached)
        delete info;

    if(! title.isEmpty())
        emit videoTitleAvailable(sourceUrl, title);
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::getSaufCaExtendedInfo
/// \param url
///
void QQPiniUrlHelper::getSaufCaExtendedInfo(QUrl &url)
{
    if(url.path().size() <= 1)
    {
        getContentType(url);
    }
    else
    {
        QUrl imgUrl = url;
        if(! url.path().endsWith("/img"))
        {
            imgUrl.setPath(url.path().append("/img"));
        }

        QString cT = "image/";
        emit mmDataAvailable(imgUrl, cT);
    }
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::getVimeoExtendedInfo
/// \param url
///
void QQPiniUrlHelper::getVimeoExtendedInfo(QUrl &url)
{
    QQPiniUrlHelper::CacheInfo *urlInfo = m_cache[url];
    if(urlInfo == nullptr)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        QString vidId = url.path().split('/', Qt::SkipEmptyParts).first();
#else
        QString vidId = url.path().split('/', QString::SkipEmptyParts).first();
#endif

        QUrl qUrl = QUrl(QString("http://vimeo.com/api/v2/video/%1.json").arg(vidId));
        QNetworkRequest r(qUrl);
        r.setAttribute((QNetworkRequest::Attribute) RequestVimeoInfo, true);
        QNetworkReply *reply = httpGet(r);
        reply->setProperty(static_cast<const char *>(INITIAL_URL_PROPERTY), url);
        reply->setProperty(static_cast<const char *>(VIDEO_ID), vidId);

        m_contentTypeReplies.append(reply);
    }
    else
    {
        QString cT = "image/";
        emit mmDataAvailable(urlInfo->videoThumbnailUrl, cT);
        emit videoTitleAvailable(url, urlInfo->videoTitle);
    }

}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::handleVimeoExtendedInfo
/// \param jsonInfo
/// \param sourceUrl
///
/*
[
 {
  "id":21003613,
  "title":"In Heaven Everything Is Fine",
  "description":"",
  "url":"http:\/\/vimeo.com\/21003613",
  "upload_date":"2011-03-13 23:28:00",
  "mobile_url":"http:\/\/vimeo.com\/21003613",
  "thumbnail_small":"http:\/\/i.vimeocdn.com\/video\/134783860_100x75.jpg",
  "thumbnail_medium":"http:\/\/i.vimeocdn.com\/video\/134783860_200x150.jpg",
  "thumbnail_large":"http:\/\/i.vimeocdn.com\/video\/134783860_640.jpg",
  "user_id":5973882,
  "user_name":"David Lynch",
  "user_url":"http:\/\/vimeo.com\/davidlynchofficial",
  "user_portrait_small":"http:\/\/i.vimeocdn.com\/portrait\/1627937_30x30.jpg",
  "user_portrait_medium":"http:\/\/i.vimeocdn.com\/portrait\/1627937_75x75.jpg",
  "user_portrait_large":"http:\/\/i.vimeocdn.com\/portrait\/1627937_100x100.jpg",
  "user_portrait_huge":"http:\/\/i.vimeocdn.com\/portrait\/1627937_300x300.jpg",
  "stats_number_of_likes":1157,
  "stats_number_of_plays":80797,
  "stats_number_of_comments":77,
  "duration":98,
  "width":1280,
  "height":720,
  "tags":"",
  "embed_privacy":"anywhere"
 }
]
*/
void QQPiniUrlHelper::handleVimeoExtendedInfo(const QByteArray &jsonInfo, QUrl &sourceUrl)
{
    QString thumbnailUrl;
    QString title;

    auto info = new QQPiniUrlHelper::CacheInfo;

    QJsonParseError error{};
    QJsonDocument d = QJsonDocument::fromJson(jsonInfo, &error);
    if(d.isEmpty())
    {
        qDebug() << Q_FUNC_INFO << "error" << error.errorString();
        delete info;
        return;
    }
    QJsonArray a = d.array();
    QJsonObject o = a[0].toObject();
    thumbnailUrl = o["thumbnail_medium"].toString();
    title = o["title"].toString();

    auto cached = false;
    if(! thumbnailUrl.isEmpty())
    {
        if(! title.isEmpty())
        {
            info->videoThumbnailUrl = thumbnailUrl;
            info->videoTitle = title;
            addToCache(sourceUrl, info);
            cached = true;
        }

        QUrl tUrl(thumbnailUrl);
        QString cT = "image/";
        emit mmDataAvailable(tUrl, cT);
    }
    if(!cached)
        delete info;

    if(! title.isEmpty())
        emit videoTitleAvailable(sourceUrl, title);
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::getYoutubeExtendedInfo
/// \param ytId
///
void QQPiniUrlHelper::getYoutubeExtendedInfo(QUrl &url)
{
    QQPiniUrlHelper::CacheInfo *urlInfo = m_cache[url];
    if(urlInfo == nullptr)
    {
        QString ytId;
        if(url.host().endsWith("youtu.be"))
        {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            QStringList pathExp = url.path().split(QChar('/'), Qt::SkipEmptyParts);
#else
            QStringList pathExp = url.path().split(QChar('/'), QString::SkipEmptyParts);
#endif
            if(! pathExp.isEmpty())
                ytId = pathExp.last();
        }
        else
        {
            QUrlQuery urlQ(url);
            ytId = urlQ.queryItemValue("v");
        }
        if(ytId.isEmpty())
            return;

        QUrl qUrl = QUrl(QString("https://www.youtube.com/oembed?format=json&url=http%3A//www.youtube.com/watch?v=%1")
                        .arg(ytId));

        QNetworkRequest r(qUrl);
        r.setAttribute((QNetworkRequest::Attribute) RequestYoutubeInfo, true);
        QNetworkReply *reply = httpGet(r);
        reply->setProperty(static_cast<const char *>(INITIAL_URL_PROPERTY), url);
        reply->setProperty(static_cast<const char *>(VIDEO_ID), ytId);

        m_contentTypeReplies.append(reply);
    }
    else
    {
        QString cT = "image/";
        emit mmDataAvailable(urlInfo->videoThumbnailUrl, cT);
        emit videoTitleAvailable(url, urlInfo->videoTitle);
    }

}

void QQPiniUrlHelper::handleYoutubeExtendedInfo(const QByteArray &jsonInfo, QUrl &sourceUrl, const QString &videoID)
{
    QString thumbnailUrl;
    QString title;

    QJsonParseError error{};
    QJsonDocument d = QJsonDocument::fromJson(jsonInfo, &error);
    if(d.isEmpty() || ! d.isObject())
    {
        qDebug() << Q_FUNC_INFO << "error" << error.errorString();
        return;
    }
    QJsonObject o = d.object();
    thumbnailUrl = o["thumbnail_url"].toString();
    title = o["title"].toString();

    if(! videoID.isEmpty())
    {
        if(! title.isEmpty())
        {
            auto info = new QQPiniUrlHelper::CacheInfo;

            info->videoThumbnailUrl = thumbnailUrl;
            info->videoTitle = title;
            addToCache(sourceUrl, info);
        }

        QUrl tUrl(thumbnailUrl);
        QString cT = "image/";
        emit mmDataAvailable(tUrl, cT);
    }

    if(! title.isEmpty())
        emit videoTitleAvailable(sourceUrl, title);

}

//////////////////////////////////////////////////////////////
/// \brief QQPiniUrlHelper::addToCache
/// \param sourceUrl
/// \param info
///
void QQPiniUrlHelper::addToCache(QUrl &sourceUrl, CacheInfo *info)
{
    QQPiniUrlHelper::m_cacheUrls.append(sourceUrl);
    QQPiniUrlHelper::m_cache.insert(sourceUrl, info);
    while(QQPiniUrlHelper::m_cacheUrls.size() > CONTENT_TYPE_CACHE_SIZE)
    {
        QQPiniUrlHelper::CacheInfo *oldInfo = QQPiniUrlHelper::m_cache.take(QQPiniUrlHelper::m_cacheUrls.takeFirst());
        delete oldInfo;
    }
}
