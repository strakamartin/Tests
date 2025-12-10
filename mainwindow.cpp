#include "mainwindow.h"
#include "dbmanager.h"
#include "testrunner.h"
#include "customtextedit.h"

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
    : QMainWindow(parent), mTeacherMode(teacherMode)
{
    // debounce timer for auto-save
    mAutoSaveTimer.setSingleShot(true);
    mAutoSaveTimer.setInterval(600); // ms
    connect(&mAutoSaveTimer, &QTimer::timeout, this, &MainWindow::doAutoSave);

    if (mTeacherMode) buildTeacherUi();
    else buildStudentUi();

    // open DB (default path). Adjust path if you use custom filename/location.
    QString err;
    QString dbPath = "../../questions.db";
    if (!DBManager::instance().openDatabase(dbPath, &err)) {
        QMessageBox::critical(this, "DB Error", err);
    }

    // load tests from DB
    if (!DBManager::instance().loadTests(mTests, &err)) {
        QMessageBox::warning(this, "DB load tests failed", err);
    }
    refreshTestList();
}

QString MainWindow::currentTestId() const
{
    int idx = currentTestIndex();
    if (idx >=0 && idx < mTests.size()) return mTests[idx].id;
    return QString();
}

int MainWindow::currentTestIndex() const
{
    if (!mListTests) return -1;
    return mListTests->currentRow();
}

/* -----------------------------
   UI construction
   ----------------------------*/
void MainWindow::buildTeacherUi()
{
    QWidget *central = new QWidget;
    setCentralWidget(central);

    // Left: tests + test metadata + questions list
    mListTests = new QListWidget;
    mListTests->setMaximumHeight(95);
    mBtnAddTest = new QPushButton("Přidat test");
    mBtnRemoveTest = new QPushButton("Odstranit test");
    mEditTestName = new QLineEdit;
    mEditTestName->setPlaceholderText("Název testu");
    mEditTestDescription = new QLineEdit;
    mEditTestDescription->setPlaceholderText("Popis testu");
    mListQuestions = new QListWidget;
    mBtnAddQuestion = new QPushButton("Přidat otázku");
    mBtnRemoveQuestion = new QPushButton("Odstranit otázku");

    // Tests list
    QHBoxLayout *testTop = new QHBoxLayout;
    testTop->addWidget(mBtnAddTest);
    testTop->addWidget(mBtnRemoveTest);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(new QLabel("Testy:"));
    leftLayout->addWidget(mListTests);
    leftLayout->addLayout(testTop);
    leftLayout->addWidget(mEditTestName);
    leftLayout->addWidget(mEditTestDescription);

    // Questions list
    leftLayout->addWidget(new QLabel("Otázky:"));
    leftLayout->addWidget(mListQuestions);
    QHBoxLayout *qBtns = new QHBoxLayout;
    qBtns->addWidget(mBtnAddQuestion);
    qBtns->addWidget(mBtnRemoveQuestion);
    leftLayout->addLayout(qBtns);

    // Number of questions in test of student(subset of all questions for the test)
    mSpinStudentCount = new QSpinBox;
    mSpinStudentCount->setMinimum(1);
    mSpinStudentCount->setMaximum(1000);
    mSpinStudentCount->setValue(10);

    QHBoxLayout *hTop = new QHBoxLayout;
    hTop->addWidget(new QLabel("Počet otázek pro studenta:"));
    hTop->addWidget(mSpinStudentCount);
    leftLayout->addLayout(hTop);
//********************right ***************************************
    // Question editor on right
    mEditQuestionText = new CustomTextEdit;
    mComboType = new QComboBox;
    mComboType->addItem("Jedna správná");
    mComboType->addItem("Více správných");
    mComboType->addItem("Textová odpověď");
    mTblAnswers = new QTableWidget(0,2);
    mTblAnswers->setHorizontalHeaderLabels(QStringList() << "Text" << "Správná");
    mTblAnswers->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    mBtnAddAnswer = new QPushButton("Přidat možnost");
    mBtnRemoveAnswer = new QPushButton("Odstranit možnost");
    mEditExpectedText = new QLineEdit;

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(new QLabel("Text otázky:"));
    rightLayout->addWidget(mEditQuestionText);
    rightLayout->addWidget(new QLabel("Typ otázky:"));
    rightLayout->addWidget(mComboType);
    rightLayout->addWidget(new QLabel("Možnosti:"));
    rightLayout->addWidget(mTblAnswers);
    QHBoxLayout *ansBtns = new QHBoxLayout;
    ansBtns->addWidget(mBtnAddAnswer);
    ansBtns->addWidget(mBtnRemoveAnswer);
    rightLayout->addLayout(ansBtns);
    rightLayout->addWidget(new QLabel("Očekávaný text (pro textovou odpověď):"));
    rightLayout->addWidget(mEditExpectedText);
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
    connect(mBtnAddTest, &QPushButton::clicked, this, &MainWindow::onAddTest);
    // connect RemoveTest to our slot (implementaton provided)
    connect(mBtnRemoveTest, &QPushButton::clicked, this, &MainWindow::onRemoveTest);

    connect(mListTests, &QListWidget::currentRowChanged, this, &MainWindow::onTestSelected);
    connect(mEditTestName, &QLineEdit::editingFinished, this, [this](){
        int idx = mListTests->currentRow();
        if (idx < 0 || idx >= mTests.size()) return;
        mTests[idx].name = mEditTestName->text().trimmed();
        QString err;
        DBManager::instance().addOrUpdateTest(mTests[idx], &err);
        refreshTestList();
        mListTests->setCurrentRow(idx);
    });
    connect(mEditTestDescription, &QLineEdit::editingFinished, this, [this](){
        int idx = mListTests->currentRow();
        if (idx < 0 || idx >= mTests.size()) return;
        mTests[idx].description = mEditTestDescription->text().trimmed();
        QString err;
        DBManager::instance().addOrUpdateTest(mTests[idx], &err);
    });
    connect(mSpinStudentCount, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value){
        int idx = mListTests->currentRow();
        if (idx < 0 || idx >= mTests.size()) return;
        mTests[idx].studentCount = value;
        QString err;
        DBManager::instance().addOrUpdateTest(mTests[idx], &err);
    });

    connect(mBtnAddQuestion, &QPushButton::clicked, this, &MainWindow::onAddQuestion);
    connect(mBtnRemoveQuestion, &QPushButton::clicked, this, &MainWindow::onRemoveQuestion);
    connect(mListQuestions, &QListWidget::currentRowChanged, this, &MainWindow::onQuestionSelected);
    connect(mComboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTypeChanged);
    connect(mBtnAddAnswer, &QPushButton::clicked, this, &MainWindow::onAddAnswer);
    connect(mBtnRemoveAnswer, &QPushButton::clicked, this, &MainWindow::onRemoveAnswer);

    connect(mEditQuestionText, &CustomTextEdit::editingFinished, this, &MainWindow::doAutoSaveWithRefresh);
    // auto-save triggers (debounced)
    connect(mEditExpectedText, &QLineEdit::textChanged, this, &MainWindow::scheduleAutoSave);
    connect(mTblAnswers, &QTableWidget::itemChanged, this, &MainWindow::answerItemChanged);
}

void MainWindow::buildStudentUi()
{
    QWidget *central = new QWidget;
    setCentralWidget(central);

    // left: tests list
    mListTests = new QListWidget;
    mListTests->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(mListTests, &QListWidget::currentRowChanged, this, &MainWindow::onTestSelected);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    mEditStudentEmail = new QLineEdit;
    mEditStudentEmail->setPlaceholderText("Teacher E-mail");
    leftLayout->addWidget(mEditStudentEmail);

    leftLayout->addWidget(new QLabel("Vyber test:"));
    leftLayout->addWidget(mListTests);

    QWidget *leftWidget = new QWidget; leftWidget->setLayout(leftLayout);

    // right: student runner area
    mLblStudentProgress = new QLabel;
    mLblStudentQuestion = new QLabel;
    mLblStudentQuestion->setWordWrap(true);
    mWidgetStudentAnswers = new QWidget;
    QVBoxLayout *aw = new QVBoxLayout;
    aw->setContentsMargins(0,0,0,0);
    mWidgetStudentAnswers->setLayout(aw);

    mBtnStudentNext = new QPushButton("Další");
    mBtnStudentSubmit = new QPushButton("Odevzdat");

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(mLblStudentProgress);
    rightLayout->addWidget(mLblStudentQuestion);
    rightLayout->addWidget(mWidgetStudentAnswers);
    QHBoxLayout *btns = new QHBoxLayout;
    btns->addWidget(mBtnStudentNext);
  //  btns->addWidget(mBtnStudentSubmit);
    rightLayout->addStretch(1);
    rightLayout->addLayout(btns);
    rightLayout->addWidget(mBtnStudentSubmit);


    QWidget *rightWidget = new QWidget; rightWidget->setLayout(rightLayout);

    QSplitter *split = new QSplitter;
    split->addWidget(leftWidget);
    split->addWidget(rightWidget);
    split->setStretchFactor(1,1);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(split);
    central->setLayout(mainLayout);

    connect(mBtnStudentNext, &QPushButton::clicked, this, &MainWindow::onStudentNext);
    connect(mBtnStudentSubmit, &QPushButton::clicked, this, &MainWindow::onStudentSubmit);
}

/* -----------------------------
   Data / UI helpers
   ----------------------------*/
void MainWindow::refreshTestList()
{
    if (!mListTests) return;
    mListTests->clear();
for (auto &t : std::as_const(mTests)) {
        mListTests->addItem(t.name);
    }
    if (!mTeacherMode && !mTests.isEmpty()) {
        // select first test by default for student
        mListTests->setCurrentRow(0);
    }
}

void MainWindow::refreshQuestionList()
{
    mListQuestions->clear();
    for (const Question &q : std::as_const(mQuestions)) {
        QString label = q.text;
        if (label.length() > 80) label = label.left(77) + "...";
        mListQuestions->addItem(label);
    }
}

/* -----------------------------
   Teacher slots
   ----------------------------*/
void MainWindow::onAddTest()
{
    Test t;
    t.id = QUuid::createUuid().toString();
    t.name = "Novy test1";
    t.description = "";
    t.studentCount = 10;
    // add to DB immediately
    QString err;
    if (!DBManager::instance().addOrUpdateTest(t, &err)) {
        QMessageBox::warning(this, "Chyba při ukládání testu", err);
    } else {
        mTests.append(t);
        refreshTestList();
        int idx = mListTests->count()-1;
        mListTests->setCurrentRow(idx);
        mEditTestName->setText(t.name);
        mEditTestDescription->setText(t.description);
        mSpinStudentCount->setValue(t.studentCount);
    }
}

// Implemented slot: remove currently selected test
void MainWindow::onRemoveTest()
{
    int idx = mListTests->currentRow();
    if (idx < 0 || idx >= mTests.size()) return;
    QString testId = mTests[idx].id;
    QString err;
    if (!DBManager::instance().removeTest(testId, &err)) {
        QMessageBox::warning(this, "Chyba při mazání testu z DB", err);
        return;
    }
    mTests.removeAt(idx);
    refreshTestList();
    // clear editor if in teacher mode
    if (mTeacherMode) {
        mListQuestions->clear();
        mEditTestName->clear();
        mEditTestDescription->clear();
        mQuestions.clear();
    }
}

void MainWindow::onAddQuestion()
{
    int tidx = mListTests->currentRow();
    if (tidx < 0) {
        QMessageBox::warning(this, "Žádný test", "Nejprve vyberte nebo vytvořte test.");
        return;
    }
    Question q;
    q.id = QUuid::createUuid().toString();
    q.testId = mTests[tidx].id;
    q.text = "Nová otázka";
    q.type = QuestionType::SingleChoice;
    q.options = { Answer{"Možnost 1", true}, Answer{"Možnost 2", false} };

    QString err;
    if (!DBManager::instance().addOrUpdateQuestion(q, &err)) {
        QMessageBox::warning(this, "Chyba při ukládání otázky", err);
        return;
    }

    // add locally and refresh
    mQuestions.append(q);
    refreshQuestionList();
    mListQuestions->setCurrentRow(mQuestions.size()-1);
}

void MainWindow::onRemoveQuestion()
{
    int row = mListQuestions->currentRow();
    if (row < 0 || row >= mQuestions.size()) return;
    QString qid = mQuestions[row].id;
    QString err;
    if (!DBManager::instance().removeQuestion(qid, &err)) {
        QMessageBox::warning(this, "Chyba mazání otázky", err);
        return;
    }
    mQuestions.removeAt(row);
    refreshQuestionList();
    if (!mQuestions.isEmpty()) mListQuestions->setCurrentRow(qMin(row, mQuestions.size()-1));
    else {
        mEditQuestionText->clear();
        mTblAnswers->setRowCount(0);
        mEditExpectedText->clear();
    }
}

void MainWindow::onQuestionSelected(int row)
{
    if (row < 0 || row >= mQuestions.size()) return;
    loadQuestionIntoEditor(row);
}

void MainWindow::loadQuestionIntoEditor(int index)
{
    if (index < 0 || index >= mQuestions.size()) return;
    const Question &q = mQuestions[index];
    mEditQuestionText->setPlainText(q.text);
    mComboType->setCurrentIndex(static_cast<int>(q.type));
    mTblAnswers->setRowCount(0);
    for (const Answer &a : q.options) {
        int r = mTblAnswers->rowCount();
        mTblAnswers->insertRow(r);
        QTableWidgetItem *t = new QTableWidgetItem(a.text);
        mTblAnswers->setItem(r, 0, t);
        QTableWidgetItem *c = new QTableWidgetItem;
        c->setFlags(c->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        c->setCheckState(a.correct ? Qt::Checked : Qt::Unchecked);
        mTblAnswers->setItem(r, 1, c);
    }
    mEditExpectedText->setText(q.expectedText);
    onTypeChanged(static_cast<int>(q.type));
}

void MainWindow::onTypeChanged(int idx)
{
    bool isChoice = (idx == 0 || idx == 1);
    mTblAnswers->setEnabled(isChoice);
    mBtnAddAnswer->setEnabled(isChoice);
    mBtnRemoveAnswer->setEnabled(isChoice);
    mEditExpectedText->setEnabled(idx == 2);
    scheduleAutoSave();
}

void MainWindow::onAddAnswer()
{
    int r = mTblAnswers->rowCount();
    mTblAnswers->insertRow(r);
    mTblAnswers->setItem(r, 0, new QTableWidgetItem(QString("Nová možnost %1").arg(r+1)));
    QTableWidgetItem *c = new QTableWidgetItem;
    c->setFlags(c->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    c->setCheckState(Qt::Unchecked);
    mTblAnswers->setItem(r, 1, c);
    scheduleAutoSave();
}

void MainWindow::onRemoveAnswer()
{
    int r = mTblAnswers->currentRow();
    if (r >= 0) {
        mTblAnswers->removeRow(r);
        scheduleAutoSave();
    }
}

/* -----------------------------
   Auto-save implementation
   ----------------------------*/
void MainWindow::scheduleAutoSave()
{
    // restart timer
    mAutoSaveTimer.start();
}

bool MainWindow::doAutoSave()
{
    // determine current selected question and persist to DB
    int qidx = mListQuestions ? mListQuestions->currentRow() : -1;
    if (mTeacherMode && qidx >= 0 && qidx < mQuestions.size()) {
        collectEditorToQuestion(qidx);
        QString err;
        bool autosave = DBManager::instance().addOrUpdateQuestion(mQuestions[qidx], &err);
        if (!autosave) {
            QMessageBox::warning(this, "Chyba při auto-ukládání otázky", err);
        }
        return autosave;
    }
    return false;
}

void MainWindow::doAutoSaveWithRefresh()
{
    if (doAutoSave()) {
        // refresh question list to reflect any text changes
        refreshQuestionList();
        int qidx = mListQuestions ? mListQuestions->currentRow() : -1;
        if (mListQuestions )
            mListQuestions->setCurrentRow(qidx);
    }
}


void MainWindow::answerItemChanged(QTableWidgetItem *item)
{
    int qidx = mListQuestions ? mListQuestions->currentRow() : -1;
    if (mTeacherMode && qidx >= 0 && qidx < mQuestions.size()) {
        QuestionType qt = static_cast<QuestionType>(mComboType->currentIndex());
        if (qt == QuestionType::SingleChoice) {
            // ensure only one checked
            size_t counter = 0;
            for (int r = 0; r < mTblAnswers->rowCount(); ++r) {
                QTableWidgetItem *c = mTblAnswers->item(r, 1);
                mTblAnswers->blockSignals(true);
                if (c && c != item && item->column() == 1 &&
                    item->checkState() == Qt::Checked &&
                    c->checkState() == Qt::Checked) {
                    c->setCheckState(Qt::Unchecked);
                }
                mTblAnswers->blockSignals(false);
                if (c && c->checkState() == Qt::Checked)
                    counter++;
            }
            if (counter == 0 && item->column() == 1) {
                // prevent unchecking all in single-choice mode
                mTblAnswers->blockSignals(true);
                item->setCheckState(Qt::Checked);
                mTblAnswers->blockSignals(false);
            }
        }
    }
    scheduleAutoSave();
}

/* collect editor fields into question model */
void MainWindow::collectEditorToQuestion(int index)
{
    if (index < 0 || index >= mQuestions.size()) return;
    Question &q = mQuestions[index];
    q.text = mEditQuestionText->toPlainText();
    q.type = static_cast<QuestionType>(mComboType->currentIndex());
    q.options.clear();
    for (int r = 0; r < mTblAnswers->rowCount(); ++r) {
        QTableWidgetItem *t = mTblAnswers->item(r, 0);
        QTableWidgetItem *c = mTblAnswers->item(r, 1);
        Answer a;
        a.text = t ? t->text() : QString();
        a.correct = (c && c->checkState() == Qt::Checked);
        q.options.append(a);
    }
    q.expectedText = mEditExpectedText->text();
}

/* -----------------------------
   Student runner (embedded)
   ----------------------------*/
void MainWindow::onTestSelected(int idx)
{
    // This slot is used in both modes: teacher list selection and student selection.
    if (mTeacherMode) {
        // load test metadata and questions for teacher editor
        if (idx < 0 || idx >= mTests.size()) {
            mEditTestName->clear();
            mEditTestDescription->clear();
            mQuestions.clear();
            refreshQuestionList();
            return;
        }
        const Test &t = mTests[idx];
        mEditTestName->setText(t.name);
        mEditTestDescription->setText(t.description);
        mSpinStudentCount->setValue(t.studentCount);

        // load questions for this test from DB
        QString err;
        QVector<Question> loadedQuestions;
        if (!DBManager::instance().loadQuestionsForTest(t.id, loadedQuestions, &err)) {
            QMessageBox::warning(this, "Chyba při načítání otázek z DB", err);
            mQuestions.clear();
        } else {
            mQuestions = std::move(loadedQuestions);
        }
        refreshQuestionList();
        if (!mQuestions.isEmpty()) mListQuestions->setCurrentRow(0);
        return;
    }

    // Student mode: when selecting a test, load questions and start test inline
    if (idx < 0 || idx >= mTests.size()) {
        mStudentQuestions.clear();
        mStudentAnswers.clear();
        mLblStudentProgress->clear();
        mLblStudentQuestion->clear();
        QLayout *l = mWidgetStudentAnswers->layout();
        QLayoutItem *it;
        while ((it = l->takeAt(0)) != nullptr) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
        return;
    }

    QString tid = mTests[idx].id;
    QString err;
    if (!DBManager::instance().loadQuestionsForTest(tid, mStudentQuestions, &err)) {
        QMessageBox::warning(this, "Chyba při načítání otázek", err);
        return;
    }

    // prepare randomized subset
    std::shuffle(mStudentQuestions.begin(), mStudentQuestions.end(), *QRandomGenerator::global());

    // Use studentCount from the selected test
    int count = mTests[idx].studentCount;
    if (mStudentQuestions.size() > count) {
        mStudentQuestions.resize(count);
    }
    mStudentCurrentIndex = 0;
    mStudentAnswers.clear();
    mStudentAnswers.resize(mStudentQuestions.size());
    // show first question
    if (!mStudentQuestions.isEmpty()) {
        showStudentQuestion(0);
    }
}

void MainWindow::showStudentQuestion(int index)
{
    if (index < 0 || index >= mStudentQuestions.size()) return;
    mStudentCurrentIndex = index;
    const Question &q = mStudentQuestions[index];
    mLblStudentProgress->setText(QString("Otázka %1 / %2").arg(index+1).arg(mStudentQuestions.size()));
    mLblStudentQuestion->setText(q.text);

    // clear previous answer widgets
    QLayout *lay = mWidgetStudentAnswers->layout();
    QLayoutItem *child;
    while ((child = lay->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    mCurrentAnswerWidgets.clear();

    // get stored student answer for this index
    QString stored = mStudentAnswers.value(index);

    if (q.type == QuestionType::TextAnswer) {
        QLineEdit *le = new QLineEdit;
        if (!stored.isEmpty()) le->setText(stored);
        lay->addWidget(le);
        mCurrentAnswerWidgets.append(le);
    } else {
        int m = q.options.size();
        QVector<int> indices;
        for (int i=0;i<m;++i)
            indices.append(i);
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
                mCurrentAnswerWidgets.append(rb);
            }
        } else {
            QStringList presel = stored.split(";@", Qt::SkipEmptyParts);
            for (int i = 0; i < m; ++i) {
                int origIdx = indices[i];
                QCheckBox *cb = new QCheckBox(q.options[origIdx].text);
                for (const QString &s : std::as_const(presel))
                    if (s.trimmed() == q.options[origIdx].text) cb->setChecked(true);
                lay->addWidget(cb);
                mCurrentAnswerWidgets.append(cb);
            }
        }
    }
}

/* Student navigation */
void MainWindow::onStudentNext()
{
    if (mStudentCurrentIndex < 0 || mStudentCurrentIndex >= mStudentQuestions.size()) return;

    // save current answer into m_studentAnswers
    const Question &curQ = mStudentQuestions[mStudentCurrentIndex];
    if (curQ.type == QuestionType::TextAnswer) {
        QLineEdit *le = qobject_cast<QLineEdit*>(mCurrentAnswerWidgets.isEmpty() ? nullptr : mCurrentAnswerWidgets.first());
        mStudentAnswers[mStudentCurrentIndex] = le ? le->text().trimmed() : QString();
    } else if (curQ.type == QuestionType::SingleChoice) {
        QString chosen;
        for (QWidget *w : std::as_const(mCurrentAnswerWidgets)) {
            QRadioButton *rb = qobject_cast<QRadioButton*>(w);
            if (!rb) continue;
            if (rb->isChecked()) { chosen = rb->text(); break; }
        }
        mStudentAnswers[mStudentCurrentIndex] = chosen;
    } else {
        QStringList sel;
        for (QWidget *w : std::as_const(mCurrentAnswerWidgets)) {
            QCheckBox *cb = qobject_cast<QCheckBox*>(w);
            if (!cb) continue;
            if (cb->isChecked()) sel.append(cb->text());
        }
        mStudentAnswers[mStudentCurrentIndex] = sel.join(";@ ");
    }

    if (mStudentCurrentIndex + 1 < mStudentQuestions.size()) {
        showStudentQuestion(mStudentCurrentIndex + 1);
    } else {
        QMessageBox::information(this, "Konec", "Došli jste na konec testu. Klikněte Odevzdat pro vyhodnocení.");
    }
}

/* Student submit: evaluate and save result */
void MainWindow::onStudentSubmit()
{
    // save current answer
    if (mStudentCurrentIndex >=0 && mStudentCurrentIndex < mStudentQuestions.size()) {
        const Question &curQ = mStudentQuestions[mStudentCurrentIndex];
        if (curQ.type == QuestionType::TextAnswer) {
            QLineEdit *le = qobject_cast<QLineEdit*>(mCurrentAnswerWidgets.isEmpty() ? nullptr : mCurrentAnswerWidgets.first());
            mStudentAnswers[mStudentCurrentIndex] = le ? le->text().trimmed() : QString();
        } else if (curQ.type == QuestionType::SingleChoice) {
            QString chosen;
            for (QWidget *w : std::as_const(mCurrentAnswerWidgets)) {
                QRadioButton *rb = qobject_cast<QRadioButton*>(w);
                if (!rb) continue;
                if (rb->isChecked()) { chosen = rb->text(); break; }
            }
            mStudentAnswers[mStudentCurrentIndex] = chosen;
        } else {
            QStringList sel;
            for (QWidget *w : std::as_const(mCurrentAnswerWidgets)) {
                QCheckBox *cb = qobject_cast<QCheckBox*>(w);
                if (!cb) continue;
                if (cb->isChecked()) sel.append(cb->text());
            }
            mStudentAnswers[mStudentCurrentIndex] = sel.join(";@ ");
        }
    }

    // Evaluate
    double totalScore = 0.0;
    QVector<DBManager::ResultDetail> details;
    for (int i = 0; i < mStudentQuestions.size(); ++i) {
        const Question &q = mStudentQuestions[i];
        bool correct = false;
        QString ua = mStudentAnswers.value(i);
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
            for (const Answer &a : q.options)
                if (a.correct)
                    correctTexts.append(a.text);

            QStringList sel = ua.split(";@", Qt::SkipEmptyParts);
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

    QString email = mEditStudentEmail ? mEditStudentEmail->text().trimmed() : QString();
    QString err;
    // save result with test id
    QString tid = currentTestId();
    if (!DBManager::instance().saveResult(email, tid, totalScore, mStudentQuestions.size(), details, &err)) {
        QMessageBox::warning(this, "Chyba ukládání výsledku", err);
    } else {
        QMessageBox::information(this, "Výsledek", QString("Skore: %1 / %2\nVýsledek uložen.").arg(totalScore).arg(mStudentQuestions.size()));
    }
}

/* Optional slot used to start test programmatically (kept because header declares it) */
void MainWindow::onStudentStartTest()
{
    if (!mTeacherMode) {
        int idx = currentTestIndex();
        if (idx >= 0) {
            onTestSelected(idx); // will load and show first question
        }
    }
}
