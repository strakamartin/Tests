#include "dbmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QUuid>
#include <QDebug>

DBManager &DBManager::instance()
{
    static DBManager inst;
    return inst;
}

static bool execOrFail(QSqlQuery &qq, QString *err)
{
    if (!qq.exec()) {
        if (err) {
            QString details = qq.lastError().text();
            details += "\nQuery: " + qq.lastQuery();

            // Try to list named bound values first
            QStringList names = qq.boundValueNames();
            if (!names.isEmpty()) {
                details += "\nBound values (named):";
                for (const QString &n : std::as_const(names)) {
                    QVariant v = qq.boundValue(n);
                    details += "\n  " + n + " = " + v.toString();
                }
            } else {
                QVariantList vals = qq.boundValues();
                if (!vals.isEmpty()) {
                    details += "\nBound values (positional):";
                    for (int i = 0; i < vals.size(); ++i) {
                        details += "\n  [" + QString::number(i) + "] = " + vals.at(i).toString();
                    }
                }
            }

            *err = details;
        }
        qDebug() << "SQL error:" << qq.lastError().text();
        qDebug() << "Query:" << qq.lastQuery();
        qDebug() << "Bound names:" << qq.boundValueNames();
        qDebug() << "Bound values:" << qq.boundValues();
        return false;
    }
    return true;
}

bool DBManager::openDatabase(const QString &path, QString *err)
{
    if (mDb.isValid() && mDb.isOpen()) mDb.close();

    mDb = QSqlDatabase::addDatabase("QSQLITE", "qt_test_maker_connection");
    mDb.setDatabaseName(path);
    if (!mDb.open()) {
        if (err) *err = mDb.lastError().text();
        return false;
    }
    return ensureSchema(err);
}

bool DBManager::ensureSchema(QString *err)
{
    QSqlQuery q(mDb);
    // tests table
    q.prepare(
        "CREATE TABLE IF NOT EXISTS tests ("
        "id TEXT PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "description TEXT,"
        "student_count INTEGER DEFAULT 10"
        ")"
        );
    if (!execOrFail(q, err)) return false;

    // Ensure tests has student_count column (migrate older DBs)
    {
        QSqlQuery qi(mDb);
        if (!qi.exec("PRAGMA table_info(tests)")) {
            if (err) *err = qi.lastError().text();
            return false;
        }
        bool hasStudentCount = false;
        while (qi.next()) {
            QString colName = qi.value(1).toString();
            if (colName == "student_count") { hasStudentCount = true; break; }
        }
        if (!hasStudentCount) {
            QSqlQuery alt(mDb);
            if (!alt.exec("ALTER TABLE tests ADD COLUMN student_count INTEGER DEFAULT 10")) {
                if (err) {
                    QString details = alt.lastError().text();
                    details += "\nFailed ALTER TABLE tests ADD COLUMN student_count";
                    *err = details;
                }
                qDebug() << "Failed to ALTER TABLE to add student_count to tests:" << alt.lastError().text();
                return false;
            } else {
                qDebug() << "ALTER TABLE tests ADD COLUMN student_count executed (migration)";
            }
        }
    }

    // questions table (we'll create without assuming presence of test_id in old DBs)
    q.prepare(
        "CREATE TABLE IF NOT EXISTS questions ("
        "id TEXT PRIMARY KEY,"
        "test_id TEXT,"
        "text TEXT NOT NULL,"
        "type INTEGER NOT NULL,"
        "expected_text TEXT,"
        "FOREIGN KEY(test_id) REFERENCES tests(id) ON DELETE CASCADE"
        ")"
        );
    if (!execOrFail(q, err)) return false;

    // Now ensure that the column test_id actually exists (for DBs created by older versions)
    {
        QSqlQuery qi(mDb);
        if (!qi.exec("PRAGMA table_info(questions)")) {
            if (err) *err = qi.lastError().text();
            return false;
        }
        bool hasTestId = false;
        while (qi.next()) {
            QString colName = qi.value(1).toString(); // name is in column index 1
            if (colName == "test_id") { hasTestId = true; break; }
        }
        if (!hasTestId) {
            // Add the column
            QSqlQuery alt(mDb);
            if (!alt.exec("ALTER TABLE questions ADD COLUMN test_id TEXT")) {
                if (err) {
                    QString details = alt.lastError().text();
                    details += "\nFailed ALTER TABLE questions ADD COLUMN test_id";
                    *err = details;
                }
                qDebug() << "Failed to ALTER TABLE to add test_id:" << alt.lastError().text();
                return false;
            } else {
                qDebug() << "ALTER TABLE questions ADD COLUMN test_id executed (migration)";
            }
        }
    }

    // options table
    q.prepare(
        "CREATE TABLE IF NOT EXISTS options ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "question_id TEXT NOT NULL,"
        "text TEXT NOT NULL,"
        "correct INTEGER NOT NULL,"
        "ord INTEGER NOT NULL,"
        "FOREIGN KEY(question_id) REFERENCES questions(id) ON DELETE CASCADE"
        ")"
        );
    if (!execOrFail(q, err)) return false;

    // results table
    q.prepare(
        "CREATE TABLE IF NOT EXISTS results ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "student_email TEXT,"
        "test_id TEXT,"
        "score REAL,"
        "total INTEGER,"
        "timestamp TEXT,"
        "FOREIGN KEY(test_id) REFERENCES tests(id) ON DELETE SET NULL"
        ")"
        );
    if (!execOrFail(q, err)) return false;

    // Ensure results has test_id (migrate older DBs)
    {
        QSqlQuery qi(mDb);
        if (!qi.exec("PRAGMA table_info(results)")) {
            if (err) *err = qi.lastError().text();
            return false;
        }
        bool hasTestId = false;
        while (qi.next()) {
            QString colName = qi.value(1).toString();
            if (colName == "test_id") { hasTestId = true; break; }
        }
        if (!hasTestId) {
            QSqlQuery alt(mDb);
            if (!alt.exec("ALTER TABLE results ADD COLUMN test_id TEXT")) {
                if (err) {
                    QString details = alt.lastError().text();
                    details += "\nFailed ALTER TABLE results ADD COLUMN test_id";
                    *err = details;
                }
                qDebug() << "Failed to ALTER TABLE to add test_id to results:" << alt.lastError().text();
                return false;
            } else {
                qDebug() << "ALTER TABLE results ADD COLUMN test_id executed (migration)";
            }
        }
    }

    // result details table
    q.prepare(
        "CREATE TABLE IF NOT EXISTS result_details ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "result_id INTEGER NOT NULL,"
        "question_id TEXT,"
        "correct INTEGER,"
        "user_answer TEXT,"
        "FOREIGN KEY(result_id) REFERENCES results(id) ON DELETE CASCADE"
        ")"
        );
    if (!execOrFail(q, err)) return false;

    return true;
}

bool DBManager::loadTests(QVector<Test> &outTests, QString *err)
{
    outTests.clear();
    QSqlQuery q(mDb);
    q.prepare("SELECT id, name, description, student_count FROM tests ORDER BY rowid");
    if (!execOrFail(q, err)) return false;
    while (q.next()) {
        Test t;
        t.id = q.value(0).toString();
        t.name = q.value(1).toString();
        t.description = q.value(2).toString();
        t.studentCount = q.value(3).toInt();
        outTests.append(t);
    }
    return true;
}

bool DBManager::addOrUpdateTest(const Test &t, QString *err)
{
    if (!mDb.transaction()) {
        if (err) *err = mDb.lastError().text();
        return false;
    }

    QSqlQuery q(mDb);
    q.prepare("SELECT COUNT(1) FROM tests WHERE id = ?");
    q.addBindValue(t.id);
    if (!execOrFail(q, err)) { mDb.rollback(); return false; }
    bool exists = false;
    if (q.next()) exists = (q.value(0).toInt() > 0);

    if (!exists) {
        QSqlQuery ins(mDb);
        ins.prepare("INSERT INTO tests (id, name, description, student_count) VALUES (?, ?, ?, ?)");
        ins.addBindValue(t.id);
        ins.addBindValue(t.name);
        ins.addBindValue(t.description);
        ins.addBindValue(t.studentCount);
        if (!execOrFail(ins, err)) { mDb.rollback(); return false; }
    } else {
        QSqlQuery upd(mDb);
        upd.prepare("UPDATE tests SET name=?, description=?, student_count=? WHERE id=?");
        upd.addBindValue(t.name);
        upd.addBindValue(t.description);
        upd.addBindValue(t.studentCount);
        upd.addBindValue(t.id);
        if (!execOrFail(upd, err)) { mDb.rollback(); return false; }
    }

    if (!mDb.commit()) {
        if (err) *err = mDb.lastError().text();
        mDb.rollback();
        return false;
    }
    return true;
}

bool DBManager::removeTest(const QString &testId, QString *err)
{
    if (!mDb.transaction()) {
        if (err) *err = mDb.lastError().text();
        return false;
    }
    QSqlQuery q(mDb);
    q.prepare("DELETE FROM tests WHERE id = ?");
    q.addBindValue(testId);
    if (!execOrFail(q, err)) { mDb.rollback(); return false; }
    if (!mDb.commit()) {
        if (err) *err = q.lastError().text();
        mDb.rollback();
        return false;
    }
    return true;
}

bool DBManager::loadAllQuestions(QVector<Question> &outQuestions, QString *err)
{
    outQuestions.clear();
    QSqlQuery q(mDb);
    q.prepare("SELECT id, test_id, text, type, expected_text FROM questions ORDER BY rowid");
    if (!execOrFail(q, err)) return false;
    while (q.next()) {
        Question qq;
        qq.id = q.value(0).toString();
        qq.testId = q.value(1).toString();
        qq.text = q.value(2).toString();
        qq.type = static_cast<QuestionType>(q.value(3).toInt());
        qq.expectedText = q.value(4).toString();
        // load options
        QSqlQuery q2(mDb);
        q2.prepare("SELECT text, correct FROM options WHERE question_id = ? ORDER BY ord");
        q2.addBindValue(qq.id);
        if (!execOrFail(q2, err)) return false;
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

bool DBManager::loadQuestionsForTest(const QString &testId, QVector<Question> &outQuestions, QString *err)
{
    outQuestions.clear();
    QSqlQuery q(mDb);
    q.prepare("SELECT id, test_id, text, type, expected_text FROM questions WHERE test_id = ? ORDER BY rowid");
    q.addBindValue(testId);
    if (!execOrFail(q, err)) return false;
    while (q.next()) {
        Question qq;
        qq.id = q.value(0).toString();
        qq.testId = q.value(1).toString();
        qq.text = q.value(2).toString();
        qq.type = static_cast<QuestionType>(q.value(3).toInt());
        qq.expectedText = q.value(4).toString();
        // load options
        QSqlQuery q2(mDb);
        q2.prepare("SELECT text, correct FROM options WHERE question_id = ? ORDER BY ord");
        q2.addBindValue(qq.id);
        if (!execOrFail(q2, err)) return false;
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
    if (!mDb.transaction()) {
        if (err) *err = mDb.lastError().text();
        return false;
    }

    QSqlQuery q(mDb);
    q.prepare("SELECT COUNT(1) FROM questions WHERE id = ?");
    q.addBindValue(qobj.id);
    if (!execOrFail(q, err)) { mDb.rollback(); return false; }
    bool exists = false;
    if (q.next()) exists = (q.value(0).toInt() > 0);

    if (!exists) {
        QSqlQuery ins(mDb);
        ins.prepare("INSERT INTO questions (id, test_id, text, type, expected_text) VALUES (?, ?, ?, ?, ?)");
        ins.addBindValue(qobj.id);
        ins.addBindValue(qobj.testId);
        ins.addBindValue(qobj.text);
        ins.addBindValue(static_cast<int>(qobj.type));
        ins.addBindValue(qobj.expectedText);
        if (!execOrFail(ins, err)) { mDb.rollback(); return false; }
    } else {
        QSqlQuery upd(mDb);
        upd.prepare("UPDATE questions SET test_id=?, text=?, type=?, expected_text=? WHERE id=?");
        upd.addBindValue(qobj.testId);
        upd.addBindValue(qobj.text);
        upd.addBindValue(static_cast<int>(qobj.type));
        upd.addBindValue(qobj.expectedText);
        upd.addBindValue(qobj.id);
        if (!execOrFail(upd, err)) { mDb.rollback(); return false; }
        // delete existing options; we will reinsert
        QSqlQuery del(mDb);
        del.prepare("DELETE FROM options WHERE question_id = ?");
        del.addBindValue(qobj.id);
        if (!execOrFail(del, err)) { mDb.rollback(); return false; }
    }

    // insert options
    for (int i = 0; i < qobj.options.size(); ++i) {
        const Answer &a = qobj.options[i];
        QSqlQuery iopt(mDb);
        iopt.prepare("INSERT INTO options (question_id, text, correct, ord) VALUES (?, ?, ?, ?)");
        iopt.addBindValue(qobj.id);
        iopt.addBindValue(a.text);
        iopt.addBindValue(a.correct ? 1 : 0);
        iopt.addBindValue(i);
        if (!execOrFail(iopt, err)) { mDb.rollback(); return false; }
    }

    if (!mDb.commit()) {
        if (err) *err = mDb.lastError().text();
        mDb.rollback();
        return false;
    }
    return true;
}

bool DBManager::removeQuestion(const QString &questionId, QString *err)
{
    if (!mDb.transaction()) {
        if (err) *err = mDb.lastError().text();
        return false;
    }
    QSqlQuery q(mDb);
    q.prepare("DELETE FROM questions WHERE id = ?");
    q.addBindValue(questionId);
    if (!execOrFail(q, err)) { mDb.rollback(); return false; }
    if (!mDb.commit()) {
        if (err) *err = q.lastError().text();
        mDb.rollback();
        return false;
    }
    return true;
}

bool DBManager::saveResult(const QString &studentEmail, const QString &testId, double score, int total,
                           const QVector<ResultDetail> &details, QString *err)
{
    if (!mDb.transaction()) {
        if (err) *err = mDb.lastError().text();
        return false;
    }

    // Prepare positional insert for results
    QSqlQuery q(mDb);
    q.prepare("INSERT INTO results (student_email, test_id, score, total, timestamp) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(studentEmail);
    q.addBindValue(testId);
    q.addBindValue(score);
    q.addBindValue(total);
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!execOrFail(q, err)) { mDb.rollback(); return false; }

    // get last inserted id
    QSqlQuery q2(mDb);
    q2.prepare("SELECT last_insert_rowid()");
    if (!execOrFail(q2, err)) { mDb.rollback(); return false; }
    if (!q2.next()) {
        if (err) *err = "Cannot read last_insert_rowid()";
        mDb.rollback();
        return false;
    }
    int resultId = q2.value(0).toInt();

    // Insert details using positional placeholders
    for (const ResultDetail &d : details) {
        QSqlQuery qd(mDb);
        qd.prepare("INSERT INTO result_details (result_id, question_id, correct, user_answer) VALUES (?, ?, ?, ?)");
        qd.addBindValue(resultId);
        qd.addBindValue(d.questionId);
        qd.addBindValue(d.correct ? 1 : 0);
        qd.addBindValue(d.userAnswer);
        if (!execOrFail(qd, err)) { mDb.rollback(); return false; }
    }

    if (!mDb.commit()) {
        if (err) *err = mDb.lastError().text();
        mDb.rollback();
        return false;
    }
    return true;
}
