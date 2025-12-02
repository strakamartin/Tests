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
    void setQuestionCount(int n);
    void startTest();

    // set test id (which test the student is taking)
    void setTestId(const QString &testId);
    void setTestName(const QString &testName);

private slots:
    void onNext();
    void onSubmit();
    void onSendEmail();

private:
    bool loadQuestionsFromDB(); // loads m_allQuestions from DB using m_testId
    void prepareRandomTest();
    void showCurrentQuestion();
    void saveCurrentAnswerForIndex(int index);
    double evaluateAndReturnScore(QVector<DBManager::ResultDetail> &outDetails);

    QVector<Question> m_allQuestions;   // all questions loaded for the selected test
    QVector<Question> m_testQuestions;  // randomized subset used in test runtime
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

    // store current test id/name for saving results / email subject
    QString m_testId;
    QString m_testName;
};

#endif // TESTRUNNER_H
