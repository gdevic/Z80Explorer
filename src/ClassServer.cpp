#include "ClassServer.h"
#include <QDebug>

ClassServer::ClassServer(QObject *parent)
    : QObject(parent), m_tcpServer(nullptr)
{
    connect(this, &ClassServer::serverMessage, this, [=](const QString &message) { qInfo() << message; });
}

ClassServer::~ClassServer()
{
    stopListening();
}

bool ClassServer::startListening(quint16 port)
{
    if (m_tcpServer && m_tcpServer->isListening())
    {
        emit serverMessage(QString("Server already listening on port %1").arg(m_tcpServer->serverPort()));
        return true;
    }

    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &ClassServer::onNewConnection);

    if (!m_tcpServer->listen(QHostAddress::LocalHost, port))
    {
        emit serverMessage(QString("Server could not start on port %1: %2")
                               .arg(port)
                               .arg(m_tcpServer->errorString()));
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return false;
    }

    emit serverMessage(QString("Server started and listening on port %1").arg(m_tcpServer->serverPort()));
    return true;
}

void ClassServer::stopListening()
{
    if (m_tcpServer)
    {
        for (QTcpSocket *socket : std::as_const(m_clientConnections))
        {
            socket->disconnectFromHost();
            // Sockets will be deleted via disconnected slot or when m_tcpServer is deleted if parented
        }
        m_clientConnections.clear();
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
        emit serverMessage("Server stopped.");
    }
}

bool ClassServer::isListening() const
{
    return m_tcpServer && m_tcpServer->isListening();
}

quint16 ClassServer::serverPort() const
{
    return m_tcpServer ? m_tcpServer->serverPort() : 0;
}

void ClassServer::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections())
    {
        QTcpSocket *clientSocket = m_tcpServer->nextPendingConnection();
        if (clientSocket)
        {
            m_clientConnections.append(clientSocket);
            connect(clientSocket, &QTcpSocket::readyRead, this, &ClassServer::onReadyRead);
            connect(clientSocket, &QTcpSocket::disconnected, this, &ClassServer::onClientDisconnected);
            connect(clientSocket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(onSocketError(QAbstractSocket::SocketError))); // For older connect syntax with overloaded signals
            emit clientConnected(clientSocket);
            emit serverMessage(QString("Client connected: %1:%2")
                                   .arg(clientSocket->peerAddress().toString())
                                   .arg(clientSocket->peerPort()));
        }
    }
}

void ClassServer::onReadyRead()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(sender());
    if (!clientSocket)
        return;

    // It's possible that multiple lines are received in one go, or partial lines.
    // Using QTextStream can help manage line-by-line reading.
    // Ensure the socket is configured for text mode if necessary, or handle encodings explicitly.
    // For simplicity, this example assumes UTF-8 and reads lines.

    while (clientSocket->canReadLine())
    {
        QByteArray lineData = clientSocket->readLine();
        QString command = QString::fromUtf8(lineData).trimmed(); // Trim whitespace and newline chars

        if (!command.isEmpty())
        {
            emit serverMessage(QString("Received from %1:%2: %3")
                                   .arg(clientSocket->peerAddress().toString())
                                   .arg(clientSocket->peerPort())
                                   .arg(command));
            emit commandReceived(command, clientSocket);
        }
    }
}

void ClassServer::onClientDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(sender());
    if (!clientSocket)
        return;

    m_clientConnections.removeAll(clientSocket);
    emit clientDisconnected(clientSocket);
    emit serverMessage(QString("Client disconnected: %1:%2")
                           .arg(clientSocket->peerAddress().toString()) // May not be valid after disconnect
                           .arg(clientSocket->peerPort()));
    clientSocket->deleteLater(); // Schedule for deletion
}

void ClassServer::onSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(sender());
    if (clientSocket)
    {
        emit serverMessage(QString("Socket Error from %1:%2: %3 (%4)")
                               .arg(clientSocket->peerAddress().toString())
                               .arg(clientSocket->peerPort())
                               .arg(clientSocket->errorString())
                               .arg(socketError));
    }
    else
    {
        emit serverMessage(QString("Socket Error: %1").arg(socketError));
    }
}
