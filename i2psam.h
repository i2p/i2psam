// Copyright (c) 2012-2013 giv
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------
// see full documentation at http://www.i2p2.i2p/samv3.html
#ifndef I2PSAM_H
#define I2PSAM_H

#include <string>
#include <list>
#include <stdint.h>
#include <memory>
#include <utility>

#ifdef WIN32
//#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN 1
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>     // for sockaddr_in
#include <arpa/inet.h>      // for ntohs and htons
#endif

#define SAM_INVALID_SOCKET      (-1)
#define SAM_SOCKET_ERROR        (-1)

#define SAM_DEFAULT_ADDRESS     "127.0.0.1"
#define SAM_DEFAULT_PORT        7656
#define SAM_DEFAULT_MIN_VER     "3.0"
#define SAM_DEFAULT_MAX_VER     "3.0"
#define SAM_GENERATE_MY_DESTINATION     "TRANSIENT"
#define SAM_MY_NAME             "ME"

namespace SAM
{

typedef int SOCKET;

class Message
{
public:
    enum SessionStyle
    {
        sssStream,
        sssDatagram,
        sssRaw
    };

    enum eStatus
    {
        OK,
        EMPTY_ANSWER,
        CLOSED_SOCKET,
        CANNOT_PARSE_ERROR,

    // The destination is already in use
    //
    // ->  SESSION CREATE ...
    // <-  SESSION STATUS RESULT=DUPLICATED_DEST
        DUPLICATED_DEST,

    // The nickname is already associated with a session
    //
    // ->  SESSION CREATE ...
    // <-  SESSION STATUS RESULT=DUPLICATED_ID
        DUPLICATED_ID,

    // A generic I2P error (e.g. I2CP disconnection, etc.)
    //
    // ->  HELLO VERSION ...
    // <-  HELLO REPLY RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  SESSION CREATE ...
    // <-  SESSION STATUS RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  STREAM ACCEPT ...
    // <-  STREAM STATUS RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  STREAM FORWARD ...
    // <-  STREAM STATUS RESULT=I2P_ERROR MESSAGE={$message}
    //
    // ->  NAMING LOOKUP ...
    // <-  NAMING REPLY RESULT=INVALID_KEY NAME={$name} MESSAGE={$message}
        I2P_ERROR,

    // Stream session ID doesn't exist
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=INVALID_ID MESSAGE={$message}
    //
    // ->  STREAM ACCEPT ...
    // <-  STREAM STATUS RESULT=INVALID_ID MESSAGE={$message}
    //
    // ->  STREAM FORWARD ...
    // <-  STREAM STATUS RESULT=INVALID_ID MESSAGE={$message}
        INVALID_ID,

    // The destination is not a valid private destination key
    //
    // ->  SESSION CREATE ...
    // <-  SESSION STATUS RESULT=INVALID_KEY MESSAGE={$message}
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=INVALID_KEY MESSAGE={$message}
    //
    // ->  NAMING LOOKUP ...
    // <-  NAMING REPLY RESULT=INVALID_KEY NAME={$name} MESSAGE={$message}
        INVALID_KEY,

    // The peer exists, but cannot be reached
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=CANT_REACH_PEER MESSAGE={$message}
        CANT_REACH_PEER,

    // Timeout while waiting for an event (e.g. peer answer)
    //
    // ->  STREAM CONNECT ...
    // <-  STREAM STATUS RESULT=TIMEOUT MESSAGE={$message}
        TIMEOUT,

    // The SAM bridge cannot find a suitable version
    //
    // ->  HELLO VERSION ...
    // <-  HELLO REPLY RESULT=NOVERSION MESSAGE={$message}
        NOVERSION,

    // The naming system can't resolve the given name
    //
    // ->  NAMING LOOKUP ...
    // <-  NAMING REPLY RESULT=INVALID_KEY NAME={$name} MESSAGE={$message}
        KEY_NOT_FOUND,

    // The peer cannot be found on the network
    //
    // ??
        PEER_NOT_FOUND,

    // ??
    //
    // ->  STREAM ACCEPT
    // <-  STREAM STATUS RESULT=ALREADY_ACCEPTING
        ALREADY_ACCEPTING,

        // ??
        FAILED,
        // ??
        CLOSED
    };

    typedef std::pair<const Message::eStatus, const std::string> Result;

    static std::string hello(const std::string& minVer, const std::string& maxVer);
    static std::string sessionCreate(SessionStyle style, const std::string& sessionID, const std::string& nickname, const std::string& destination = SAM_GENERATE_MY_DESTINATION, const std::string& options = "");
    static std::string streamAccept(const std::string& sessionID, bool silent = false);
    static std::string streamConnect(const std::string& sessionID, const std::string& destination, bool silent = false);
    static std::string streamForward(const std::string& sessionID, const std::string& host, uint16_t port, bool silent = false);

    static std::string namingLookup(const std::string& name);
    static std::string destGenerate();

    static eStatus checkAnswer(const std::string& answer);
    static std::string getValue(const std::string& answer, const std::string& key);
private:
    static std::string createSAMRequest(const char* format, ...);
};

class Socket
{
private:
    SOCKET socket_;
    sockaddr_in servAddr_;
    std::string SAMHost_;
    uint16_t SAMPort_;
    const std::string minVer_;
    const std::string maxVer_;
    std::string version_;

#ifdef WIN32
    static int instances_;
    static void initWSA();
    static void freeWSA();
#endif

    void handshake();
    void init();

    Socket& operator=(const Socket&);
public:
    Socket(const std::string& SAMHost, uint16_t SAMPort, const std::string &minVer, const std::string& maxVer);
    Socket(const sockaddr_in& addr, const std::string& minVer, const std::string& maxVer);
    Socket(const Socket& rhs); // creates a new socket with the same parameters
    ~Socket();

    void write(const std::string& msg);
    std::string read();
    SOCKET release();
    void close();

    bool isOk() const;

    const std::string& getVersion() const;
    const std::string& getHost() const;
    uint16_t getPort() const;
    const std::string& getMinVer() const;
    const std::string& getMaxVer() const;

    const sockaddr_in& getAddress() const;
};

class StreamSession
{
private:
    /*mutable*/ std::auto_ptr<Socket> socket_;
    std::string nickname_;
    std::string sessionID_;
    std::string myDestination_;

    struct ForwardedStream
    {
        Socket* socket;
        std::string host;
        uint16_t port;
        bool silent;
    };

    typedef std::list<ForwardedStream> ForwardedStreamsContainer;
    ForwardedStreamsContainer forwardedStreams_;

    bool createStreamSession(std::auto_ptr<Socket>& newSocket, const std::string& nickname, const std::string& myDestination = SAM_GENERATE_MY_DESTINATION);
    bool reforwardAll();

    static Message::Result request(Socket& socket, const std::string& requestStr, const std::string& keyOnSuccess);

    // commands
    static Message::Result createStreamSession(Socket& socket, const std::string& sessionID, const std::string& nickname, const std::string& destination);
    static Message::Result namingLookup(Socket& socket, const std::string& name);
    static std::pair<const Message::eStatus, std::pair<const std::string, const std::string> > destGenerate(Socket& socket);
    static Message::Result accept(Socket& socket, const std::string& sessionID, bool silent);
    static Message::Result connect(Socket& socket, const std::string& sessionID, const std::string& destination, bool silent);
    static Message::Result forward(Socket& socket, const std::string& sessionID, const std::string& host, uint16_t port, bool silent);

public:
    StreamSession(
            const std::string& nickname,
            const std::string& SAMHost = SAM_DEFAULT_ADDRESS,
            uint16_t SAMPort = SAM_DEFAULT_PORT,
            const std::string& myDestination = SAM_GENERATE_MY_DESTINATION,
            const std::string& minVer = SAM_DEFAULT_MIN_VER,
            const std::string& maxVer = SAM_DEFAULT_MAX_VER);
    ~StreamSession();

    bool createStreamSession(
            const std::string& nickname,
            const std::string& SAMHost = SAM_DEFAULT_ADDRESS,
            uint16_t SAMPort = SAM_DEFAULT_PORT,
            const std::string& myDestination = SAM_GENERATE_MY_DESTINATION,
            const std::string& minVer = SAM_DEFAULT_MIN_VER,
            const std::string& maxVer = SAM_DEFAULT_MAX_VER); // recreates with new parameters

    bool createStreamSession(); // recreates with current parameters

    std::string namingLookup(const std::string& name);
    std::string getMyAddress();
    std::pair<const std::string, const std::string> destGenerate();  // .first is a public key, .second is a private key

    SOCKET accept(bool silent = false);
    SOCKET connect(const std::string& destination, bool silent = false);
    bool forward(const std::string& host, uint16_t port, bool silent = false); // forwards stream to a socket. the socket should be bound with the port, should listen and accept connections on that port

    void stopForwarding(const std::string& host, uint16_t port);

    static std::string generateSessionID();

    const sockaddr_in& getAddress() const;
    const std::string& getHost() const;
    uint16_t getPort() const;
    const std::string& getNickname() const;
    const std::string& getSessionID() const;
    const std::string& getMyDestination() const;
    const std::string& getMinVer() const;
    const std::string& getMaxVer() const;
    const std::string& getVersion() const;
};

} // namespace SAM

#endif // I2PSAM_H
