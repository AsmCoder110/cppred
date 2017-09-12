#include "CppRedMainMenu.h"
#include "CppRed.h"
#include "CppRedSRam.h"
#include "MemoryOperations.h"

CppRedMainMenu::CppRedMainMenu(CppRed &parent): parent(&parent){
}

MainMenuResult CppRedMainMenu::display(){
	auto &wram = this->parent->wram;
	auto &hram = this->parent->hram;

	wram.wMainData.wLetterPrintingDelayFlags.set_raw_value(1);
	wram.wMainData.wOptions = { true, BattleStyle::Shift, TextSpeed::Medium };

	wram.wOptionsInitialized = 0;
	wram.wSaveFileStatus = SaveFileStatus::NoSave;

	if (this->check_for_player_name_in_sram())
		this->parent->call_predef(Predef::LoadSAV);

	auto &text = this->parent->text;

	while (true){
		this->parent->delay_frames(20);
		wram.wLinkState = LinkState::None;
		wram.wPartyAndBillsPCSavedMenuItem = 0;
		wram.wBagSavedMenuItem = 0;
		wram.wBattleAndStartSavedMenuItem = 0;
		wram.wPlayerMoveListIndex = 0;
		wram.wDefaultMap = MapId::PalletTown;
		wram.wMainData.wd72e.set_using_link_cable(false);
		this->parent->prepare_menu();
		wram.wMainData.wd730.set_no_print_delay(true);

		{
			unsigned y;
			CppRedText::Region *region;
			if (wram.wSaveFileStatus == SaveFileStatus::NoSave){
				y = 6;
				region = &text.ContinueText;
			}else{
				y = 4;
				region = &text.NewGameText;
			}
			auto pos = this->parent->get_tilemap_location(0, 0);
			text.text_box_border(pos, 13, y);
			pos = this->parent->get_tilemap_location(2, 2);
			text.place_string(pos, *region);
		}

		wram.wMainData.wd730.set_no_print_delay(false);
		this->parent->update_sprites();

		wram.wCurrentMenuItem = 0;
		wram.wLastMenuItem = 0;
		wram.wMenuJoypadPollCount = 0;
		wram.wTopMenuItemX = 1;
		wram.wTopMenuItemY = 2;
		wram.wMenuWatchedKeys = input_a | input_b | input_start;
		wram.wMaxMenuItem = wram.wSaveFileStatus.value;

		auto input = this->parent->handle_menu_input();
		if (check_flag(input, input_b))
			return MainMenuResult::Cancelled;

		this->parent->delay_frames(20);

		byte_t status = wram.wCurrentMenuItem;
		if (wram.wSaveFileStatus != SaveFileStatus::SaveExists)
			status++;
		if (status){
			if (status == 1)
				return MainMenuResult::NewGame;
			this->display_options_menu();
			wram.wOptionsInitialized = 1;
			continue;
		}

		//Selected continue.

		this->display_continue_game_info();
		wram.wCurrentMapScriptFlags |= 1 << 5;

		byte_t held;
		do{
			hram.hJoyPressed = 0;
			hram.hJoyReleased = 0;
			hram.hJoyHeld.clear();
			this->parent->joypad();
			held = hram.hJoyHeld.get_raw_value();
			if (check_flag(held, input_a))
				return MainMenuResult::ContinueGame;
		}while (!check_flag(held, input_b));
	}
}

bool CppRedMainMenu::check_for_player_name_in_sram(){
	auto memory = this->parent->load_sram();
	SRam sram(memory.data(), { { read_memory_u8, write_memory_u8 } });
	for (auto &c : sram.player_name)
		if (c == SpecialCharacters::terminator)
			return true;
	return false;
}
