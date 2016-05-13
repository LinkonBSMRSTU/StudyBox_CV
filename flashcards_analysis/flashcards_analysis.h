#ifndef FLASHCARDS_ANALYSIS
#define FLASHCARDS_ANALYSIS
#include<string>
#include<vector>
#include "../json/Json.hpp"

class Flashcard
{
	std::string question;
	std::string answer;
	std::vector<std::string> tips;
public:
    Flashcard(std::string q, std::string a, std::vector<std::string> t)
        :question(q)
		, answer(a)
		, tips(t)
	{};
	std::string getQuestion() const;
	std::string getAnswer() const;
	std::vector<std::string> getTips() const;
};

std::vector<Flashcard> flashcardsFromImage(const cv::Mat img);
Json::Array flashcardsToJson(const std::vector<Flashcard> flashcards);

#endif