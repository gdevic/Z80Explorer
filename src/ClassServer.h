#ifndef CLASSSERVER_H
#define CLASSSERVER_H

#include <QList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>

class ClassServer : public QObject
{
    Q_OBJECT

public:
    explicit ClassServer(QObject *parent = nullptr);
    ~ClassServer();

    bool startListening(quint16 port);
    void stopListening();
    bool isListening() const;
    quint16 serverPort() const;

signals:
    void commandReceived(const QString &command, QTcpSocket *clientSocket = nullptr); // Optionally pass client for response
    void clientConnected(QTcpSocket *clientSocket);
    void clientDisconnected(QTcpSocket *clientSocket);
    void serverMessage(const QString &message); // For logging or status updates

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QTcpServer *m_tcpServer;
    QList<QTcpSocket*> m_clientConnections;
};

#endif // CLASSSERVER_H
