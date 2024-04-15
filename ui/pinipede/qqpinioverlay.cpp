#include "qqpinioverlay.h"

#include <QGraphicsVideoItem>
#include <QMediaPlayer>

#include "core/qqsettings.h"
#include "core/qqwebdownloader.h"
#include "ui/pinipede/qqduckpixmapitem.h"
#include "ui/qqtotozviewer.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QGraphicsProxyWidget>
#include <QResizeEvent>
#include <QTemporaryFile>
#include <QToolTip>

//////////////////////////////////////////////////////////////
/// \brief The OverlayPlayer class
///
class OverlayPlayer
{
public:
	virtual ~OverlayPlayer() = default;
	enum PlayerType { GenericOverlayPlayer, TypeVideoPlayer, TypeTotozPlayer, TypeImagePlayer };

	virtual PlayerType playerType() { return GenericOverlayPlayer; }
	virtual void show() {}
	virtual void hide() {}
};

class ImagePlayer : public OverlayPlayer
{
public:
	explicit ImagePlayer(QGraphicsProxyWidget *gpw, QQImageViewer *imgV) :
	    m_gpw(gpw), m_imgV(imgV) {}
	virtual ~ImagePlayer()
	{
		if(m_gpw != nullptr)
		{
			m_gpw->hide();
			m_gpw->deleteLater();
		}
		if(m_imgV != nullptr)
			m_imgV->deleteLater();
	}

	PlayerType playerType() override { return TypeImagePlayer; }
	void show() override { m_gpw->show(); }
	void hide() override { m_gpw->hide(); }
private:
	QGraphicsProxyWidget *m_gpw;
	QQImageViewer *m_imgV;
};

//////////////////////////////////////////////////////////////
/// \brief The TotozPlayer class
///
class TotozPlayer : public OverlayPlayer
{
public:
	explicit TotozPlayer(QGraphicsProxyWidget *gpw, QQTotozViewer *ttV) :
	    m_gpw(gpw), m_ttV(ttV) {}
	virtual ~TotozPlayer()
	{
		if(m_gpw != nullptr)
		{
			m_gpw->hide();
			m_gpw->deleteLater();
		}
		if(m_ttV != nullptr)
			m_ttV->deleteLater();
	}

	PlayerType playerType() override { return TypeTotozPlayer; }
	void show() override { m_gpw->show(); }
	void hide() override { m_gpw->hide(); }

private:
	QGraphicsProxyWidget *m_gpw;
	QQTotozViewer *m_ttV;
};

//////////////////////////////////////////////////////////////
/// \brief The VideoPlayer class
///
class VideoPlayer : public OverlayPlayer
{
public:
	explicit VideoPlayer(QGraphicsVideoItem *item, QMediaPlayer *player) :
	    m_gObj(item), m_player(player) {}

	virtual ~VideoPlayer()
	{
		if(m_player != nullptr)
			m_player->deleteLater();
		if(m_gObj != nullptr)
		{
			m_gObj->hide();
			m_gObj->deleteLater();
		}
	}

	PlayerType playerType() override { return TypeVideoPlayer; }
	void show() override { m_gObj->show(); }
	void hide() override { m_gObj->hide(); }
	void stop() { m_player->stop(); }

	QString errorString() { return m_player->errorString(); }

private:
	QGraphicsVideoItem *m_gObj;
	QMediaPlayer *m_player;
	QMediaPlaylist *m_media;
};

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::QQPiniOverlay
/// \param parent
///
QQPiniOverlay::QQPiniOverlay(QWidget *parent) :
    QGraphicsView(parent),
    m_downloader(new QQWebDownloader(this)),
    m_img(nullptr),
    m_currentPlayer(nullptr),
    m_pendingPlayer(nullptr)
{
	setRenderHint(QPainter::Antialiasing);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setAttribute(Qt::WA_TranslucentBackground);
	setStyleSheet("background: transparent;");
	setFrameStyle(QFrame::NoFrame);

	auto scene = new QGraphicsScene();
	scene->setSceneRect(rect());
	setScene(scene);

	if(parent != nullptr)
		resize(parent->size());

	connect(m_downloader, SIGNAL(ready(QUrl&)), this, SLOT(dlReady(QUrl&)));
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::~QQPiniOverlay
///
QQPiniOverlay::~QQPiniOverlay()
{
	clearOverview();

	QGraphicsScene *s = scene();
	delete s;
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::focusOutEvent
/// \param event
///
void QQPiniOverlay::focusOutEvent(QFocusEvent *event)
{
	qDebug() << Q_FUNC_INFO << event->reason();
	QGraphicsView::focusOutEvent(event);
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::resizeEvent
/// \param event
///
void QQPiniOverlay::resizeEvent(QResizeEvent *event)
{
	QGraphicsView::resizeEvent(event);
	if(scene())
		scene()->setSceneRect(0, 0, event->size().width(), event->size().height());
	event->accept();
}

//////////////////////////////////////////////////////////////
/// \brief dlReady
///
void QQPiniOverlay::dlReady(QUrl &url)
{
	if (m_pendingURLs.isEmpty() || m_pendingURLs.top() != url)
		return;

	m_pendingURLs.clear();

	QQSettings settings;
	int maxSize = settings.value(SETTINGS_WEB_IMAGE_PREVIEW_SIZE, DEFAULT_WEB_IMAGE_PREVIEW_SIZE).toInt();

	QString contentType = m_downloader->dataContentType();
	if(contentType.startsWith("image/"))
	{
		auto imgV = new QQImageViewer();
		imgV->updateImg(m_downloader->imgData(), QSize(maxSize, maxSize));
		auto gpw = scene()->addWidget(imgV, Qt::Widget);
		moveToMousePos(gpw, imgV->size());

		delete m_currentPlayer;
		m_currentPlayer = new ImagePlayer(gpw, imgV);
		m_currentPlayer->show();
	}
	else if(contentType.startsWith("video/"))
	{
		auto f = new QTemporaryFile();
		if (f->open()) {
			f->write(m_downloader->imgData());
			f->flush();
			m_tmpFiles.append(f);
			QUrl url = QUrl::fromLocalFile(f->fileName());
			showVideo(url);
		}
	}
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::doVideoStateChanged
/// \param newState
///
void QQPiniOverlay::doVideoStateChanged(QMediaPlayer::PlaybackState newState)
{
	if(newState == QMediaPlayer::PlayingState)
	{
		delete m_currentPlayer;

		m_currentPlayer = m_pendingPlayer;
		m_pendingPlayer = nullptr;

		if(m_currentPlayer != nullptr)
			m_currentPlayer->show();
	}
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::handleVideoError
/// \param error
///
void QQPiniOverlay::handleVideoError(QMediaPlayer::Error error, const QString &errorString)
{
	if(error != QMediaPlayer::NoError)
	{
		qWarning() << Q_FUNC_INFO << error;

		if (m_pendingPlayer != nullptr
			&& m_pendingPlayer->playerType() == OverlayPlayer::TypeVideoPlayer) {
			dynamic_cast<VideoPlayer *>(m_pendingPlayer)->stop();
			delete m_pendingPlayer;
		} else if (m_currentPlayer != nullptr
				   && m_currentPlayer->playerType() == OverlayPlayer::TypeVideoPlayer) {
			dynamic_cast<VideoPlayer *>(m_currentPlayer)->stop();
			delete m_currentPlayer;
		}

		auto v = new QQImageViewer();
		v->setText(QString("Error playing media : %1").arg(errorString));
		auto gpw = scene()->addWidget(v, Qt::Widget);
		moveToMousePos(gpw, v->size());

		m_currentPlayer = new ImagePlayer(gpw, v);
		m_currentPlayer->show();
	}
}

//////////////////////////////////////////////////////////////
/// \brief QQHuntingView::launchDuck
/// \param srcBouchot
/// \param postId
///
void QQPiniOverlay::launchDuck(QString srcBouchot, QString postId, bool selfDuck)
{
	QQSettings settings;

	if(settings.value(SETTINGS_HUNT_MODE, DEFAULT_HUNT_MODE).toInt() == QuteQoin::HuntMode_Disabled ||
	        settings.value(SETTINGS_HUNT_MAX_ITEMS, DEFAULT_HUNT_MAX_ITEMS).toInt() <= m_duckList.size())
		return;

	auto duck = new QQDuckPixmapItem(srcBouchot, postId, selfDuck);
	duck->setPos(mapFromGlobal(QCursor::pos()));
	scene()->addItem(duck);

	m_duckList << duck;

	duck->animate();
}

//////////////////////////////////////////////////////////////
/// \brief QQHuntingView::killDuck
/// \param forceSilent
///
void QQPiniOverlay::killDuck(bool forceSilent)
{
	auto shotPoint = mapFromGlobal(QCursor::pos());
	auto shotPointF(shotPoint);

	foreach(QQDuckPixmapItem *duck, m_duckList)
	{
		auto itemPos = duck->pos();
		auto itemRect = duck->boundingRect();
		if(shotPointF.x() >= itemPos.x() && shotPointF.x() <= (itemPos.x() + itemRect.width()) &&
		        shotPointF.y() >= itemPos.y() && shotPointF.y() <= (itemPos.y() + itemRect.height()))
		{
			QQSettings settings;
			if(! (forceSilent
			      || settings.value(SETTINGS_HUNT_SILENT_ENABLED, DEFAULT_HUNT_SILENT_ENABLED).toBool()
			      || duck->isSelfItem()))
				emit duckKilled(duck->bouchotName(), duck->postId());

			m_duckList.removeOne(duck);
			duck->animateKill();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::showTotoz
/// \param totozId
///
void QQPiniOverlay::showTotoz(const QString &totozId)
{
	QQSettings settings;
	if(settings.value(SETTINGS_TOTOZ_VISUAL_MODE, DEFAULT_TOTOZ_VISUAL_MODE).toString() ==
	        TOTOZ_VISUAL_MODE_DISABLED)
		return;

	auto v = new QQTotozViewer(totozId, nullptr);
	auto gpw = scene()->addWidget(v, Qt::Widget);
	moveToMousePos(gpw, v->size());

	delete m_currentPlayer;
	m_currentPlayer = new TotozPlayer(gpw, v);
	m_currentPlayer->show();
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::showUrl
/// \param url
///
void QQPiniOverlay::showUrl(const QUrl &url, QString &contentType)
{

#ifdef WIN32
	if(contentType.startsWith("image/"))
#else
	showWaitAnim();

	if(contentType.startsWith("video/") ||
	        contentType.startsWith("image/"))
#endif
	{
		m_pendingURLs.push(url);
		if(contentType.startsWith("video/"))
			showVideo(url);
		else
			m_downloader->getURL(url);
	}
}

//////////////////////////////////////////////////////////////
/// \brief clearOverview
///
void QQPiniOverlay::clearOverview()
{
	m_pendingURLs.clear();
	if(m_currentPlayer != nullptr)
	{
		delete m_currentPlayer;
		m_currentPlayer = nullptr;
	}

	while(! m_tmpFiles.isEmpty())
	{
		QTemporaryFile *f = m_tmpFiles.takeFirst();

		delete f;
	}
}

//////////////////////////////////////////////////////////////
/// \brief QQImageViewer::setPos
///
void QQPiniOverlay::moveToMousePos(QGraphicsObject *gpw, const QSize &s)
{
	bool shouldMoveToolTip = true;
	QPoint cursorPos = mapFromGlobal(QCursor::pos());

	if(cursorPos.x() > (width() / 2)) // a droite
	{
		cursorPos.rx() -= s.width();
		shouldMoveToolTip = false;
	}
	if(cursorPos.y() > (height() / 2)) // en dessous
	{
		cursorPos.ry() -= s.height();
		shouldMoveToolTip = false;
	}

	gpw->setX(cursorPos.x());
	gpw->setY(cursorPos.y());

	if(shouldMoveToolTip && QToolTip::isVisible())
	{
		QString txt = QToolTip::text();
		QToolTip::hideText();

		QPoint ttPos = cursorPos;
		ttPos.rx() += s.width();

		QToolTip::hideText();
		QCoreApplication::processEvents();
		QToolTip::showText(mapToGlobal(ttPos), txt, parentWidget());
	}
}

//////////////////////////////////////////////////////////////
/// \brief showVideo
/// \param url
///
void QQPiniOverlay::showVideo(const QUrl &url)
{
	QQSettings settings;
	auto maxSize = settings.value(SETTINGS_WEB_IMAGE_PREVIEW_SIZE, DEFAULT_WEB_IMAGE_PREVIEW_SIZE).toInt();
	QSize s(maxSize, maxSize);

	auto i = new QGraphicsVideoItem();
	i->setAspectRatioMode(Qt::KeepAspectRatio);
	i->setSize(s);
	scene()->addItem(i);

	auto player = new QMediaPlayer(this);

	auto audioOutput = new QAudioOutput(player);
	audioOutput->setMuted(settings.value(SETTINGS_GENERAL_STEALTH_MODE, DEFAULT_GENERAL_STEALTH_MODE).toBool());
	player->setAudioOutput(audioOutput);

	player->setLoops(QMediaPlayer::Infinite);

	moveToMousePos(i, s);

	player->setVideoOutput(i);
	player->setSource(url);

	m_pendingPlayer = new VideoPlayer(i, player);
	if(m_currentPlayer != nullptr)
		m_currentPlayer->hide();

	connect(player, &QMediaPlayer::playbackStateChanged, this, &QQPiniOverlay::doVideoStateChanged);

	connect(player, &QMediaPlayer::errorOccurred, this, &QQPiniOverlay::handleVideoError);

	player->play();
}

//////////////////////////////////////////////////////////////
/// \brief QQPiniOverlay::showWaitAnim
///
void QQPiniOverlay::showWaitAnim()
{
	auto imgV = new QQImageViewer();
	imgV->displayWaitMovie();
	auto gpw = scene()->addWidget(imgV, Qt::Widget);
	moveToMousePos(gpw, imgV->size());

	delete m_currentPlayer;
	m_currentPlayer = new ImagePlayer(gpw, imgV);
	m_currentPlayer->show();

}
