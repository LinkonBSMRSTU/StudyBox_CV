#ifndef PATR_PROPERTY_TREE_WRITER_H
#define PATR_PROPERTY_TREE_WRITER_H

#include <ostream>

class PropertyTree;

/// Dokonuje zapisu drzewa tree do strumienia stream.
/**
 * @param whitespace true je�eli przy tworzeniu reprezentacji JSON
 * dodawane maj� by� bia�e znaki.
 * @throw std::range_error, je�eli drzewo nie zawiera poprawnych warto�ci dla JSON.
 */
void WriteJson(const PropertyTree& tree, std::ostream& stream, bool whitespace = true);

namespace Json
{
    class Object;
}

/// Dokonuje konwersji do obiektu Json::Object.
/**
 * Mo�e zosta� wykorzystana w specjalizacji PropertyTreeInputConverter<Json::Object>.
 * @throw std::range_error, je�eli drzewo nie zawiera poprawnych warto�ci dla JSON.
 */
Json::Object ToJsonObject(const PropertyTree& tree);

#endif // PATR_PROPERTY_TREE_WRITER_H
