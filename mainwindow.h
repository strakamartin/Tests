#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QTimer>
#include "models.h"

class CustomTextEdit;
class QListWidget;
class QPushButton;
class QTextEdit;
class QComboBox;
class QTableWidget;
class QTableWidgetItem;
class QHeaderView;
class QLineEdit;
class QSpinBox;
class QLabel;
class QStackedWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(bool teacherMode = false, QWidget *parent = nullptr);

private slots:
    // common
    void onAddTest();
    void onRemoveTest();
    void onTestSelected(int idx);

    // teacher-specific
    void onAddQuestion();
    void onRemoveQuestion();
    void onQuestionSelected(int row);
    void onTypeChanged(int idx);
    void onAddAnswer();
    void onRemoveAnswer();

    void answerItemChanged(QTableWidgetItem *item);
    // auto-save
    void scheduleAutoSave();
    bool doAutoSave();
    void doAutoSaveWithRefresh();

    // student-specific
    void onStudentStartTest();
    void onStudentNext();
    void onStudentSubmit();

    void saveCurrentTestToDb();
    void saveCurrentTestToDb1(int i);

private:
    void buildTeacherUi();
    void buildStudentUi();
    void refreshTestList();
    void refreshQuestionList();
    void loadQuestionIntoEditor(int index);
    void collectEditorToQuestion(int index);

    // new helper for student UI
    void showStudentQuestion(int index);

    // data
    bool mTeacherMode;
    QVector<Test> mTests;
    QVector<Question> mQuestions; // in teacher mode: questions for selected test
    QVector<Question> mStudentQuestions; // in student mode: current test questions
    QVector<QString> mStudentAnswers; // per-student answers (parallel to m_studentQuestions)
    int mStudentCurrentIndex = 0;

    // AUTO SAVE timer (debounce)
    QTimer mAutoSaveTimer;

    // Teacher widgets
    //tests widgets
    QListWidget *mListTests; // also used in student mode as test selector
    QLineEdit *mEditTestName;
    QLineEdit *mEditTestDescription;
    QPushButton *mBtnAddTest;
    QPushButton *mBtnRemoveTest;

    // questions widgets
    QListWidget *mListQuestions;
    CustomTextEdit *mEditQuestionText;
    QPushButton *mBtnAddQuestion;
    QPushButton *mBtnRemoveQuestion;

    // answer editor widgets
    QComboBox *mComboType;
    QTableWidget *mTblAnswers;
    QPushButton *mBtnAddAnswer;
    QPushButton *mBtnRemoveAnswer;
    QLineEdit *mEditExpectedText;

    // Student widgets (right side)
    QLabel *mLblStudentProgress;
    QLabel *mLblStudentQuestion;
    QWidget *mWidgetStudentAnswers; // placeholder layout for dynamic widgets
    QPushButton *mBtnStudentNext;
    QPushButton *mBtnStudentSubmit;
    QLineEdit *mEditStudentEmail;
    QSpinBox *mSpinStudentCount;

    // helper to build answer widgets (used for both modes)
    QList<QWidget*> mCurrentAnswerWidgets;

    // track current selected test id (for teacher/student)
    QString currentTestId() const;
    int currentTestIndex() const;
};

#endif // MAINWINDOW_H
