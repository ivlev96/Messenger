#include "requester.h"
#include "authorization/authorizationinfo.h"

using namespace Network;

Requester::Requester(const QUrl& serverUrl, QObject* parent)
	: QObject(parent)
	, m_serverUrl(serverUrl)
{

}

void Requester::onThreadStarted()
{
	m_socket = std::make_unique<QWebSocket>();

	VERIFY(connect(m_socket.get(), &QWebSocket::connected, this, &Requester::onConnected));
	VERIFY(connect(m_socket.get(), &QWebSocket::disconnected, this, &Requester::onDisconnected));
	//VERIFY(connect(m_socket.get(), qOverload<QAbstractSocket::SocketError>(&QWebSocket::error), this, &Requester::onError));
	VERIFY(connect(m_socket.get(), static_cast<void (QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this, &Requester::onError));
	VERIFY(connect(m_socket.get(), &QWebSocket::textMessageReceived, this, &Requester::onMessageReceived));

	m_socket->open(m_serverUrl);
}

Requester::~Requester()
{

}

void Requester::onConnected()
{
	while (!m_pendingMessages.empty())
	{
		const auto command = m_pendingMessages.front();
		m_pendingMessages.pop();
		sendMessage(command);
	}

	emit connected();
}

void Requester::onDisconnected()
{
}

void Requester::onError(QAbstractSocket::SocketError)
{
	emit error(m_socket->errorString());

	if (m_socket->error() != QAbstractSocket::SocketError::OperationError)
	{
		m_socket->open(m_serverUrl);
	}
}

void Requester::onMessageReceived(const QString& message)
{
	QJsonParseError error;
	const QJsonDocument doc = QJsonDocument::fromJson(QByteArray().append(message), &error);
	ASSERT(error.error == QJsonParseError::NoError);

	const QJsonObject json(doc.object());
	ASSERT(!json[Common::typeField].isNull());

	const QString type = json[Common::typeField].toString();

	if (type == Common::logInResponse)
	{
		const Common::LogInResponse response(json);
		emit logInResponse(response.person);
		return;
	}

	if (type == Common::registrationResponse)
	{
		const Common::RegistrationResponse response(json);
		emit signUpResponse(response.person);
		return;
	}

	if (type == Common::getLastMessagesResponse)
	{
		const Common::GetLastMessagesResponse response(json);
		emit getLastMessagesResponse(response.id, response.messages, response.before);
		return;
	}

	if (type == Common::sendMessagesResponse)
	{
		const Common::SendMessagesResponse response(json);
		emit sendMessagesResponse(response.messages);
		return;
	}
	
	if (type == Common::getMessagesResponse)
	{
		const Common::GetMessagesResponse response(json);
		const Common::PersonIdType myId = Authorization::AuthorizationInfo::instance().id();
		Common::PersonIdType otherId;

		if (response.id1 == myId)
		{
			otherId = response.id2;
		}
		else if (response.id2 == myId)
		{
			otherId = response.id1;
		}
		else
		{
			ASSERT(!"Incorrect ids in command");
			return;
		}

		emit getMessagesResponse(otherId, response.messages, response.before);
		return;
	}

	if (type == Common::newMessageCommand)
	{
		const Common::NewMessageCommand command(json);
		emit newMessage(command.from, command.message);
		return;
	}

	if (type == Common::findFriendResponse)
	{
		const Common::FindFriendResponse response(json);
		emit possibleFriendsList(response.persons);
		return;
	}

	ASSERT(!"Not implemented");
}

void Requester::onLogIn(const QString& login, const QString& password)
{
	const QJsonObject json = Common::LogInRequest(login, password).toJson();

	sendMessage(json);
}

void Requester::onSignUp(
	const QString& firstName,
	const QString& lastName,
	const QString& avatarUrl,
	const QString& login,
	const QString& password)
{
	const Common::RegistrationRequest request(firstName, lastName, avatarUrl, login, password);
	const QJsonObject json = request.toJson();
	
	sendMessage(json);
}

void Requester::onGetMessages(Common::PersonIdType otherId, int count,
	const std::optional<Common::MessageIdType>& before)
{
	const Common::PersonIdType myId = Authorization::AuthorizationInfo::instance().id();
	const QJsonObject json = Common::GetMessagesRequest(myId, otherId, count, before).toJson();

	sendMessage(json);
}

void Requester::onGetLastMessages(int count, const std::optional<Common::MessageIdType>& before)
{
	const Common::PersonIdType myId = Authorization::AuthorizationInfo::instance().id();
	const QJsonObject json = Common::GetLastMessagesRequest(myId, count, before).toJson();

	sendMessage(json);
}

void Requester::onSendMessages(const std::vector<Common::Message>& messages)
{
	const QJsonObject json = Common::SendMessagesRequest(messages).toJson();

	sendMessage(json);
}

void Requester::onFindFriends(const QString& name, bool withMessages, bool withoutMessages)
{
	if (name.isEmpty())
	{
		return;
	}

	const QJsonObject json = 
		Common::FindFriendRequest(
			Authorization::AuthorizationInfo::instance().id(), 
			name, 
			withMessages, 
			withoutMessages,
			{},
			Common::defaultPersonsCount
		).toJson();

	sendMessage(json);
}

void Requester::sendMessage(const QJsonObject& json)
{
	const QJsonDocument doc(json);
	const QString message = doc.toJson();

	if (!m_socket->isValid())
	{
		m_socket->open(m_serverUrl);
		m_pendingMessages.push(json);
		return;
	}
	m_socket->sendTextMessage(message);
	m_socket->flush();
}
