#ifndef CLASSANNOTATE_H
#define CLASSANNOTATE_H

#include <QObject>
#include <QPainter>
#include <QStaticText>
#include <QTextDocumentFragment>

// Contains individual annotation object
struct annotation
{
    QString text;           // Annotation text: primary, non-expanded, editable string
    QStaticText cache;      // Annotation text with {net|bus} macros expanded and prepared for painter rendering
    QRect rect;             // Annotation bounding rectangle in the texture space
    QPoint pos;             // Coordinates of the text in the texture space
    uint pix;               // Text size in pixels
    bool overline {};       // Signal is inverted and needs a line on top of text
    bool drawrect {true};   // Draw the bounding rectangle as part of the annotation

    annotation(const QString t = QString(), const QRect r = QRect()): text(t), rect(r) {}
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

    QVector<annotation> &get() { return m_annot; }
    void set(QVector<annotation> &list) { m_annot = list; }
    void add(QString text, QRect box);  // Adds annotation to the list
    QVector<uint> get(QPoint &pos);     // Returns a list of annotation indices at the given coordinate
    QVector<uint> get(QRect r);         // Returns a list of annotation indices within the given rectangle
    uint count() { return m_annot.count(); } // Returns the total number of annotations

    void draw(QPainter &painter, const QRect &viewport, qreal scale);
    bool load(QString fileName);        // Loads user annotations
    bool save(QString fileName);        // Saves user annotations

public slots:
    void onShutdown();                  // Called when the app is closing

private:
    QVector<annotation> m_annot;        // List of annotations
    QString m_jsonFile;                 // File name used to load the annotations
    QFont m_fixedFont;                  // Font used to render annotations
    const qreal m_someXFactor = 1.8;    // Depending on a font, we need to stretch its rendering
    const QRect m_imgRect = QRect(0, 0, 4700, 5000); // XXX Z80-specific, hard-coded image size rectangle!

    int textLength(const QString &text) // Returns the length of a text with HTML tags stripped
        { return QTextDocumentFragment::fromHtml(text).toPlainText().length(); }
};

#endif // CLASSANNOTATE_H
