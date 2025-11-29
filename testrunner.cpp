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
