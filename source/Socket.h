#ifndef PATR_SOCKET_H
#define PATR_SOCKET_H

#include <memory>

#include <set>
#include <map>
#include <queue>
#include <string>
#include <unordered_set>

#include <functional>

struct addrinfo;

/// Przestrze� dla klas i funkcji zwi�zanych z niskopoziomow� komunikacj� TCP/IP.
/**
 * B�d�ce tu klasy mo�na podzieli� na 4 cz�ci:
 * Service - odpowiedzialne za demultipleksacj� i komunikacj� mi�dzy elementami,
 * Interface - odpowiedzialne za udost�pnianie interfejs�w w celu rozszerzania funkcjonalno�ci,
 * Implementation - odpowiedzialne za szczeg�y implementacyjne, spe�niaj�ce interfejs powy�ej,
 * Pozosta�e - s�u��ce jako frontend u�ytkownika.
 *
 * U�ytkownik musi powo�a� obiekt StreamService aby m�c wykorzystywa� asynchroniczne
 * aspekty api. W przeciwnym wypadku musi spe�ni� StreamServiceInterface w stopniu,
 * kt�ry by na to pozwala�.
 */
namespace Tcp {

// Oznacza, �e wykorzystywana funkcja nie jest zaimplementowana.
class NotImplemented : public std::logic_error
{
public:
    using std::logic_error::logic_error;
};

// Bazowa klasa do oznaczania b��d�w zwi�zanych z komunikacj� TCP.
class TcpError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

#define DEFINE_ERROR(errorName, baseClass) \
class errorName : public baseClass \
{ \
public: \
using baseClass::baseClass; \
}
// B��d zwi�zany ze specyficzn� platform�
DEFINE_ERROR(PlatformError, TcpError);
// B��d zwi�zany z gniazdami.
DEFINE_ERROR(SocketError, TcpError);
// B��d zwi�zany z i/o gniazd.
DEFINE_ERROR(StreamError, SocketError);
// B��d wyj�cia gniazda.
DEFINE_ERROR(SendError, StreamError);
// B��d wej�cia gniazda.
DEFINE_ERROR(ReceiveError, StreamError);
// B��d w nas�uchiwaniu po��cze�.
DEFINE_ERROR(AcceptorError, SocketError);
// B��d w zwi�zaniu gniazda z adresem.
DEFINE_ERROR(BindError, AcceptorError);
// B��d w przygotowaniu gniazda do nas�uchiwania.
DEFINE_ERROR(ListenError, AcceptorError);
// B��d otwarcia po��czenia.
DEFINE_ERROR(AcceptError, AcceptorError);
// B��d demultipleksacji / asynchronicznego wywo�ania.
DEFINE_ERROR(ServiceError, TcpError);
// B��d gniazda wyj�ciowego.
DEFINE_ERROR(EndpointError, TcpError);
// B��d ustawienia opcji gniazda.
DEFINE_ERROR(SocketOptionError, SocketError);
#undef DEFINE_ERROR

// Typ wykorzystywany do przesy�u informacji przez gniazda.
typedef std::pair<char* /* data */, int /* size */> Buffer;
typedef std::pair<const char* /* data */, int /* size */> ConstBuffer;






/// Klasa do blokowania kopiowania obiekt�w.
/**
 * Nie nale�y usuwa� obiekt�w przez wska�nik do tej klasy ze wzgl�du
 * na brak konstruktora.
 */
struct NonCopyable
{
    NonCopyable() = default;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator =(const NonCopyable&) = delete;
    NonCopyable& operator =(NonCopyable&&) = delete;
};






/// S�u�y zamianie kontenera na Tcp::Buffer.
/**
 * Zamianie mo�e podlega� dowolny kontener spe�niaj�cy wymogi
 * ContiguousContainer posiadaj�cy value_type char lub r�wnowa�ny. 
 */
template<typename T>
Buffer MakeBuffer(T& container)
{
    return std::make_pair(container.data(), static_cast<int>(container.size()));
}





/// S�u�y zamianie kontenera na Tcp::ConstBuffer.
/**
* Zamianie mo�e podlega� dowolny kontener spe�niaj�cy wymogi
* ContiguousContainer posiadaj�cy value_type char lub r�wnowa�ny.
*/
template<typename T>
ConstBuffer MakeBuffer(const T& container)
{
    return std::make_pair(container.data(), static_cast<int>(container.size()));
}

/// Przestrze� opcji gniazd tcp.
namespace Option {
    /// G��wna struktura, z jakiej korzystaj Tcp::Socket.
    struct Option
    {
        int option;
        int value;
    };

    /// Specjalizacja Tcp::Option dla opcji SO_REUSEADDR.
    struct ReuseAddress : public Option
    {
        ReuseAddress(bool value);
    };

    // Mo�liwa rozbudowa, je�eli zajdzie taka potrzeba.
}




class StreamService;
class Socket;
class AcceptorImplementation;
class SocketImplementation;
class SocketInterface;
class AcceptorInterface;
class SocketInterface;
class StreamServiceInterface;




/// Klasa bazowa obiekt�w zawieraj�cych uchwyty.
/**
 * Tutaj uchwytami s� deskryptory plik�w, ze wzgl�du na wybrany
 * rodzaj implementacji.
 */
class Service
{
public:
    typedef int HandleType;

    // Gwarantuje dost�p do uchwytu.
    HandleType getHandle() const;
    void setHandle(HandleType handle);

    // por�wnuje obiekty ze wzgl�du na rozmiar ich uchwytu.
    bool operator ==(const Service&) const;
    bool operator !=(const Service&) const;
    bool operator <(const Service&) const;
    bool operator >(const Service&) const;

protected:
    ~Service() = default;

    HandleType handle;
};




/// Interfejs s�u��cy do komunikacji z StreamService.
class SignalServiceInterface
{
public:
    typedef int SignalType;

    virtual ~SignalServiceInterface() = default;

    /// W celu sprawdzenia jaki sygna� zosta� wywo�any.
    /**
     * @return warto�� sygna�u je�eli ten zosta� wywo�any.
     */
    virtual int get() const = 0;
    /// W celu sprawdzenia czy sygna� zosta� wywo�any.
    virtual bool received() const = 0;
};




/// S�u�y do sprawdzania zdarze� przerwa� systemowych.
class SignalService : public SignalServiceInterface
{
public:
    SignalService(int& sigVal, bool& sigFlag);

    /// Zwraca jaki sygna� zosta� wywo�any.
    /**
    * W przypadku, gdy �aden nie zosta� wywo�any,
    * zwracana warto�� jest nieokre�lona.
    */
    int get() const override;
    /// Zwraca, czy sygna� zosta� wywo�any.
    bool received() const override;

private:
    int& sigVal;
    bool& sigFlag;
};





/// S�u�y do obs�ugi asynchronicznych wywo�a� ::accept().
/**
 * Dzia�a jako backend operacji asynchronicznych dla dowolnych
 * implementacji spe�niaj�cych AcceptorInterface.
 */
class AcceptorService : public Service
{
public:
    /// Funkcja wywo�ana przy asynchronicznym zdarzezniu.
    /**
     * Socket to gniazdo, dla kt�rego zosta�o otwarte po��czenie.
     */
    typedef std::function<void(Socket)> Handler;
    typedef Service::HandleType HandleType;

    /// Tworzy obiekt z implementacj� o interfejsie AcceptorInterface.
    AcceptorService(AcceptorInterface& implementation);

    /// Wybudza implementacj� w celu dokonania asynchronicznej akcji.
    void acceptReady();
    /// Wprowadza now� funkcj� do wywo�ania w kolejce.
    void enqueue(Handler handler);

private:
    AcceptorInterface& implementation;
    std::queue<Handler> handlers;
};




/// S�u�y do obs�ugi asynchronicznych odczyt�w i zapis�w do gniazd.
/**
 * Dzia�a jako backend operacji asynchronicznych dla dowolnych
 * implementacji spe�niaj�cych SocketInterface.
 */
class SocketService : public Service
{
public:
    typedef Service::HandleType HandleType;
    /// Prototyp funkcji s�u��cej do obs�ugi asynchronicznego odczytu z gniazda.
    typedef std::function<void(int /*error code */, std::size_t /* bytes read */)> ReadHandler;
    /// Prototyp funkcji s�u��cej do obs�ugi asynchronicznego zapisu z gniazda.
    /**
     * Asynchroniczny zapis nie jest obecnie zaimplementowany.
     */
    typedef std::function<void(int, int)> WriteHandler;
    /// Bufor wywkorzystywany do komunikacji z implementacj�.
    typedef Buffer BufferType;
    typedef ConstBuffer ConstBufferType;

    /// Tworzy obiekt z implementacj� o interfejsie SocketInterface.
    SocketService(StreamServiceInterface& service, SocketInterface& implementation, HandleType handle);
    /// Oznacza, �e gniazdo jest gotowe do nieblokuj�cego odczytu.
    int readReady();
    /// Oznacza, �e gniazdo jest gotowe do nieblokuj�cego zapisu.
    int writeReady();

    /// S�u�y wprowadzeniu nowych funkcji obs�uguj�cych asynchroniczny odczyt.
    void enqueue(BufferType& buffer, ReadHandler handler);
    /// S�u�y wprowadzeniu nowych funkcji obs�uguj�cych asynchroniczny zapis.
    void enqueue(const ConstBufferType& buffer, WriteHandler handler);

    /// Oznacza gniazdo jako gotowe do zamkni�cia.
    /**
     * Zamkni�cie mo�e nie nast�pi� natychmiast.
     */
    void shutdown();

private:
    bool shut;

    std::queue<std::pair<BufferType, ReadHandler>> readHandlers;
    std::queue<std::pair<ConstBufferType, WriteHandler>> writeHandlers;
    SocketInterface& implementation;
    StreamServiceInterface& service;
};




/// Interfejs dla informacji o gniazdu docelowym.
class EndpointInterface
{
public:
    typedef addrinfo* AddressType;
    typedef addrinfo* ProtocolType;

    virtual ~EndpointInterface() = default;
    /// W celu uzyskania informacji o adresie.
    virtual AddressType address() const = 0;
    /// W celu uzyskania informacji o protokole.
    virtual ProtocolType protocol() const = 0;
};





/// Klasa zawieraj�ca szczeg�y implementacyjne dla wybierania informacji docelowym miejscu.
class EndpointImplementation : public EndpointInterface
{
public:
    typedef addrinfo* AddressType;
    typedef addrinfo* ProtocolType;

    /// Tworzy obiekt o danym adresie i porcie.
    /** 
     * address mo�e przyjmowa� form� nazwy domeny (np. www.example.com),
     * mnemonik� (np. localhost), IPv4 (np. 0.0.0.0), IPv6 (np 0::0).
     * Port mo�e przyjmowa� warto�ci od 1 do 49150 w postaci �a�cucha znak�w.
     */
    EndpointImplementation(const std::string& address, const std::string& port);
    ~EndpointImplementation();

    /// Zwraca informacje o adresie niezb�dne do wykonania operacji.
    AddressType address() const override;
    /// Zwraca informacje o protokole w celu otwarcia gniazda.
    ProtocolType protocol() const override;

private:
    AddressType addressVal;
};




/// S�u�y jako frontend u�ytkownika.
/**
 * Przekierowuje odpowiedzialno�� do implementacji.
 */
class Endpoint
{
public:
    typedef EndpointImplementation::AddressType AddressType;
    typedef EndpointImplementation::ProtocolType ProtocolType;

    /// Tworzy obiekt z adresem i portem o domy�lnej implementacji.
    Endpoint(const std::string& address, const std::string& port);
    /// Tworzy obiekt o danej implementacji, zawieraj�cej ju� informacje szczeg�owe.
    Endpoint(std::unique_ptr<EndpointInterface> implementation);

    /// Przekierowuje do implementacji.
    AddressType address() const;
    /// Przekierowuje do implementacji.
    ProtocolType protocol() const;

private:
    std::unique_ptr<EndpointInterface> implementation;
};




/// Jako interfejs klas nas�uchuj�cych po��cze�.
class AcceptorInterface : public NonCopyable
{
public:
    typedef AcceptorService::HandleType HandleType;
    typedef AcceptorService::Handler Handler;

    /// Tworzy nowy obiekt, kt�ry do asynchronicznej komunikacji wykorzysta
    /// obiekt implementuj�cy StreamServiceInterface.
    AcceptorInterface(StreamServiceInterface&);
    virtual ~AcceptorInterface() = default;

    /// Otwiera gniazdo o danym protokole.
    virtual void open(Endpoint::ProtocolType protocol) = 0;
    /// Ustawia opcj� dla gniazda.
    virtual void setOption(const Option::Option& option) = 0;
    /// Przypisuje gniazdo do adresu.
    virtual void bind(Endpoint::AddressType address) = 0;
    /// Flaguje gniazdo do nas�uchiwania na po�aczenia.
    /**
     * backlog oznacza maksymaln� ilo�� po��cze� oczekuj�cych w kolejce do pod��czenia,
     * kt�re nie zosta�y jeszcze obs�u�one.
     */
    virtual void listen(int backlog) = 0;

    /// Blokuj�ca funkcja do przyj�cia po��czenia.
    /**
     * Funkcja b�dzize blokowa�, dop�ki nie zostanie wybudzona przez
     * nadchodz�ce po��czenie.
     */
    virtual Socket accept() = 0;
    /// Asynchroniczna wersja przyjmuje funkcj� do obs�ugi zdarzenia, gdy ono nadejdzie.
    /**
     * Przekazywana funkcja zostanie wykonana, gdy obiekt zostanie wybudzony.
     * Funkcja wraca bez blokowania.
     */
    void asyncAccept(Handler handler);

protected:
    /// Celem ograniczenia interakcji frontendu z obiektami typu *Service.
    HandleType handle() const;
    void handle(HandleType handle);

private:
    AcceptorService service;
};




/// Interfejs gniazd TCP.
class SocketInterface
{
public:
    typedef SocketService::ReadHandler ReadHandler;
    typedef SocketService::WriteHandler WriteHandler;
    typedef SocketService::BufferType BufferType;
    typedef SocketService::ConstBufferType ConstBufferType;
    typedef SocketService::HandleType HandleType;

    /// Tworzy obiekt zwi�zany z obiektem typu *Service.
    SocketInterface(StreamServiceInterface& service, HandleType handle);
    virtual ~SocketInterface() = default;

    /// Blokuje, dop�ki nie odczyta okre�lonej liczby bajt�w.
    /**
     * @return ilo�� odczytanych bajt�w - powinna wynosi� rozmiar bufora.
     */
    virtual int read(BufferType& buffer) = 0;
    /// Odczytuje dost�pn� w tej chwili liczb� bajt�w.
    /**
     * Mo�e blokowa�, je�eli w danej chwili bufor gniazda jest pusty.
     * @return ilo�� odczytanych bajt�w - powinna by� r�wna ilo�ci dost�pnych bajt�w na gnie�dzie.
     */
    virtual int readSome(BufferType& buffer) = 0;
    /// Oznacza asynchroniczne wywo�anie funkcji ReadHandler.
    /**
     * Funkcja powinna wraca� bez blokowania.
     */
    void asyncReadSome(BufferType buffer, ReadHandler handler);
    /// Odpowiednik read dla zapisu.
    /**
     * @return ilo�� faktycznie zapisanych bajt�w - powinna wynosi� rozmiar bufora.
     */
    virtual int write(const ConstBufferType& buffer) = 0;
    /// Odpowiednik readSome dla zapisu.
    /**
     * @return ilo�� faktycznie zapisanych bajt�w.
     */
    virtual int writeSome(const ConstBufferType& buffer) = 0;
    /// Odpowiednik asyncReadSome dla zapisu.
    void asyncWriteSome(const ConstBufferType& buffer, WriteHandler handler);

    /// Bezpo�rednio zamyka gniazdo.
    virtual void close() = 0;
    /// Oznacza gniazdo do zamkni�cia.
    void shutdown();

protected:
    HandleType handle() const;

private:
    SocketService service;
};


/// Interfejs do tworzenia implementacji interfejs�w dla danego serwisu.
class ServiceFactory
{
public:
    /// Tworzy nowy obiekt implementuj�cy AcceptorInterface.
    virtual std::unique_ptr<AcceptorInterface> getImplementation() = 0;
    /// Tworzy nowy obiekt implementuj�cy EndpointInterface.
    virtual std::unique_ptr<EndpointInterface> resolve(const std::string& host, const std::string& port) = 0;
};


/// Interfejs g��wnego serwisu reaktywnego.
class StreamServiceInterface
{
public:
    virtual ~StreamServiceInterface() = default;

    /// Uruchamia serwis.
    /**
     * Wywo�anie powinno blokowa� do momentu wywo�ania obs�ugiwanego przerwania
     * systemowego lub wyzerowania licznika otwartych gniazd.
     */
    virtual int run() = 0;
    /// Dodaje istniej�cy serwis gniazda.
    virtual void add(SocketService* service);
    /// Usuwa serwis gniazda.
    /**
     * Gniazdo powinno samo zagwarantowa� poprawn� komunikacj� z serwisem.
     */
    virtual void remove(SocketService* service);
    /// Dodaje istniej�cy serwis akceptora.
    virtual void add(AcceptorService* service);
    /// Dodaje serwis do obs�ugi sygna��w.
    /**
     * Obs�ugiwany jest tylko jeden serwis sygna��w ze wzgl�d�w implementacyjnych.
     */
    virtual void add(SignalService* service);

    /// Zwraca nowy obiekt do tworzenia obiekt�w klasy spe�niaj�cych wymagania danego serwisu.
    virtual std::unique_ptr<ServiceFactory> getFactory() = 0;

protected:
    std::set<SocketService*> sockets;
    std::set<AcceptorService*> acceptors;
    SignalService* signal;
};





/// Implementacja fabryki dla klasy StreamService.
/**
 * Tworzy obiekty klasy obs�ugiwanej przez StreamService.
 */
class StreamServiceFactory : public ServiceFactory, public NonCopyable
{
public:
    StreamServiceFactory(StreamService& service);

    /// Zwraca implementacj� akceptora zgodn� z wymaganiami StreamService.
    virtual std::unique_ptr<AcceptorInterface> getImplementation() override;
    /// Zwraca informacje o gnie�dzie ko�cowym o implementacji zwi�zanej z StreamService.
    virtual std::unique_ptr<EndpointInterface> resolve(const std::string & host, const std::string & port) override;

private:
    StreamService& service;
};



/// Klasa do demultipleksacji po��cze� oraz asynchronicznej komunikacji z innymi serwisami.
/**
 * Demultipleksacji podlegaj� sygna�y gotowo�ci do odczytu, zapisu oraz nowych po�acze�.
 */
class StreamService : public StreamServiceInterface
{
public:
    /// Tworzy nowy obiekt serwisu.
    StreamService();
    /// Zamyka pozosta�e otwarte po��czenia.
    ~StreamService();

    /// Implementuje interfejs StreamServiceInterface.
    int run() override;
    void add(SocketService* service) override;
    void remove(SocketService* service) override;

    /// Zwraca fabryk� do tworzenia obiekt�w klas obs�ugiwanych przez serwis.
    std::unique_ptr<ServiceFactory> getFactory() override;

private:
    struct StreamServicePimpl;
    std::unique_ptr<StreamServicePimpl> pimpl;
};



/// Frontend u�ytkownika dla klasy nas�uchuj�cej nowych po��cze�.
/**
 * Oddelegowuje odpowiedzialno�� implementacji do klasy implementacyjnej spe�niaj�cych
 * interfejs AcceptorInterface
 */
class Acceptor : public NonCopyable
{
public:
    typedef AcceptorInterface::Handler Handler;

    /// Tworzy nowy obiekt zwi�zany z danym serwisem.
    Acceptor(StreamServiceInterface& service);
    /// Tworzy nowy obiekt o okre�lonej implementacji.
    Acceptor(std::unique_ptr<AcceptorInterface> implementation);

    /// Przekierowuje do implementacji.
    void open(Endpoint::ProtocolType protocol);
    void setOption(const Option::Option& option);
    void bind(Endpoint::AddressType address);
    void listen(int backlog);

    Socket accept();
    void asyncAccept(Handler handler);

private:
    std::unique_ptr<AcceptorInterface> implementation;
};


/// Frontend u�ytkownika dla klasy gniazda.
/**
 * Udost�pnia funkcje odczytu i zapisu blokuj�cego oraz nie dla gniazda TCP.
 */
class Socket : public NonCopyable
{
public:
    typedef SocketInterface::ReadHandler ReadHandler;
    typedef SocketInterface::WriteHandler WriteHandler;
    typedef SocketInterface::BufferType BufferType;
    typedef SocketInterface::ConstBufferType ConstBufferType;
    typedef SocketInterface::HandleType HandleType;

    //Socket(StreamService& service, HandleType handle);
    /// Tworzy nowy obiekt z okre�lon� implementacj�.
    Socket(std::unique_ptr<SocketInterface> implementation);

    /// Przekierowuje wywo�anie do implementacji.
    int read(BufferType& buffer);
    int readSome(BufferType& buffer);
    void asyncReadSome(BufferType buffer, ReadHandler handler);
    int write(const ConstBufferType& buffer);
    int writeSome(const ConstBufferType& buffer);
    void asyncWriteSome(const ConstBufferType& buffer, WriteHandler handler);

    void close();
    void shutdown();

private:
    std::unique_ptr<SocketInterface> implementation;
};






/// Frontend u�ytkownika do obs�ugi sygna��w.
/**
 * Standardowe sygna�y, kt�re powinny by� 
 * zdefiniowane na wszystkich platformach to:
 * SIGABRT
 * SIGFPE
 * SIGILL
 * SIGINT
 * SIGSEGV
 * SIGTERM
 */
class SignalSet
{
public:
    typedef int SignalType;

    /// Tworzy nowy obiekt wsp�dzia�aj�cy z serwisem.
    SignalSet(StreamServiceInterface&);

    /// Dodaje sygna� do sygna��w obs�ugiwanych przez obiekt.
    /**
     * Ze wzgl�d�w implementacyjnych sygna�y s� dzielone przez r�ne obiekty tego typu.
     */
    void add(SignalType signal);

private:
    SignalService service;
    static int Signal;
    static bool Occured;
    static void SignalHandler(int signal);
};





/// Klasa zawieraj�ca szczeg�y implementacyjne na akceptora.
class AcceptorImplementation : public AcceptorInterface
{
public:
    AcceptorImplementation(StreamServiceInterface& service);

    /// Implementuje zgodnie z AcceptorInterface.
    void open(Endpoint::ProtocolType protocol) override;
    void setOption(const Option::Option& option) override;
    void bind(Endpoint::AddressType address) override;
    void listen(int backlog) override;

    Socket accept() override;

private:
    StreamServiceInterface& streamService;
};



/// Klasa zawieraj�ca szczeg�y implementacyjne gniazda TCP.
class SocketImplementation : public SocketInterface
{
public:
    typedef SocketService::HandleType HandleType;
    typedef SocketService::ReadHandler ReadHandler;
    typedef SocketService::WriteHandler WriteHandler;
    typedef SocketService::BufferType BufferType;
    typedef SocketService::ConstBufferType ConstBufferType;

    /// Tworzy nowy obiekt zwi�zany z StreamServiceInterface oraz przydzielonym uchwytem.
    SocketImplementation(StreamServiceInterface& service, HandleType handle);
    ~SocketImplementation();

    /// Implementuje zgodnie z SocketInterface.
    int read(BufferType& buffer) override;
    int readSome(BufferType& buffer) override;
    int write(const ConstBufferType& buffer) override;
    int writeSome(const ConstBufferType& buffer) override;

    /// Zamyka gniazdo.
    void close() override;

private:
    bool closed;
};

} // namespace Tcp

#endif // PATR_SOCKET_H
