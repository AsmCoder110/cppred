#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <map>
#include <string>
#include <stdexcept>
#include <sstream>
#include <utility>

typedef std::uint8_t byte_t;
typedef std::string (*command_handler)(const std::string &);

std::pair<std::string, std::string> charmap[] = {
	{ "\\\\",       "\\\\01" },
	{ "<POKE>",     "\\\\02" },
	{ "<pkmn>",     "\\\\03" },
	{ "<PLAYER>",   "\\\\04" },
	{ "<RIVAL>",    "\\\\05" },
	{ "<USER>",     "\\\\06" },
	{ "<TARGET>",   "\\\\07" },
	{ "<CURRENCY>", "\\\\08" },
	{ "'d",         "\\\\09" },
	{ "'l",         "\\\\0A" },
	{ "'s",         "\\\\0B" },
	{ "'t",         "\\\\0C" },
	{ "'v",         "\\\\0D" },
	{ "'r",         "\\\\0E" },
	{ "'m",         "\\\\0F" },
};

std::string handle_mem(const std::string &input){
	std::string ret;
	auto space = input.find_first_of(" \t");
	ret += " << MEM(";
	ret += input.substr(0, space);
	ret += ")";
	return ret;
}

std::string handle_num(const std::string &input){
	std::stringstream temp(input);
	std::string name;
	int first = 0, second = 0;
	temp >> name >> first >> second;
	std::stringstream ret;
	ret << " << NUM(" << name << ", " << first << ", " << second << ")";
	return ret.str();
}

std::string handle_bcd(const std::string &input){
	std::stringstream temp(input);
	std::string name;
	int first = 0;
	temp >> name >> first;
	std::stringstream ret;
	ret << " << BCD(" << name << ", " << first << ")";
	return ret.str();
}

std::string handle_dex(const std::string &){
	return " << DEX\n";
}

bool is_hex(char c){
	return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

std::string filter_text(const std::string &input){
	std::string ret;
	for (size_t i = 0; i < input.size(); ){
		auto c = input[i];
		if (c == '@'){
			i++;
			continue;
		}
		bool Continue = false;
		for (auto &p : charmap){
			if (p.first.size() > input.size() - i)
				continue;
			if (p.first != input.substr(i, p.first.size()))
				continue;
			ret += p.second;
			i += p.first.size();
			Continue = true;
			break;
		}
		if (Continue)
			continue;
		if (c == '<'){
			if (i + 2 > input.size())
				throw std::runtime_error("Syntax error: string can't contain '<': " + input);
			if (input[i + 1] != '$')
				throw std::runtime_error("Missed a case: " + input);
			if (i + 5 > input.size())
				throw std::runtime_error("Syntax error: Incomplete <$XX> sequence: " + input);
			auto a = input[i + 2];
			auto b = input[i + 3];
			if (!is_hex(a) || !is_hex(b) || input[i + 4] != '>')
				throw std::runtime_error("Syntax error: Invalid <$XX> sequence: " + input);
			if (a == '0' && b == '0')
				throw std::runtime_error("Internal error: Can't represent <$00>: " + input);
			ret += "\\\\x\\x";
			ret += (char)toupper(a);
			ret += (char)toupper(b);
			i += 5;
			continue;
		}
		ret += c;
		i++;
	}
	return ret;
}

std::string handle_text(const std::string &input0){
	auto input = input0;
	if (input.size() < 2 || input[0] != '"')
		throw std::runtime_error("Syntax error: A string must be delimited by \".");
	auto last_quote = input.find_last_not_of(" \t");
	if (last_quote == input.npos || input[last_quote] != '"')
		throw std::runtime_error("Syntax error: A string must be delimited by \".");
	input.assign(input.begin() + 1, input.begin() + last_quote);

	auto filtered = filter_text(input);
	if (!filtered.size())
		return "";

	return " << \"" + filtered + "\"";
}

std::string handle_para(const std::string &input){
	return " << PARA\n" + handle_text(input);
}

std::string handle_line(const std::string &input){
	return " << LINE\n" + handle_text(input);
}

std::string handle_cont(const std::string &input){
	return " << CONT\n" + handle_text(input);
}

std::string handle_done(const std::string &){
	return " << DONE\n";
}

std::string handle_next(const std::string &input){
	return " << NEXT\n" + handle_text(input);
}

std::string handle_page(const std::string &input){
	return " << PAGE\n" + handle_text(input);
}

std::string handle_label(const std::string &input){
	return "SEQ(" + input + ")\n";
}

std::string handle_prompt(const std::string &){
	return " << PROMPT\n";
}

std::map<std::string, command_handler> command_handlers = {
	{ ".",      handle_label  },
	{ "TEXT",   handle_text   },
	{ "LINE",   handle_line   },
	{ "CONT",   handle_cont   },
	{ "DONE",   handle_done   },
	{ "MEM",    handle_mem    },
	{ "PROMPT", handle_prompt },
	{ "PARA",   handle_para   },
	{ "NUM",    handle_num    },
	{ "BCD",    handle_bcd    },
	{ "NEXT",   handle_next   },
	{ "PAGE",   handle_page   },
	{ "DEX",    handle_dex    },
};

std::string parse_text_format(std::istream &stream){
	std::string ret;
	std::string current_label;
	unsigned line_no = 0;
	bool first = true;
	while (true){
		line_no++;
		std::string line;
		std::getline(stream, line);
		if (!stream)
			break;
		if (!line.size())
			continue;

		if (line[0] == '.'){
			if (first)
				first = false;
			else
				ret += ";\n\n";
			current_label = line.substr(1);
			ret += command_handlers["."](current_label);
			continue;
		}
		
		auto first_space = line.find(' ');
		auto it = command_handlers.find(line.substr(0, first_space));
		if (it == command_handlers.end()){
			std::stringstream ss;
			ss << "Don't know how to parse line " << line_no << ": " << line;
			throw std::runtime_error(ss.str());
		}

		auto first_non_space = line.find_first_not_of(" \t", first_space);
		if (first_non_space == line.npos)
			first_non_space = line.size();
		ret += it->second(line.substr(first_non_space));
	}
	return ret;
}

int main(){
	std::string output;
	try{
		std::ifstream input("input/text.txt");
		if (!input)
			throw std::runtime_error("input/text.txt not found!");
		output = parse_text_format(input);
	}catch (std::exception &e){
		std::cerr << e.what() << std::endl;
		return -1;
	}
	std::ofstream file("output/text.cpp");
	file << output;
	return 0;
}
