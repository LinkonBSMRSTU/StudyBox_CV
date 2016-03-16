#ifndef PATR_SERVER_H
#define PATR_SERVER_H

#include <memory>

#include <list>
#include <array>
#include <queue>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include "Socket.h"



/// Przestrze� klas i funkcji oraz sta�ych zwi�zanych z dzia�aniem serwera HTTP.
/**
 * Pozostaj� w du�ym zwi�zku z przestrzeni� TCP - komunikacja przebiega za pomoc� gniazd
 * przy wykorzystaniu serwis�w reaktywnych. 
 * Komunikacja z frontendem u�ytkownika przebiega asynchronicznie za pomoc� serwis�w proaktywnych,
 * - wyst�puje wielow�tkowo�� w celu zredukowania czasu blokowania.
 */
namespace Http {

/// Sta�a okre�laj�ca now� linie wykorzystywan� przez HTTP.
constexpr auto CRLF = "\r\n";
/// Separator nag��wk�w HTTP.
constexpr auto HEADER_SEPARATOR = ": ";




/// Klasa okre�laj�ca URI.
/**
 * Pozwala na dekodowanie, tokenizacj� i segmentacj� oraz szeregowanie
 * subdomen, katalog�w, zapyta� GET.
 */
class Uri
{
public:
    typedef std::unordered_map<std::string, std::string> QueryType;

    /// Przeprowadza dekodowanie z formatu HTTP.
    static bool Decode(const std::string& in, std::string& out);

    /// Tworzy nowy URI z podanego �a�cucha znak�w.
    Uri(const std::string& uri);
    /// Tworzy pusty URI.
    Uri();

    /// Zwraca surowy �a�cuch znak�w w postaci, w jakiej je otrzyma�.
    std::string& raw();
    const std::string& raw() const;
    /// Zwraca argumenty zapytania GET
    /**
     * Tokenizacja przebiega po pierwszym znaku '?',
     * rodzielona '&' oraz '='.
     * Elementy nie s� dekodowane.
     */
    QueryType query() const;
    /// Zwraca rodzica katalogu.
    /**
     * Dla /foo/bar/ zwraca /foo/bar
     * Dla /foo/bar zwraca /foo
     * Elementy nie s� dekodowane.
     */
    Uri parent() const;
    /// Zwraca �cie�k� bez argument�w GET.
    /**
     * �cie�ka nie jest dekodowana.
     */
    Uri absolutePath() const;
    /// Rozdziela �cie�k� absolutn� na poszczeg�lne katalogi.
    /**
     * Segmenty nie s� dekodowane.
     */
    std::vector<std::string> segments() const;

private:
    std::string uri;
};



/// Typ wykorzystywany jako cia�o dokumentu HTTP.
typedef std::string BodyType;
/// Typ wykorzystywany jako oznaczenie typu MIME.
typedef std::string MediaType;



/// Posiada dost�pne domy�lne metody HTTP.
namespace RequestMethod
{
#define MAKE_HTTP_METHOD(methodName) constexpr auto methodName = #methodName
    MAKE_HTTP_METHOD(OPTIONS);
    MAKE_HTTP_METHOD(GET);
    MAKE_HTTP_METHOD(HEAD);
    MAKE_HTTP_METHOD(POST);
    MAKE_HTTP_METHOD(PUT);
    MAKE_HTTP_METHOD(TRACE);
    MAKE_HTTP_METHOD(CONNECT);
    constexpr auto DELETE = "DELETE"; //< Ze wzgl�du na ekspansj� makra DELETE.
#undef MAKE_HTTP_METHOD
}



/// Struktura okre�laj�ca wersj� HTTP.
struct Version
{
    int major, minor;
};



/// Typ wykorzystywany do reprezentacji Nag��wka.
typedef std::pair<std::string /* name */, std::string /* value */> Header;
/// Typ wykorzystywany do kolekcji nag��wk�w.
//typedef std::unordered_map<Header::first_type, Header::second_type> HeaderContainer;
typedef std::vector<Header> HeaderContainer;



/// Klasa okre�laj�ca zapytanie HTTP.
class Request
{
public:
    /// Zwraca ci�g znak�w, jaki otrzymany by� w zapytaniu.
    std::string raw() const;
    /// Zwraca metod� HTTP.
    const std::string& method() const;
    /// Zwraca URI.
    const Uri& uri() const;
    /// Zwraca wersj� w formacie {}.{}
    std::string version() const;
    /// Zwraca kolekcj� wszystkich nag��wk�w.
    const HeaderContainer& headers() const;
    /// Zwraca cia�o dokumentu.
    const BodyType body() const;

private:
    /// Posiada informacje szczeg�owe o g��wynm wierszu.
    struct RequestLine
    {
        std::string method;
        Uri uri;
        Version version;
    } requestLine;
    
    HeaderContainer headerCollection;
    BodyType content;

    friend class RequestParser;
};



/// Parser zapyta� HTTP.
/**
 * Jest maszyn� stan�w w celu optymalizacji asynchronicznego odczytywania zapyta�.
 */
class RequestParser
{
public:
    /// Wynik parsowania.
    enum class Result
    {
        Good, //< Zapytanie poprawne pod wzgl�dem syntaktycznym.
        Bad, //< Zapytanie niepoprawne pod wzgl�dem syntaktycznym.
        Indeterminate //< Zapytanie dotychczas nie zawiera�o b��d�w syntaktycznych.
    };

    /// Tworzy nowy obiekt.
    RequestParser();

    /// Przeprowadza parsowanie dla podanego zasi�gu.
    /**
     * Kontener musi mie� value_type == char lub kompatybilny i
     * mo�liwo�� dost�pu sekwencyjnego.
     */
    template<typename InputIt>
    std::pair<Result, InputIt> parse(InputIt begin, InputIt end, Request& request)
    {
        while (begin != end)
        {
            Result result = consume(*begin++, request);
            if (result == Result::Good || result == Result::Bad)
                return std::make_pair(result, begin);
        }
        return std::make_pair(Result::Indeterminate, begin);
    }

    /// Dodaje do cia�a zapytania dany zasi�g.
    /**
     * Przeprowadza sprawdzenie z nag��wkiem Content-Length w celu
     * okreslenia maksymalnej wielko�ci cia�a zapytania.
     */
    template<typename InputIt>
    static bool fill(InputIt begin, InputIt end, Request& request)
    {
        std::size_t max = contentLength(request);

        if (request.content.length() == max)
            return true;

        while (begin != end)
        {
            request.content += *begin++;
            if (request.content.length() == max)
                return true;
        }
        return false;
    }

    void reset();

private:
    /// Zwraca d�ugo�� cia�a dla zapytania HTTP.
    static int contentLength(const Request& request);
    /// Sprawdza zgodno�� znaku w danej chwili dla zapytania.
    Result consume(char value, Request& request);

    /// Lista mo�liwych stan�w parsera.
    enum State
    {
        MethodStart,
        Method,
        Uri,
        HttpVersionH,
        HttpVersionT1,
        HttpVersionT2,
        HttpVersionP,
        HttpVersionSlash,
        HttpVersionMajorStart,
        HttpVersionMajor,
        HttpVersionMinorStart,
        HttpVersionMinor,
        ExpectingNewline1,
        HeaderLineStart,
        HeaderLws,
        HeaderName,
        SpaceBeforeHeaderValue,
        HeaderValue,
        ExpectingNewline2,
        ExpectingNewline3
    } state;
};



/// Klasa odpowiedzi HTTP.
class Response
{
public:
    /// Lista mo�liwych status�w odpowiedzi wraz z odpowiadaj�cymi kodami.
    enum class Status
    {
        /* Informational */
        Continue = 100,
        SwitchingProtocols = 101,

        /* Success */
        Ok = 200,
        Accepted = 201,
        NonAuthoritativeInformation =  203,
        NoContent = 204,
        ResetContent = 205,
        PartialContent = 206,

        /* Redirection */
        MultipleChoices = 300,
        MovedPermanently = 301,
        Found = 302,
        SeeOther = 303,
        NotModified = 304,
        UseProxy = 305,
        TemporaryRedirect = 307,

        /* Client Error */
        BadRequest = 400,
        Unauthorized = 401,
        PaymentRequired = 402,
        Forbidden = 403,
        NotFound = 404,
        MethodNotAllowed = 405,
        NotAcceptable = 406,
        ProxyAuthenticationRequired = 407,
        RequestTimeout = 408,
        Conflict = 409,
        Gone = 410,
        LengthRequired = 411,
        PreconditionFailed = 412,
        RequestEntityTooLarge = 413,
        RequestUriTooLarge = 414,
        UnsupportedMediaType = 415,
        RequestedRangeNotSatisfiable = 416,
        ExpectationFailed = 417,

        /* Server Error */
        InternalServerError = 500,
        NotImplemented = 501,
        BadGateway = 502,
        ServiceUnavailable = 503,
        GatewayTimeout = 504,
        HttpVersionNotSupported = 505
    };

    /// Tworzy odpowied� z danym kodem i cia�em o podanym typie medi�w.
    Response(Status code, const BodyType& content, const MediaType& mediaType);

    /// Zwraca odpowied� w formie, jaka zostanie wys�ana do klienta.
    std::string raw() const;
    /// Zwraca status odpowiedzi.
    Status status() const;

    /// Posiada nag��wki mo�liwe do modyfikacji w zale�no�ci od potrzeb.
    /**
     * Nag��wki nie przechodz� walidacji syntaktycznej.
     */
    HeaderContainer headers;

private:
    Status responseStatus;
    std::string response;
};



class HandlerStrategy;


/// Klasa enkapsuluj�ca po��czenie z klientem.
/**
 * Odpowiedzialna za odczytanie zapytania i wywo�anie odpowiedzi.
 */
class Connection// : public std::enable_shared_from_this<Connection>
{
public:
    typedef std::array<char, 8192> BufferType;

    /// Tworzy nowe po��czenie dla gniazda oraz klasy obs�uguj�cej zapytanie.
    /**
     * Klasa obs�uguj�ca zapytanie ma rol� mened�era.
     */
    Connection(Tcp::Socket socket, HandlerStrategy& handler);

    /// Otwiera po��czenie.
    void start();
    /// Zamyka po��czenie.
    void stop();

private:
    /// Odczytuje cz�� nag��wkow� zapytania HTTP.
    /**
     * W przypadku niepoprawno�ci mo�e wcze�niej zako�czy� po��czenie z
     * komunikatem Bad Request.
     */
    void read();
    /// Dokonuje odczytu cia�a zapytania HTTP.
    void readBody();
    void write();

    BufferType buffer;

    Tcp::Socket socket;
    HandlerStrategy& globalHandler;
    RequestParser parser;
    Request request;
};

/// Klasa odpowiedzialna za rozdzia� zada� do odpowiednich w�tk�w.
class ConnectionPool
{
public:
    typedef std::function<void()> RequestHandler;

    /// Tworzy nowy obiekt o okre�lonym maksymalnym obci��eniu.
    /**
     * Liczba w�tk�w dedukowana jest na podstawie wykrytej liczby procesor�w.
     */
    ConnectionPool(std::size_t maxLoad = 500);
    /// Tworzy nowy obiekt o okre�lonych parametrach.
    ConnectionPool(std::size_t maxThreads, std::size_t maxLoad = 500);

    ~ConnectionPool();

    /// dodaje zadanie do rozdzielenia.
    /**
     * @return czy zadanie zosta�o dodane do kolejki czy nie.
     */
    bool add(RequestHandler handler);

private:
    std::size_t maxThreads;
    std::size_t maxLoad;

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;

    // synchronization
    std::mutex mutex;
    std::condition_variable condition;
    bool stop;
};



using ConnectionPtr = std::shared_ptr<Connection>;



/// Klasa okre�laj�ca strategi� podzia�u zada� serwera HTTP.
class HandlerStrategy
{
public:
    typedef std::function<Response(Request)> RequestHandler;
    typedef std::function<void(RequestHandler)> ConnectionResponse;

    virtual ~HandlerStrategy() = default;

    /// Przekazuje sparsowane zapytanie do funkcji obs�uguj�cej.
    /**
     * Spos�b wywo�ania funkcji le�y po stronie implementacji.
     */
    virtual void handle(ConnectionResponse response) = 0;
    /// Bezpo�rednio wysy�a odpowied� do gniazda.
    virtual void respond(Tcp::Socket& socket, Response::Status stockResponse) = 0;

    /// Uruchamia po��czenie.
    virtual void start(ConnectionPtr connection) = 0;
    /// Ko�czy po��czenie.
    virtual void stop(ConnectionPtr connection) = 0;
};



/// Klasa odpowiedzialna za strategi� podzia�u zada� na ograniczon� liczb� w�tk�w.
class ThreadedHandlerStrategy : public HandlerStrategy
{
public:

    ThreadedHandlerStrategy(RequestHandler handler);

    /// Przekazuje zadanie do oddzielnego w�tku.
    void handle(ConnectionResponse response) override;
    /// Odpowiada na g��wnym w�tku.
    void respond(Tcp::Socket& socket, Response::Status stockResponse) override;

    /// Implementuje HandlerStrategy.
    void start(ConnectionPtr connection) override;
    void stop(ConnectionPtr connection) override;

private:
    //std::unordered_set<ConnectionPtr> connections;
    RequestHandler handler;
    ConnectionPool pool;
};



/// Klasa b�d�ca frontendem u�ytkownika do serwera HTTP.
/**
 * Za jej pomoc� u�ytkownik mo�e ustali� strategi� 
 * rozdzielania zada� oraz serwis demultipleksacji po��cze�.
 */
class Server
{
public:
    typedef HandlerStrategy::RequestHandler RequestHandler;
    typedef std::unique_ptr<Tcp::StreamServiceInterface> ServicePtr;
    typedef std::unique_ptr<HandlerStrategy> StrategyPtr;
    /// Tworzy nowy obiekt na danym adresie i porcie.
    /**
     * Daje najwi�ksze mo�liwo�ci dostosowania.
     */
    Server(const std::string& host, const std::string& port, ServicePtr service, StrategyPtr globalHandler);
    /// Tworzy nowy obiekt z zadan� funkcj� odpowiadaj�c� na zapytania.
    /**
     * Obiekt wykorzystuje domy�ln� strategi� ThreadedHandlerStrategy.
     */
    Server(const std::string& host, const std::string& port, RequestHandler handler, ServicePtr service = ServicePtr(new Tcp::StreamService()));

    /// Uruchamia serwer.
    /**
     * Serwer b�dzie dzia�a� do czasu otrzymania sygna�u przerwania systemowego.
     * Zwr�cona warto�� to warto�� sygna�u.
     */
    int run();

private:
    /// asynchronicznie akceptuje po��czenie.
    void accept();

    ServicePtr service;
    Tcp::Acceptor acceptor;
    Tcp::SignalSet signals;
    StrategyPtr globalHandler;
};

} // namespace Http

#endif // PATR_SERVER_H
