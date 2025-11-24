#include "ClassJSLoader.h"

#include <QFile>
#include <QMetaEnum>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

#ifdef JSLOADER_INTERACTIVE
#include <chrono>
namespace chrono = std::chrono;

static uint arrayCount = 0;
static chrono::nanoseconds lexTime = {};
#endif

static auto TOKENS = uR"(
    (?<WSPREFIX> (?: (?&Whitespace)+ | (?&Comment) )* )     # Captured to count lines
    (?: (?<DECIMAL> [+-]? (?&DecIntLit) \. (?&DecDigits)? (?&Exp)?
                  | [+-]? (?&DecIntLit) (?&Exp)?
                  | [+-]? \. (?&DecDigits) (?&Exp)? ) |
        (?<BOOL> (?: true | false ) (?! (?&IdPart) ) ) |
        (?<PUNCTUATOR> [][,=] ) |
        (?<IDENTIFIER> (?&IdStart) (?&IdPart)* ) |
        (?<STRING> '[^\v]*' | "[^\v]*" ) |
        (?<EOF> $ ) |
        (?<UNEXPECTED> . )
    )
    (?(DEFINE)
        (?<IdStart> [a-zA-Z$_] )
        (?<IdPart>  [a-zA-Z0-9_] )
        (?<DecDigits> [0-9] (?:_?[0-9])* )
        (?<DecIntLit> [1-9] _? (?&DecDigits) | [1-9] | 0 )
        (?<Exp> [eE] [+-]? (?&DecDigits) )
        (?<Whitespace> [\v\s] )
        (?<Comment> (?://.*?(?:\v|$)) | (?:/[*].*?[*]/) )
    )
)"_s;

enum TokenType {
    T_INVALID,
    T_WSPREFIX,
    T_DECIMAL,
    T_BEGIN = T_DECIMAL,
    T_BOOL,
    T_PUNCTUATOR,
    T_IDENTIFIER,
    T_STRING,
    T_EOF,
    T_UNEXPECTED,
    T_END
};

struct TypePart
{
    ASTChunk::Type type : 4;
    uint16_t exponent : 11;
    uint16_t sign : 1;
};
struct JustType
{
    uint16_t _pad[3];
    TypePart t;
};
struct Number
{
    double value;
};
struct Bool
{
    bool value;
    uint16_t _pad[2];
    TypePart t;
};
struct Length
{
    int32_t len;
    uint16_t _pad;
    TypePart t;
};
struct Punctuator
{
    char16_t ch[3];
    TypePart t;
};

union ASTUnion {
    struct ASTChunk chunk;
    struct JustType j;
    struct Bool b;
    struct Number n;
    struct Length id;
    struct Punctuator pu;
};

bool ASTChunk::asBool() const
{
    Bool b;
    memcpy(&b, this, sizeof(*this));
    Q_ASSERT(b.t.type == Type::Bool);
    return b.value;
}

double ASTChunk::asNumber() const
{
    JustType jt;
    Number n;
    memcpy(&jt, this, sizeof(*this));
    Q_ASSERT(jt.t.type == Type::Number);
    memcpy(&n, this, sizeof(*this));
    return n.value;
}

QStringView ASTChunk::asString() const
{
    Length len;
    memcpy(&len, this, sizeof(*this));
    Q_ASSERT(len.t.type == Type::String);
    return {reinterpret_cast<const char16_t *>(this + 1), len.len};
}

QStringView ASTChunk::asIdentifier() const
{
    Length len;
    memcpy(&len, this, sizeof(*this));
    Q_ASSERT(len.t.type == Type::Identifier);
    return {reinterpret_cast<const char16_t *>(this + 1), len.len};
}

QStringView ASTChunk::asPunctuator() const
{
    Punctuator p;
    memcpy(&p, this, sizeof(*this));
    Q_ASSERT(p.t.type == Type::Punctuator);
    return static_cast<const char16_t *>(p.ch);
}

int ASTChunk::asArrayLength() const
{
    Length len;
    memcpy(&len, this, sizeof(*this));
    Q_ASSERT(len.t.type == Type::Array);
    return len.len;
}

struct ClassJSLoader::Token
{
    int type = 0;
    uint16_t line = 0, column = 0;
    QStringView capture;

    operator bool() const noexcept { return type >= T_BEGIN && type < T_END; }
    Token() = default;
    Token(const Token &) = default;
    Token(const QRegularExpressionMatch &match) noexcept { *this = match; }

    Token &operator=(const QRegularExpressionMatch &match) noexcept
    {
        if (match.lastCapturedIndex() >= T_BEGIN)
        {
            type = match.lastCapturedIndex();
            capture = match.capturedView(type);
        }
        return *this;
    }

#if 0
    Token &update(const QString &text, qsizetype offset)
    {
        QStringView view = QStringView(text).sliced(offset);
        return *this;
    }
#endif

    bool asBool() const
    {
        Q_ASSERT(type == T_BOOL);
        return capture == u"true"_s;
    }

    double asNumber() const
    {
        Q_ASSERT(type == T_DECIMAL);
        return capture.toDouble(); // 824,3316 ms
        // return {}; // 787, 3145 ms
    }

    String asString() const
    {
        Q_ASSERT(type == T_STRING);
        return String(capture.sliced(1, capture.size() - 2));
    }

    Identifier asIdentifier() const
    {
        Q_ASSERT(type == T_IDENTIFIER);
        return Identifier(capture);
    }

    bool isValuePrefix() const noexcept
    {
        switch (type)
        {
        case T_BOOL:
        case T_DECIMAL:
        case T_STRING:
            return true;
        case T_PUNCTUATOR:
            return capture == u"["_s;
        default:
            return false;
        }
    }
};

inline ClassJSLoader::Token &ClassJSLoader::token() const
{
    return m_tokens[0];
}

inline ClassJSLoader::Token &ClassJSLoader::accepted() const
{
    return m_tokens[1];
}

static QRegularExpression lexer()
{
    static QRegularExpression lexer {TOKENS,
                                     QRegularExpression::DotMatchesEverythingOption
                                         | QRegularExpression::ExtendedPatternSyntaxOption
                                         | QRegularExpression::UseUnicodePropertiesOption};
    lexer.optimize();
    if (!lexer.isValid())
        qWarning().noquote() << "Error setting up the lexer:" << lexer.errorString();
    return lexer;
}

ClassJSLoader::ClassJSLoader(QObject *parent)
    : QObject {parent}
    , m_tokens {new Token[2]}
    , m_lexer {lexer()}
    , m_tokenNames {m_lexer.namedCaptureGroups()}
{}

ClassJSLoader::~ClassJSLoader() {}

bool ClassJSLoader::load(QString fileName)
{
    QFile file(fileName);
    if (m_lexer.isValid() && file.open(QFile::ReadOnly | QFile::Text))
    {
        m_contents = QString::fromUtf8(file.readAll());
        // Guarantees that m_contents are valid Unicode
        file.close();

        m_fileName = fileName;
        m_line = 1;
        m_offset = 0;

        qDebug() << m_lexer.match(m_contents,
                                  m_offset,
                                  {},
                                  QRegularExpression::AnchorAtOffsetMatchOption
                                      | QRegularExpression::DontCheckSubjectStringMatchOption);
        qDebug() << m_tokenNames;

#ifdef JSLOADER_INTERACTIVE
        auto start = chrono::high_resolution_clock::now();
        arrayCount = 0;
        lexTime = {};
#endif

        next();
        root = getStatement();
        bool result = root && expect(T_EOF);

#ifdef JSLOADER_INTERACTIVE
        auto loadTime = chrono::high_resolution_clock::now() - start;
        qDebug() << "Loading took" << chrono::duration_cast<chrono::milliseconds>(loadTime);
        qDebug() << "Lexing took" << chrono::duration_cast<chrono::milliseconds>(lexTime);
#endif

        return result;
    }
    return false;
}

void ClassJSLoader::next()
{
#ifdef JSLOADER_INTERACTIVE
    auto start = chrono::high_resolution_clock::now();
#endif
    auto match = m_lexer.match(m_contents,
                               m_offset,
                               {},
                               QRegularExpression::AnchorAtOffsetMatchOption
                                   | QRegularExpression::DontCheckSubjectStringMatchOption);
#ifdef JSLOADER_INTERACTIVE
    lexTime += (chrono::high_resolution_clock::now() - start);
#endif

    if (match.hasMatch())
    {
        m_end = match.capturedEnd(0);
        m_lineAdvance = match.capturedView(T_WSPREFIX).count(u'\n');
        token().line = m_line + m_lineAdvance;
    }
    token() = match;
}

bool ClassJSLoader::accept()
{
    Q_ASSERT(m_end > 0);
    m_offset = m_end;
    m_line += m_lineAdvance;
    m_end = 0;
    accepted() = std::move(token());
    next();
    return true;
}

bool ClassJSLoader::accept(int type, QStringView value)
{
    return token().type == type && (value.isEmpty() || token().capture == value) && accept();
}

bool ClassJSLoader::expect(int type, QStringView value)
{
    if (accept(type, value))
        return true;
    QDebug warning = qWarning().nospace().noquote();
    warning << qUtf8Printable(m_fileName) << ":" << token().line << " Got " << m_tokenNames.at(token().type) << " \""
            << token().capture.toUtf8().constData() << "\", but expected " << m_tokenNames.at(type);
    if (!value.isEmpty())
        warning << " \"" << value.toUtf8().constData() << "\"";
    return false;
}

/*
 * Recursive-Descent Parser
 */
ClassJSLoader::Value ClassJSLoader::getValue()
{
    if (accept(T_DECIMAL))
        return accepted().asNumber();
    if (accept(T_STRING))
        return accepted().asString();
    if (accept(T_BOOL))
        return accepted().asBool();
    if (accept(T_IDENTIFIER))
        return accepted().asString();
    if (accept(T_PUNCTUATOR, u"["_s))
    {
        Array array = getArray();
        if (expect(T_PUNCTUATOR, u"]"_s))
            return array;
        else
            return {};
    }
    qWarning().nospace().noquote() << qUtf8Printable(m_fileName) << ":" << token().line << " Value syntax error, got "
                                   << m_tokenNames.at(token().type) << " \"" << token().capture.toUtf8().constData()
                                   << "\"";
    return {};
}

ClassJSLoader::Array ClassJSLoader::getArray()
{
#ifdef JSLOADER_INTERACTIVE
    ++arrayCount;
    if ((arrayCount % 5000) == 0)
        qDebug() << "Array #" << arrayCount << "@" << m_offset;
#endif
    Array array;
    array.reserve(4);
    do
    {
        if (!token().isValuePrefix())
            break;
        array += getValue(); // 860,3406ms
    } while (accept(T_PUNCTUATOR, u","_s));
    return array;
}

ClassJSLoader::Statement ClassJSLoader::getStatement()
{
    if (accept(T_IDENTIFIER, u"var"_s))
    {
        VarDef def;
        if (!expect(T_IDENTIFIER))
            return {};
        def.name = accepted().asIdentifier();
        if (!expect(T_PUNCTUATOR, u"="_s))
            return {};
        def.value = getValue();
        if (!def.value)
            return {};
        return def;
    }
    Value value = getValue();
    if (value)
        return value;
    return {};
}

const ClassJSLoader::Array *ClassJSLoader::rootArray() const
{
    const Value *value = std::get_if<Value>(&root);
    if (!value)
    {
        const VarDef *def = std::get_if<VarDef>(&root);
        if (def)
            value = &def->value;
    }
    if (value)
    {
        const Array *array = std::get_if<Array>(value);
        if (array)
            return array;
    }
    return nullptr;
}

#ifdef JSLOADER_INTERACTIVE
#include <QCoreApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ClassJSLoader loader;

    qDebug() << QDir::current().absolutePath();
    qDebug() << loader.load("../../resource/transdefs.js");
    qDebug() << loader.load("../../resource/segdefs.js");
}
#endif
