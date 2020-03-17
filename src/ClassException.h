#ifndef CLASS_EXCEPTION_H
#define CLASS_EXCEPTION_H

#include <QString>
#include <iostream>

/*
 * Define core-level exception structure.
 * Classes that are underneath the controller class might opt to throw
 * exceptions by some methods if convenient. Use this structure since it
 * derives from std::exception and it adds additional fields:
 *
 * origin()  -> optionally return the origin function of the exception
 *              print this field in a debug log
 * what()    -> the error message
 *              send this error message to the user
 *
 * Consequently, use the Exception(string, string) constructor to send
 * technical jibberish as the first string and a decent message to the
 * user as the second string.
 */
struct Exception : public std::exception
{
    Exception(QString message) : // Simple constructor: post only one error message
        m_origin(),
        m_message(message.toLatin1()) {};

    Exception(QString origin, QString message) : // A better constructor: post the origin and a user-readable messages
        m_origin(origin),
        m_message(message.toLatin1()) {}
    ~Exception() throw() {}
    const char* what() const throw() { return m_message.c_str(); }
    const QString origin() { return m_origin + " "; }

private:
    QString m_origin;
    std::string m_message;
};

#endif // CLASS_EXCEPTION_H
