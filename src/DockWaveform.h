#ifndef DOCKWAVE_H
#define DOCKWAVE_H

#include <QDockWidget>

namespace Ui { class DockWaveform; }

// Defines one view item
struct viewitem
{
    QString name; // Net or bus name

    template <class Archive> void serialize(Archive & ar) { ar(name); }
};

class DockWaveform : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockWaveform(QWidget *parent = nullptr);
    ~DockWaveform();

    inline viewitem *getFirst(int &it)     // Iterator
        { it = 1; return (m_view.count() > 0) ? m_view.data() : nullptr; }
    inline viewitem *getNext(int &it)      // Iterator
        { return (it < m_view.count()) ? &m_view[it++] : nullptr; }

private slots:
    void onLoad();
    void onSaveAs();
    void onSave();
    void onEdit();
    void onUp();
    void onDown();

private:
    bool load(QString fileName);
    bool save(QString fileName);
    void rebuildList();

private:
    Ui::DockWaveform *ui;

    QStringList getNames();             // Returns a list of all view item names
    void updateViewitems(QStringList items);
    viewitem *find(QString name);
    void add(QString name);

    QVector<viewitem> m_view;           // A collection of view items

    QString m_defName {"viewlist.vl"};  // Default or last file name used for this viewlist XXX ??
};

#endif // DOCKWAVE_H
