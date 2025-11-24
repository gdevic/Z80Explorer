#ifndef CLASSJSLOADER_H
#define CLASSJSLOADER_H

#include <variant>
#include <QObject>
#include <QRegularExpression>

class QRegularExpressionMatch;

/*
 * The AST is stored in a contiguous array of chunks as below
 */
struct ASTChunk
{
    enum class Type : uint16_t {
        End,
        Number,
        Bool,
        Identifier,
        Punctuator,
        String,
        Array,
        VarDef,
        Statement,
    };
    bool asBool() const;
    double asNumber() const;
    QStringView asString() const;
    QStringView asIdentifier() const;
    QStringView asPunctuator() const;
    int asArrayLength() const;

private:
    uint64_t pad;
};

/*
 * A very basic JavaScript loader that deals with a subset of JSON, and accepts variable assignments.
 * It is used to quickly load various .js resource files. It is much faster than QJSEngine::evaluate,
 * and it avoids the need for rolling custom parsers.
 * The AST in `root` is the result of load(), and is not valid after destruction of the loader,
 * since it uses string views into m_contents.
 */
class ClassJSLoader : public QObject
{
    Q_OBJECT
public:
    explicit ClassJSLoader(QObject *parent = nullptr);
    ~ClassJSLoader();

    bool load(QString fileName);

    struct Value;
    struct Array : QVector<Value>
    {
        using QVector<Value>::QVector;
    };
    struct String : QStringView
    {
        String(QStringView other)
            : QStringView(other)
        {}
        using QStringView::QStringView;
    };
    struct Identifier : QStringView
    {
        Identifier(QStringView other)
            : QStringView(other)
        {}
        using QStringView::QStringView;
    };
    struct Punctuator : QStringView
    {
        Punctuator(QStringView other)
            : QStringView(other)
        {}
        using QStringView::QStringView;
    };

    using ValueBase = std::variant<bool, double, String, Identifier, Punctuator, Array>;
    struct Value : ValueBase
    {
        using ValueBase::variant;
        operator bool() const noexcept { return index() != std::variant_npos; }
    };

    struct VarDef
    {
        Identifier name;
        Value value;
        operator bool() const noexcept { return !name.isEmpty() && value; }
    };

    using StatementBase = std::variant<VarDef, Value>;
    struct Statement : StatementBase
    {
        using StatementBase::variant;
        operator bool() const noexcept { return index() != std::variant_npos; }
    };

    Statement root;
    const Array *rootArray() const; // return the array from the sole statement `var = [...]` or `[...]`

private:
    class Token;

    inline Token &token() const;
    inline Token &accepted() const;
    bool m_accepted_at_1 = false;

    void next();
    bool accept();
    bool accept(int token, QStringView value = {});
    bool expect(int token, QStringView value = {});

    Value getValue();
    Array getArray();
    VarDef getVarDef();
    Statement getStatement();

    const std::unique_ptr<Token[]> m_tokens;
    int m_line = 1, m_lineAdvance = 0;
    qsizetype m_offset = 0, m_end = 0;
    QString m_contents, m_fileName;
    const QRegularExpression m_lexer;
    const QStringList m_tokenNames;
};

#endif // CLASSJSLOADER_H
