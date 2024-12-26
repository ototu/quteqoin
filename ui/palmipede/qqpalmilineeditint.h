#ifndef QQPALMILINEEDITINT_H
#define QQPALMILINEEDITINT_H

#include <QLineEdit>
#include <QPair>
#include <QQueue>

class QFocusEvent;
class QKeyEvent;

class QQPalmiLineEditInt : public QLineEdit
{
	Q_OBJECT

public:
	explicit QQPalmiLineEditInt(QWidget *parent = 0);

	void pushCurrentToHistory();

signals:
	void changeBoard(bool next);

public slots:
	void insertText(const QString &str);

protected:
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void focusInEvent(QFocusEvent *e);
	virtual void focusOutEvent(QFocusEvent *e);
	virtual void keyPressEvent(QKeyEvent *e);

private:
	void updateTotozCompleter();
	void completeTotoz();
	void rotateHistory(bool forward = true);

	int m_indexInPostHistory;
	QQueue<QString> m_postHistory;

	QStringList m_listTotoz;

	quint32 m_pctUpload;
	QPair<int, int> m_savedSelection;
	QWidget *m_bgWidget;
};

#endif // QQPALMILINEEDITINT_H
