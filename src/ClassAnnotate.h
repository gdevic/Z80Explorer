#ifndef CLASSANNOTATE_H
#define CLASSANNOTATE_H

#include <QObject>
#include <QPainter>

// Contains individual annotation object
struct Annotation
{
    QString text;       // Annotation text
    QPoint pos;         // Coordinates of the text in the texture space
    int pts;            // Text size in points
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

    void draw(QPainter &painter, QRectF imageView, qreal scale);
    bool load(QString dir);         // Loads user annotations
    bool save(QString dir);         // Saves user annotations

private:
    QVector<Annotation> m_annot;    // List of annotations

    void read(const QJsonObject &json);
    void write(QJsonObject &json) const;
};

#endif // CLASSANNOTATE_H
