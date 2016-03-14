#pragma once
#include "was/storage_account.h"
#include "was/blob.h"
#include <string>
#include <vector>
#include <sstream>
#include <time.h>

/// Klasa obsluguj�ca komunikacj� z Azure
/**
* W chwili obecnej pozwala na wys�anie i odebranie pliku z Azure Cloud
*/
class AzureStorageManager
{
public:
    /// Domy�lny konstruktor
    /**
    * Nadaje domy�lne warto�ci dla p�l accountName, containerName oraz accountKey
    */
    AzureStorageManager();


    /// Konstruktor z parametrami
    /**
    * Ustawia warto�ci accountName, containerName oraz accountKey na podstawie otrzymanych parametr�w
    */
    AzureStorageManager(std::string _accountName, std::string _containerName, std::string _accountKey);


    /// Domy�lny destruktor
    ~AzureStorageManager();


    /// Metoda do pobierania zdj�cia z serwera Azure
    /**
    * Pobiera z serwera plik o podanej w parametrze nazwie
    * true je�li uda�o si� pobra� zdj�cie lub false kiedy wyst�pi� jaki� b��d
    * Ustawia pole temporaryFileName na nazw� tymczasowego pliku zapisanego lokalnie
    */
    bool downloadFromServer(std::string _fileAddr);


    /// Metoda do wysy�ania nowego pliku na serwer Azure
    /**
    * Wysy�a na serwer azure plik o �cie�ce (pe�na �cie�ka np "tmp//plik.png") podanej w parametrze
    * Zwraca adres do tego pliku
    */
    std::string uploadToServer(std::string _path);


    ///Zwraca nazw� ostatnio pobranego pliku tymczasowego
    std::string getTemporaryFileName();


private:
    /// Nazwa konta Azure
    std::string accountName;
    /// Nazwa kontenera Azure
    std::string containerName;
    /// Klucz konta Azure
    std::string accountKey;
    /// Nazwa pliku tymczasowego pobranego z serwera
    std::string temporaryFileName;
};

