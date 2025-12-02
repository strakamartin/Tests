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

    // Tests (sady otázek)
    bool loadTests(QVector<Test> &outTests, QString *err = nullptr);
    bool addOrUpdateTest(const Test &t, QString *err = nullptr);
    bool removeTest(const QString &testId, QString *err = nullptr);

    // CRUD for questions
    bool loadAllQuestions(QVector<Question> &outQuestions, QString *err = nullptr); // legacy: load all questions regardless test
    bool loadQuestionsForTest(const QString &testId, QVector<Question> &outQuestions, QString *err = nullptr);
    bool addOrUpdateQuestion(const Question &q, QString *err = nullptr);
    bool removeQuestion(const QString &questionId, QString *err = nullptr);

    // Save test result (with details per question)
    struct ResultDetail {
        QString questionId;
        bool correct;
        QString userAnswer; // text describing user's answer
    };
    bool saveResult(const QString &studentEmail, const QString &testId, double score, int total,
                    const QVector<ResultDetail> &details, QString *err = nullptr);

private:
    DBManager() = default;
    bool ensureSchema(QString *err = nullptr);

    QSqlDatabase m_db;
};

#endif // DBMANAGER_H
