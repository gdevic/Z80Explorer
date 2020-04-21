#ifndef CLASSANNOTATE_H
#define CLASSANNOTATE_H

#include <QObject>
#include <QPainter>
#include <QStaticText>

// Contains individual annotation object
struct annotation
{
    QStaticText text;   // Annotation text, supports subset of HTML
    QPoint pos;         // Coordinates of the text in the texture space
    uint pix;           // Text size in pixels

    annotation(const QString t): text(t) {}
    annotation() {};
    bool operator==(const annotation &b) { return text == b.text; }
};
Q_DECLARE_METATYPE(annotation);

/*
 * This class implements data and methods for image annotation
 */
class ClassAnnotate : public QObject
{
    Q_OBJECT
public:
    explicit ClassAnnotate(QObject *parent = nullptr);
    ~ClassAnnotate();
    bool init();
    QVector<annotation> &get() { return m_annot; }
    void set(QVector<annotation> &list) { m_annot = list; }
    void add(QString text, QRect box);  // Adds annotation to the list

    void draw(QPainter &painter, QRectF imageView, qreal scale);
    bool load(QString dir);             // Loads user annotations
    bool save(QString dir);             // Saves user annotations

private:
    QVector<annotation> m_annot;        // List of annotations
    QFont m_fixedFont;

    void read(const QJsonObject &json);
    void write(QJsonObject &json) const;
};

#endif // CLASSANNOTATE_H
