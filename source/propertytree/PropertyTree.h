#include <string>
#include <tuple>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <stdexcept>
#include <typeinfo>
#include <sstream>

/// Type trait sprawdza czy typ jest łańcuchem znaków
template <typename T>
struct is_string
    : public std::integral_constant<bool,
    std::is_same<      char *, typename std::decay<T>::type>::value ||
    std::is_same<const char *, typename std::decay<T>::type>::value
    > {};
template <>
struct is_string<std::string> : std::true_type {};

/// Type trait sprawdza czy typ jest kontenerem.
/**
 * Żeby podlegał klasyfikacji, musi zawierać metody begin i end
 * oraz zwracał const_iterator zdefiniowany jako typedef.
 */
template <typename T>
struct is_container
{
    template <typename U, typename it_t = typename U::const_iterator >
    struct sfinae
    {
        //typedef typename U::const_iterator it_t;
        template < typename V, typename IT, IT(U::*)() const, IT(U::*)() const >
        struct type_ {};

        typedef type_<U, it_t, static_cast<it_t(U::*)() const>(&U::begin), static_cast<it_t(U::*)() const>(&U::end)> type;
    };

    template <typename U> static char test(typename sfinae<U>::type*);
    template <typename U> static long test(...);

    enum { value = (1 == sizeof test<T>(0)) };
};

/// Służy do konwersji PropertyTree na wartości typu T.
template<typename T, typename = void>
struct PropertyTreeInputConverter;

/// Służy do konwersji wartości typu T na PropertyTree.
template<typename T, typename = void>
struct PropertyTreeOutputConverter;

/// Klasa opisująca drzewo parametrów.
/**
 * Drzewo zawiera wektor poddrzew (ma potomków) albo wartość zapisaną jako std::string (jest liściem).
 * Zawiera 3 metody:
 *   find() - zwraca poddrzewo odpowiadające podanemu kluczowi.
 *   get<T>() - zwraca wartość drzewa jako określony typ.
 *   put() - wstawia wartość do drzewa.
 *
 * Dostęp do elementów drzewa odbywa się za pomocą etykiet.
 * Etykieta to ciąg znaków delimitowany znakiem '.', który oddziela
 * poziomy drzewa.
 * Na przykład:
 *   "foo" powoduje wyszukanie poddrzewa o nazwie "foo".
 *   "foo.bar" powoduje wyszukanie poddrzewa o nazwie "foo" a w nim
 *   poddrzewa o nazwie "bar".
 *   Jeżeli drzewo jest typu Array dostęp do poszczególnych elementów uzyskany jest poprzez
 *   znak '.' na końcu etykiety, np.: "foo.bar." oznacza element tablicy bar z poddrzewa foo.
 *   Dostęp do poszczególnych elementów tablic uzyskiwany jest za pomocą wartości przekazywanych
 *   jako kolejne argumenty do funkcji wyszukujących.
 * Kod:
 *   tree.find("foo.bar.baz.", 2, 3) // spowoduje zwrócenie 3. elementu tablicy "baz" będącej 2. elementem
 *   tablicy "bar", która z kolei jest elementem drzewa foo, który nie jest tablicą, zatem indeksowanie
 *   nie ma mocy.
 */
class PropertyTree
{
public:
    typedef std::string KeyType;
    typedef std::string DataType;
    typedef std::vector<std::pair<KeyType, PropertyTree>> Children;
    typedef Children::iterator Iterator;
    typedef Children::const_iterator ConstIterator;
    
    /// Podstawowe rozróżnialne typy.
    /**
     * Domyślnym typem jest Object.
     * Za ustawianie typów odpowiedzialny jest użytkownik lub
     * klasa wykonująca konwersję do drzewa.
     */
    enum Type
    {
        Null,
        Boolean,
        Value, // wartość liczbowa.
        String,
        Array,
        Object // wartość domyślna.
    };

    /// Tworzy nowy obiekt z domyślnym typem wartości.
    PropertyTree() : valueType(Type::Object)
    {
    }

    /// Zwraca wartość liścia lub pusty łańcuch jeżeli nie jest liściem.
    const std::string& string() const
    {
        return data;
    }

    std::string& string()
    {
        return data;
    }

    /// Zwraca typ drzewa.
    Type& type()
    {
        return valueType;
    }

    const Type& type() const
    {
        return valueType;
    }

    /// Sprawdza, czy drzewo zawiera potomków.
    /**
     * @return true, jeżeli jest liściem, false w przeciwnym wypadku.
     */
    bool empty() const
    {
        if (valueType == Type::Array)
            return false;
        return begin() == end();
    }

    /// Zwraca ilość poddrzew.
    std::size_t size() const
    {
        return children.size();
    }

    /// Zwraca iterator do potomków.
    Iterator begin()
    {
        return children.begin();
    }

    ConstIterator begin() const
    {
        return children.begin();
    }

    Iterator end()
    {
        return children.end();
    }

    ConstIterator end() const
    {
        return children.end();
    }

    /// Grupa metod odpowiedzialnych za pobranie wartości w określonym typie.
    /**
     * Zwracają wartości otwrzymane od obiektu Converter dla danego typu.
     * Domyślnym typem obiektu Converter jest PropertyTreeInputConverter<T>,
     * który zawiera podstawowe specjalizacje oraz posiada możliwości rozbudowy przez użytkownika.
     * Dostęp do zagnieżdżonych elementów uzyskać można poprzez wykorzystanie etykiet (p. etykiety w opisie klasy).
     */

    /// Zwraca wartość typu std::string, jeżeli jest liściem.
    /**
     * Metoda wywołuje Converter::operator()(const std::string&).
     * Jeżeli drzewo nie jest liściem (albo nie ma wartości), rzuca wyjątek std::bad_cast.
     */
    template<typename T = DataType, typename Converter = PropertyTreeInputConverter<T>, typename std::enable_if<std::is_same<T, KeyType>::value>::type* = nullptr>
    T get(Converter&& c = PropertyTreeInputConverter<T>()) const
    {
        if (type() == String || !data.empty())
        {
            return c(data);
        }
        else
        {
            throw std::bad_cast();
        }
    }

    /// Zwraca wartość typu T, gdzie T nie jest std::string.
    /**
     * Metoda wywołuje Converter::operator()(const PropertyTree&).
     */
    template<typename T = DataType, typename Converter = PropertyTreeInputConverter<T>, typename std::enable_if<!std::is_same<T, KeyType>::value && !is_string<Converter>::value>::type* = nullptr>
    T get(Converter&& c = PropertyTreeInputConverter<T>()) const
    {
        return c(*this);
    }

    /// Grupa metod odpowiedzialnych za zwrócenie poddrzewa zgodnie z zadanymi parametrami.
    /**
     * Wykorzystanie funkcji jest analogiczne do metod get().
     * Dostęp do zagnieżdżonych elementów uzyskać można poprzez wykorzystanie etykiet (p. etykiety w opisie klasy).
     */

    /// Zwraca poddrzewo dla podanej etykiety.
    const PropertyTree& find(const KeyType& label) const
    {
        return find(label, 0);
    }

    /// Zwraca poddrzewo dla podanej etykiety, zgodnie z zadanymi indeksami elementów. 
    template<typename... Indices>
    const PropertyTree& find(const KeyType& label, std::size_t index, Indices&&... indices) const
    {
        return findImpl(label, 0, index, std::forward<Indices>(indices)...);
    }

    PropertyTree& find(const KeyType& label, std::size_t index = 0)
    {
        return const_cast<PropertyTree&>(const_cast<const PropertyTree&>(*this).find(label, index));
    }

    /// Grupa metod odpowiedzialnych za dodanie wartości do drzewa.
    /**
    * Dodają wartości otwrzymane od obiektu Converter dla danego typu.
    * Domyślnym typem obiektu Converter jest PropertyTreeOutputConverter<T>,
    * który zawiera podstawowe specjalizacje oraz posiada możliwości rozbudowy przez użytkownika.
    * Converter jest obiektem spełniającym std::is_function<U> przyjmującym wartość T i zwracającym
    * drzewo PropertyTree.
    */

    /// Dodaje poddrzewo do drzewa.
    /**
     * Operacja odbywa się bez dodatkowych obiektów konwertujących.
     */
    PropertyTree& put(const KeyType& key, const PropertyTree& tree)
    {
        children.push_back(std::make_pair(key, tree));
        return *this;
    }

    /// Dodaje wartość typu std::string do drzewa.
    /**
     * Tworzy nową parę "opis":"wartość".
     */
    PropertyTree& put(const KeyType& key, const DataType& value)
    {
        PropertyTree tree;
        tree.put(value);
        tree.type() = PropertyTree::Type::String;
        children.push_back(std::make_pair(key, tree));
        return *this;
    }

    /// Dodaje wartość do drzewa.
    /**
     * Wartość podlega konwersji poprzez obiekt typu Converter.
     */
    template<typename T, typename Converter = PropertyTreeOutputConverter<T>>
    PropertyTree& put(const KeyType& key, const T& value, Converter&& c = PropertyTreeOutputConverter<T>())
    {
        children.push_back(std::make_pair(key, c(value)));
        return *this;
    }

    /// Dodaje poddrzewo do drzewa z pustą nazwą.
    PropertyTree& put(const PropertyTree& tree)
    {
        put("", tree);
        return *this;
    }

    /// Podmienia drzewo na takie wynikająca z konwersji przez obiekt Converter.
    /**
     * Istotna jest różnica między put(PropertyTree) a put(T), ponieważ to pierwsze dodaje poddrzewo,
     * a drugie podmienia obecne drzewo.
     */
    template<typename T, typename Converter = PropertyTreeOutputConverter<T>, typename std::enable_if<std::is_function<Converter>::value>::type>
    PropertyTree& put(const T& data, Converter&& c = PropertyTreeOutputConverter<T>())
    {
        //Converter c;
        *this = std::move(c(data));
        return *this;
    }

    /// Ustawia wartość drzewa na dany łańcuch znaków.
    /**
     * Istotna jest różnica między innymi jednoargumentowyni funkcjami put() ze względu na to,
     * że ta powoduje podmianę charakterystyczną dla liści.
     * Różnice te wynikają z trzymania w jednym węźle danych o potomkach i danych własnych charakterystycznych
     * dla liści.
     */
    PropertyTree& put(const std::string& data)
    {
        this->data = data;
        return *this;
    }

    /// Ustawia wartość drzewa na dany łańcuch znaków.
    /**
     * p. put(std::string).
     * Przeciążenie potrzebne aby uniknąć niejednoznaczności w kompilacji.
     */
    PropertyTree& put(const char* data)
    {
        this->data = data;
        return *this;
    }

private:

    /// Wrapper do metody wyszukującej dla ignorowanych indeksów.
    const PropertyTree& findImpl(const KeyType& label) const
    {
        return findImpl(label, 0);
    }

    /// Implementacja metody wyszukującej.
    /**
     * W przypadku wyjścia poza zasięg rzuca wyjątek std::out_of_range.
     */
    template<typename... Indices>
    const PropertyTree& findImpl(const KeyType& label, std::size_t index, Indices&&... indices) const
    {
        auto dot = label.find_first_of('.');
        if (dot != KeyType::npos)
        {
            std::string begin(label.begin(), label.begin() + dot);
            std::string end(label.begin() + dot + 1, label.end());
            auto& tree = findImpl(begin, index);
            if (tree.type() == Type::Array)
            {
                end = '.' + end;
                return tree.findImpl(std::move(end), std::forward<Indices>(indices)...);
            }
            if (label.begin() + dot + 1 == label.end())
                return tree;
            return tree.findImpl(std::move(end), index, std::forward<Indices>(indices)...);
        }

        std::size_t currentIndex = 0;

        for (const auto& pair : children)
        {
            if (pair.first == label)
            {
                if (type() == Type::Array && currentIndex++ == index)
                    return pair.second;
                else if (type() != Type::Array)
                    return pair.second;
            }
        }
        throw std::out_of_range("index out of range");
    }

    Type valueType;
    DataType data;
    Children children;
};

/// Type trait do sprawdzenia czy klasa zawiera metodę T::serialize.
template<typename T>
struct is_serializable
{
    typedef char one;
    typedef long two;

    template <typename C> static one test(decltype(&C::serialize));
    template <typename C> static two test(...);

public:
    enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

/// Type trait do sprawdzenia czy klasa zawiera metodę T::deserialize.
template<typename T>
struct is_deserializable
{
    typedef char one;
    typedef long two;

    template <typename C> static one test(decltype(&C::deserialize));
    template <typename C> static two test(...);

public:
    enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

/// Ogólna klasa do konwersji liści drzew na typ T.
/**
 * Do konwersji wykorzystuje std::istringstream.
 * Aby typ był konwertowalny przez tą klasę (bez specjalizacji),
 * musi przeciążać operator >>(std::istream&, T).
 */
template<typename T, typename>
struct PropertyTreeInputConverter
{
    T operator ()(const std::string& data)
    {
        T t;
        std::istringstream ss(data);
        ss.setf(std::ios_base::boolalpha);
        ss >> t;
        if (ss.fail())
            throw std::bad_cast();
        return t;
    }

    T operator ()(const PropertyTree& data)
    {
        return operator()(data.string());
    }
};

/// Specjalizacja wykorzystująca metodę deserialize() typu T.
/**
 * Klasy w pełni specjalizujące klasę PropertyTreeInputConverter
 * mają pierwszeństwo przed tą specjalizacją.
 */
template<typename T>
struct PropertyTreeInputConverter<T, typename std::enable_if<is_deserializable<T>::value>::type>
{
    T operator ()(const PropertyTree& data)
    {
        T t;
        t.deserialize(data);
        return t;
    }
};

/// Specjalizacja dla łańcuchów znakowych.
template<>
struct PropertyTreeInputConverter<std::string>
{
    std::string operator ()(const PropertyTree& data)
    {
        std::string retval;
        for (auto& val : data)
        {
            retval += val.second.get<std::string>();
        }

        return retval;
    }

    std::string operator ()(const std::string& data)
    {
        return data;
    }
};

/// Ogólna klasa do konwersji typów typ PropertyTree (liść).
/**
* Do konwersji wykorzystuje std::ostringstream.
* Aby typ był konwertowalny przez tą klasę (bez specjalizacji),
* musi przeciążać operator <<(std::ostream&, T).
*/
template<typename T, typename>
struct PropertyTreeOutputConverter
{
    PropertyTree operator ()(const T& data)
    {
        std::ostringstream ss;
        ss << data;
        PropertyTree tree;
        tree.put(ss.str());
        tree.type() = PropertyTree::Type::Value;
        return tree;
    }
};

/// Specjalizacja wykorzystująca metodę serialize() typu T.
/**
* Klasy w pełni specjalizujące klasę PropertyTreeOutputConverter
* mają pierwszeństwo przed tą specjalizacją.
*/
template<typename T>
struct PropertyTreeOutputConverter<T, typename std::enable_if<is_serializable<T>::value>::type>
{
    PropertyTree operator ()(const T& data)
    {
        return data.serialize();
    }
};

/// Specjalizacja dla std::string oraz pochodnych.
template<typename T>
struct PropertyTreeOutputConverter<T, typename std::enable_if<is_string<T>::value>::type>
{
    PropertyTree operator ()(const T& data)
    {
        PropertyTree tree;
        tree.type() = PropertyTree::Type::String;
        tree.put(data);
        return tree;
    }
};

/// Specjalizacja dla typu bool.
/**
 * Ustawia typ wartości drzewa na PropertyTree::Type::Boolean.
 */
template<>
struct PropertyTreeOutputConverter<bool>
{
    PropertyTree operator ()(bool data)
    {
        std::ostringstream ss;
        ss << std::boolalpha << data;
        PropertyTree tree;
        tree.type() = PropertyTree::Type::Boolean;
        tree.put(ss.str());
        return tree;
    }
};

/// Specjalizacja dla typu nullptr_t.
/**
* Ustawia typ wartości drzewa na PropertyTree::Type::Null.
*/
template<typename T>
struct PropertyTreeOutputConverter<T, typename std::enable_if<std::is_null_pointer<T>::value>::type>
{
    PropertyTree operator ()(const T& data)
    {
        PropertyTree tree;
        tree.type() = PropertyTree::Type::Null;
        tree.put("null");
        return tree;
    }
};

/// Specjalizacja dla kontenerów.
/**
 * p. definicja type trait is_container.
 * Konstruuje poddrzewo z wartościami kontenera.
 * Ustawia typ na PropertyTree::Type::Array.
 */
template<typename T>
struct PropertyTreeOutputConverter<T, typename std::enable_if<is_container<T>::value>::type>
{
    PropertyTree operator ()(const T& data)
    {
        PropertyTree tree;
        tree.type() = PropertyTree::Type::Array;

        for (const auto& val : data)
        {
            tree.put("", val);
        }

        return tree;
    }
};

inline bool operator ==(const PropertyTree& lhs, const PropertyTree& rhs)
{
    try
    {
        if (lhs.empty())
        {
            if (rhs.empty())
                if (lhs.string().empty() && rhs.string().empty())
                    return true;
                else
                    return lhs.get<std::string>() == rhs.get<std::string>();
            else
                return false;
        }
    }
    catch (const std::out_of_range&)
    {
        return false;
    }
    catch (const std::bad_cast&)
    {
        return false;
    }

    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}
