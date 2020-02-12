#pragma once

#include <string>
#include <fstream>
#include <map>

class OptionFile {
public:

	bool loadFromFile(std::string filename) {
		std::string line, mark, cont;
		this->data.clear();
		std::ifstream file(filename);
		if (file.fail())
			return false;
		while (!file.eof()) {
			std::getline(file, line);
			if (line[0] == '#')
				continue;
			std::size_t pos = line.find_first_of('=');
			if (pos == std::string::npos)
				continue;
			mark = line.substr(0, pos);
			cont = line.substr(pos + 1, line.length() - pos - 1);
			this->data[mark] = cont;
		}
		return true;
	}

	const std::string& getContent(std::string key) {
		std::map<std::string, std::string>::iterator i;
		if ((i = this->data.find(key)) != this->data.end()) {
			return i->second;
		} else
			return empty;
	}

private:
	std::map<std::string, std::string> data;
	std::string empty;
};
