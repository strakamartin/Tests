#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QTimer>
#include "models.h"

class QListWidget;
class QPushButton;
class QTextEdit;
class QComboBox;
class QTableWidget;
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

    // auto-save
    void scheduleAutoSave();
    void doAutoSave();

    // student-specific
    void onStudentStartTest();
    void onStudentNext();
    void onStudentSubmit();

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
    bool m_teacherMode;
    QVector<Test> m_tests;
    QVector<Question> m_questions; // in teacher mode: questions for selected test
    QVector<Question> m_studentQuestions; // in student mode: current test questions
    QVector<QString> m_studentAnswers; // per-student answers (parallel to m_studentQuestions)
    int m_studentCurrentIndex = 0;

    // AUTO SAVE timer (debounce)
    QTimer m_autoSaveTimer;

    // Teacher widgets
    QListWidget *m_listTests; // also used in student mode as test selector
    QLineEdit *m_editTestName;
    QLineEdit *m_editTestDescription;
    QListWidget *m_listQuestions;
    QTextEdit *m_editQuestionText;
    QComboBox *m_comboType;
    QTableWidget *m_tblAnswers;
    QPushButton *m_btnAddTest;
    QPushButton *m_btnRemoveTest;
    QPushButton *m_btnAddQuestion;
    QPushButton *m_btnRemoveQuestion;
    QPushButton *m_btnAddAnswer;
    QPushButton *m_btnRemoveAnswer;
    QLineEdit *m_editExpectedText;

    // Student widgets (right side)
    QLabel *m_lblStudentProgress;
    QLabel *m_lblStudentQuestion;
    QWidget *m_widgetStudentAnswers; // placeholder layout for dynamic widgets
    QPushButton *m_btnStudentNext;
    QPushButton *m_btnStudentSubmit;
    QLineEdit *m_editStudentEmail;
    QSpinBox *m_spinStudentCount;

    // helper to build answer widgets (used for both modes)
    QList<QWidget*> m_currentAnswerWidgets;

    // track current selected test id (for teacher/student)
    QString currentTestId() const;
    int currentTestIndex() const;
};

#endif // MAINWINDOW_H
