#ifndef PATR_SEGMENTATION_RESPONSE_H
#define PATR_SEGMENTATION_RESPONSE_H

#include <utility>
#include <string>

namespace cv {
    class Mat;
}
namespace Router {
    class RequestRouter;
}

/// Zwraca obrazek ze �cie�ki wzgl�dnej od po�o�enia pliku wykonywalnego.
cv::Mat GetImageLocal(const std::string& path);
/// Zwraca obrazek z azure storage.
cv::Mat GetImageFromAzure(const std::string& url);
/// Tworzy odpowied� na zapytanie o segmentacj� obrazka.
std::pair<std::string, int> SegmentationResponse(const std::string& body, cv::Mat(*GetImageaFromString)(const std::string&));
/// Dodaje odpowied� na segmentacj� obrazka do Router::RequestRouter.
void registerSegmentationResponse(Router::RequestRouter& router);

#endif // PATR_SEGMENTATION_RESPONSE_H
