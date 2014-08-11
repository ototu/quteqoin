#include "qqtextbrowser.h"

#include "core/qqbouchot.h"
#include "core/qqpiniurlhelper.h"
#include "core/qqpost.h"
#include "core/qqsettings.h"
#include "ui/qqmessageblockuserdata.h"

#include <QtDebug>
#include <QApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCodec>
#include <QTextLayout>
#include <QTextTable>
#include <QTextTableCell>
#include <QToolTip>
#if(QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QUrlQuery>
#endif

#define TIME_UA_AREA_WIDTH_CHAR 26 // 10 + 1 + 15 Chars
#define NOTIF_AREA_WIDTH 18 //Px
#define ITEM_AREA_WIDTH 6 //Px

QQTextBrowser::QQTextBrowser(QString groupName, QQPinipede *parent) :
	QTextBrowser(parent),
	m_notifArea(new QQNotifArea(this)),
	m_urlHelper(new QQPiniUrlHelper(this)),
	m_highlightedNorlogeRef(""),
	m_displayedTotozId(""),
	m_shownUrl(),
	m_groupName(groupName)
{
	QQSettings settings;

	setFrameStyle(QFrame::NoFrame);
	setReadOnly(true);
	setOpenExternalLinks(true);

	connect(m_urlHelper, SIGNAL(contentTypeAvailable(QUrl&,QString&)), this, SLOT(handleContentTypeAvailable(QUrl&,QString&)));

	QTextDocument * doc = document();
	doc->setUndoRedoEnabled(false);
	doc->setDocumentMargin(0);
	QFont docFont;
	docFont.fromString(settings.value(SETTINGS_GENERAL_DEFAULT_FONT, DEFAULT_GENERAL_DEFAULT_FONT).toString());
	doc->setDefaultFont(docFont);

	QTextOption opt = doc->defaultTextOption();
	opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	opt.setAlignment(Qt::AlignLeft);

	QList<QTextOption::Tab> listTabs;
	qreal baseWidth = QFontMetricsF(docFont).averageCharWidth();
	m_timeUAAreaWidthPx = baseWidth * TIME_UA_AREA_WIDTH_CHAR;
	qreal tabPosPx = baseWidth * (TIME_UA_AREA_WIDTH_CHAR + 1); // + margin

	QTextOption::Tab tab(tabPosPx, QTextOption::DelimiterTab, '\t');
	listTabs << tab;

	opt.setTabs(listTabs);
	doc->setDefaultTextOption(opt);

	this->setMouseTracking(true);
	viewport()->setCursor(Qt::ArrowCursor);
	setViewportMargins(notifAreaWidth(), 0, 0, 0);

	connect(this->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(updateNotifArea(int)));

	verticalScrollBar()->setInvertedControls(true);
	verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMaximum);

}

QQTextBrowser::~QQTextBrowser()
{
}

int QQTextBrowser::notifAreaWidth()
{
	return NOTIF_AREA_WIDTH;
}

void QQTextBrowser::notifAreaPaintEvent(QPaintEvent * event)
{
	QPainter painter(m_notifArea);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

	// Pour les nouveaux posts
	QColor newPostsBrushColor(0, 255, 0, 100);
	QColor newPostsPenColor(0, 100, 0, 100);

	// Pour les propres posts
	QColor selfPostsBrushColor(0, 0, 255, 100);
	QColor selfPostsPenColor(0, 0, 100, 100);

	// Pour les reponses
	QColor repPostsBrushColor(127, 0, 127, 100);
	QColor repPostsPenColor(50, 0, 50, 100);

	// Pour le bigorno
	QColor bigornoBrushColor(255, 0, 0, 100);
	QColor bigornoPenColor(100, 0, 0);

	// Recuperation du premier bloc a dessiner
	QTextBlock block = cursorForPosition(
				event->rect().topLeft()
				).block();

	// Recuperation du dernier bloc a dessiner
	QTextBlock lastBlock = cursorForPosition(
				event->rect().bottomRight()
				).block();
	while(block.isValid() && block.blockNumber() <= lastBlock.blockNumber())
	{
		QQMessageBlockUserData * uData = (QQMessageBlockUserData *) block.userData();
		int offset = 0;
		if(uData != NULL && uData->post() != NULL)
		{
			QTextCursor curs(block);
			qreal height = block.layout()->boundingRect().height();
			int posY = cursorRect(curs).y();
			QQPost * post = uData->post();

			///////////////////////////////////////////////////////////////////
			/////        BACKGROUND                           /////////////////
			///////////////////////////////////////////////////////////////////
			painter.setBrush(post->bouchot()->settings().color());
			painter.setPen(QPen(QBrush(post->bouchot()->settings().color()), 0.6));
			QRect drawRect(0, posY, notifAreaWidth(), height);
			painter.drawRect(drawRect);

			///////////////////////////////////////////////////////////////////
			/////        LES NOUVEAUX POSTS                   /////////////////
			/////        LE TRACKING DES POSTS                /////////////////
			///////////////////////////////////////////////////////////////////
			if(post->isSelfPost())
			{
				offset += ITEM_AREA_WIDTH;
				painter.setBrush(selfPostsBrushColor);
				painter.setPen(QPen(QBrush(selfPostsPenColor), 0.6));
				QRect drawRect(offset, posY, ITEM_AREA_WIDTH, height);
				painter.drawRect(drawRect);
			}
			else
			{
				if(post->isUnread())
				{
					painter.setBrush(newPostsBrushColor);
					painter.setPen(QPen(QBrush(newPostsPenColor), 0.6));
					QRect drawRect(0, posY, ITEM_AREA_WIDTH, height);
					painter.drawRect(drawRect);
				}

				offset += ITEM_AREA_WIDTH;

				if(uData->hasNRefToSelfPost())
				{
					painter.setBrush(repPostsBrushColor);
					painter.setPen(QPen(QBrush(repPostsPenColor), 0.6));
					QRect drawRect(offset, posY, ITEM_AREA_WIDTH, height);
					painter.drawRect(drawRect);
				}
			}
			offset += ITEM_AREA_WIDTH;

			///////////////////////////////////////////////////////////////////
			/////        LE BIGONO (ENCORE)                  /////////////////
			///////////////////////////////////////////////////////////////////
			QList<QQBigornoItem> bigItems = uData->bigornoItems();
			if(! bigItems.isEmpty())
			{
				painter.setBrush(QBrush(bigornoBrushColor));
				painter.setPen(QPen(QBrush(bigornoPenColor), 0.6));
				QRect drawRect(offset, posY, ITEM_AREA_WIDTH, height);
				painter.drawRect(drawRect);
			}
		}
		block = block.next();
	}
}

void QQTextBrowser::updateNotifArea(int)
{
	m_notifArea->update();
}

void QQTextBrowser::highlightNorloge(QQNorlogeRef nRef)
{
	QString id = nRef.nRefId().append("@").append(nRef.dstBouchot());
	if(id != m_highlightedNorlogeRef)
	{
		unHighlightNorloge();
		emit norlogeRefHovered(nRef);
		m_highlightedNorlogeRef = id;
	}
}

void QQTextBrowser::unHighlightNorloge()
{
	if(! m_highlightedNorlogeRef.isEmpty())
	{
		emit unHighlight(this);
		m_highlightedNorlogeRef.clear();
	}
}

void QQTextBrowser::showTotoz(QString & totozId)
{
	if(totozId != m_displayedTotozId)
	{
		m_displayedTotozId = totozId;
		emit displayTotoz(totozId);
	}
}

void QQTextBrowser::handleContentTypeAvailable(QUrl &url, QString &contentType)
{
	QString ttText = QToolTip::text();
	if(ttText.length() > 0 && url == m_shownUrl)
	{
		ttText.append(" (").append(contentType).append(")");
		QToolTip::showText(QCursor::pos(), ttText, this);

		if(contentType.startsWith("image/"))
		{
			emit displayWebImage(url);
		}
	}
}

void QQTextBrowser::webSearchActionTriggered()
{
	QQSettings settings;
	QString webSearchUrl = settings.value(SETTINGS_GENERAL_WEBSEARCH_URL, DEFAULT_GENERAL_WEBSEARCH_URL).toString();
	QTextCursor cursor = textCursor();
	QString selection = cursor.selectedText();

	if(! selection.isEmpty())
	{
		webSearchUrl.replace("%s", QUrl::toPercentEncoding(selection));
		QDesktopServices::openUrl(QUrl::fromEncoded(webSearchUrl.toLatin1()));
	}
}

void QQTextBrowser::clearViewers()
{
	if(m_displayedTotozId.length() > 0)
	{
		m_displayedTotozId.clear();
		emit hideViewers();
	}
	if(! QToolTip::isVisible())
	{
		m_shownUrl.clear();
		emit hideViewers();
	}
}

/*
 * Recuperation de d'un evenement lorsque la souris quitte la
 *surface de l'afficheur.
 */
void QQTextBrowser::leaveEvent(QEvent * event)
{
	QTextBrowser::leaveEvent(event);

	// On masque les élements d'affichage dynamiques
	unHighlightNorloge();
	QToolTip::hideText();
	clearViewers();
}

void QQTextBrowser::mouseMoveEvent(QMouseEvent * event)
{
	//qDebug() << "####################################";
	//qDebug() << "QQTextBrowser::mouseMoveEvent x=" << event->x() << ",y=" << event->y();
	//qDebug() << "####################################";
	QTextBrowser::mouseMoveEvent(event);

	if(event->buttons() == Qt::LeftButton)
		return;

	QString anchor = anchorAt(event->pos());
	if(anchor.size() > 0)
	{
		QUrl anchorUrl = QUrl(anchor);
		if(anchorUrl != m_shownUrl)
		{
			unHighlightNorloge();
			hideViewers();
			m_shownUrl = anchorUrl;
		}
		else
			return;

		QString anchorScheme = anchorUrl.scheme();
		if(anchorScheme == "totoz") // Un [:totoz]
		{
			viewport()->unsetCursor();
			QString totozId = anchorUrl.path().remove(0, 1); // Le / initial
			showTotoz(totozId);
		}
		else if (anchorScheme == "nref") // Une norloge
		{
			viewport()->unsetCursor();
			QQMessageBlockUserData * blockData = (QQMessageBlockUserData *)
					(cursorForPosition(event->pos()).block().userData());

			bool isInt = true;
			QUrlQuery anchorUrlQuery(anchorUrl);
			int index = anchorUrlQuery.queryItemValue("index").toInt(&isInt);
			if(isInt)
			{
				QQNorlogeRef nRef = blockData->norlogeRefForIndex(index);
				highlightNorloge(nRef);
			}
			else
				qWarning() << "NRef anchor badly formatted :" << anchorUrl;
		}
		else // if(anchorScheme == "http" || anchorScheme == "https") Une [url]
		{
			QFontMetrics fm(QToolTip::font());
			QToolTip::showText(event->globalPos(), fm.elidedText(anchor, Qt::ElideMiddle, 500), this);

			m_urlHelper->getContentType(m_shownUrl);
		}
	}
	else
	{
		// Il faut unhilighter puisqu'on ne survole pas de zone de norloge ni de zone de message
		unHighlightNorloge();
		clearViewers();
	}
}

void QQTextBrowser::mousePressEvent(QMouseEvent * event)
{
	// Stockage de la postion
	m_lastPoint = event->pos();

	QTextBrowser::mousePressEvent(event);

	if(event->button() == Qt::LeftButton)
		viewport()->setCursor(Qt::IBeamCursor);
}

void QQTextBrowser::mouseReleaseEvent(QMouseEvent * event)
{
	QTextBrowser::mouseReleaseEvent(event);

	QString httpAnchor = anchorAt(event->pos());
	QCursor mCursor(Qt::ArrowCursor);
	if(httpAnchor.length() > 0)
		mCursor.setShape(Qt::PointingHandCursor);
	viewport()->setCursor(mCursor);

	// Verification que l'on est pas en pleine selection
	if(event->pos() != m_lastPoint)
		return;

	// Reset de l'icone de l'application
	QIcon icon = QIcon(QString::fromLatin1(":/img/rubber_duck_yellow.svg"));
	window()->setWindowIcon(icon);

	// Marquage des posts comme lus
	bool needUpdate = false;
	foreach(QQBouchot *b, QQBouchot::listBouchotsGroup(m_groupName))
	{
		QListIterator<QQPostPtr> pI(b->postsHistory());
		pI.toBack();
		while(pI.hasPrevious())
		{
			QQPost *p = pI.previous();
			if(p->isUnread())
			{
				p->setRead();
				needUpdate = true;
			}
			else
				break;
		}
	}
	if(needUpdate)
	{
		m_notifArea->update();
		emit newPostsAcknowledged(m_groupName);
	}

	// Gestion du clic sur une norloge ou un login
	QTextCursor cursor = cursorForPosition(event->pos());
	QTextBlock block = cursor.block();
	QQMessageBlockUserData *blockData = (QQMessageBlockUserData *) block.userData();

	if(blockData != NULL)
	{
		QQPost * post = blockData->post();
		Q_ASSERT(post != NULL);

		// Clic sur Norloge
		//Zone message
		int posInBlock =  cursor.positionInBlock();
		bool duckLaunched = false;
		if(blockData->isIndexInZRange(posInBlock, QQMessageBlockUserData::NORLOGE))
		{
			QQNorloge norloge(post->bouchot()->name(),
							  post->norloge());
			if(post->isNorlogeMultiple())
				norloge.setNorlogeIndex(post->norlogeIndex());
			emit norlogeClicked(post->bouchot()->name(), norloge);
		}
		// Clic sur Login/UA
		else if(blockData->isIndexInZRange(posInBlock, QQMessageBlockUserData::LOGINUA))
		{
			QString login = post->login();
			if(login.size() == 0)
			{
				QString ua = post->UA();
				if(ua.size() != 0 && ! ua.contains('/', Qt::CaseSensitive))
					emit loginClicked(post->bouchot()->name(), ua);
			}
			else
				emit loginClicked(post->bouchot()->name(), login);
		}
		// Clic sur Norloge dans Message (NorlogeRef)
		else if(blockData->isIndexInZRange(posInBlock, QQMessageBlockUserData::MESSAGE))
		{
			QQNorlogeRef nRef = blockData->norlogeRefForIndex(posInBlock);
			if(nRef.isValid())
				emit norlogeRefClicked(post->bouchot()->name(), nRef);
			else
			{
				QPair<int, QString> duck = blockData->duckForIndex(posInBlock);
				if(duck.first >= 0)
				{
					duckLaunched = true;
					emit duckClicked(blockData->post()->bouchot()->name(), blockData->post()->id(), blockData->post()->isSelfPost());
				}
			}
		}
		if(!duckLaunched)
			emit shotDuck(event->button() == Qt::MiddleButton);
	}
}

void QQTextBrowser::paintEvent(QPaintEvent * event)
{
	QTextBrowser::paintEvent(event);

	// Pour le bigorno
	QPainter bigornoPainter(viewport());
	bigornoPainter.setRenderHint(QPainter::Antialiasing, true);
	bigornoPainter.setRenderHint(QPainter::HighQualityAntialiasing, true);
	QColor bigornoBrushColor(60, 0, 0, 100);
	bigornoPainter.setBrush(QBrush(bigornoBrushColor));
	QColor bigornoPenColor(150, 0, 0);
	bigornoPainter.setPen(QPen(QBrush(bigornoPenColor), 0.6));

	// Recuperation du premier bloc a dessiner
	QTextBlock block = cursorForPosition(
				event->rect().topLeft()
				).block();

	// Recuperation du dernier bloc a dessiner
	QTextBlock lastBlock = cursorForPosition(
				event->rect().bottomRight()
				).block();
	while(block.isValid() && block.blockNumber() <= lastBlock.blockNumber())
	{
		QQMessageBlockUserData * uData = (QQMessageBlockUserData *) block.userData();
		if(uData != NULL)
		{
			///////////////////////////////////////////////////////////////////
			/////        LE BIGORNO                           /////////////////
			///////////////////////////////////////////////////////////////////
			QList<QQBigornoItem> bigItems = uData->bigornoItems();
			if(! bigItems.isEmpty())
			{
				QListIterator<QQBigornoItem> i(bigItems);
				while(i.hasNext())
				{
					QQBigornoItem bigItem = i.next();
					QTextCursor cursor = QTextCursor(block);
					cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, bigItem.position());
					QRect rect = cursorRect(cursor);
					rect.setWidth(QFontMetrics(document()->defaultFont()).boundingRect(bigItem.word()).width());
					rect.adjust(-1, 0, +1, 0);

					bigornoPainter.drawRoundedRect(rect, 5.0, 5.0);
				}
			}
		}
		block = block.next();
	}
	m_notifArea->update();
}

void QQTextBrowser::resizeEvent(QResizeEvent * event)
{
	QScrollBar * vScrollBar = verticalScrollBar();
	bool isMax = (vScrollBar->sliderPosition() == vScrollBar->maximum());
	QTextBrowser::resizeEvent(event);
	if(isMax)
		vScrollBar->triggerAction(QAbstractSlider::SliderToMaximum);

	QRect cr = contentsRect();
	m_notifArea->setGeometry(QRect(cr.left(), cr.top(), notifAreaWidth(), cr.height()));
}

void QQTextBrowser::wheelEvent(QWheelEvent * event)
{
	if(! (event->modifiers() && Qt::ControlModifier != 0))
		QTextBrowser::wheelEvent(event);
}

void QQTextBrowser::contextMenuEvent(QContextMenuEvent * ev)
{
	if(m_displayedTotozId.size() > 0)
	{
		QPoint pos = ev->globalPos();
		emit displayTotozContextMenu(pos);
		ev->accept();
	}
	else
	{
		QPoint evPos = ev->pos();
		evPos.setX(evPos.x() + horizontalScrollBar()->value());
		evPos.setY(evPos.y() + verticalScrollBar()->value());
		QMenu *menu = createStandardContextMenu(evPos);
		QTextCursor cursor = textCursor();
		if(cursor.hasSelection())
		{
			QAction *action = menu->addAction(tr("&Search on web"));
			connect(action, SIGNAL(triggered()), this, SLOT(webSearchActionTriggered()));
		}
		menu->exec(QCursor::pos());
	}
}
