#ifndef QQPINIOVERLAY_H
#define QQPINIOVERLAY_H

#include <QGraphicsView>
#include <QMimeType>
#include <QMediaPlayer>
#include <QStack>
#include <QUrl>

class OverlayPlayer; // Internal

class QQDuckPixmapItem;
class QQImageViewer;
class QQTotozViewer;
class QQWebDownloader;
class QQWebImageViewer;

class QGraphicsObject;
class QGraphicsVideoItem;
class QMediaPlaylist;
class QTemporaryFile;

class QQPiniOverlay : public QGraphicsView
{
	Q_OBJECT
public:
	explicit QQPiniOverlay(QWidget *parent = nullptr);
	~QQPiniOverlay();

signals:
	void duckKilled(QString srcBouchot, QString postId);

public slots:
	void launchDuck(QString srcBouchot, QString postId, bool selfDuck);
	void killDuck(bool forceSilent);

	void showTotoz(const QString &totozId);
	void showUrl(const QUrl &url, QString &contentType);
	void clearOverview();

protected:
	virtual void focusOutEvent(QFocusEvent* event);
	virtual void resizeEvent(QResizeEvent *event);

protected slots:
	void dlReady(QUrl &url);
	void doVideoStateChanged(QMediaPlayer::PlaybackState newState);
	void handleVideoError(QMediaPlayer::Error error, const QString &errorString);

private:
	void moveToMousePos(QGraphicsObject *gpw, const QSize &s);
	void showVideo(const QUrl &url);
	void showWaitAnim();

	QQWebDownloader *m_downloader;
	QGraphicsPixmapItem *m_img;
	QList<QQDuckPixmapItem *> m_duckList;
	QList<QTemporaryFile *> m_tmpFiles;
	QStack<QUrl> m_pendingURLs;

	OverlayPlayer *m_currentPlayer;
	OverlayPlayer *m_pendingPlayer;

};

#endif // QQPINIOVERLAY_H
