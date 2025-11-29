#ifndef TESTRUNNER_H
#define TESTRUNNER_H

#include <QDialog>
#include "models.h"
#include "dbmanager.h"

class QLabel;
class QPushButton;
class QLineEdit;
class QWidget;

class Testrunner : public QDialog
{
    Q_OBJECT
public:
    explicit Testrunner(QWidget *parent = nullptr);
    void setQuestions(const QVector<Question> &qs);
    void setQuestionCount(int n);
    void startTest();

private slots:
    void onNext();
    void onSubmit();
    void onSendEmail();

private:
    void prepareRandomTest();
    void showCurrentQuestion();
    void saveCurrentAnswerForIndex(int index);
    double evaluateAndReturnScore(QVector<DBManager::ResultDetail> &outDetails);

    QVector<Question> m_allQuestions;
    QVector<Question> m_testQuestions;
    int m_questionCount = 10;
    int m_currentIndex = 0;

    // UI
    QLabel *m_lblProgress;
    QLabel *m_lblQuestion;
    QWidget *m_answerWidget; // holder for dynamic widgets
    QPushButton *m_btnNext;
    QPushButton *m_btnSubmit;

    // email UI
    QLineEdit *m_editStudentEmail;
    QPushButton *m_btnSendEmail;

    // per-question dynamic widgets & storage
    QList<QWidget*> m_currentAnswerWidgets;
    struct StoredAnswer {
        QString questionId;
        // for choices: store selected option texts (matching by text)
        QStringList selectedOptionsTexts;
        // for text: stored text
        QString textAnswer;
    };
    QVector<StoredAnswer> m_userAnswers; // same size as m_testQuestions
};

#endif // TESTRUNNER_H
