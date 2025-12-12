#ifndef MODELS_H
#define MODELS_H

#include <QString>
#include <QVector>

// Test model (sada otázek)
struct Test {
    QString id;
    QString name;
    QString description;
    int studentCount = 4;
};

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
    QString testId; // which test this question belongs to
    QString text;
    QuestionType type = QuestionType::SingleChoice;
    QVector<Answer> options; // for choice questions
    QString expectedText;    // for exact text answers
};

#endif // MODELS_H
