#include "Server.h"
#include <csignal>
#include <sstream>
#include <string>
#include <algorithm>

namespace {

    std::unordered_map<Http::Response::Status, std::string> StockResponse = {
    { Http::Response::Status::Ok, "HTTP/1.0 200 OK\r\n" },
    { Http::Response::Status::Created, "HTTP/1.0 201 Created\r\n" },
    { Http::Response::Status::Accepted, "HTTP/1.0 202 Accepted\r\n" },
    { Http::Response::Status::NoContent, "HTTP/1.0 204 No Content\r\n" },
    { Http::Response::Status::MultipleChoices, "HTTP/1.0 300 Multiple Choices\r\n" },
    { Http::Response::Status::MovedPermanently, "HTTP/1.0 301 Moved Permanently\r\n" },
    { Http::Response::Status::Found, "HTTP/1.0 302 Found\r\n" },
    { Http::Response::Status::NotModified, "HTTP/1.0 304 Not Modified\r\n" },
    { Http::Response::Status::BadRequest, "HTTP/1.0 400 Bad request\r\n" },
    { Http::Response::Status::Unauthorized, "HTTP/1.0 401 Unauthorized\r\n" },
    { Http::Response::Status::Forbidden, "HTTP/1.0 403 Forbidden\r\n" },
    { Http::Response::Status::NotFound, "HTTP/1.0 404 Not Found\r\n" },
    { Http::Response::Status::InternalServerError, "HTTP/1.0 500 Internal Server Error\r\n" },
    { Http::Response::Status::NotImplemented, "HTTP/1.0 501 Not Implemented\r\n" },
    { Http::Response::Status::BadGateway, "HTTP/1.0 502 Bad Gateway\r\n" },
    { Http::Response::Status::ServiceUnavailable, "HTTP/1.0 503 Service Unavailable\r\n" }
    };

    Tcp::ConstBuffer MakeBuffer(Http::Response::Status status)
    {
        return Tcp::MakeBuffer(const_cast<const std::string&>(StockResponse[status]));
    }
}


Http::Connection::Connection(Tcp::Socket socket, HandlerStrategy& handler) : socket(std::move(socket)), globalHandler(handler)
{
}

void Http::Connection::start()
{
    read();
}

void Http::Connection::stop()
{
    socket.close();
}

void Http::Connection::read()
{
    std::size_t bytes = socket.readSome(Tcp::MakeBuffer(buffer));
    int ec = bytes <= 0;
    if (!ec) // Nie wykryto b��du.
    {
        RequestParser::Result result;
        auto it = buffer.begin();
        std::tie(result, it) = parser.parse(buffer.begin(), buffer.begin() + bytes, request);
        if (result == RequestParser::Result::Good) // Zapytanie sparsowane poprawnie.
        {
            if (parser.fill(it, buffer.begin() + bytes, request)) // Zacznij czyta� cia�o.
            {
                write(); // Je�eli cia�o nie zosta�o wykryte, przejd� do odpowiedzi.
            }
            else
            {
                readBody(); // W przeciwnym wypadku wywo�aj kolejne asynchroniczne czytanie cia�a.
            }
        }
        else if (result == RequestParser::Result::Bad) // Zapytanie sparsowane niepoprawnie.
        {
            globalHandler.respond(socket, Response::Status::BadRequest); // Od razu odpowiedz klientowi.
        }
        else
        {
            read(); // Czytaj dalej.
        }
    }
}

void Http::Connection::readBody()
{
    std::size_t bytes = socket.readSome(Tcp::MakeBuffer(buffer));
    int ec = bytes > 0;
    if (!ec)
    {
        if (parser.fill(buffer.begin(), buffer.begin() + bytes, request))
        {
            write();
        }
        else
        {
            readBody();
        }
    }
}

void Http::Connection::write()
{
    globalHandler.handle(
        [this](HandlerStrategy::RequestHandler handler)
        {
            socket.write(Tcp::MakeBuffer(handler(request).raw()));
        }
    );
}

Http::ThreadedHandlerStrategy::ThreadedHandlerStrategy(RequestHandler handler) : handler(handler)
{
}

void Http::ThreadedHandlerStrategy::handle(ConnectionResponse response)
{
    response(handler);
}

void Http::ThreadedHandlerStrategy::respond(Tcp::Socket& socket, Response::Status stockResponse)
{
    socket.write(MakeBuffer(stockResponse));
}

void Http::ThreadedHandlerStrategy::start(ConnectionPtr connection)
{
    pool.add([connection]
    {
        connection->start();
    });
}

void Http::ThreadedHandlerStrategy::stop(ConnectionPtr connection)
{
}

Http::Response::Response(Status code, const BodyType & content, const MediaType & mediaType) : responseStatus(code), response(content)
{
    if (content.length() > 0) // w przypadku braku cia�a nie uzupe�niaj nag��wk�w.
    {
        headers.push_back(Header("Content-Length", std::to_string(content.length())));
        headers.push_back(Header("Content-Type", mediaType));
    }
}

std::string Http::Response::raw() const
{
    std::string retval;
    retval += MakeBuffer(responseStatus).first;
    for (const auto& header : headers)
    {
        retval += header.first + HEADER_SEPARATOR + header.second + CRLF;
    }
    retval += CRLF + response;

    return retval;
}

Http::Response::Status Http::Response::status() const
{
    return responseStatus;
}

Http::RequestParser::RequestParser() : state(MethodStart)
{
}

void Http::RequestParser::reset()
{
    state = MethodStart;
}

int Http::RequestParser::contentLength(const Request& request)
{
    auto sizeFound = std::find_if(request.headerCollection.begin(), 
                                  request.headerCollection.end(),
        [](const std::pair<std::string, std::string>& s)
        {
            return s.first == "Content-Length";
        }
    );
    if (sizeFound != request.headerCollection.end())
        return std::stoi(sizeFound->second);
    return 0;
}

namespace {

    bool isChar(char c)
    {
        return c >= 0 && c <= 127;
    }

    bool isControl(char c)
    {
        return (c >= 0 && c <= 31) || (c == 127);
    }

    bool isSpecial(char c)
    {
        switch (c)
        {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
        case ' ':
        case '\t':
            return true;
        default:
            return false;
        }
    }

    bool isDigit(char c)
    {
        return c >= '0' && c <= '9';
    }
}

Http::RequestParser::Result Http::RequestParser::consume(char input, Request& request)
{
    switch (state) // Obs�uga maszyny stan�w.
    {
    case MethodStart:
        if (!isChar(input) || isControl(input) || isSpecial(input))
        {
            return Result::Bad;
        }
        else
        {
            state = Method;
            request.requestLine.method.push_back(input);
            return Result::Indeterminate;
        }
    case Method:
        if (input == ' ')
        {
            state = Uri;
            return Result::Indeterminate;
        }
        else if (!isChar(input) || isControl(input) || isSpecial(input))
        {
            return Result::Bad;
        }
        else
        {
            request.requestLine.method.push_back(input);
            return Result::Indeterminate;
        }
    case Uri:
        if (input == ' ')
        {
            state = HttpVersionH;
            return Result::Indeterminate;
        }
        else if (isControl(input))
        {
            return Result::Bad;
        }
        else
        {
            request.requestLine.uri.raw() += input;
            return Result::Indeterminate;
        }
    case HttpVersionH:
        if (input == 'H')
        {
            state = HttpVersionT1;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HttpVersionT1:
        if (input == 'T')
        {
            state = HttpVersionT2;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HttpVersionT2:
        if (input == 'T')
        {
            state = HttpVersionP;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HttpVersionP:
        if (input == 'P')
        {
            state = HttpVersionSlash;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HttpVersionSlash:
        if (input == '/')
        {
            request.requestLine.version.major = 0;
            request.requestLine.version.minor = 0;
            state = HttpVersionMajorStart;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HttpVersionMajorStart:
        if (isDigit(input))
        {
            request.requestLine.version.major = request.requestLine.version.major * 10 + input - '0';
            state = HttpVersionMajor;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HttpVersionMajor:
        if (input == '.')
        {
            state = HttpVersionMinorStart;
            return Result::Indeterminate;
        }
        else if (isDigit(input))
        {
            request.requestLine.version.major = request.requestLine.version.major * 10 + input - '0';
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HttpVersionMinorStart:
        if (isDigit(input))
        {
            request.requestLine.version.minor = request.requestLine.version.minor * 10 + input - '0';
            state = HttpVersionMinor;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HttpVersionMinor:
        if (input == '\r')
        {
            state = ExpectingNewline1;
            return Result::Indeterminate;
        }
        else if (isDigit(input))
        {
            request.requestLine.version.minor = request.requestLine.version.minor * 10 + input - '0';
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case ExpectingNewline1:
        if (input == '\n')
        {
            state = HeaderLineStart;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HeaderLineStart:
        if (input == '\r')
        {
            state = ExpectingNewline3;
            return Result::Indeterminate;
        }
        else if (!request.headerCollection.empty() && (input == ' ' || input == '\t'))
        {
            state = HeaderLws;
            return Result::Indeterminate;
        }
        else if (!isChar(input) || isControl(input) || isSpecial(input))
        {
            return Result::Bad;
        }
        else
        {
            request.headerCollection.push_back(Header());
            request.headerCollection.back().first.push_back(input);
            state = HeaderName;
            return Result::Indeterminate;
        }
    case HeaderLws:
        if (input == '\r')
        {
            state = ExpectingNewline2;
            return Result::Indeterminate;
        }
        else if (input == ' ' || input == '\t')
        {
            return Result::Indeterminate;
        }
        else if (isControl(input))
        {
            return Result::Bad;
        }
        else
        {
            state = HeaderValue;
            request.headerCollection.back().second.push_back(input);
            return Result::Indeterminate;
        }
    case HeaderName:
        if (input == ':')
        {
            state = SpaceBeforeHeaderValue;
            return Result::Indeterminate;
        }
        else if (!isChar(input) || isControl(input) || isSpecial(input))
        {
            return Result::Bad;
        }
        else
        {
            request.headerCollection.back().first.push_back(input);
            return Result::Indeterminate;
        }
    case SpaceBeforeHeaderValue:
        if (input == ' ')
        {
            state = HeaderValue;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case HeaderValue:
        if (input == '\r')
        {
            state = ExpectingNewline2;
            return Result::Indeterminate;
        }
        else if (isControl(input))
        {
            return Result::Bad;
        }
        else
        {
            request.headerCollection.back().second.push_back(input);
            return Result::Indeterminate;
        }
    case ExpectingNewline2:
        if (input == '\n')
        {
            state = HeaderLineStart;
            return Result::Indeterminate;
        }
        else
        {
            return Result::Bad;
        }
    case ExpectingNewline3:
        return (input == '\n') ? Result::Good : Result::Bad;
    default:
        return Result::Bad;
    }
}

std::string Http::Request::raw() const
{
    auto s = method() + ' ' + uri().raw() + " HTTP/" + version() + CRLF;
    for (const auto& header : headers())
    {
        s += header.first + ": " + header.second + CRLF;
    }
    s += CRLF + body();

    return s;
}

const std::string& Http::Request::method() const
{
    return requestLine.method;
}

const Http::Uri& Http::Request::uri() const
{
    return requestLine.uri;
}

std::string Http::Request::version() const
{
    return std::to_string(requestLine.version.major) + '.' + std::to_string(requestLine.version.minor);
}

const Http::HeaderContainer& Http::Request::headers() const
{
    return headerCollection;
}

const Http::BodyType Http::Request::body() const
{
    return content;
}

bool Http::Uri::Decode(const std::string & in, std::string& out)
{
    out.clear();
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '%')
        {
            if (i + 3 <= in.size())
            {
                int value = 0;
                std::istringstream is(in.substr(i + 1, 2));
                if (is >> std::hex >> value)
                {
                    out += static_cast<char>(value);
                    i += 2;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        else if (in[i] == '+')
        {
            out += ' ';
        }
        else
        {
            out += in[i];
        }
    }
    return true;
}

Http::Uri::Uri(const std::string & uri) : uri(uri)
{
}

Http::Uri::Uri()
{
}

namespace {

template<typename InputIt>
std::vector<std::string> tokenize(InputIt begin, InputIt end, char c)
{
    std::vector<std::string> result;

    do
    {
        auto nbegin = begin;

        while (begin != end && *begin != c)
            begin++;

        result.push_back(std::string(nbegin, begin));

        if (begin == end) break;
    } while (begin++ != end);

    return result;
}

}

std::string & Http::Uri::raw()
{
    return uri;
}

const std::string & Http::Uri::raw() const
{
    return uri;
}

Http::Uri::QueryType Http::Uri::query() const
{
    QueryType retval;
    auto results = std::move(tokenize(uri.begin(), uri.end(), '?')); // Podziel ze wzgl�du na '?'.
    if (results.size() == 1)
        return retval;
    
    std::string full;

    for (auto i = results.rbegin(); i != results.rend() - 1; ++i)
    {
        full += std::move(*i);
    }

    results = std::move(tokenize(full.begin(), full.end(), '&')); // Podziel ze wzgl�du na '&'.

    for (const auto& token : results)
    {
        auto nresults = std::move(tokenize(token.begin(), token.end(), '=')); // Podziel ze wzgl�du na '='.
        full.clear();
        for (auto i = nresults.rbegin(); i != nresults.rend() - 1; ++i)
        {
            full += std::move(*i);
        }
        retval[nresults[0]] = full;
    }

    return retval;
}

Http::Uri Http::Uri::parent() const
{
    auto results = std::move(segments());
    int offset = 0;
    if (uri.back() != '/')
        offset = 1;

    return Uri(*(results.crbegin() - offset));
}

Http::Uri Http::Uri::absolutePath() const
{
    auto results = std::move(tokenize(uri.begin(), uri.end(), '?'));
    return Uri(results[0]);
}

std::vector<std::string> Http::Uri::segments() const
{
    auto results = std::move(tokenize(uri.begin(), uri.end(), '?'));

    results = std::move(tokenize(results[0].begin(), results[0].end(), '/'));

    return results;
}

Http::Server::Server(const std::string& host, const std::string& port, ServicePtr service, StrategyPtr globalHandler) :
    service(std::move(service)),
    acceptor(*this->service),
    signals(*this->service),
    globalHandler(std::move(globalHandler))//(handler)
{
    signals.add(SIGINT); // nie wspierane na Windows
    signals.add(SIGTERM);
#if defined(SIGQUIT)
    signals.add(SIGQUIT);
#endif // defined(SIGQUIT)

    Tcp::Endpoint endpoint = this->service->getFactory()->resolve(host, port);
    acceptor.open(endpoint.protocol());
    acceptor.setOption(Tcp::Option::ReuseAddress(true));
    acceptor.bind(endpoint.address());
    acceptor.listen(50);
}

Http::Server::Server(const std::string & host, const std::string & port, RequestHandler handler, ServicePtr service) : Server(host, port, std::move(service), std::unique_ptr<HandlerStrategy>(new ThreadedHandlerStrategy(std::move(handler))))
{
}

int Http::Server::run()
{
    try
    {
        for (;;)
            accept();
    }
    catch (const Tcp::AcceptError&)
    {
        return 0;
    }
}

void Http::Server::accept()
{
    auto socket = acceptor.accept();
    {
        globalHandler->start(std::make_shared<Connection>(std::move(socket), *globalHandler));
    }
}


Http::ConnectionPool::ConnectionPool(std::size_t maxLoadPerThread) : ConnectionPool(std::thread::hardware_concurrency(), maxLoadPerThread)
{
}

Http::ConnectionPool::ConnectionPool(std::size_t maxThreads, std::size_t maxLoadPerThread) : maxThreads(maxThreads), maxLoad(maxLoadPerThread), stop(false)
{
    for (size_t i = 0; i < maxThreads; ++i)
        workers.emplace_back(
            [this]
            {
                for (;;)
                {
                    std::function<void()> work;

                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        condition.wait(lock,
                            [this] { return stop || !jobs.empty(); });
                        if (stop && jobs.empty())
                            return;
                        work = std::move(jobs.front());
                        jobs.pop();
                    }

                    work();
                }
            }
        );
}

Http::ConnectionPool::~ConnectionPool()
{
    {
        std::unique_lock<std::mutex> lock(mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
        worker.join();
}

bool Http::ConnectionPool::add(RequestHandler handler)
{
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (stop)
            throw std::runtime_error("attempted to add to stopped pool");

        if (jobs.size() > maxLoad)
            return false;

        jobs.emplace(std::move(handler));
    }
    condition.notify_one();

    return true;
}
