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
    bool loadQuestionsFromDB(); // loads mAllQuestions from DB using mTestId
    void prepareRandomTest();
    void showCurrentQuestion();
    void saveCurrentAnswerForIndex(int index);
    double evaluateAndReturnScore(QVector<DBManager::ResultDetail> &outDetails);

    QVector<Question> mAllQuestions;   // all questions loaded for the selected test
    QVector<Question> mTestQuestions;  // randomized subset used in test runtime
    int mQuestionCount = 10;
    int mCurrentIndex = 0;

    // UI
    QLabel *mLblProgress;
    QLabel *mLblQuestion;
    QWidget *mAnswerWidget; // holder for dynamic widgets
    QPushButton *mBtnNext;
    QPushButton *mBtnSubmit;

    // email UI
    QLineEdit *mEditStudentEmail;
    QPushButton *mBtnSendEmail;

    // per-question dynamic widgets & storage
    QList<QWidget*> mCurrentAnswerWidgets;
    struct StoredAnswer {
        QString questionId;
        // for choices: store selected option texts (matching by text)
        QStringList selectedOptionsTexts;
        // for text: stored text
        QString textAnswer;
    };
    QVector<StoredAnswer> mUserAnswers; // same size as mTestQuestions

    // store current test id/name for saving results / email subject
    QString mTestId;
    QString mTestName;
};

#endif // TESTRUNNER_H
