// GSI_Teasts.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "Simple-Web-Server-master/client_http.hpp"

using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;
int main() {
    HttpClient httpClient("localhost:1337");

	try {
		auto r2 = httpClient.request("POST", "/string", "posting DASDASDASDASDASDASDASD");
		std::cout << r2->content.rdbuf() << std::endl;
	}
	catch (const SimpleWeb::system_error & e) {
		std::cerr << "Client request error: " << e.what() << std::endl;
	}
}