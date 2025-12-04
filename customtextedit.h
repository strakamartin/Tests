// CustomTextEdit.h
#include <QTextEdit>
#include <QFocusEvent>

class CustomTextEdit : public QTextEdit
{
    Q_OBJECT
public:

signals:
    void editingFinished();

protected:
    void focusOutEvent(QFocusEvent *event) override
    {
        emit editingFinished();
        QTextEdit::focusOutEvent(event);
    }
    // You might also want to handle the Enter key if it's meant to finish editing
};
