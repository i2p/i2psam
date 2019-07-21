// Copyright (c) 2017 The I2P Project
// Copyright (c) 2013-2015 The Anoncoin Core developers
// Copyright (c) 2012-2013 giv
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------

#include "i2psam.h"

#include <iostream>
#include <stdio.h>
#include <string.h>         // for memset
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

// Was 65536, seemed unnecessarily large
#define SAM_BUFSIZE         4096
#define I2P_DESTINATION_SIZE 516

// Define this, if you want more of the original standard output diagnostics
// #define DEBUG_ON_STDOUT

namespace SAM
{

static void print_error(const std::string& err)
{
#ifdef DEBUG_ON_STDOUT
#ifdef WIN32
    std::cout << err << "(" << WSAGetLastError() << ")" << std::endl;
#else
    std::cout << err << "(" << errno << ")" << std::endl;
#endif
#endif // DEBUG_ON_STDOUT
}

#ifdef WIN32
int I2pSocket::instances_ = 0;

void I2pSocket::initWSA()
{
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR)
        print_error("Failed to initialize winsock library");
}

void I2pSocket::freeWSA()
{
    WSACleanup();
}
#endif

I2pSocket::I2pSocket(const std::string& SAMHost, uint16_t SAMPort)
    : socket_(INVALID_SOCKET), SAMHost_(SAMHost), SAMPort_(SAMPort)
{
#ifdef WIN32
    if (instances_++ == 0)
        initWSA();
#endif

    memset(&servAddr_, 0, sizeof(servAddr_));

    servAddr_.sin_family = AF_INET;
    servAddr_.sin_addr.s_addr = inet_addr(SAMHost.c_str());
    servAddr_.sin_port = htons(SAMPort);
    bootstrapI2P();
}

I2pSocket::I2pSocket(const sockaddr_in& addr)
    : socket_(INVALID_SOCKET), servAddr_(addr)
{
#ifdef WIN32
    if (instances_++ == 0)
        initWSA();
#endif
    bootstrapI2P();
}

I2pSocket::I2pSocket(const I2pSocket& rhs)
    : socket_(INVALID_SOCKET), servAddr_(rhs.servAddr_)
{
#ifdef WIN32
    if (instances_++ == 0)
        initWSA();
#endif
    bootstrapI2P();
}

I2pSocket::~I2pSocket()
{
    close();

#ifdef WIN32
    if (--instances_ == 0)
        freeWSA();
#endif
}

void I2pSocket::bootstrapI2P()
{
    init();
    if (isOk())
        handshake();
}

void I2pSocket::init()
{
    // Here is where the only real OS socket is called for creation to talk with the router,
    // the value returned is stored in our variable socket_  which holds the connection
    // to our stream for everything else
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET)
    {
        print_error("Failed to create socket");
        return;
    }

    if (connect(socket_, (const sockaddr*)&servAddr_, sizeof(servAddr_)) == SOCKET_ERROR)
    {
        close();
        print_error("Failed to connect to SAM");
        return;
    }
}

SOCKET I2pSocket::release()
{
    SOCKET temp = socket_;
    socket_ = INVALID_SOCKET;
    return temp;
}

// If the handshake works, we're talking to a valid I2P router.
void I2pSocket::handshake()
{
    this->write(Message::hello(minVer_, maxVer_));
    const std::string answer = this->read();
    const Message::eStatus answerStatus = Message::checkAnswer(answer);
    if (answerStatus == Message::OK)
        version_ = Message::getValue(answer, "VERSION");
    else
        print_error("Handshake failed");
}

void I2pSocket::write(const std::string& msg)
{
    if (!isOk())
    {
        print_error("Failed to send data because socket is closed");
        return;
    }
#ifdef DEBUG_ON_STDOUT
    std::cout << "Send: " << msg << std::endl;
#endif
    ssize_t sentBytes = send(socket_, msg.c_str(), msg.length(), 0);
    if (sentBytes == SOCKET_ERROR)
    {
        close();
        print_error("Failed to send data");
        return;
    }
    if (sentBytes == 0)
    {
        close();
        print_error("I2pSocket was closed");
        return;
    }
}

std::string I2pSocket::read()
{
    if (!isOk())
    {
        print_error("Failed to read data because socket is closed");
        return std::string();
    }
    char buffer[SAM_BUFSIZE];
    memset(buffer, 0, SAM_BUFSIZE);
    ssize_t recievedBytes = recv(socket_, buffer, SAM_BUFSIZE, 0);
    if (recievedBytes == SOCKET_ERROR)
    {
        close();
        print_error("Failed to receive data");
        return std::string();
    }
    if (recievedBytes == 0)
    {
        close();
        print_error("I2pSocket was closed");
    }
#ifdef DEBUG_ON_STDOUT
    std::cout << "Reply: " << buffer << std::endl;
#endif
    return std::string(buffer);
}

void I2pSocket::close()
{
    if (socket_ != INVALID_SOCKET) {
#ifdef WIN32
        ::closesocket(socket_);
#else
        ::close(socket_);
#endif
        socket_ = INVALID_SOCKET;
    }
}

bool I2pSocket::isOk() const
{
    return socket_ != INVALID_SOCKET;
}

const std::string& I2pSocket::getHost() const
{
    return SAMHost_;
}

uint16_t I2pSocket::getPort() const
{
    return SAMPort_;
}

const std::string& I2pSocket::getVersion() const
{
    return version_;
}

const sockaddr_in& I2pSocket::getAddress() const
{
    return servAddr_;
}


//--------------------------------------------------------------------------------------------------

StreamSession::StreamSession(
        const std::string& nickname,
        const std::string& SAMHost     /*= SAM_DEFAULT_ADDRESS*/,
              uint16_t     SAMPort     /*= SAM_DEFAULT_PORT*/,
        const std::string& destination /*= SAM_GENERATE_MY_DESTINATION*/,
        const std::string& i2pOptions  /*= SAM_DEFAULT_I2P_OPTIONS*/,
        const std::string& signatureType /*= SAM_SIGNATURE_TYPE */ )
    : socket_(SAMHost, SAMPort)
    , nickname_(nickname)
    , sessionID_(generateSessionID())
    , i2pOptions_(i2pOptions)
    , isSick_(false)
{
    myDestination_ = createStreamSession(destination);
#ifdef DEBUG_ON_STDOUT
    std::cout << "Created a brand new SAM session (" << sessionID_ << ")" << std::endl;
#endif
}

StreamSession::StreamSession(StreamSession& rhs)
    : socket_(rhs.socket_)
    , nickname_(rhs.nickname_)
    , sessionID_(generateSessionID())
    , myDestination_(rhs.myDestination_)
    , i2pOptions_(rhs.i2pOptions_)
    , isSick_(false)
{
    rhs.fallSick();
    rhs.socket_.close();
    (void)createStreamSession(myDestination_.priv);

    for(ForwardedStreamsContainer::const_iterator it = rhs.forwardedStreams_.begin(), end = rhs.forwardedStreams_.end(); it != end; ++it)
        forward(it->host, it->port, it->silent);

#ifdef DEBUG_ON_STDOUT
    std::cout << "Created a new SAM session (" << sessionID_ << ")  from another (" << rhs.sessionID_ << ")" << std::endl;
#endif
}

StreamSession::~StreamSession()
{
    stopForwardingAll();
#ifdef DEBUG_ON_STDOUT
    std::cout << "Closing SAM session (" << sessionID_ << ") ..." << std::endl;
#endif
}

/*static*/
std::string StreamSession::generateSessionID()
{
    static const int minSessionIDLength = 5;
    static const int maxSessionIDLength = 9;
    static const char sessionIDAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int length = minSessionIDLength - 1;
    std::string result;

    srand(time(NULL));

    while(length < minSessionIDLength)
        length = rand() % maxSessionIDLength;

    while (length-- > 0)
        result += sessionIDAlphabet[rand() % (sizeof(sessionIDAlphabet)-1)];

#ifdef DEBUG_ON_STDOUT
    std::cout << "Generated session ID: " << result << std::endl;
#endif
    return result;
}

RequestResult<std::unique_ptr<I2pSocket> > StreamSession::accept(bool silent)
{
    typedef RequestResult<std::unique_ptr<I2pSocket> > ResultType;

    std::unique_ptr<I2pSocket> streamSocket(new I2pSocket(socket_));
    const Message::eStatus status = accept(*streamSocket, sessionID_, silent);
    switch(status)
    {
    case Message::OK:
        return ResultType(std::move(streamSocket));
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
    case Message::INVALID_ID:
    case Message::I2P_ERROR:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

RequestResult<std::unique_ptr<I2pSocket> > StreamSession::connect(const std::string& destination, bool silent)
{
    typedef RequestResult<std::unique_ptr<I2pSocket> > ResultType;

    std::unique_ptr<I2pSocket> streamSocket(new I2pSocket(socket_));
    const Message::eStatus status = connect(*streamSocket, sessionID_, destination, silent);
    switch(status)
    {
    case Message::OK:
        return ResultType(std::move(streamSocket));
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
    case Message::INVALID_ID:
    case Message::I2P_ERROR:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

RequestResult<void> StreamSession::forward(const std::string& host, uint16_t port, bool silent)
{
    typedef RequestResult<void> ResultType;

    std::unique_ptr<I2pSocket> newSocket(new I2pSocket(socket_));
    const Message::eStatus status = forward(*newSocket, sessionID_, host, port, silent);
    switch(status)
    {
    case Message::OK:
        forwardedStreams_.push_back(ForwardedStream(newSocket.get(), host, port, silent));
        newSocket.release();    // release after successful push_back only
        return ResultType(true);
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
    case Message::INVALID_ID:
    case Message::I2P_ERROR:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

RequestResult<const std::string> StreamSession::namingLookup(const std::string& name) const
{
    typedef RequestResult<const std::string> ResultType;
    typedef Message::Answer<const std::string> AnswerType;

    std::unique_ptr<I2pSocket> newSocket(new I2pSocket(socket_));
    const AnswerType answer = namingLookup(*newSocket, name);
    switch(answer.status)
    {
    case Message::OK:
        return ResultType(answer.value);
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

RequestResult<const FullDestination> StreamSession::destGenerate() const
{
    typedef RequestResult<const FullDestination> ResultType;
    typedef Message::Answer<const FullDestination> AnswerType;

    std::unique_ptr<I2pSocket> newSocket(new I2pSocket(socket_));
    const AnswerType answer = destGenerate(*newSocket);
    switch(answer.status)
    {
    case Message::OK:
        return ResultType(answer.value);
    case Message::EMPTY_ANSWER:
    case Message::CLOSED_SOCKET:
        fallSick();
        break;
    default:
        break;
    }
    return ResultType();
}

FullDestination StreamSession::createStreamSession(const std::string& destination)
{
    typedef Message::Answer<const std::string> AnswerType;

    const AnswerType answer = createStreamSession(socket_, sessionID_, nickname_, destination, i2pOptions_, SAM_SIGNATURE_TYPE);
    if (answer.status != Message::OK)
    {
        fallSick();
        return FullDestination();
    }
    return FullDestination(answer.value.substr(0, I2P_DESTINATION_SIZE), answer.value, (destination == SAM_GENERATE_MY_DESTINATION));
}

void StreamSession::fallSick() const
{
    isSick_ = true;
}

void StreamSession::stopForwarding(const std::string& host, uint16_t port)
{
    for (ForwardedStreamsContainer::iterator it = forwardedStreams_.begin(); it != forwardedStreams_.end(); )
    {
        if (it->port == port && it->host == host)
        {
            delete (it->socket);
            it = forwardedStreams_.erase(it);
        }
        else
            ++it;
    }
}

void StreamSession::stopForwardingAll()
{
    for (ForwardedStreamsContainer::iterator it = forwardedStreams_.begin(); it != forwardedStreams_.end(); ++it)
        delete (it->socket);
    forwardedStreams_.clear();
    socket_.close();
}

/*static*/
Message::Answer<const std::string> StreamSession::rawRequest(I2pSocket& socket, const std::string& requestStr)
{
    typedef Message::Answer<const std::string> AnswerType;

    if (!socket.isOk())
        return AnswerType(Message::CLOSED_SOCKET);
    socket.write(requestStr);
    const std::string answer = socket.read();
    const Message::eStatus status = Message::checkAnswer(answer);
    return AnswerType(status, answer);
}

/*static*/
Message::Answer<const std::string> StreamSession::request(I2pSocket& socket, const std::string& requestStr, const std::string& keyOnSuccess)
{
    typedef Message::Answer<const std::string> AnswerType;

    const AnswerType answer = rawRequest(socket, requestStr);
    return (answer.status == Message::OK) ?
                AnswerType(answer.status, Message::getValue(answer.value, keyOnSuccess)) :
                answer;
}

/*static*/
Message::eStatus StreamSession::request(I2pSocket& socket, const std::string& requestStr)
{
    return rawRequest(socket, requestStr).status;
}

/*static*/
Message::Answer<const std::string> StreamSession::createStreamSession(I2pSocket& socket, const std::string& sessionID, const std::string& nickname, const std::string& destination, const std::string& options, const std::string& signatureType)
{
    return request(socket, Message::sessionCreate(Message::sssStream, sessionID, nickname, destination, options, signatureType), "DESTINATION");
}

/*static*/
Message::Answer<const std::string> StreamSession::namingLookup(I2pSocket& socket, const std::string& name)
{
    return request(socket, Message::namingLookup(name), "VALUE");
}

/*static*/
Message::Answer<const FullDestination> StreamSession::destGenerate(I2pSocket& socket)
{
// while answer for a DEST GENERATE request doesn't contain a "RESULT" field we parse it manually
    typedef Message::Answer<const FullDestination> ResultType;

    if (!socket.isOk())
        return ResultType(Message::CLOSED_SOCKET, FullDestination());
    socket.write(Message::destGenerate());
    const std::string answer = socket.read();
    const std::string pub = Message::getValue(answer, "PUB");
    const std::string priv = Message::getValue(answer, "PRIV");
    return (!pub.empty() && !priv.empty()) ? ResultType(Message::OK, FullDestination(pub, priv, /*isGenerated*/ true)) : ResultType(Message::EMPTY_ANSWER, FullDestination());
}

/*static*/
Message::eStatus StreamSession::accept(I2pSocket& socket, const std::string& sessionID, bool silent)
{
    return request(socket, Message::streamAccept(sessionID, silent));
}

/*static*/
Message::eStatus StreamSession::connect(I2pSocket& socket, const std::string& sessionID, const std::string& destination, bool silent)
{
    return request(socket, Message::streamConnect(sessionID, destination, silent));
}

/*static*/
Message::eStatus StreamSession::forward(I2pSocket& socket, const std::string& sessionID, const std::string& host, uint16_t port, bool silent)
{
    return request(socket, Message::streamForward(sessionID, host, port, silent));
}

const std::string& StreamSession::getNickname() const
{
#ifdef DEBUG_ON_STDOUT
    std::cout << "getNickname: " << nickname_ << std::endl;
#endif // DEBUG_ON_STDOUT
    return nickname_;
}

const std::string& StreamSession::getSessionID() const
{
#ifdef DEBUG_ON_STDOUT
    std::cout << "getSessionID: " << sessionID_ << std::endl;
#endif // DEBUG_ON_STDOUT
    return sessionID_;
}

const std::string& StreamSession::getOptions() const
{
#ifdef DEBUG_ON_STDOUT
    std::cout << "getOptions: " << i2pOptions_ << std::endl;
#endif // DEBUG_ON_STDOUT
    return i2pOptions_;
}

const FullDestination& StreamSession::getMyDestination() const
{
#ifdef DEBUG_ON_STDOUT
    std::cout << "getMyDestination: " << myDestination_.priv << std::endl;
#endif // DEBUG_ON_STDOUT
    return myDestination_;
}

bool StreamSession::isSick() const
{
#ifdef DEBUG_ON_STDOUT
    std::cout << "isSick: " << isSick_ << std::endl;
#endif // DEBUG_ON_STDOUT
    return isSick_;
}

const sockaddr_in& StreamSession::getSAMAddress() const
{
#ifdef DEBUG_ON_STDOUT
    // std::cout << "getSAMAddress: " << socket_.getAddress() << std::endl;
    std::cout << "getSAMAddress: not yet parsed" << std::endl;
#endif // DEBUG_ON_STDOUT
    return socket_.getAddress();
}

const std::string& StreamSession::getSAMHost() const
{
#ifdef DEBUG_ON_STDOUT
    std::cout << "getSAMHost: " << socket_.getHost() << std::endl;
#endif // DEBUG_ON_STDOUT
    return socket_.getHost();
}

uint16_t StreamSession::getSAMPort() const
{
#ifdef DEBUG_ON_STDOUT
    std::cout << "getSAMPort: " << socket_.getPort() << std::endl;
#endif // DEBUG_ON_STDOUT
    return socket_.getPort();
}

const std::string& StreamSession::getSAMVersion() const
{
#ifdef DEBUG_ON_STDOUT
    std::cout << "getSAMVersion: " << socket_.getVersion() << std::endl;
#endif // DEBUG_ON_STDOUT
    return socket_.getVersion();
}


const std::string& StreamSession::getSAMMinVer() const
{
  return socket_.minVer_;
}

const std::string& StreamSession::getSAMMaxVer() const
{
  return socket_.maxVer_;
}

//--------------------------------------------------------------------------------------------------


std::string Message::createSAMRequest(const char* format, ...)
{
    char buffer[SAM_BUFSIZE];
    // ToDo: GR note: Creating a 65K byte buffer on the stack, and then wasting the time to zero it out
    //                before using it.  Just to send a 30 byte string?, seems really wasteful to me, time more than storage, many mSec...
    memset(buffer, 0, SAM_BUFSIZE);

    va_list args;
    va_start (args, format);
    const int sizeToSend = vsnprintf(buffer, SAM_BUFSIZE, format, args);
    va_end(args);

    if (sizeToSend < 0)
    {
        print_error("Failed to format message");
        return std::string();
    }

#ifdef DEBUG_ON_STDOUT
    std::cout << "Buffer: " << buffer << std::endl;
#endif // DEBUG_ON_STDOUT

    return std::string(buffer);
}

std::string Message::hello(const std::string &minVer, const std::string &maxVer)
{
///////////////////////////////////////////////////////////
//
//    ->  HELLO VERSION
//              MIN=$min
//              MAX=$max
//
//    <-  HELLO REPLY
//              RESULT=OK
//              VERSION=$version
//
///////////////////////////////////////////////////////////

    static const char* helloFormat = "HELLO VERSION MIN=%s MAX=%s\n";
    return createSAMRequest(helloFormat, minVer.c_str(), maxVer.c_str());
}

std::string Message::sessionCreate(SessionStyle style, const std::string& sessionID, const std::string& nickname, const std::string& destination /*= SAM_GENERATE_MY_DESTINATION*/, const std::string& options /*= ""*/, const std::string& signatureType /*= SAM_SIGNATURE_TYPE*/)
{
///////////////////////////////////////////////////////////
//
//    ->  SESSION CREATE
//              STYLE={STREAM,DATAGRAM,RAW}
//              ID={$nickname}
//              DESTINATION={$private_destination_key,TRANSIENT}
//              [option=value]*
//
//    <-  SESSION STATUS
//              RESULT=OK
//              DESTINATION=$private_destination_key
//
///////////////////////////////////////////////////////////

    std::string sessionStyle;
    switch(style)
    {
    case sssStream:   sessionStyle = "STREAM";   break;
    case sssDatagram: sessionStyle = "DATAGRAM"; break;
    case sssRaw:      sessionStyle = "RAW";      break;
    }

    static const char* sessionCreateFormat = "SESSION CREATE STYLE=%s ID=%s DESTINATION=%s SIGNATURE_TYPE=%s inbound.nickname=%s %s\n";  // we add inbound.nickname option
    return createSAMRequest(sessionCreateFormat, sessionStyle.c_str(), sessionID.c_str(), destination.c_str(), signatureType.c_str(), nickname.c_str(), options.c_str());
}

std::string Message::streamAccept(const std::string& sessionID, bool silent /*= false*/)
{
///////////////////////////////////////////////////////////
//
//    ->  STREAM ACCEPT
//             ID={$nickname}
//             [SILENT={true,false}]
//
//    <-  STREAM STATUS
//             RESULT=$result
//             [MESSAGE=...]
//
///////////////////////////////////////////////////////////

    static const char* streamAcceptFormat = "STREAM ACCEPT ID=%s SILENT=%s\n";
    return createSAMRequest(streamAcceptFormat, sessionID.c_str(), silent ? "true" : "false");
}

std::string Message::streamConnect(const std::string& sessionID, const std::string& destination, bool silent /*= false*/)
{
///////////////////////////////////////////////////////////
//
//    ->  STREAM CONNECT
//             ID={$nickname}
//             DESTINATION=$peer_public_base64_key
//             [SILENT={true,false}]
//
//    <-  STREAM STATUS
//             RESULT=$result
//             [MESSAGE=...]
//
///////////////////////////////////////////////////////////

    static const char* streamConnectFormat = "STREAM CONNECT ID=%s DESTINATION=%s SILENT=%s\n";
    return createSAMRequest(streamConnectFormat, sessionID.c_str(), destination.c_str(), silent ? "true" : "false");
}

std::string Message::streamForward(const std::string& sessionID, const std::string& host, uint16_t port, bool silent /*= false*/)
{
///////////////////////////////////////////////////////////
//
//    ->  STREAM FORWARD
//             ID={$nickname}
//             PORT={$port}
//             [HOST={$host}]
//             [SILENT={true,false}]
//
//    <-  STREAM STATUS
//             RESULT=$result
//             [MESSAGE=...]
//
///////////////////////////////////////////////////////////
    static const char* streamForwardFormat = "STREAM FORWARD ID=%s PORT=%u HOST=%s SILENT=%s\n";
    return createSAMRequest(streamForwardFormat, sessionID.c_str(), (unsigned)port, host.c_str(), silent ? "true" : "false");
}

std::string Message::namingLookup(const std::string& name)
{
///////////////////////////////////////////////////////////
//
//    -> NAMING LOOKUP
//            NAME=$name
//
//    <- NAMING REPLY
//            RESULT=OK
//            NAME=$name
//            VALUE=$base64key
//
///////////////////////////////////////////////////////////

    static const char* namingLookupFormat = "NAMING LOOKUP NAME=%s\n";
    return createSAMRequest(namingLookupFormat, name.c_str());
}

std::string Message::destGenerate()
{
///////////////////////////////////////////////////////////
//
//    -> DEST GENERATE
//
//    <- DEST REPLY
//            PUB=$pubkey
//            PRIV=$privkey
//
///////////////////////////////////////////////////////////

    static const char* destGenerateFormat = "DEST GENERATE\n";
    return createSAMRequest(destGenerateFormat);
}

#define SAM_MAKESTRING(X) SAM_MAKESTRING2(X)
#define SAM_MAKESTRING2(X) #X

#define SAM_CHECK_RESULT(value) \
    if (result == SAM_MAKESTRING(value)) return value

Message::eStatus Message::checkAnswer(const std::string& answer)
{
    if (answer.empty())
        return EMPTY_ANSWER;

    const std::string result = getValue(answer, "RESULT");

    SAM_CHECK_RESULT(OK);
    SAM_CHECK_RESULT(DUPLICATED_DEST);
    SAM_CHECK_RESULT(DUPLICATED_ID);
    SAM_CHECK_RESULT(I2P_ERROR);
    SAM_CHECK_RESULT(INVALID_ID);
    SAM_CHECK_RESULT(INVALID_KEY);
    SAM_CHECK_RESULT(CANT_REACH_PEER);
    SAM_CHECK_RESULT(TIMEOUT);
    SAM_CHECK_RESULT(NOVERSION);
    SAM_CHECK_RESULT(KEY_NOT_FOUND);
    SAM_CHECK_RESULT(PEER_NOT_FOUND);
    SAM_CHECK_RESULT(ALREADY_ACCEPTING);

    return CANNOT_PARSE_ERROR;
}

#undef SAM_CHECK_RESULT
#undef SAM_MAKESTRING2
#undef SAM_MAKESTRING

std::string Message::getValue(const std::string& answer, const std::string& key)
{
    if (key.empty())
        return std::string();

    const std::string keyPattern = key + "=";
    size_t valueStart = answer.find(keyPattern);
    if (valueStart == std::string::npos)
        return std::string();

    valueStart += keyPattern.length();
    size_t valueEnd = answer.find_first_of(' ', valueStart);
    if (valueEnd == std::string::npos)
        valueEnd = answer.find_first_of('\n', valueStart);
    return answer.substr(valueStart, valueEnd - valueStart);
}


} // namespace SAM


// C interface

extern "C" {

#include "i2psam-c.h"

  struct i2psam_stream_session
  {
    SAM::StreamSession * impl = nullptr;
  };

  struct i2psam_socket
  {
    std::unique_ptr<SAM::I2pSocket> impl;
  };

  struct i2psam_destination
  {
    char * pub;
    char * priv;
  };

  struct i2psam_stream_session * i2psam_stream_session_new(struct i2psam_stream_settings * settings)
  {
    struct i2psam_stream_session * session = new i2psam_stream_session;
    session->impl = new SAM::StreamSession(settings->nickname,
                                           settings->samhost,
                                           settings->samport,
                                           settings->destination,
                                           settings->i2cp_opts);
    return session;
  }

  void i2psam_stream_session_free(struct i2psam_stream_session * session)
  {
    delete session->impl;
    delete session;
  }

  const char * i2psam_get_samhost(struct i2psam_stream_session * session)
  {
    return strdup(session->impl->getSAMHost().c_str());
  }

  uint16_t i2psam_get_samport(struct i2psam_stream_session * session)
  {
    return session->impl->getSAMPort();
  }

  const char * i2psam_get_nickname(struct i2psam_stream_session * session)
  {
    return strdup(session->impl->getNickname().c_str());
  }

  const char * i2psam_get_session_id(struct i2psam_stream_session * session)
  {
    return strdup(session->impl->getSessionID().c_str());
  }

  const char * i2psam_get_sam_min_version(struct i2psam_stream_session * session)
  {
    return strdup(session->impl->getSAMMinVer().c_str());
  }

  const char * i2psam_get_sam_max_version(struct i2psam_stream_session * session)
  {
    return strdup(session->impl->getSAMMaxVer().c_str());
  }

  const char * i2psam_get_sam_version(struct i2psam_stream_session * session)
  {
    return strdup(session->impl->getSAMVersion().c_str());
  }

  const char * i2psam_get_i2cp_options(struct i2psam_stream_session * session)
  {
    return strdup(session->impl->getOptions().c_str());
  }

  int i2psam_is_sick(struct i2psam_stream_session * session)
  {
    if(session->impl->isSick())
      return 1;
    else
      return 0;
  }

  struct i2psam_socket * i2psam_accept(struct i2psam_stream_session * session, int silent)
  {
    auto result = session->impl->accept(silent);
    if (result.isOk)
    {
      struct i2psam_socket * socket = new i2psam_socket;
      socket->impl = std::move(result.value);
      return socket;
    }
    else
      return nullptr;
  }

  struct i2psam_socket * i2psam_connect(struct i2psam_stream_session * session, const char * destination, int silent)
  {
    auto result = session->impl->connect(destination, silent != 0);
    if(result.isOk)
    {
      struct i2psam_socket * socket = new i2psam_socket;
      socket->impl = std::move(result.value);
      return socket;
    }
    else
      return nullptr;
  }

  int i2psam_forward(struct i2psam_stream_session * session, const char * host, uint16_t port, int silent)
  {
    std::string remote(host);
    auto result = session->impl->forward(host, port, silent);

    if(result.isOk) return 0;
    else return -1;
  }

  const char * i2psam_namelookup(struct i2psam_stream_session * session, const char * name)
  {
    auto result = session->impl->namingLookup(name);

    if(result.isOk) return strdup(result.value.c_str());
    else return nullptr;
  }

  struct i2psam_destination * i2psam_dest_generate(struct i2psam_stream_session * session)
  {
    auto result = session->impl->destGenerate();
    if(result.isOk)
    {
      struct i2psam_destination * dest = new i2psam_destination;
      dest->pub = strdup(result.value.pub.c_str());
      dest->priv = strdup(result.value.priv.c_str());
      return dest;
    }
    else
      return nullptr;
  }

  void i2psam_stop_forwarding(struct i2psam_stream_session * session, const char * host, uint16_t port)
  {
    session->impl->stopForwarding(host, port);
  }

  void i2psam_stop_forwarding_all(struct i2psam_stream_session * session)
  {
    session->impl->stopForwardingAll();
  }

  struct i2psam_destination * i2psam_get_my_destination(struct i2psam_stream_session * session)
  {
    struct i2psam_destination * dest = new i2psam_destination;
    const auto & mydest = session->impl->getMyDestination();
    dest->pub = strdup(mydest.pub.c_str());
    dest->priv = strdup(mydest.priv.c_str());
    return dest;
  }

  void i2psam_write(struct i2psam_socket * sock, const char * data, size_t dlen)
  {
    const std::string buf(data, dlen);
    sock->impl->write(buf);
  }

  char * i2psam_read(struct i2psam_socket * sock, size_t * dlen)
  {
    std::string buf = sock->impl->read();
    size_t sz = sizeof(char) * buf.size();
    *dlen = sz;
    if(sz)
    {
      char * ret = (char*) malloc(sz);
      memcpy(ret, buf.c_str(), sz);
      return ret;
    }
    else
      return nullptr;
  }

  void i2psam_socket_close(struct i2psam_socket *sock)
  {
    sock->impl->close();
  }

  int i2psam_socket_is_ok(struct i2psam_socket * sock)
  {
    if(sock->impl->isOk())
      return 1;
    else
      return 0;
  }

  void i2psam_socket_free(struct i2psam_socket * sock)
  {
    delete sock;
  }

  const char * i2psam_destination_priv(struct i2psam_destination * dest)
  {
    return strdup(dest->priv);
  }

  const char * i2psam_destination_pub(struct i2psam_destination * dest)
  {
    return strdup(dest->pub);
  }

  void i2psam_destination_free(struct i2psam_destination * dest)
  {
    free(dest->pub);
    free(dest->priv);
    delete dest;
  }

}
