#ifndef _TEXT_BOUNDING_HPP
#define _TEXT_BOUNDING_HPP


#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/opencv.hpp"
using namespace cv;
//Klasa zawieraj�ca (narazie) funkcj�, kt�ra buduje obramowanie wok� blok�w tekstu. 
class ImageProcess
{
public:
	std::vector <Rect> findRectangles(Mat *img);
};
#endif
