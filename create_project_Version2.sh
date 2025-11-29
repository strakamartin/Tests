#!/usr/bin/env bash
set -e

cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.5)

project(QtTestMaker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets Sql REQUIRED)

add_executable(QtTestMaker
    main.cpp
    models.h
    dbmanager.h
    dbmanager.cpp
    mainwindow.h
    mainwindow.cpp
    testrunner.h
    testrunner.cpp
)

target_link_libraries(QtTestMaker PRIVATE Qt5::Widgets Qt5::Sql)
EOF

cat > main.cpp <<'EOF'
#include <QApplication>
#include "mainwindow.h"
#include "dbmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Optional: initialize a default DB file in the app working directory
    QString defaultDb = "questions.db";
    QString err;
    if (!DBManager::instance().openDatabase(defaultDb, &err)) {
        // If DB cannot be created/opened, show a warning but still run app
        qWarning("DB open failed: %s", qPrintable(err));
    }

    MainWindow w;
    w.resize(1000, 600);
    w.show();
    return a.exec();
}
EOF

cat > models.h <<'EOF'
#ifndef MODELS_H
#define MODELS_H

#include <QString>
#include <QVector>

// Question types
enum class QuestionType {
    SingleChoice = 0,
    MultipleChoice = 1,
    TextAnswer = 2
};

// Answer option for choice questions
struct Answer {
    QString text;
    bool correct = false;
};

// Question model
struct Question {
    QString id; // unique id (QString, e.g. UUID)
    QString text;
    QuestionType type = QuestionType::SingleChoice;
    QVector<Answer> options; // for choice questions
    QString expectedText;    // for exact text answers
};

#endif // MODELS_H
EOF

cat > dbmanager.h <<'EOF'
#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QString>
#include <QVector>
#include <QSqlDatabase>
#include "models.h"

// Simple DB manager for SQLite usage
class DBManager
{
public:
    static DBManager &instance();

    // open (and create) database file
    bool openDatabase(const QString &path, QString *err = nullptr);

    // CRUD for questions
    bool loadAllQuestions(QVector<Question> &outQuestions, QString *err = nullptr);
    bool addOrUpdateQuestion(const Question &q, QString *err = nullptr);
    bool removeQuestion(const QString &questionId, QString *err = nullptr);

    // Save test result (with details per question)
    struct ResultDetail {
        QString questionId;
        bool correct;
        QString userAnswer; // text describing user's answer
    };
    bool saveResult(const QString &studentEmail, double score, int total,
                    const QVector<ResultDetail> &details, QString *err = nullptr);

private:
    DBManager() = default;
    bool ensureSchema(QString *err = nullptr);

    QSqlDatabase m_db;
};

#endif // DBMANAGER_H
EOF

cat > dbmanager.cpp <<'EOF'
#include "dbmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QUuid>

DBManager &DBManager::instance()
{
    static DBManager inst;
    return inst;
}

bool DBManager::openDatabase(const QString &path, QString *err)
{
    if (m_db.isValid() && m_db.isOpen()) m_db.close();

    m_db = QSqlDatabase::addDatabase("QSQLITE", "qt_test_maker_connection");
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        if (err) *err = m_db.lastError().text();
        return false;
    }
    return ensureSchema(err);
}

bool DBManager::ensureSchema(QString *err)
{
    QSqlQuery q(m_db);
    // questions table
    if (!q.exec(
        "CREATE TABLE IF NOT EXISTS questions ("
        "id TEXT PRIMARY KEY,"
        "text TEXT NOT NULL,"
        "type INTEGER NOT NULL,"
        "expected_text TEXT"
        ")"
    )) {
        if (err) *err = q.lastError().text();
        return false;
    }

    // options table
    if (!q.exec(
        "CREATE TABLE IF NOT EXISTS options ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "question_id TEXT NOT NULL,"
        "text TEXT NOT NULL,"
        "correct INTEGER NOT NULL,"
        "ord INTEGER NOT NULL,"
        "FOREIGN KEY(question_id) REFERENCES questions(id) ON DELETE CASCADE"
        ")"
    )) {
        if (err) *err = q.lastError().text();
        return false;
    }

    // results table
    if (!q.exec(
        "CREATE TABLE IF NOT EXISTS results ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "student_email TEXT,"
        "score REAL,"
        "total INTEGER,"
        "timestamp TEXT"
        ")"
    )) {
        if (err) *err = q.lastError().text();
        return false;
    }

    // result details table
    if (!q.exec(
        "CREATE TABLE IF NOT EXISTS result_details ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "result_id INTEGER NOT NULL,"
        "question_id TEXT,"
        "correct INTEGER,"
        "user_answer TEXT,"
        "FOREIGN KEY(result_id) REFERENCES results(id) ON DELETE CASCADE"
        ")"
    )) {
        if (err) *err = q.lastError().text();
        return false;
    }

    return true;
}

bool DBManager::loadAllQuestions(QVector<Question> &outQuestions, QString *err)
{
    outQuestions.clear();
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, text, type, expected_text FROM questions ORDER BY rowid")) {
        if (err) *err = q.lastError().text();
        return false;
    }
    while (q.next()) {
        Question qq;
        qq.id = q.value(0).toString();
        qq.text = q.value(1).toString();
        qq.type = static_cast<QuestionType>(q.value(2).toInt());
        qq.expectedText = q.value(3).toString();
        // load options
        QSqlQuery q2(m_db);
        q2.prepare("SELECT text, correct FROM options WHERE question_id = :qid ORDER BY ord");
        q2.bindValue(":qid", qq.id);
        if (!q2.exec()) {
            if (err) *err = q2.lastError().text();
            return false;
        }
        while (q2.next()) {
            Answer a;
            a.text = q2.value(0).toString();
            a.correct = q2.value(1).toInt() != 0;
            qq.options.append(a);
        }
        outQuestions.append(qq);
    }
    return true;
}

bool DBManager::addOrUpdateQuestion(const Question &qobj, QString *err)
{
    // Transactional: insert/update question row and replace options
    if (!m_db.transaction()) {
        if (err) *err = m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    // check existence
    q.prepare("SELECT COUNT(1) FROM questions WHERE id = :id");
    q.bindValue(":id", qobj.id);
    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        m_db.rollback();
        return false;
    }
    bool exists = false;
    if (q.next()) exists = (q.value(0).toInt() > 0);

    if (!exists) {
        QSqlQuery ins(m_db);
        ins.prepare("INSERT INTO questions (id, text, type, expected_text) VALUES (:id, :text, :type, :expected)");
        ins.bindValue(":id", qobj.id);
        ins.bindValue(":text", qobj.text);
        ins.bindValue(":type", static_cast<int>(qobj.type));
        ins.bindValue(":expected", qobj.expectedText);
        if (!ins.exec()) {
            if (err) *err = ins.lastError().text();
            m_db.rollback();
            return false;
        }
    } else {
        QSqlQuery upd(m_db);
        upd.prepare("UPDATE questions SET text=:text, type=:type, expected_text=:expected WHERE id=:id");
        upd.bindValue(":text", qobj.text);
        upd.bindValue(":type", static_cast<int>(qobj.type));
        upd.bindValue(":expected", qobj.expectedText);
        upd.bindValue(":id", qobj.id);
        if (!upd.exec()) {
            if (err) *err = upd.lastError().text();
            m_db.rollback();
            return false;
        }
        // delete existing options; we will reinsert
        QSqlQuery del(m_db);
        del.prepare("DELETE FROM options WHERE question_id = :qid");
        del.bindValue(":qid", qobj.id);
        if (!del.exec()) {
            if (err) *err = del.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    // insert options
    for (int i = 0; i < qobj.options.size(); ++i) {
        const Answer &a = qobj.options[i];
        QSqlQuery iopt(m_db);
        iopt.prepare("INSERT INTO options (question_id, text, correct, ord) VALUES (:qid, :text, :corr, :ord)");
        iopt.bindValue(":qid", qobj.id);
        iopt.bindValue(":text", a.text);
        iopt.bindValue(":corr", a.correct ? 1 : 0);
        iopt.bindValue(":ord", i);
        if (!iopt.exec()) {
            if (err) *err = iopt.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        if (err) *err = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool DBManager::removeQuestion(const QString &questionId, QString *err)
{
    if (!m_db.transaction()) {
        if (err) *err = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM questions WHERE id = :id");
    q.bindValue(":id", questionId);
    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        if (err) *err = q.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool DBManager::saveResult(const QString &studentEmail, double score, int total,
                           const QVector<ResultDetail> &details, QString *err)
{
    if (!m_db.transaction()) {
        if (err) *err = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO results (student_email, score, total, timestamp) VALUES (:email, :score, :total, :ts)");
    q.bindValue(":email", studentEmail);
    q.bindValue(":score", score);
    q.bindValue(":total", total);
    q.bindValue(":ts", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.prepare("SELECT last_insert_rowid()");
    if (!q.exec()) {
        if (err) *err = q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.next();
    int resultId = q.value(0).toInt();

    for (const ResultDetail &d : details) {
        QSqlQuery qd(m_db);
        qd.prepare("INSERT INTO result_details (result_id, question_id, correct, user_answer) "
                   "VALUES (:rid, :qid, :corr, :ua)");
        qd.bindValue(":rid", resultId);
        qd.bindValue(":qid", d.questionId);
        qd.bindValue(":corr", d.correct ? 1 : 0);
        qd.bindValue(":ua", d.userAnswer);
        if (!qd.exec()) {
            if (err) *err = qd.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        if (err) *err = q.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}
EOF

cat > mainwindow.h <<'EOF'
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
EOF

cat > mainwindow.cpp <<'EOF'
#include "mainwindow.h"
#include "dbmanager.h"
#include "testrunner.h"

#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QCheckBox>
#include <QApplication>
#include <QUuid>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    // Load questions from current DB (if any)
    onLoadFromDB();
}

void MainWindow::buildUi()
{
    QWidget *central = new QWidget;
    setCentralWidget(central);

    m_listQuestions = new QListWidget;
    m_btnAddQuestion = new QPushButton("Přidat otázku");
    m_btnRemoveQuestion = new QPushButton("Odstranit");
    m_btnLoadDB = new QPushButton("Načíst z DB");
    m_btnSaveDB = new QPushButton("Uložit do DB");

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(m_listQuestions);
    QHBoxLayout *leftButtons = new QHBoxLayout;
    leftButtons->addWidget(m_btnAddQuestion);
    leftButtons->addWidget(m_btnRemoveQuestion);
    leftLayout->addLayout(leftButtons);
    leftLayout->addWidget(m_btnLoadDB);
    leftLayout->addWidget(m_btnSaveDB);

    QWidget *leftWidget = new QWidget;
    leftWidget->setLayout(leftLayout);

    // editor on right
    QLabel *lblQ = new QLabel("Text otázky:");
    m_editQuestionText = new QTextEdit;
    QLabel *lblType = new QLabel("Typ otázky:");
    m_comboType = new QComboBox;
    m_comboType->addItem("Jedna správná (single)");
    m_comboType->addItem("Více správných (multiple)");
    m_comboType->addItem("Textová odpověď (exact text)");

    QLabel *lblOptions = new QLabel("Možnosti (pro choice):");
    m_tblAnswers = new QTableWidget(0, 2);
    m_tblAnswers->setHorizontalHeaderLabels(QStringList() << "Text" << "Správná");
    m_tblAnswers->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tblAnswers->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    m_btnAddAnswer = new QPushButton("Přidat možnost");
    m_btnRemoveAnswer = new QPushButton("Odstranit možnost");

    QLabel *lblExpected = new QLabel("Očekávaný text (pro text. odpověď):");
    m_editExpectedText = new QLineEdit;

    m_btnApplyEdit = new QPushButton("Uložit změny do paměti");

    QLabel *lblTestCount = new QLabel("Počet otázek v testu:");
    m_spinTestCount = new QSpinBox;
    m_spinTestCount->setMinimum(1);
    m_spinTestCount->setMaximum(1000);
    m_spinTestCount->setValue(10);
    m_btnStartTest = new QPushButton("Spustit test");

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(lblQ);
    rightLayout->addWidget(m_editQuestionText);
    rightLayout->addWidget(lblType);
    rightLayout->addWidget(m_comboType);
    rightLayout->addWidget(lblOptions);
    rightLayout->addWidget(m_tblAnswers);
    QHBoxLayout *ansBtns = new QHBoxLayout;
    ansBtns->addWidget(m_btnAddAnswer);
    ansBtns->addWidget(m_btnRemoveAnswer);
    rightLayout->addLayout(ansBtns);
    rightLayout->addWidget(lblExpected);
    rightLayout->addWidget(m_editExpectedText);
    rightLayout->addWidget(m_btnApplyEdit);
    QHBoxLayout *testSettings = new QHBoxLayout;
    testSettings->addWidget(lblTestCount);
    testSettings->addWidget(m_spinTestCount);
    testSettings->addWidget(m_btnStartTest);
    rightLayout->addLayout(testSettings);
    rightLayout->addStretch(1);

    QWidget *rightWidget = new QWidget;
    rightWidget->setLayout(rightLayout);

    QSplitter *split = new QSplitter;
    split->addWidget(leftWidget);
    split->addWidget(rightWidget);
    split->setStretchFactor(1, 1);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(split);
    central->setLayout(mainLayout);

    // connections
    connect(m_btnAddQuestion, &QPushButton::clicked, this, &MainWindow::onAddQuestion);
    connect(m_btnRemoveQuestion, &QPushButton::clicked, this, &MainWindow::onRemoveQuestion);
    connect(m_listQuestions, &QListWidget::currentRowChanged, this, &MainWindow::onQuestionSelected);
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTypeChanged);
    connect(m_btnAddAnswer, &QPushButton::clicked, this, &MainWindow::onAddAnswer);
    connect(m_btnRemoveAnswer, &QPushButton::clicked, this, &MainWindow::onRemoveAnswer);
    connect(m_btnLoadDB, &QPushButton::clicked, this, &MainWindow::onLoadFromDB);
    connect(m_btnSaveDB, &QPushButton::clicked, this, &MainWindow::onSaveToDB);
    connect(m_btnApplyEdit, &QPushButton::clicked, this, &MainWindow::onApplyEdit);
    connect(m_btnStartTest, &QPushButton::clicked, this, &MainWindow::onStartTest);

    // initial state
    onTypeChanged(0);
}

void MainWindow::onAddQuestion()
{
    Question q;
    q.id = QUuid::createUuid().toString();
    q.text = "Nová otázka";
    q.type = QuestionType::SingleChoice;
    q.options = { Answer{"Možnost 1", true}, Answer{"Možnost 2", false} };
    m_questions.append(q);
    refreshQuestionList();
    m_listQuestions->setCurrentRow(m_questions.size()-1);
}

void MainWindow::onRemoveQuestion()
{
    int row = m_listQuestions->currentRow();
    if (row < 0 || row >= m_questions.size()) return;

    QString err;
    if (!DBManager::instance().removeQuestion(m_questions[row].id, &err)) {
        QMessageBox::warning(this, "Chyba při mazání z DB", err);
        // still remove locally if desired; here we attempt to reload
    }

    m_questions.remove(row);
    refreshQuestionList();
    if (!m_questions.isEmpty()) m_listQuestions->setCurrentRow(qMin(row, m_questions.size()-1));
    else {
        m_editQuestionText->clear();
        m_tblAnswers->setRowCount(0);
        m_editExpectedText->clear();
    }
}

void MainWindow::refreshQuestionList()
{
    m_listQuestions->clear();
    for (const Question &q : m_questions) {
        QString label = q.text;
        if (label.length() > 80) label = label.left(77) + "...";
        m_listQuestions->addItem(label);
    }
}

void MainWindow::onQuestionSelected()
{
    int idx = m_listQuestions->currentRow();
    if (idx < 0 || idx >= m_questions.size()) return;
    loadQuestionIntoEditor(idx);
}

void MainWindow::loadQuestionIntoEditor(int index)
{
    if (index < 0 || index >= m_questions.size()) return;
    const Question &q = m_questions[index];
    m_editQuestionText->setPlainText(q.text);
    m_comboType->setCurrentIndex(static_cast<int>(q.type));
    m_tblAnswers->setRowCount(0);
    for (const Answer &a : q.options) {
        int r = m_tblAnswers->rowCount();
        m_tblAnswers->insertRow(r);
        QTableWidgetItem *t = new QTableWidgetItem(a.text);
        m_tblAnswers->setItem(r, 0, t);
        QTableWidgetItem *c = new QTableWidgetItem;
        c->setCheckState(a.correct ? Qt::Checked : Qt::Unchecked);
        c->setFlags(c->flags() | Qt::ItemIsUserCheckable);
        m_tblAnswers->setItem(r, 1, c);
    }
    m_editExpectedText->setText(q.expectedText);
    onTypeChanged(static_cast<int>(q.type));
}

void MainWindow::onTypeChanged(int idx)
{
    bool isChoice = (idx == 0 || idx == 1);
    m_tblAnswers->setEnabled(isChoice);
    m_btnAddAnswer->setEnabled(isChoice);
    m_btnRemoveAnswer->setEnabled(isChoice);
    m_editExpectedText->setEnabled(idx == 2);
}

void MainWindow::onAddAnswer()
{
    int r = m_tblAnswers->rowCount();
    m_tblAnswers->insertRow(r);
    m_tblAnswers->setItem(r, 0, new QTableWidgetItem(QString("Nová možnost %1").arg(r+1)));
    QTableWidgetItem *c = new QTableWidgetItem;
    c->setCheckState(Qt::Unchecked);
    c->setFlags(c->flags() | Qt::ItemIsUserCheckable);
    m_tblAnswers->setItem(r, 1, c);
}

void MainWindow::onRemoveAnswer()
{
    int r = m_tblAnswers->currentRow();
    if (r >= 0) m_tblAnswers->removeRow(r);
}

void MainWindow::onLoadFromDB()
{
    QString err;
    QVector<Question> loaded;
    if (!DBManager::instance().loadAllQuestions(loaded, &err)) {
        QMessageBox::warning(this, "Chyba při načítání DB", err);
        return;
    }
    m_questions = std::move(loaded);
    refreshQuestionList();
    if (!m_questions.isEmpty()) m_listQuestions->setCurrentRow(0);
}

void MainWindow::onSaveToDB()
{
    // apply current editor into question
    int idx = m_listQuestions->currentRow();
    if (idx >= 0) collectEditorToQuestion(idx);

    // Save all questions into DB (addOrUpdate)
    QString err;
    for (const Question &q : m_questions) {
        if (!DBManager::instance().addOrUpdateQuestion(q, &err)) {
            QMessageBox::warning(this, "Chyba při ukládání do DB", err);
            return;
        }
    }
    QMessageBox::information(this, "Uloženo", "Všechny otázky byly uloženy do DB.");
}

void MainWindow::onApplyEdit()
{
    int idx = m_listQuestions->currentRow();
    if (idx < 0) {
        QMessageBox::warning(this, "Žádná otázka", "Vyberte otázku v seznamu.");
        return;
    }
    collectEditorToQuestion(idx);
    refreshQuestionList();
    m_listQuestions->setCurrentRow(idx);
    QMessageBox::information(this, "Uloženo", "Změny uložené do paměti (nezapomeňte kliknout 'Uložit do DB').");
}

void MainWindow::collectEditorToQuestion(int index)
{
    if (index < 0 || index >= m_questions.size()) return;
    Question &q = m_questions[index];
    q.text = m_editQuestionText->toPlainText();
    q.type = static_cast<QuestionType>(m_comboType->currentIndex());
    q.options.clear();
    for (int r = 0; r < m_tblAnswers->rowCount(); ++r) {
        QTableWidgetItem *t = m_tblAnswers->item(r, 0);
        QTableWidgetItem *c = m_tblAnswers->item(r, 1);
        Answer a;
        a.text = t ? t->text() : QString();
        if (c) a.correct = (c->checkState() == Qt::Checked);
        q.options.append(a);
    }
    q.expectedText = m_editExpectedText->text();
}

void MainWindow::onStartTest()
{
    // apply current edit
    int idx = m_listQuestions->currentRow();
    if (idx >= 0) collectEditorToQuestion(idx);

    if (m_questions.isEmpty()) {
        QMessageBox::warning(this, "Žádné otázky", "Nejsou žádné otázky. Načtěte nebo vytvořte otázky.");
        return;
    }
    int count = m_spinTestCount->value();
    Testrunner dlg(this);
    dlg.setQuestions(m_questions);
    dlg.setQuestionCount(count);
    // startTest will open dialog modally and handle saving results
    dlg.startTest();
}
EOF

cat > testrunner.h <<'EOF'
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
EOF

cat > testrunner.cpp <<'EOF'
#include "testrunner.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QButtonGroup>
#include <QScrollArea>
#include <QMessageBox>
#include <QRandomGenerator>
#include <algorithm>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>

Testrunner::Testrunner(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Test");

    QVBoxLayout *main = new QVBoxLayout;
    m_lblProgress = new QLabel;
    m_lblQuestion = new QLabel;
    m_lblQuestion->setWordWrap(true);
    m_answerWidget = new QWidget;
    QVBoxLayout *aw = new QVBoxLayout;
    aw->setContentsMargins(0,0,0,0);
    m_answerWidget->setLayout(aw);

    m_btnNext = new QPushButton("Další");
    m_btnSubmit = new QPushButton("Odevzdat test");

    m_editStudentEmail = new QLineEdit;
    m_editStudentEmail->setPlaceholderText("Zadejte svůj e-mail pro zaslání výsledků (volitelné)");
    m_btnSendEmail = new QPushButton("Odeslat výsledky emailem (otevře pošt. klient)");

    QHBoxLayout *btns = new QHBoxLayout;
    btns->addWidget(m_btnNext);
    btns->addWidget(m_btnSubmit);

    main->addWidget(m_lblProgress);
    main->addWidget(m_lblQuestion);
    main->addWidget(m_answerWidget);
    main->addLayout(btns);
    main->addWidget(m_editStudentEmail);
    main->addWidget(m_btnSendEmail);

    setLayout(main);

    connect(m_btnNext, &QPushButton::clicked, this, &Testrunner::onNext);
    connect(m_btnSubmit, &QPushButton::clicked, this, &Testrunner::onSubmit);
    connect(m_btnSendEmail, &QPushButton::clicked, this, &Testrunner::onSendEmail);
}

void Testrunner::setQuestions(const QVector<Question> &qs) {
    m_allQuestions = qs;
}

void Testrunner::setQuestionCount(int n) {
    m_questionCount = n;
}

void Testrunner::startTest()
{
    if (m_allQuestions.isEmpty()) {
        QMessageBox::warning(this, "Chyba", "Nejsou otázky.");
        return;
    }
    prepareRandomTest();
    m_userAnswers.clear();
    m_userAnswers.resize(m_testQuestions.size());
    m_currentIndex = 0;
    showCurrentQuestion();
    exec();
}

void Testrunner::prepareRandomTest()
{
    // choose random subset of questions
    int total = m_allQuestions.size();
    int n = qMin(m_questionCount, total);
    QVector<int> idx;
    idx.reserve(total);
    for (int i = 0; i < total; ++i) idx.append(i);
    std::shuffle(idx.begin(), idx.end(), *QRandomGenerator::global());
    m_testQuestions.clear();
    for (int i = 0; i < n; ++i) m_testQuestions.append(m_allQuestions[idx[i]]);
}

void Testrunner::showCurrentQuestion()
{
    // clear previous answer widgets
    QLayout *lay = m_answerWidget->layout();
    QLayoutItem *child;
    while ((child = lay->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
    m_currentAnswerWidgets.clear();

    if (m_currentIndex < 0 || m_currentIndex >= m_testQuestions.size()) return;

    const Question &q = m_testQuestions[m_currentIndex];
    m_lblProgress->setText(QString("Otázka %1 / %2").arg(m_currentIndex+1).arg(m_testQuestions.size()));
    m_lblQuestion->setText(q.text);

    if (q.type == QuestionType::TextAnswer) {
        QLineEdit *le = new QLineEdit;
        // restore previous if exists
        const StoredAnswer &sa = m_userAnswers[m_currentIndex];
        if (!sa.textAnswer.isEmpty()) le->setText(sa.textAnswer);
        lay->addWidget(le);
        m_currentAnswerWidgets.append(le);
    } else {
        // shuffle options
        int m = q.options.size();
        QVector<int> indices;
        for (int i=0;i<m;++i) indices.append(i);
        std::shuffle(indices.begin(), indices.end(), *QRandomGenerator::global());

        if (q.type == QuestionType::SingleChoice) {
            QButtonGroup *grp = new QButtonGroup(this);
            grp->setExclusive(true);
            for (int i = 0; i < m; ++i) {
                int origIdx = indices[i];
                QRadioButton *rb = new QRadioButton(q.options[origIdx].text);
                lay->addWidget(rb);
                grp->addButton(rb, origIdx);
                m_currentAnswerWidgets.append(rb);
                // restore selection if present
                const StoredAnswer &sa = m_userAnswers[m_currentIndex];
                if (!sa.selectedOptionsTexts.isEmpty()) {
                    if (sa.selectedOptionsTexts.contains(q.options[origIdx].text)) rb->setChecked(true);
                }
            }
        } else { // multiple
            for (int i = 0; i < m; ++i) {
                int origIdx = indices[i];
                QCheckBox *cb = new QCheckBox(q.options[origIdx].text);
                lay->addWidget(cb);
                m_currentAnswerWidgets.append(cb);
                // restore selection if present
                const StoredAnswer &sa = m_userAnswers[m_currentIndex];
                if (!sa.selectedOptionsTexts.isEmpty()) {
                    if (sa.selectedOptionsTexts.contains(q.options[origIdx].text)) cb->setChecked(true);
                }
            }
        }
    }
}

void Testrunner::saveCurrentAnswerForIndex(int index)
{
    if (index < 0 || index >= m_testQuestions.size()) return;
    StoredAnswer sa;
    sa.questionId = m_testQuestions[index].id;
    const Question &q = m_testQuestions[index];

    if (q.type == QuestionType::TextAnswer) {
        if (!m_currentAnswerWidgets.isEmpty()) {
            QLineEdit *le = qobject_cast<QLineEdit*>(m_currentAnswerWidgets.first());
            if (le) sa.textAnswer = le->text().trimmed();
        }
    } else if (q.type == QuestionType::SingleChoice) {
        for (QWidget *w : m_currentAnswerWidgets) {
            QRadioButton *rb = qobject_cast<QRadioButton*>(w);
            if (!rb) continue;
            if (rb->isChecked()) sa.selectedOptionsTexts.append(rb->text());
        }
    } else { // multiple
        for (QWidget *w : m_currentAnswerWidgets) {
            QCheckBox *cb = qobject_cast<QCheckBox*>(w);
            if (!cb) continue;
            if (cb->isChecked()) sa.selectedOptionsTexts.append(cb->text());
        }
    }

    m_userAnswers[index] = sa;
}

void Testrunner::onNext()
{
    // save current answers
    saveCurrentAnswerForIndex(m_currentIndex);

    if (m_currentIndex + 1 < m_testQuestions.size()) {
        ++m_currentIndex;
        showCurrentQuestion();
    } else {
        QMessageBox::information(this, "Konec", "Došli jsme na konec testu. Klikněte Odevzdat test pro vyhodnocení.");
    }
}

void Testrunner::onSubmit()
{
    // save current answers
    saveCurrentAnswerForIndex(m_currentIndex);

    QVector<DBManager::ResultDetail> details;
    double score = evaluateAndReturnScore(details);
    QString msg = QString("Skóre: %1 / %2").arg(score).arg(m_testQuestions.size());
    QMessageBox::information(this, "Výsledek testu", msg);

    // Save result to DB (student email optional)
    QString err;
    QString email = m_editStudentEmail->text().trimmed();
    if (!DBManager::instance().saveResult(email, score, m_testQuestions.size(), details, &err)) {
        QMessageBox::warning(this, "Chyba ukládání výsledku", err);
    } else {
        QMessageBox::information(this, "Uloženo", "Výsledek byl uložen do databáze.");
    }

    accept();
}

double Testrunner::evaluateAndReturnScore(QVector<DBManager::ResultDetail> &outDetails)
{
    double totalScore = 0.0;
    outDetails.clear();
    for (int i = 0; i < m_testQuestions.size(); ++i) {
        const Question &q = m_testQuestions[i];
        const StoredAnswer &sa = m_userAnswers[i];
        bool correct = false;
        QString ua; // description of user answer

        if (q.type == QuestionType::TextAnswer) {
            QString given = sa.textAnswer.trimmed();
            QString expected = q.expectedText.trimmed();
            ua = given;
            if (QString::compare(given, expected, Qt::CaseInsensitive) == 0 && !expected.isEmpty()) {
                correct = true;
            }
        } else if (q.type == QuestionType::SingleChoice) {
            // find which selected text is correct
            ua = sa.selectedOptionsTexts.join("; ");
            for (int oi = 0; oi < q.options.size(); ++oi) {
                const Answer &a = q.options[oi];
                if (a.correct) {
                    if (sa.selectedOptionsTexts.contains(a.text)) correct = true;
                    break;
                }
            }
        } else if (q.type == QuestionType::MultipleChoice) {
            // require exact match of selected set == correct set
            QStringList correctTexts;
            for (const Answer &a : q.options) if (a.correct) correctTexts.append(a.text);
            QStringList sel = sa.selectedOptionsTexts;
            ua = sel.join("; ");
            // compare as sets
            auto normalize = [](QStringList l) {
                for (QString &s : l) s = s.trimmed();
                std::sort(l.begin(), l.end());
                return l;
            };
            QStringList nl = normalize(correctTexts);
            QStringList ns = normalize(sel);
            correct = (nl == ns);
        }

        if (correct) totalScore += 1.0;

        DBManager::ResultDetail rd;
        rd.questionId = q.id;
        rd.correct = correct;
        rd.userAnswer = ua;
        outDetails.append(rd);
    }
    return totalScore;
}

void Testrunner::onSendEmail()
{
    // Build mailto URL with subject and body from last evaluated answers (we'll evaluate now but do not save)
    QVector<DBManager::ResultDetail> details;
    double score = evaluateAndReturnScore(details);
    QString body;
    body += QString("Skóre: %1 / %2\n\n").arg(score).arg(m_testQuestions.size());
    for (int i = 0; i < details.size(); ++i) {
        body += QString("Otázka %1: %2\n").arg(i+1).arg(details[i].correct ? "OK" : "Špatně");
        body += QString("Odpověď studenta: %1\n\n").arg(details[i].userAnswer);
    }

    QUrl mailto;
    QString to = m_editStudentEmail->text().trimmed();
    mailto.setScheme("mailto");
    mailto.setPath(to);
    QUrlQuery q;
    q.addQueryItem("subject", "Výsledek testu");
    q.addQueryItem("body", body);
    mailto.setQuery(q);

    QDesktopServices::openUrl(mailto);
}
EOF

cat > README.md <<'EOF'
# QtTestMaker (SQLite edition)

Tato verze aplikace používá SQLite databázi místo JSON souborů. Data (otázky a možnosti) jsou ukládána do SQLite souboru, výsledky testů se ukládají také do DB. Po dokončení testu můžete výsledky otevřít ve výchozím poštovním klientovi (předvyplněný email) a odeslat je.

Hlavní vlastnosti:
- Typy otázek: single-choice, multiple-choice, text (přesný text)
- Editor otázek (přidávání/úprava/mazání)
- Uložení/načtení otázek v SQLite DB
- Spouštění testu: náhodný výběr podmnožiny otázek, náhodné pořadí možností
- Ukládání výsledků testu do DB (včetně detailů)
- Odeslat výsledky jako email (otevře výchozí mail klient s předvyplněným tělem)

Sestavení:
1) Potřebujete Qt 5 (Widgets + Sql) a CMake.
2) mkdir build && cd build
3) cmake .. && cmake --build .

Použití:
- Po spuštění se vytvoří/otevře `questions.db` v aktuálním adresáři.
- V levém panelu spravujte seznam otázek.
- Pro uložení změn do DB klikněte "Uložit do DB".
- Pro spuštění testu nastavte počet otázek a klikněte "Spustit test".
- Po dokončení testu si můžete nechat výsledky uložit do DB a/nebo je poslat emailem (otevře se výchozí mailový klient).

Poznámky:
- Pro robustní nasazení přidejte validace (např. single-choice má právě jednu správnou odpověď).
- Odesílání emailu v této ukázce používá `mailto:` (otevře výchozí poštovní aplikaci). Pokud chcete přímé odesílání přes SMTP z aplikace (včetně autentizace/STARTTLS), mohu přidat jednoduchého SMTP klienta nebo podporu konfigurace SMTP serveru.
EOF

echo "All files created."