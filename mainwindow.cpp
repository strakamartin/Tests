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
    // Load tests and questions from DB (if any)
    onLoadFromDB();
}

void MainWindow::buildUi()
{
    QWidget *central = new QWidget;
    setCentralWidget(central);

    // Top of left: test selector and actions
    m_comboTests = new QComboBox;
    m_btnAddTest = new QPushButton("Přidat test");
    m_btnRemoveTest = new QPushButton("Odstranit test");
    m_editTestName = new QLineEdit;
    m_editTestName->setPlaceholderText("Název testu");
    m_editTestDescription = new QLineEdit;
    m_editTestDescription->setPlaceholderText("Popis testu");

    QHBoxLayout *testTop = new QHBoxLayout;
    testTop->addWidget(new QLabel("Vyber test:"));
    testTop->addWidget(m_comboTests);
    testTop->addWidget(m_btnAddTest);
    testTop->addWidget(m_btnRemoveTest);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addLayout(testTop);
    leftLayout->addWidget(m_editTestName);
    leftLayout->addWidget(m_editTestDescription);

    m_listQuestions = new QListWidget;
    leftLayout->addWidget(m_listQuestions);

    QHBoxLayout *leftButtons = new QHBoxLayout;
    m_btnAddQuestion = new QPushButton("Přidat otázku");
    m_btnRemoveQuestion = new QPushButton("Odstranit");
    leftButtons->addWidget(m_btnAddQuestion);
    leftButtons->addWidget(m_btnRemoveQuestion);
    leftLayout->addLayout(leftButtons);

    m_btnLoadDB = new QPushButton("Načíst z DB");
    m_btnSaveDB = new QPushButton("Uložit do DB");
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

    // connections - tests
    connect(m_btnAddTest, &QPushButton::clicked, this, &MainWindow::onAddTest);
    connect(m_btnRemoveTest, &QPushButton::clicked, this, &MainWindow::onRemoveTest);
    connect(m_comboTests, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTestSelected);
    // save test name/description when focus lost or on explicit Save DB
    connect(m_editTestName, &QLineEdit::editingFinished, this, &MainWindow::onSaveTest);
    connect(m_editTestDescription, &QLineEdit::editingFinished, this, &MainWindow::onSaveTest);

    // connections - questions
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

void MainWindow::refreshTestCombo()
{
    m_comboTests->clear();
    for (const Test &t : m_tests) {
        m_comboTests->addItem(t.name, t.id);
    }
}

void MainWindow::onAddTest()
{
    Test t;
    t.id = QUuid::createUuid().toString();
    t.name = "Novy test";
    t.description = "";
    m_tests.append(t);
    refreshTestCombo();
    int idx = m_comboTests->count()-1;
    m_comboTests->setCurrentIndex(idx);
    m_editTestName->setText(t.name);
    m_editTestDescription->setText(t.description);
}

void MainWindow::onRemoveTest()
{
    int idx = m_comboTests->currentIndex();
    if (idx < 0 || idx >= m_tests.size()) return;
    QString testId = m_tests[idx].id;
    QString err;
    if (!DBManager::instance().removeTest(testId, &err)) {
        QMessageBox::warning(this, "Chyba při mazání testu z DB", err);
        // continue to remove locally even if DB delete failed? here we remove locally then refresh
    }
    m_tests.remove(idx);
    refreshTestCombo();
    // clear questions
    m_questions.clear();
    refreshQuestionList();
    m_editTestName->clear();
    m_editTestDescription->clear();
}

void MainWindow::onTestSelected()
{
    int idx = m_comboTests->currentIndex();
    if (idx < 0 || idx >= m_tests.size()) {
        // clear UI
        m_questions.clear();
        refreshQuestionList();
        m_editTestName->clear();
        m_editTestDescription->clear();
        return;
    }
    const Test &t = m_tests[idx];
    m_editTestName->setText(t.name);
    m_editTestDescription->setText(t.description);

    // load questions for this test
    QString err;
    QVector<Question> loaded;
    if (!DBManager::instance().loadQuestionsForTest(t.id, loaded, &err)) {
        QMessageBox::warning(this, "Chyba při načítání otázek z DB", err);
        m_questions.clear();
    } else {
        m_questions = std::move(loaded);
    }
    refreshQuestionList();
    if (!m_questions.isEmpty()) m_listQuestions->setCurrentRow(0);
}

void MainWindow::onSaveTest()
{
    int idx = m_comboTests->currentIndex();
    if (idx < 0 || idx >= m_tests.size()) return;
    Test &t = m_tests[idx];
    t.name = m_editTestName->text().trimmed();
    t.description = m_editTestDescription->text().trimmed();

    QString err;
    if (!DBManager::instance().addOrUpdateTest(t, &err)) {
        QMessageBox::warning(this, "Chyba při ukládání testu do DB", err);
    } else {
        refreshTestCombo();
        m_comboTests->setCurrentIndex(idx);
    }
}

void MainWindow::onAddQuestion()
{
    int tidx = m_comboTests->currentIndex();
    if (tidx < 0) {
        QMessageBox::warning(this, "Žádný test", "Nejprve vytvořte nebo vyberte test.");
        return;
    }
    Question q;
    q.id = QUuid::createUuid().toString();
    q.testId = m_tests[tidx].id;
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
    QVector<Test> loadedTests;
    if (!DBManager::instance().loadTests(loadedTests, &err)) {
        QMessageBox::warning(this, "Chyba při načítání DB (tests)", err);
        return;
    }
    m_tests = std::move(loadedTests);
    refreshTestCombo();

    // select first test if exists
    if (!m_tests.isEmpty()) {
        m_comboTests->setCurrentIndex(0);
        onTestSelected();
    } else {
        m_questions.clear();
        refreshQuestionList();
    }
}

void MainWindow::onSaveToDB()
{
    // apply current editor into question
    int qidx = m_listQuestions->currentRow();
    if (qidx >= 0) collectEditorToQuestion(qidx);

    // Save current test metadata (if any)
    int tidx = m_comboTests->currentIndex();
    if (tidx >= 0) {
        Test &t = m_tests[tidx];
        t.name = m_editTestName->text().trimmed();
        t.description = m_editTestDescription->text().trimmed();
        QString err;
        if (!DBManager::instance().addOrUpdateTest(t, &err)) {
            QMessageBox::warning(this, "Chyba při ukládání testu do DB", err);
            return;
        }
    }

    // Save all questions of current test into DB (addOrUpdate)
    QString err;
    for (const Question &q : m_questions) {
        if (!DBManager::instance().addOrUpdateQuestion(q, &err)) {
            QMessageBox::warning(this, "Chyba při ukládání do DB", err);
            return;
        }
    }
    QMessageBox::information(this, "Uloženo", "Test a otázky byly uloženy do DB.");
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

// ...

void MainWindow::onStartTest()
{
    // apply current edit
    int qidx = m_listQuestions->currentRow();
    if (qidx >= 0) collectEditorToQuestion(qidx);

    if (m_tests.isEmpty()) {
        QMessageBox::warning(this, "Žádný test", "Nejsou žádné testy. Vytvořte nebo načtěte test.");
        return;
    }
    int tidx = m_comboTests->currentIndex();
    if (tidx < 0 || tidx >= m_tests.size()) {
        QMessageBox::warning(this, "Žádný test vybran", "Vyberte test, který chcete spustit.");
        return;
    }

    int count = m_spinTestCount->value();
    Testrunner dlg(this);
    dlg.setQuestionCount(count);
    dlg.setTestId(m_tests[tidx].id);
    dlg.setTestName(m_tests[tidx].name);
    dlg.startTest();
}
