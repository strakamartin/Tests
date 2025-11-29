#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include "models.h"

class QListWidget;
class QPushButton;
class QTextEdit;
class QComboBox;
class QTableWidget;
class QLineEdit;
class QSpinBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onAddQuestion();
    void onRemoveQuestion();
    void onQuestionSelected();
    void onTypeChanged(int idx);
    void onAddAnswer();
    void onRemoveAnswer();
    void onLoadFromDB();
    void onSaveToDB();
    void onApplyEdit();
    void onStartTest();

private:
    void buildUi();
    void refreshQuestionList();
    void loadQuestionIntoEditor(int index);
    void collectEditorToQuestion(int index);

    QVector<Question> m_questions;

    // editor widgets
    QListWidget *m_listQuestions;
    QPushButton *m_btnAddQuestion;
    QPushButton *m_btnRemoveQuestion;
    QPushButton *m_btnLoadDB;
    QPushButton *m_btnSaveDB;
    QTextEdit *m_editQuestionText;
    QComboBox *m_comboType;
    QTableWidget *m_tblAnswers;
    QPushButton *m_btnAddAnswer;
    QPushButton *m_btnRemoveAnswer;
    QLineEdit *m_editExpectedText;
    QPushButton *m_btnApplyEdit;

    QSpinBox *m_spinTestCount;
    QPushButton *m_btnStartTest;
};

#endif // MAINWINDOW_H
