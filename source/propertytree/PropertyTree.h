#include <string>
#include <tuple>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <stdexcept>
#include <typeinfo>
#include <sstream>

/// Type trait sprawdza czy typ jest �a�cuchem znak�w
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
 * �eby podlega� klasyfikacji, musi zawiera� metody begin i end
 * oraz zwraca� const_iterator zdefiniowany jako typedef.
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

/// S�u�y do konwersji PropertyTree na warto�ci typu T.
template<typename T, typename = void>
struct PropertyTreeInputConverter;

/// S�u�y do konwersji warto�ci typu T na PropertyTree.
template<typename T, typename = void>
struct PropertyTreeOutputConverter;

/// Klasa opisuj�ca drzewo parametr�w.
/**
 * Drzewo zawiera wektor poddrzew (ma potomk�w) albo warto�� zapisan� jako std::string (jest li�ciem).
 * Zawiera 3 metody:
 *   find() - zwraca poddrzewo odpowiadaj�ce podanemu kluczowi.
 *   get<T>() - zwraca warto�� drzewa jako okre�lony typ.
 *   put() - wstawia warto�� do drzewa.
 *
 * Dost�p do element�w drzewa odbywa si� za pomoc� etykiet.
 * Etykieta to ci�g znak�w delimitowany znakiem '.', kt�ry oddziela
 * poziomy drzewa.
 * Na przyk�ad:
 *   "foo" powoduje wyszukanie poddrzewa o nazwie "foo".
 *   "foo.bar" powoduje wyszukanie poddrzewa o nazwie "foo" a w nim
 *   poddrzewa o nazwie "bar".
 *   Je�eli drzewo jest typu Array dost�p do poszczeg�lnych element�w uzyskany jest poprzez
 *   znak '.' na ko�cu etykiety, np.: "foo.bar." oznacza element tablicy bar z poddrzewa foo.
 *   Dost�p do poszczeg�lnych element�w tablic uzyskiwany jest za pomoc� warto�ci przekazywanych
 *   jako kolejne argumenty do funkcji wyszukuj�cych.
 * Kod:
 *   tree.find("foo.bar.baz.", 2, 3) // spowoduje zwr�cenie 3. elementu tablicy "baz" b�d�cej 2. elementem
 *   tablicy "bar", kt�ra z kolei jest elementem drzewa foo, kt�ry nie jest tablic�, zatem indeksowanie
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
    
    /// Podstawowe rozr�nialne typy.
    /**
     * Domy�lnym typem jest Object.
     * Za ustawianie typ�w odpowiedzialny jest u�ytkownik lub
     * klasa wykonuj�ca konwersj� do drzewa.
     */
    enum Type
    {
        Null,
        Boolean,
        Value, // warto�� liczbowa.
        String,
        Array,
        Object // warto�� domy�lna.
    };

    /// Tworzy nowy obiekt z domy�lnym typem warto�ci.
    PropertyTree() : valueType(Type::Object)
    {
    }

    /// Zwraca warto�� li�cia lub pusty �a�cuch je�eli nie jest li�ciem.
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

    /// Sprawdza, czy drzewo zawiera potomk�w.
    /**
     * @return true, je�eli jest li�ciem, false w przeciwnym wypadku.
     */
    bool empty() const
    {
        if (valueType == Type::Array)
            return false;
        return begin() == end();
    }

    /// Zwraca iterator do potomk�w.
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

    /// Grupa metod odpowiedzialnych za pobranie warto�ci w okre�lonym typie.
    /**
     * Zwracaj� warto�ci otwrzymane od obiektu Converter dla danego typu.
     * Domy�lnym typem obiektu Converter jest PropertyTreeInputConverter<T>,
     * kt�ry zawiera podstawowe specjalizacje oraz posiada mo�liwo�ci rozbudowy przez u�ytkownika.
     * Dost�p do zagnie�d�onych element�w uzyska� mo�na poprzez wykorzystanie etykiet (p. etykiety w opisie klasy).
     */

    /// Zwraca warto�� typu std::string, je�eli jest li�ciem.
    /**
     * Metoda wywo�uje Converter::operator()(const std::string&).
     * Je�eli drzewo nie jest li�ciem (albo nie ma warto�ci), rzuca wyj�tek std::bad_cast.
     */
    template<typename T = DataType, typename Converter = PropertyTreeInputConverter<T>, typename std::enable_if<std::is_same<T, KeyType>::value>::type* = nullptr>
    T get(Converter&& c = PropertyTreeInputConverter<T>()) const
    {
        if (!data.empty())
        {
            return c(data);
        }
        else
        {
            throw std::bad_cast();
        }
    }

    /// Zwraca warto�� typu T, gdzie T nie jest std::string.
    /**
     * Metoda wywo�uje Converter::operator()(const PropertyTree&).
     */
    template<typename T = DataType, typename Converter = PropertyTreeInputConverter<T>, typename std::enable_if<!std::is_same<T, KeyType>::value && !is_string<Converter>::value>::type* = nullptr>
    T get(Converter&& c = PropertyTreeInputConverter<T>()) const
    {
        return c(*this);
    }

    /// Grupa metod odpowiedzialnych za zwr�cenie poddrzewa zgodnie z zadanymi parametrami.
    /**
     * Wykorzystanie funkcji jest analogiczne do metod get().
     * Dost�p do zagnie�d�onych element�w uzyska� mo�na poprzez wykorzystanie etykiet (p. etykiety w opisie klasy).
     */

    /// Zwraca poddrzewo dla podanej etykiety.
    const PropertyTree& find(const KeyType& label) const
    {
        return find(label, 0);
    }

    /// Zwraca poddrzewo dla podanej etykiety, zgodnie z zadanymi indeksami element�w. 
    template<typename... Indices>
    const PropertyTree& find(const KeyType& label, std::size_t index, Indices&&... indices) const
    {
        return findImpl(label, 0, index, std::forward<Indices>(indices)...);
    }

    PropertyTree& find(const KeyType& label, std::size_t index = 0)
    {
        return const_cast<PropertyTree&>(const_cast<const PropertyTree&>(*this).find(label, index));
    }

    /// Grupa metod odpowiedzialnych za dodanie warto�ci do drzewa.
    /**
    * Dodaj� warto�ci otwrzymane od obiektu Converter dla danego typu.
    * Domy�lnym typem obiektu Converter jest PropertyTreeOutputConverter<T>,
    * kt�ry zawiera podstawowe specjalizacje oraz posiada mo�liwo�ci rozbudowy przez u�ytkownika.
    * Converter jest obiektem spe�niaj�cym std::is_function<U> przyjmuj�cym warto�� T i zwracaj�cym
    * drzewo PropertyTree.
    */

    /// Dodaje poddrzewo do drzewa.
    /**
     * Operacja odbywa si� bez dodatkowych obiekt�w konwertuj�cych.
     */
    PropertyTree& put(const KeyType& key, const PropertyTree& tree)
    {
        children.push_back(std::make_pair(key, tree));
        return *this;
    }

    /// Dodaje warto�� typu std::string do drzewa.
    /**
     * Tworzy now� par� "opis":"warto��".
     */
    PropertyTree& put(const KeyType& key, const DataType& value)
    {
        PropertyTree tree;
        tree.put(value);
        children.push_back(std::make_pair(key, tree));
        return *this;
    }

    /// Dodaje warto�� do drzewa.
    /**
     * Warto�� podlega konwersji poprzez obiekt typu Converter.
     */
    template<typename T, typename Converter = PropertyTreeOutputConverter<T>>
    PropertyTree& put(const KeyType& key, const T& value, Converter&& c = PropertyTreeOutputConverter<T>())
    {
        children.push_back(std::make_pair(key, c(value)));
        return *this;
    }

    /// Dodaje poddrzewo do drzewa z pust� nazw�.
    PropertyTree& put(const PropertyTree& tree)
    {
        put("", tree);
        return *this;
    }

    /// Podmienia drzewo na takie wynikaj�ca z konwersji przez obiekt Converter.
    /**
     * Istotna jest r�nica mi�dzy put(PropertyTree) a put(T), poniewa� to pierwsze dodaje poddrzewo,
     * a drugie podmienia obecne drzewo.
     */
    template<typename T, typename Converter = PropertyTreeOutputConverter<T>, typename std::enable_if<std::is_function<Converter>::value>::type>
    PropertyTree& put(const T& data, Converter&& c = PropertyTreeOutputConverter<T>())
    {
        //Converter c;
        *this = std::move(c(data));
        return *this;
    }

    /// Ustawia warto�� drzewa na dany �a�cuch znak�w.
    /**
     * Istotna jest r�nica mi�dzy innymi jednoargumentowyni funkcjami put() ze wzgl�du na to,
     * �e ta powoduje podmian� charakterystyczn� dla li�ci.
     * R�nice te wynikaj� z trzymania w jednym w�le danych o potomkach i danych w�asnych charakterystycznych
     * dla li�ci.
     */
    PropertyTree& put(const std::string& data)
    {
        this->data = data;
        return *this;
    }

    /// Ustawia warto�� drzewa na dany �a�cuch znak�w.
    /**
     * p. put(std::string).
     * Przeci��enie potrzebne aby unikn�� niejednoznaczno�ci w kompilacji.
     */
    PropertyTree& put(const char* data)
    {
        this->data = data;
        return *this;
    }

private:

    /// Wrapper do metody wyszukuj�cej dla ignorowanych indeks�w.
    const PropertyTree& findImpl(const KeyType& label) const
    {
        return findImpl(label, 0);
    }

    /// Implementacja metody wyszukuj�cej.
    /**
     * W przypadku wyj�cia poza zasi�g rzuca wyj�tek std::out_of_range.
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

/// Type trait do sprawdzenia czy klasa zawiera metod� T::serialize.
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

/// Type trait do sprawdzenia czy klasa zawiera metod� T::deserialize.
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

/// Og�lna klasa do konwersji li�ci drzew na typ T.
/**
 * Do konwersji wykorzystuje std::istringstream.
 * Aby typ by� konwertowalny przez t� klas� (bez specjalizacji),
 * musi przeci��a� operator >>(std::istream&, T).
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

/// Specjalizacja wykorzystuj�ca metod� deserialize() typu T.
/**
 * Klasy w pe�ni specjalizuj�ce klas� PropertyTreeInputConverter
 * maj� pierwsze�stwo przed t� specjalizacj�.
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

/// Specjalizacja dla �a�cuch�w znakowych.
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

/// Og�lna klasa do konwersji typ�w typ PropertyTree (li��).
/**
* Do konwersji wykorzystuje std::ostringstream.
* Aby typ by� konwertowalny przez t� klas� (bez specjalizacji),
* musi przeci��a� operator <<(std::ostream&, T).
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

/// Specjalizacja wykorzystuj�ca metod� serialize() typu T.
/**
* Klasy w pe�ni specjalizuj�ce klas� PropertyTreeOutputConverter
* maj� pierwsze�stwo przed t� specjalizacj�.
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
 * Ustawia typ warto�ci drzewa na PropertyTree::Type::Boolean.
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
* Ustawia typ warto�ci drzewa na PropertyTree::Type::Null.
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

/// Specjalizacja dla kontener�w.
/**
 * p. definicja type trait is_container.
 * Konstruuje poddrzewo z warto�ciami kontenera.
 * Ustawia typ na PropertyTree::Type::Array.
 */
template<typename T>
struct PropertyTreeOutputConverter<T, typename std::enable_if<is_container<T>::value>::type>
{
    PropertyTree operator ()(const T& data)
    {
        PropertyTree tree;
        tree.type() = PropertyTree::Type::Array;

        for (auto& val : data)
        {
            tree.put("", val);
        }

        return tree;
    }
};

inline bool operator ==(const PropertyTree& lhs, const PropertyTree& rhs)
{
    if (lhs.empty())
        return lhs.get<std::string>() == rhs.get<std::string>();

    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

