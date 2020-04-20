#ifndef CLASSANNOTATE_H
#define CLASSANNOTATE_H

#include <QObject>

// Contains individual annotation object
struct Annotation
{
    QString text;       // Annotation text
    QPoint pos;         // Coordinates in the texture space
};

/*
 * This class implements data and methods for image annotation
 */
class ClassAnnotate : public QObject
{
    Q_OBJECT
public:
    explicit ClassAnnotate(QObject *parent = nullptr);
    ~ClassAnnotate();

    bool load(QString dir);         // Loads user annotations
    bool save(QString dir);         // Saves user annotations

private:
    QVector<Annotation> m_annot;    // List of annotations

    void read(const QJsonObject &json);
    void write(QJsonObject &json) const;
};

#endif // CLASSANNOTATE_H
