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
#include <QMessageBox>
#include <QCheckBox>
#include <QApplication>
#include <QUuid>
#include <QButtonGroup>
#include <QRadioButton>
#include <QCheckBox>
#include <QLayoutItem>
#include <QRandomGenerator>
#include <algorithm>

MainWindow::MainWindow(bool teacherMode, QWidget *parent)
    : QMainWindow(parent), m_teacherMode(teacherMode)
{
    // debounce timer for auto-save
    m_autoSaveTimer.setSingleShot(true);
    m_autoSaveTimer.setInterval(600); // ms
    connect(&m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::doAutoSave);

    if (m_teacherMode) buildTeacherUi();
    else buildStudentUi();

    // open DB (default path). Adjust path if you use custom filename/location.
    QString err;
    QString dbPath = "questions.db";
    if (!DBManager::instance().openDatabase(dbPath, &err)) {
        QMessageBox::critical(this, "DB Error", err);
    }

    // load tests from DB
    if (!DBManager::instance().loadTests(m_tests, &err)) {
        QMessageBox::warning(this, "DB load tests failed", err);
    }
    refreshTestList();
}

QString MainWindow::currentTestId() const
{
    int idx = currentTestIndex();
    if (idx >=0 && idx < m_tests.size()) return m_tests[idx].id;
    return QString();
}

int MainWindow::currentTestIndex() const
{
    if (!m_listTests) return -1;
    return m_listTests->currentRow();
}

/* -----------------------------
   UI construction
   ----------------------------*/
void MainWindow::buildTeacherUi()
{
    QWidget *central = new QWidget;
    setCentralWidget(central);

    // Left: tests + test metadata + questions list
    m_listTests = new QListWidget;
    m_btnAddTest = new QPushButton("Přidat test");
    m_btnRemoveTest = new QPushButton("Odstranit test");
    m_editTestName = new QLineEdit;
    m_editTestName->setPlaceholderText("Název testu");
    m_editTestDescription = new QLineEdit;
    m_editTestDescription->setPlaceholderText("Popis testu");

    QHBoxLayout *testTop = new QHBoxLayout;
    testTop->addWidget(m_btnAddTest);
    testTop->addWidget(m_btnRemoveTest);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(new QLabel("Testy:"));
    leftLayout->addWidget(m_listTests);
    leftLayout->addLayout(testTop);
    leftLayout->addWidget(m_editTestName);
    leftLayout->addWidget(m_editTestDescription);

    // Question editor on right
    m_listQuestions = new QListWidget;
    m_btnAddQuestion = new QPushButton("Přidat otázku");
    m_btnRemoveQuestion = new QPushButton("Odstranit otázku");
    m_editQuestionText = new QTextEdit;
    m_comboType = new QComboBox;
    m_comboType->addItem("Jedna správná");
    m_comboType->addItem("Více správných");
    m_comboType->addItem("Textová odpověď");
    m_tblAnswers = new QTableWidget(0,2);
    m_tblAnswers->setHorizontalHeaderLabels(QStringList() << "Text" << "Správná");
    m_tblAnswers->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_btnAddAnswer = new QPushButton("Přidat možnost");
    m_btnRemoveAnswer = new QPushButton("Odstranit možnost");
    m_editExpectedText = new QLineEdit;

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(new QLabel("Otázky:"));
    rightLayout->addWidget(m_listQuestions);
    QHBoxLayout *qBtns = new QHBoxLayout;
    qBtns->addWidget(m_btnAddQuestion);
    qBtns->addWidget(m_btnRemoveQuestion);
    rightLayout->addLayout(qBtns);
    rightLayout->addWidget(new QLabel("Text otázky:"));
    rightLayout->addWidget(m_editQuestionText);
    rightLayout->addWidget(new QLabel("Typ otázky:"));
    rightLayout->addWidget(m_comboType);
    rightLayout->addWidget(new QLabel("Možnosti:"));
    rightLayout->addWidget(m_tblAnswers);
    QHBoxLayout *ansBtns = new QHBoxLayout;
    ansBtns->addWidget(m_btnAddAnswer);
    ansBtns->addWidget(m_btnRemoveAnswer);
    rightLayout->addLayout(ansBtns);
    rightLayout->addWidget(new QLabel("Očekávaný text (pro textovou odpověď):"));
    rightLayout->addWidget(m_editExpectedText);
    rightLayout->addStretch(1);

    QSplitter *split = new QSplitter;
    QWidget *leftWidget = new QWidget; leftWidget->setLayout(leftLayout);
    QWidget *rightWidget = new QWidget; rightWidget->setLayout(rightLayout);
    split->addWidget(leftWidget);
    split->addWidget(rightWidget);
    split->setStretchFactor(1,1);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(split);
    central->setLayout(mainLayout);

    // connections
    connect(m_btnAddTest, &QPushButton::clicked, this, &MainWindow::onAddTest);
    // connect RemoveTest to our slot (implementaton provided)
    connect(m_btnRemoveTest, &QPushButton::clicked, this, &MainWindow::onRemoveTest);

    connect(m_listTests, &QListWidget::currentRowChanged, this, &MainWindow::onTestSelected);
    connect(m_editTestName, &QLineEdit::editingFinished, this, [this](){
        int idx = m_listTests->currentRow();
        if (idx < 0 || idx >= m_tests.size()) return;
        m_tests[idx].name = m_editTestName->text().trimmed();
        QString err;
        DBManager::instance().addOrUpdateTest(m_tests[idx], &err);
        refreshTestList();
    });
    connect(m_editTestDescription, &QLineEdit::editingFinished, this, [this](){
        int idx = m_listTests->currentRow();
        if (idx < 0 || idx >= m_tests.size()) return;
        m_tests[idx].description = m_editTestDescription->text().trimmed();
        QString err;
        DBManager::instance().addOrUpdateTest(m_tests[idx], &err);
    });

    connect(m_btnAddQuestion, &QPushButton::clicked, this, &MainWindow::onAddQuestion);
    connect(m_btnRemoveQuestion, &QPushButton::clicked, this, &MainWindow::onRemoveQuestion);
    connect(m_listQuestions, &QListWidget::currentRowChanged, this, &MainWindow::onQuestionSelected);
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTypeChanged);
    connect(m_btnAddAnswer, &QPushButton::clicked, this, &MainWindow::onAddAnswer);
    connect(m_btnRemoveAnswer, &QPushButton::clicked, this, &MainWindow::onRemoveAnswer);

    // auto-save triggers (debounced)
    connect(m_editQuestionText, &QTextEdit::textChanged, this, &MainWindow::scheduleAutoSave);
    connect(m_editExpectedText, &QLineEdit::textChanged, this, &MainWindow::scheduleAutoSave);
    connect(m_tblAnswers, &QTableWidget::itemChanged, this, &MainWindow::scheduleAutoSave);
}

void MainWindow::buildStudentUi()
{
    QWidget *central = new QWidget;
    setCentralWidget(central);

    // left: tests list
    m_listTests = new QListWidget;
    m_listTests->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_listTests, &QListWidget::currentRowChanged, this, &MainWindow::onTestSelected);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(new QLabel("Vyber test:"));
    leftLayout->addWidget(m_listTests);

    QWidget *leftWidget = new QWidget; leftWidget->setLayout(leftLayout);

    // right: student runner area
    m_lblStudentProgress = new QLabel;
    m_lblStudentQuestion = new QLabel;
    m_lblStudentQuestion->setWordWrap(true);
    m_widgetStudentAnswers = new QWidget;
    QVBoxLayout *aw = new QVBoxLayout;
    aw->setContentsMargins(0,0,0,0);
    m_widgetStudentAnswers->setLayout(aw);

    m_btnStudentNext = new QPushButton("Další");
    m_btnStudentSubmit = new QPushButton("Odevzdat");
    m_editStudentEmail = new QLineEdit;
    m_editStudentEmail->setPlaceholderText("E-mail (volitelné)");
    m_spinStudentCount = new QSpinBox;
    m_spinStudentCount->setMinimum(1);
    m_spinStudentCount->setMaximum(1000);
    m_spinStudentCount->setValue(10);

    QHBoxLayout *hTop = new QHBoxLayout;
    hTop->addWidget(new QLabel("Počet otázek:"));
    hTop->addWidget(m_spinStudentCount);
    hTop->addStretch();

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addLayout(hTop);
    rightLayout->addWidget(m_lblStudentProgress);
    rightLayout->addWidget(m_lblStudentQuestion);
    rightLayout->addWidget(m_widgetStudentAnswers);
    QHBoxLayout *btns = new QHBoxLayout;
    btns->addWidget(m_btnStudentNext);
    btns->addWidget(m_btnStudentSubmit);
    rightLayout->addLayout(btns);
    rightLayout->addWidget(m_editStudentEmail);
    rightLayout->addStretch(1);

    QWidget *rightWidget = new QWidget; rightWidget->setLayout(rightLayout);

    QSplitter *split = new QSplitter;
    split->addWidget(leftWidget);
    split->addWidget(rightWidget);
    split->setStretchFactor(1,1);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(split);
    central->setLayout(mainLayout);

    connect(m_btnStudentNext, &QPushButton::clicked, this, &MainWindow::onStudentNext);
    connect(m_btnStudentSubmit, &QPushButton::clicked, this, &MainWindow::onStudentSubmit);
}

/* -----------------------------
   Data / UI helpers
   ----------------------------*/
void MainWindow::refreshTestList()
{
    if (!m_listTests) return;
    m_listTests->clear();
    for (const Test &t : m_tests) {
        m_listTests->addItem(t.name);
    }
    if (!m_teacherMode && !m_tests.isEmpty()) {
        // select first test by default for student
        m_listTests->setCurrentRow(0);
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

/* -----------------------------
   Teacher slots
   ----------------------------*/
void MainWindow::onAddTest()
{
    Test t;
    t.id = QUuid::createUuid().toString();
    t.name = "Novy test";
    t.description = "";
    // add to DB immediately
    QString err;
    if (!DBManager::instance().addOrUpdateTest(t, &err)) {
        QMessageBox::warning(this, "Chyba při ukládání testu", err);
    } else {
        m_tests.append(t);
        refreshTestList();
        int idx = m_listTests->count()-1;
        m_listTests->setCurrentRow(idx);
        m_editTestName->setText(t.name);
        m_editTestDescription->setText(t.description);
    }
}

// Implemented slot: remove currently selected test
void MainWindow::onRemoveTest()
{
    int idx = m_listTests->currentRow();
    if (idx < 0 || idx >= m_tests.size()) return;
    QString testId = m_tests[idx].id;
    QString err;
    if (!DBManager::instance().removeTest(testId, &err)) {
        QMessageBox::warning(this, "Chyba při mazání testu z DB", err);
        return;
    }
    m_tests.removeAt(idx);
    refreshTestList();
    // clear editor if in teacher mode
    if (m_teacherMode) {
        m_listQuestions->clear();
        m_editTestName->clear();
        m_editTestDescription->clear();
        m_questions.clear();
    }
}

void MainWindow::onAddQuestion()
{
    int tidx = m_listTests->currentRow();
    if (tidx < 0) {
        QMessageBox::warning(this, "Žádný test", "Nejprve vyberte nebo vytvořte test.");
        return;
    }
    Question q;
    q.id = QUuid::createUuid().toString();
    q.testId = m_tests[tidx].id;
    q.text = "Nová otázka";
    q.type = QuestionType::SingleChoice;
    q.options = { Answer{"Možnost 1", true}, Answer{"Možnost 2", false} };

    QString err;
    if (!DBManager::instance().addOrUpdateQuestion(q, &err)) {
        QMessageBox::warning(this, "Chyba při ukládání otázky", err);
        return;
    }

    // add locally and refresh
    m_questions.append(q);
    refreshQuestionList();
    m_listQuestions->setCurrentRow(m_questions.size()-1);
}

void MainWindow::onRemoveQuestion()
{
    int row = m_listQuestions->currentRow();
    if (row < 0 || row >= m_questions.size()) return;
    QString qid = m_questions[row].id;
    QString err;
    if (!DBManager::instance().removeQuestion(qid, &err)) {
        QMessageBox::warning(this, "Chyba mazání otázky", err);
        return;
    }
    m_questions.removeAt(row);
    refreshQuestionList();
    if (!m_questions.isEmpty()) m_listQuestions->setCurrentRow(qMin(row, m_questions.size()-1));
    else {
        m_editQuestionText->clear();
        m_tblAnswers->setRowCount(0);
        m_editExpectedText->clear();
    }
}

void MainWindow::onQuestionSelected(int row)
{
    if (row < 0 || row >= m_questions.size()) return;
    loadQuestionIntoEditor(row);
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
        c->setFlags(c->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        c->setCheckState(a.correct ? Qt::Checked : Qt::Unchecked);
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
    scheduleAutoSave();
}

void MainWindow::onAddAnswer()
{
    int r = m_tblAnswers->rowCount();
    m_tblAnswers->insertRow(r);
    m_tblAnswers->setItem(r, 0, new QTableWidgetItem(QString("Nová možnost %1").arg(r+1)));
    QTableWidgetItem *c = new QTableWidgetItem;
    c->setFlags(c->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    c->setCheckState(Qt::Unchecked);
    m_tblAnswers->setItem(r, 1, c);
    scheduleAutoSave();
}

void MainWindow::onRemoveAnswer()
{
    int r = m_tblAnswers->currentRow();
    if (r >= 0) {
        m_tblAnswers->removeRow(r);
        scheduleAutoSave();
    }
}

/* -----------------------------
   Auto-save implementation
   ----------------------------*/
void MainWindow::scheduleAutoSave()
{
    // restart timer
    m_autoSaveTimer.start();
}

void MainWindow::doAutoSave()
{
    // determine current selected question and persist to DB
    int qidx = m_listQuestions ? m_listQuestions->currentRow() : -1;
    if (m_teacherMode && qidx >= 0 && qidx < m_questions.size()) {
        collectEditorToQuestion(qidx);
        QString err;
        if (!DBManager::instance().addOrUpdateQuestion(m_questions[qidx], &err)) {
            QMessageBox::warning(this, "Chyba při auto-ukládání otázky", err);
        } else {
            refreshQuestionList();
            m_listQuestions->setCurrentRow(qidx);
        }
    }
}

/* collect editor fields into question model */
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
        a.correct = (c && c->checkState() == Qt::Checked);
        q.options.append(a);
    }
    q.expectedText = m_editExpectedText->text();
}

/* -----------------------------
   Student runner (embedded)
   ----------------------------*/
void MainWindow::onTestSelected(int idx)
{
    // This slot is used in both modes: teacher list selection and student selection.
    if (m_teacherMode) {
        // load test metadata and questions for teacher editor
        if (idx < 0 || idx >= m_tests.size()) {
            m_editTestName->clear();
            m_editTestDescription->clear();
            m_questions.clear();
            refreshQuestionList();
            return;
        }
        const Test &t = m_tests[idx];
        m_editTestName->setText(t.name);
        m_editTestDescription->setText(t.description);

        // load questions for this test from DB
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
        return;
    }

    // Student mode: when selecting a test, load questions and start test inline
    if (idx < 0 || idx >= m_tests.size()) {
        m_studentQuestions.clear();
        m_studentAnswers.clear();
        m_lblStudentProgress->clear();
        m_lblStudentQuestion->clear();
        QLayout *l = m_widgetStudentAnswers->layout();
        QLayoutItem *it;
        while ((it = l->takeAt(0)) != nullptr) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
        return;
    }

    QString tid = m_tests[idx].id;
    QString err;
    if (!DBManager::instance().loadQuestionsForTest(tid, m_studentQuestions, &err)) {
        QMessageBox::warning(this, "Chyba při načítání otázek", err);
        return;
    }
    // prepare randomized subset
    int count = m_spinStudentCount->value();
    if (m_studentQuestions.size() > count) {
        std::shuffle(m_studentQuestions.begin(), m_studentQuestions.end(), *QRandomGenerator::global());
        m_studentQuestions.resize(count);
    }
    m_studentCurrentIndex = 0;
    m_studentAnswers.clear();
    m_studentAnswers.resize(m_studentQuestions.size());
    // show first question
    if (!m_studentQuestions.isEmpty()) {
        showStudentQuestion(0);
    }
}

void MainWindow::showStudentQuestion(int index)
{
    if (index < 0 || index >= m_studentQuestions.size()) return;
    m_studentCurrentIndex = index;
    const Question &q = m_studentQuestions[index];
    m_lblStudentProgress->setText(QString("Otázka %1 / %2").arg(index+1).arg(m_studentQuestions.size()));
    m_lblStudentQuestion->setText(q.text);

    // clear previous answer widgets
    QLayout *lay = m_widgetStudentAnswers->layout();
    QLayoutItem *child;
    while ((child = lay->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    m_currentAnswerWidgets.clear();

    // get stored student answer for this index
    QString stored = m_studentAnswers.value(index);

    if (q.type == QuestionType::TextAnswer) {
        QLineEdit *le = new QLineEdit;
        if (!stored.isEmpty()) le->setText(stored);
        lay->addWidget(le);
        m_currentAnswerWidgets.append(le);
    } else {
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
                if (!stored.isEmpty() && stored == q.options[origIdx].text) rb->setChecked(true);
                lay->addWidget(rb);
                grp->addButton(rb, origIdx);
                m_currentAnswerWidgets.append(rb);
            }
        } else {
            QStringList presel = stored.split(";", Qt::SkipEmptyParts);
            for (int i = 0; i < m; ++i) {
                int origIdx = indices[i];
                QCheckBox *cb = new QCheckBox(q.options[origIdx].text);
                for (QString s : presel) if (s.trimmed() == q.options[origIdx].text) cb->setChecked(true);
                lay->addWidget(cb);
                m_currentAnswerWidgets.append(cb);
            }
        }
    }
}

/* Student navigation */
void MainWindow::onStudentNext()
{
    if (m_studentCurrentIndex < 0 || m_studentCurrentIndex >= m_studentQuestions.size()) return;

    // save current answer into m_studentAnswers
    const Question &curQ = m_studentQuestions[m_studentCurrentIndex];
    if (curQ.type == QuestionType::TextAnswer) {
        QLineEdit *le = qobject_cast<QLineEdit*>(m_currentAnswerWidgets.isEmpty() ? nullptr : m_currentAnswerWidgets.first());
        m_studentAnswers[m_studentCurrentIndex] = le ? le->text().trimmed() : QString();
    } else if (curQ.type == QuestionType::SingleChoice) {
        QString chosen;
        for (QWidget *w : m_currentAnswerWidgets) {
            QRadioButton *rb = qobject_cast<QRadioButton*>(w);
            if (!rb) continue;
            if (rb->isChecked()) { chosen = rb->text(); break; }
        }
        m_studentAnswers[m_studentCurrentIndex] = chosen;
    } else {
        QStringList sel;
        for (QWidget *w : m_currentAnswerWidgets) {
            QCheckBox *cb = qobject_cast<QCheckBox*>(w);
            if (!cb) continue;
            if (cb->isChecked()) sel.append(cb->text());
        }
        m_studentAnswers[m_studentCurrentIndex] = sel.join("; ");
    }

    if (m_studentCurrentIndex + 1 < m_studentQuestions.size()) {
        showStudentQuestion(m_studentCurrentIndex + 1);
    } else {
        QMessageBox::information(this, "Konec", "Došli jste na konec testu. Klikněte Odevzdat pro vyhodnocení.");
    }
}

/* Student submit: evaluate and save result */
void MainWindow::onStudentSubmit()
{
    // save current answer
    if (m_studentCurrentIndex >=0 && m_studentCurrentIndex < m_studentQuestions.size()) {
        const Question &curQ = m_studentQuestions[m_studentCurrentIndex];
        if (curQ.type == QuestionType::TextAnswer) {
            QLineEdit *le = qobject_cast<QLineEdit*>(m_currentAnswerWidgets.isEmpty() ? nullptr : m_currentAnswerWidgets.first());
            m_studentAnswers[m_studentCurrentIndex] = le ? le->text().trimmed() : QString();
        } else if (curQ.type == QuestionType::SingleChoice) {
            QString chosen;
            for (QWidget *w : m_currentAnswerWidgets) {
                QRadioButton *rb = qobject_cast<QRadioButton*>(w);
                if (!rb) continue;
                if (rb->isChecked()) { chosen = rb->text(); break; }
            }
            m_studentAnswers[m_studentCurrentIndex] = chosen;
        } else {
            QStringList sel;
            for (QWidget *w : m_currentAnswerWidgets) {
                QCheckBox *cb = qobject_cast<QCheckBox*>(w);
                if (!cb) continue;
                if (cb->isChecked()) sel.append(cb->text());
            }
            m_studentAnswers[m_studentCurrentIndex] = sel.join("; ");
        }
    }

    // Evaluate
    double totalScore = 0.0;
    QVector<DBManager::ResultDetail> details;
    for (int i = 0; i < m_studentQuestions.size(); ++i) {
        const Question &q = m_studentQuestions[i];
        bool correct = false;
        QString ua = m_studentAnswers.value(i);
        if (q.type == QuestionType::TextAnswer) {
            QString given = ua.trimmed();
            QString expected = q.expectedText.trimmed(); // original expected from DB
            if (!expected.isEmpty()) {
                if (QString::compare(given, expected, Qt::CaseInsensitive) == 0) correct = true;
            } else {
                // no expected answer provided -> cannot auto-evaluate; leave correct = false
                correct = false;
            }
            if (correct) totalScore += 1.0;
        } else if (q.type == QuestionType::SingleChoice) {
            QString correctText;
            for (const Answer &a : q.options) if (a.correct) { correctText = a.text; break; }
            if (!correctText.isEmpty() && ua == correctText) {
                correct = true;
                totalScore += 1.0;
            }
        } else { // multiple
            QStringList correctTexts;
            for (const Answer &a : q.options) if (a.correct) correctTexts.append(a.text);
            QStringList sel = ua.split(";", Qt::SkipEmptyParts);
            auto normalize = [](QStringList l){
                for (QString &s : l) s = s.trimmed();
                std::sort(l.begin(), l.end());
                return l;
            };
            if (normalize(correctTexts) == normalize(sel)) {
                correct = true;
                totalScore += 1.0;
            }
        }

        DBManager::ResultDetail rd;
        rd.questionId = q.id;
        rd.correct = correct;
        rd.userAnswer = ua;
        details.append(rd);
    }

    QString email = m_editStudentEmail ? m_editStudentEmail->text().trimmed() : QString();
    QString err;
    // save result with test id
    QString tid = currentTestId();
    if (!DBManager::instance().saveResult(email, tid, totalScore, m_studentQuestions.size(), details, &err)) {
        QMessageBox::warning(this, "Chyba ukládání výsledku", err);
    } else {
        QMessageBox::information(this, "Výsledek", QString("Skóre: %1 / %2\nVýsledek uložen.").arg(totalScore).arg(m_studentQuestions.size()));
    }
}

/* Optional slot used to start test programmatically (kept because header declares it) */
void MainWindow::onStudentStartTest()
{
    if (!m_teacherMode) {
        int idx = currentTestIndex();
        if (idx >= 0) {
            onTestSelected(idx); // will load and show first question
        }
    }
}
