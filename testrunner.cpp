#include "testrunner.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QButtonGroup>
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
    mLblProgress = new QLabel;
    mLblQuestion = new QLabel;
    mLblQuestion->setWordWrap(true);
    mAnswerWidget = new QWidget;
    QVBoxLayout *aw = new QVBoxLayout;
    aw->setContentsMargins(0,0,0,0);
    mAnswerWidget->setLayout(aw);

    mBtnNext = new QPushButton("Další");
    mBtnSubmit = new QPushButton("Odevzdat test");

    mEditStudentEmail = new QLineEdit;
    mEditStudentEmail->setPlaceholderText("Zadejte svůj e-mail pro zaslání výsledků (volitelné)");
    mBtnSendEmail = new QPushButton("Odeslat výsledky emailem (otevře pošt. klient)");

    QHBoxLayout *btns = new QHBoxLayout;
    btns->addWidget(mBtnNext);
    btns->addWidget(mBtnSubmit);

    main->addWidget(mLblProgress);
    main->addWidget(mLblQuestion);
    main->addWidget(mAnswerWidget);
    main->addLayout(btns);
    main->addWidget(mEditStudentEmail);
    main->addWidget(mBtnSendEmail);

    setLayout(main);

    connect(mBtnNext, &QPushButton::clicked, this, &Testrunner::onNext);
    connect(mBtnSubmit, &QPushButton::clicked, this, &Testrunner::onSubmit);
    connect(mBtnSendEmail, &QPushButton::clicked, this, &Testrunner::onSendEmail);
}

void Testrunner::setQuestionCount(int n) {
    mQuestionCount = n;
}

void Testrunner::setTestId(const QString &testId)
{
    mTestId = testId;
}

void Testrunner::setTestName(const QString &testName)
{
    mTestName = testName;
}

bool Testrunner::loadQuestionsFromDB()
{
    if (mTestId.isEmpty()) {
        QMessageBox::warning(this, "Chyba", "Neurčeno ID testu.");
        return false;
    }
    QString err;
    QVector<Question> loaded;
    if (!DBManager::instance().loadQuestionsForTest(mTestId, loaded, &err)) {
        QMessageBox::warning(this, "Chyba při načítání otázek z DB", err);
        return false;
    }
    mAllQuestions = std::move(loaded);
    return true;
}

void Testrunner::startTest()
{
    // load questions for the test from DB
    if (!loadQuestionsFromDB()) return;

    if (mAllQuestions.isEmpty()) {
        QMessageBox::warning(this, "Chyba", "Vybraný test neobsahuje žádné otázky.");
        return;
    }

    prepareRandomTest();
    mUserAnswers.clear();
    mUserAnswers.resize(mTestQuestions.size());
    mCurrentIndex = 0;
    showCurrentQuestion();
    exec();
}

void Testrunner::prepareRandomTest()
{
    // choose random subset of questions
    int total = mAllQuestions.size();
    int n = qMin(mQuestionCount, total);
    QVector<int> idx;
    idx.reserve(total);
    for (int i = 0; i < total; ++i) idx.append(i);
    std::shuffle(idx.begin(), idx.end(), *QRandomGenerator::global());
    mTestQuestions.clear();
    for (int i = 0; i < n; ++i) mTestQuestions.append(mAllQuestions[idx[i]]);
}

void Testrunner::showCurrentQuestion()
{
    // clear previous answer widgets
    QLayout *lay = mAnswerWidget->layout();
    QLayoutItem *child;
    while ((child = lay->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
    mCurrentAnswerWidgets.clear();

    if (mCurrentIndex < 0 || mCurrentIndex >= mTestQuestions.size()) return;

    const Question &q = mTestQuestions[mCurrentIndex];
    mLblProgress->setText(QString("Otázka %1 / %2").arg(mCurrentIndex+1).arg(mTestQuestions.size()));
    mLblQuestion->setText(q.text);

    if (q.type == QuestionType::TextAnswer) {
        QLineEdit *le = new QLineEdit;
        // restore previous if exists
        const StoredAnswer &sa = mUserAnswers[mCurrentIndex];
        if (!sa.textAnswer.isEmpty()) le->setText(sa.textAnswer);
        lay->addWidget(le);
        mCurrentAnswerWidgets.append(le);
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
                mCurrentAnswerWidgets.append(rb);
                // restore selection if present
                const StoredAnswer &sa = mUserAnswers[mCurrentIndex];
                if (!sa.selectedOptionsTexts.isEmpty()) {
                    if (sa.selectedOptionsTexts.contains(q.options[origIdx].text)) rb->setChecked(true);
                }
            }
        } else { // multiple
            for (int i = 0; i < m; ++i) {
                int origIdx = indices[i];
                QCheckBox *cb = new QCheckBox(q.options[origIdx].text);
                lay->addWidget(cb);
                mCurrentAnswerWidgets.append(cb);
                // restore selection if present
                const StoredAnswer &sa = mUserAnswers[mCurrentIndex];
                if (!sa.selectedOptionsTexts.isEmpty()) {
                    if (sa.selectedOptionsTexts.contains(q.options[origIdx].text)) cb->setChecked(true);
                }
            }
        }
    }
}

void Testrunner::saveCurrentAnswerForIndex(int index)
{
    if (index < 0 || index >= mTestQuestions.size()) return;
    StoredAnswer sa;
    sa.questionId = mTestQuestions[index].id;
    const Question &q = mTestQuestions[index];

    if (q.type == QuestionType::TextAnswer) {
        if (!mCurrentAnswerWidgets.isEmpty()) {
            QLineEdit *le = qobject_cast<QLineEdit*>(mCurrentAnswerWidgets.first());
            if (le) sa.textAnswer = le->text().trimmed();
        }
    } else if (q.type == QuestionType::SingleChoice) {
        for (const QWidget *w : std::as_const(mCurrentAnswerWidgets)) {
            QRadioButton *rb = qobject_cast<QRadioButton*>(w);
            if (!rb) continue;
            if (rb->isChecked()) sa.selectedOptionsTexts.append(rb->text());
        }
    } else { // multiple
        for (const QWidget *w : std::as_const(mCurrentAnswerWidgets)) {
            QCheckBox *cb = qobject_cast<QCheckBox*>(w);
            if (!cb) continue;
            if (cb->isChecked()) sa.selectedOptionsTexts.append(cb->text());
        }
    }

    mUserAnswers[index] = sa;
}

void Testrunner::onNext()
{
    // save current answers
    saveCurrentAnswerForIndex(mCurrentIndex);

    if (mCurrentIndex + 1 < mTestQuestions.size()) {
        ++mCurrentIndex;
        showCurrentQuestion();
    } else {
        QMessageBox::information(this, "Konec", "Došli jsme na konec testu. Klikněte Odevzdat test pro vyhodnocení.");
    }
}

void Testrunner::onSubmit()
{
    // save current answers
    saveCurrentAnswerForIndex(mCurrentIndex);

    QVector<DBManager::ResultDetail> details;
    double score = evaluateAndReturnScore(details);
    QString msg = QString("Skóre: %1 / %2").arg(score).arg(mTestQuestions.size());
    QMessageBox::information(this, "Výsledek testu", msg);

    // Save result to DB (student email optional)
    QString err;
    QString email = mEditStudentEmail->text().trimmed();

    if (!DBManager::instance().saveResult(email, mTestId, score, mTestQuestions.size(), details, &err)) {
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
    for (int i = 0; i < mTestQuestions.size(); ++i) {
        const Question &q = mTestQuestions[i];
        const StoredAnswer &sa = mUserAnswers[i];
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
    body += QString("Skóre: %1 / %2\n\n").arg(score).arg(mTestQuestions.size());
    for (int i = 0; i < details.size(); ++i) {
        body += QString("Otázka %1: %2\n").arg(i+1).arg(details[i].correct ? "OK" : "Špatně");
        body += QString("Odpověď studenta: %1\n\n").arg(details[i].userAnswer);
    }

    QUrl mailto;
    QString to = mEditStudentEmail->text().trimmed();
    mailto.setScheme("mailto");
    mailto.setPath(to);
    QUrlQuery q;
    QString subject = "Výsledek testu";
    if (!mTestName.isEmpty()) subject += ": " + mTestName;
    q.addQueryItem("subject", subject);
    q.addQueryItem("body", body);
    mailto.setQuery(q);

    QDesktopServices::openUrl(mailto);
}
