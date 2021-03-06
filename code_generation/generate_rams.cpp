#include "code_generators.h"
#include "Type.h"
#include "../common/csv_parser.h"
#include <sstream>
#include <algorithm>
#include <iostream>

//address,size,type,name
const std::vector<std::string> wram_order = { "address", "size", "type", "name", };
static const char * const hash_key = "generate_rams";
static const char * const date_string = __DATE__ __TIME__;

#define DECLARE_ENUM_IN_MAP(x) { #x, [](){ return std::make_unique<EnumUint>(#x, 1); } }

template <typename T>
basic_type_constructor create_constructor(){
	return [](){
		return std::make_unique<T>();
	};
}

typemap_t declare_default_types(){
	return {
		{ "u8",                [](){ return std::make_unique<TypeUint>(1); } },
		{ "u16",               [](){ return std::make_unique<TypeUint>(2); } },
		{ "u24",               [](){ return std::make_unique<TypeUint>(3); } },
		{ "big_u16",           [](){ return std::make_unique<TypeUint>(2, true); } },
		{ "big_u24",           [](){ return std::make_unique<TypeUint>(3, true); } },
		{ "big_u32",           [](){ return std::make_unique<TypeUint>(4, true); } },
		{ "bcd4",              [](){ return std::make_unique<TypeBcdInt>(4); } },
		{ "bcd6",              [](){ return std::make_unique<TypeBcdInt>(6); } },
		{ "options",           [](){ return std::make_unique<PackedBitsWrapper>("UserOptions"); } },
		{ "TwoItemMenuType_t", [](){ return std::make_unique<PackedBitsWrapper>("TwoItemMenuType_wrapper"); } },
		{ "spritestatedata1",  create_constructor<spritestatedata1Struct>() },
		{ "spritestatedata2",  create_constructor<spritestatedata2Struct>() },
		{ "mapspritedata",     create_constructor<mapspritedataStruct>() },
		{ "missableobject",    create_constructor<missableobjectStruct>() },
		{ "pcboxmember",       create_constructor<pcboxmemberStruct>() },
		{ "partymember",       create_constructor<partymemberStruct>() },
		{ "spritedata",        create_constructor<spritedataStruct>() },
		{ "partydata",         create_constructor<partydataStruct>() },
		{ "maindata",          create_constructor<maindataStruct>() },
		{ "boxdata",           create_constructor<boxdataStruct>() },
		{ "spriteobject",      create_constructor<spriteobjectStruct>() },
		{ "pcbox",             create_constructor<pcboxStruct>() },
		{ "pointer",           create_constructor<DataPointer>() },
		{ "big_pointer",       create_constructor<DataPointer>() },
		{ "code_pointer",      create_constructor<CodePointer>() },
		DECLARE_ENUM_IN_MAP(SerialConnectionStatus),
		DECLARE_ENUM_IN_MAP(SpeciesId),
		DECLARE_ENUM_IN_MAP(Sound),
		DECLARE_ENUM_IN_MAP(AudioBank),
		DECLARE_ENUM_IN_MAP(MenuType),
		DECLARE_ENUM_IN_MAP(TextBoxId),
		DECLARE_ENUM_IN_MAP(LinkState),
		DECLARE_ENUM_IN_MAP(SaveFileStatus),
		DECLARE_ENUM_IN_MAP(PlayerDirection),
		DECLARE_ENUM_IN_MAP(PlayerDirectionBitmap),
		DECLARE_ENUM_IN_MAP(MapId),
		DECLARE_ENUM_IN_MAP(NpcMovementDirection),
		DECLARE_ENUM_IN_MAP(MenuExitMethod),
		DECLARE_ENUM_IN_MAP(MoveId),
		DECLARE_ENUM_IN_MAP(PartyMenuHpColor),
		DECLARE_ENUM_IN_MAP(NamingScreenType),
	};
}

struct xram{
	const char *file_name;
	const char *class_name;
	unsigned base_address;
};

static std::string get_input_path(const xram &x){
	return (std::string)"input/" + x.file_name + ".csv";
}

static std::string get_output1_path(const xram &x){
	return (std::string)"output/" + x.file_name + ".h";
}

static std::string get_output2_path(const xram &x){
	return (std::string)"output/" + x.file_name + ".inl";
}

static void generate_xram(const typemap_t &typemap, const xram &x){
	std::string input = get_input_path(x);
	std::string output1 = get_output1_path(x);
	std::string output2 = get_output2_path(x);

	CsvParser csv(input.c_str());
	std::ofstream output_h(output1);
	std::ofstream output_cpp(output2);

	output_h   << generated_file_warning << "\n";
	output_cpp << generated_file_warning << "\n";

	const auto rows = csv.row_count();
	bool first = true;
	for (size_t i = 0; i < rows; i++){
		auto row = csv.get_ordered_row(i, wram_order);
		try{
			if (row[1] == "marker" || !row[2].size())
				continue;
			auto address = hex_no_prefix_to_unsigned(row[0]);
			std::unique_ptr<Number> size(new CompoundNumber(row[1]));
			auto type = parse_string_to_Type(typemap, row[2]);
			auto name = row[3];

			type->class_name = x.class_name;

			output_h
				<< "//Address: " << row[0] << std::endl
				<< type->generate_declarations(name);
			if (first)
				first = false;
			else
				output_cpp << ",\n";
			output_cpp << type->generate_initializer(address, x.base_address, size, name);

		}catch (std::exception &e){
			std::stringstream stream;
			stream << "Error while processesing line " << i + 2 << " of " << input << ": " << e.what();
			throw std::runtime_error(stream.str());
		}
	}
}

template <size_t N>
std::vector<std::string> to_input_files(const xram (&xrams)[N]){
	std::vector<std::string> ret;
	for (auto &x : xrams)
		ret.push_back(get_input_path(x));
	return ret;
}

static void declare_bitmaps(typemap_t &typemap, const bitmaps_declarations_t &declarations){
	for (auto &d : declarations){
		auto class_name = d.class_name;
		typemap[d.type_name] = [class_name](){ return std::make_unique<PackedBitsWrapper>(class_name); };
	}
}

void generate_rams(known_hashes_t &known_hashes, const generate_bitmaps_result &bitmaps_result){
	static const xram xrams[] = {
		{ "wram",        "WRam",       0xC000 },
		{ "hram",        "HRam",       0xFF80 },
		{ "main_data",   "MainData",   0xD2F7 },
		{ "sprite_data", "SpriteData", 0xC100 },
		{ "party_data",  "PartyData",  0xD163 },
		{ "box_data",    "BoxData",    0xDA80 },
	};

	try{
		auto current_hash = hash_files(to_input_files(xrams), date_string);
		if (check_for_known_hash(known_hashes, hash_key, current_hash) && !bitmaps_result.changed){
			std::cout << "Skipping generating bitmaps.\n";
			std::cout << "Skipping generating RAMs.\n";
			return;
		}
		std::cout << "Generating RAMs...\n";

		auto typemap = declare_default_types();
		declare_bitmaps(typemap, bitmaps_result.function());
		for (auto &x : xrams)
			generate_xram(typemap, x);

		known_hashes[hash_key] = current_hash;
	}catch (std::exception &e){
		throw std::runtime_error((std::string)"generate_rams(): " + e.what());
	}
}
