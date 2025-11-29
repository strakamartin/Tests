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
