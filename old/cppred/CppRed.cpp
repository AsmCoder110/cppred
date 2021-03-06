#include "CppRed.h"
#include "CppRedTitleScreen.h"
#include "HostSystem.h"
#include "timer.h"
#include "../CodeGeneration/output/gfx.h"
#include "CppRedData.h"
#include "CppRedClearSave.h"
#include "CppRedMainMenu.h"
#include "CppRedScripts.h"
#include "CppRedSRam.h"
#include "MemoryOperations.h"
#include "CppRedIntro.h"
#include <cassert>
#include <stdexcept>
#include <random>

const SpeciesId CppRed::starter_mons[3] = {
	SpeciesId::Charmander,
	SpeciesId::Squirtle,
	SpeciesId::Bulbasaur,
};

#define INITIALIZE_EMPTY_HARDWARE_REGISTER(name) \
	,name(nullptr, { \
		[this](byte_t &dst, const void *){}, \
		[this](const byte_t &src, void *){} \
	})
#define INITIALIZE_HARDWARE_REGISTER(name, controller, function) \
	,name(nullptr, { \
		[this](byte_t &dst, const void *){ \
			this->simulate_time(); \
			dst = this->controller.get_##function(); \
		}, \
		[this](const byte_t &src, void *){ \
			this->simulate_time(); \
			this->controller.set_##function(src); \
		} \
	})
#define INITIALIZE_HARDWARE_REGISTER_RO(name, controller, function) \
	,name(nullptr, {\
		[this](byte_t &dst, const void *){ \
			this->simulate_time(); \
			dst = this->controller.get_##function(); \
		}, \
		[this](const byte_t &src, void *){} \
	})

CppRed::CppRed(HostSystem &host):
		host(&host),
		display_controller(*this),
		sound_controller(*this),
		input_controller(*this),
		clock(*this),
		audio(*this),
		continue_running(false)
		INITIALIZE_EMPTY_HARDWARE_REGISTER(SB)
		INITIALIZE_EMPTY_HARDWARE_REGISTER(SC)
		INITIALIZE_EMPTY_HARDWARE_REGISTER(BGP)
		INITIALIZE_EMPTY_HARDWARE_REGISTER(OBP0)
		INITIALIZE_EMPTY_HARDWARE_REGISTER(OBP1)
		INITIALIZE_HARDWARE_REGISTER(LCDC, display_controller, lcd_control)
		INITIALIZE_HARDWARE_REGISTER(STAT, display_controller, status)
		,IF(0)
		,IE(0)
		INITIALIZE_EMPTY_HARDWARE_REGISTER(InterruptMasterFlag)
		INITIALIZE_HARDWARE_REGISTER(SCX, display_controller, scroll_x)
		INITIALIZE_HARDWARE_REGISTER(SCY, display_controller, scroll_y)
		INITIALIZE_HARDWARE_REGISTER(WX, display_controller, window_x_position)
		INITIALIZE_HARDWARE_REGISTER(WY, display_controller, window_y_position)
		INITIALIZE_HARDWARE_REGISTER_RO(LY, display_controller, y_coordinate)
		INITIALIZE_HARDWARE_REGISTER(LYC, display_controller, y_coordinate_compare)
		INITIALIZE_HARDWARE_REGISTER_RO(DIV, clock, DIV_register)
		,
		text(*this),
		message_box(*this)
{
	this->nonemulation_init();
}

void CppRed::nonemulation_init(){
	this->realtime_counter_frequency = get_timer_resolution();
	//this->emulated_memory.reset(new byte_t[0x10000]);

	std::random_device rnd;
	for (auto &i : this->random_state)
		i = rnd();
}

void CppRed::init(){
	this->IF = 0;
	this->IE = 0;
	this->set_bg_scroll(0, 0);
	this->set_window_position(0, 0);
	this->SB = 0;
	this->SC = 0;
	this->BGP = 0;
	this->OBP0 = 0;
	this->OBP1 = 0;
	//Enable display:
	this->LCDC = DisplayController::lcdc_display_enable_mask;

	this->disable_lcd();

	this->clear_wram();
	this->clear_vram();
	this->clear_hram();
	this->clear_sprites();

	//At this point, a DMA transfer routine is copied to HRAM. Since we can
	//execute stuff whenever we want, we don't need to do this.

	this->hram.hTilesetType = 0;
	this->STAT = 0;
	this->hram.hSCX = 0;
	this->hram.hSCY = 0;

	this->IF = 0;
	this->IE = vblank_mask | timer_mask | serial_mask;
	this->hram.hWY = lcd_height;
	this->set_window_position(7, lcd_height);

	this->hram.hSerialConnectionStatus = SerialConnectionStatus::ConnectionNotEstablished;

	this->clear_bg_map(vBGMap0 >> 8);
	this->clear_bg_map(vBGMap1 >> 8);

	this->LCDC = lcdc_default;

	this->hram.hSoftReset = 16;
	this->stop_all_sounds();

	this->InterruptMasterFlag = 1;

	this->hram.H_AUTOBGTRANSFERDEST = 0x9C00;
	this->wram.wUpdateSpritesEnabled = reduce_sign(-1);

	CppRedIntro::play(*this);

	this->disable_lcd();
	this->clear_vram();
	this->gb_pal_normal();
	this->clear_sprites();
	
	this->LCDC = lcdc_default;

	this->set_default_names_before_titlescreen();
}

void CppRed::clear_wram(){
	this->wram.clear();
}

void CppRed::clear_hram(){
	this->hram.clear();
}

void CppRed::clear_sprites(){
	this->wram.wOAMBuffer.fill_bytes(0);
}

const std::string ninten_text = "NINTEN";
const std::string sony_text = "SONY";

void CppRed::set_default_names_before_titlescreen(){
	this->wram.wPlayerName = ninten_text;
	this->wram.wMainData.wRivalName = sony_text;
	this->set_window_position(-1, 0);
	this->wram.wMainData.wLetterPrintingDelayFlags.clear();
	this->wram.wMainData.wd732.clear();
	this->wram.wMainData.wFlags_D733.clear();
	this->wram.wMainData.wBeatLorelei.clear();
	this->wram.wAudioROMBank.value = 0;
	this->wram.wAudioSavedROMBank.value = 0;
}

TitleScreenResult CppRed::display_titlescreen(){
	CppRedTitleScreen title_screen(*this);
	return title_screen.display();
}

void CppRed::run(){
	if (this->continue_running)
		return;
	if (!this->registered){
		this->registered = true;
		this->host->get_timing_provider()->register_periodic_notification(this->periodic_notification);
	}
	this->continue_running = true;
	auto This = this;
	this->interpreter_thread.reset(new std::thread([This](){ This->interpreter_thread_function(); }));
}

CppRed::~CppRed(){
	std::cout << "Joining.\n";
	this->interpreter_thread->join();
}

void CppRed::interpreter_thread_function(){
	if (this->accumulated_time < 0)
		this->accumulated_time = 0;
	if (!this->start_time.is_initialized())
		this->start_time = this->host->get_datetime_provider()->local_now().to_posix();

	std::shared_ptr<std::exception> thrown;

	try{
		this->real_time_multiplier = 1.0 / (double)this->realtime_counter_frequency;
		this->current_timer_start = get_timer_count();
		this->main();
	}catch (GameBoyException &ex){
		thrown.reset(ex.clone());
	}catch (std::exception &e){
		thrown.reset(new GenericException(e.what()));
	}catch (...){
		thrown.reset(new GenericException("Unknown exception."));
	}

	if (!thrown)
		this->continue_running = false;
	else
		this->host->throw_exception(thrown);
}

#if 0
void CppRed::run_until_next_frame(){
	do{
		this->cpu.run_one_instruction();
		if (this->input_controller.get_button_down())
			this->cpu.joystick_irq();
		this->sound_controller.update(this->speed_multiplier, this->speed_changed);
		this->speed_changed = false;
	}while (!this->display_controller.update() && (this->continue_running || force));
}
#endif

void CppRed::main(){
	MainMenuResult main_menu_result = MainMenuResult::Cancelled;
	while (main_menu_result == MainMenuResult::Cancelled){
		this->init();
		bool done = false;
		while (!done){
			switch (this->display_titlescreen()){
				case TitleScreenResult::GoToMainMenu:
					main_menu_result = this->display_main_menu();
					done = main_menu_result != MainMenuResult::Cancelled;
					break;
				case TitleScreenResult::GoToClearSaveDialog:
					this->display_clear_save_dialog();
					done = true;
					break;
			}
		}
	}
	switch (main_menu_result){
		case MainMenuResult::NewGame:
			this->start_new_game();
			break;
		case MainMenuResult::ContinueGame:
			this->start_loaded_game();
			break;
		default:
			throw std::runtime_error("Invalid switch value.");
	}
}

void CppRed::set_bg_scroll(int x, int y){
	if (x >= 0)
		this->SCX = (byte_t)x;
	if (y >= 0)
		this->SCY = (byte_t)y;
}

void CppRed::set_window_position(int x, int y){
	if (x >= 0)
		this->WX = (byte_t)x;
	if (y >= 0)
		this->WY = (byte_t)y;
}

void CppRed::enable_lcd(){
	this->LCDC |= DisplayController::lcdc_display_enable_mask;
}

void CppRed::disable_lcd(){
	this->IF = 0;
	auto ie = this->IE;
	this->IE = ie & ~vblank_mask;

	this->wait_for_ly(lcd_blank);
	
	this->LCDC &= ~DisplayController::lcdc_display_enable_mask;

	this->IE = ie;
}

void CppRed::clear_vram(){
	this->display_controller.clear_vram();
}

void CppRed::clear_bg_map(unsigned page){
	page <<= 8;
	auto vmem = &this->display_controller.access_vram(page);
	memset(vmem, SpecialCharacters::blank, 0x400);
}

void CppRed::gb_pal_normal(){
	this->BGP = 0xE4;
	this->OBP0 = 0xD0;
}

void CppRed::gb_pal_whiteout(){
	this->BGP = 0;
	this->OBP0 = 0;
	this->OBP1 = 0;
}

void CppRed::clear_screen(){
	auto begin = this->wram.wTileMap.begin();
	std::fill(begin, begin + this->wram.wTileMap.size, 0x7F);
}

void CppRed::load_font_tile_patterns(){
	auto font_graphics = decode_image_data(FontGraphics);
	auto dst = &this->display_controller.access_vram(vFont);
	memcpy(dst, &font_graphics[0], font_graphics.size());
}

void CppRed::clear_both_bg_maps(){
	auto bg = &this->display_controller.access_vram(vBGMap0);
	memset(bg, 0x7F, 0x800);
}

void CppRed::save_screen_tiles_to_buffer2(){
	auto src = this->get_tilemap_location(0, 0);
	const auto size = this->wram.wTileMapBackup2.size;
	auto dst = this->wram.wTileMapBackup2.begin();
	std::copy(src, src + size, dst);
}

void CppRed::load_screen_tiles_from_buffer1(){
	this->hram.H_AUTOBGTRANSFERENABLED = 0;
	auto src = this->wram.wTileMapBackup.begin();
	const auto size = this->wram.wTileMapBackup.size;
	auto dst = this->get_tilemap_location(0, 0);
	std::copy(src, src + size, dst);
	this->hram.H_AUTOBGTRANSFERENABLED = 1;
}

void CppRed::load_screen_tiles_from_buffer2(){
	this->load_screen_tiles_from_buffer2_disable_bg_transfer();
	this->hram.H_AUTOBGTRANSFERENABLED = 1;
}

void CppRed::load_screen_tiles_from_buffer2_disable_bg_transfer(){
	this->hram.H_AUTOBGTRANSFERENABLED = 0;
	auto src = this->wram.wTileMapBackup2.begin();
	const auto size = this->wram.wTileMapBackup2.size;
	auto dst = this->get_tilemap_location(0, 0);
	std::copy(src, src + size, dst);
}

void CppRed::save_screen_tiles_to_buffer1(){
	auto src = this->get_tilemap_location(0, 0);
	const auto size = this->wram.wTileMapBackup2.size;
	auto dst = this->wram.wTileMapBackup2.begin();
	std::copy(src, src + size, dst);
}

void CppRed::delay_frames(unsigned count){
	while (count--)
		this->delay_frame();
}

void CppRed::stop_all_sounds(){
	this->wram.wAudioROMBank = AudioBank::Bank1;
	this->wram.wAudioSavedROMBank = AudioBank::Bank1;
	this->wram.wAudioFadeOutControlCounterRequest = 0;
	this->wram.wNewSoundID = Sound::None;
	this->wram.wLastMusicSoundID = Sound::None;
	this->play_sound(Sound::Stop);
}

void CppRed::play_sound(Sound sound){
	this->audio.play_sound(sound);
}

void CppRed::wait_for_sound_to_finish(){
	//TODO
	return;
	if (check_flag<byte_t>(this->wram.wLowHealthAlarm, 0x80))
		return;
	byte_t a;
	//TODO: Replace spinlock with a wait or something:
	//TODO: VERY IMPORTANT: This spinlock can never break!
	do{
		a = this->wram.wChannelSoundIDs[4];
		a |= this->wram.wChannelSoundIDs[5];
		a |= this->wram.wChannelSoundIDs[7];
	}while (a);
}

void CppRed::play_cry(SpeciesId species){
	this->play_sound(this->get_cry_data(species));
	this->wait_for_sound_to_finish();
}

Sound CppRed::get_cry_data(SpeciesId species){
	auto index = (unsigned)species;
	assert(index < array_length(pokemon_by_species_id));
	auto &data = pokemon_by_species_id[index]->cry_data;
	this->wram.wFrequencyModifier = data.pitch;
	this->wram.wTempoModifier = data.length;
	return (Sound)(data.base_cry + (unsigned)Sound::SFX_Cry00_1);
}

void CppRed::load_gb_pal(){
	int offset = this->wram.wMainData.wMapPalOffset;
	const byte_t *data = (const byte_t *)fade_palettes;
	int index = 3 * (int)sizeof(FadePaletteData) - offset;
	assert(index >= 0 && index < sizeof(fade_palettes) - 3);
	auto palette = *(const FadePaletteData *)(data + index);
	this->BGP = palette.background_palette;
	this->OBP0 = palette.obp0_palette;
	this->OBP1 = palette.obp1_palette;
}

void CppRed::display_clear_save_dialog(){
	CppRedClearSaveDialog dialog(*this);
	if (dialog.display())
		this->clear_save();
}

MainMenuResult CppRed::display_main_menu(){
	CppRedMainMenu dialog(*this);
	return dialog.display();
}

void CppRed::start_loaded_game(){
	this->gb_pal_white_out_with_delay3();
	this->clear_screen();
	this->wram.wMainData.wPlayerDirection = PlayerDirectionBitmap::Right;
	this->delay_frames(10);
	if (!this->wram.wMainData.wNumHoFTeams){
		this->special_enter_map(MapId::PalletTown);
		return;
	}
	auto current_map = this->wram.wMainData.wCurMap.enum_value();
	if (current_map != MapId::HallOfFame){
		this->special_enter_map(current_map);
		return;
	}
	this->wram.wMainData.wDestinationMap = MapId::PalletTown;
	this->wram.wMainData.wd732.set_fly_warp(true);
	this->special_enter_map(this->special_warp_in());
}

void CppRed::print_text(const CppRedText::Region &region){
	this->text.print_text(region);
}

CppRed::tilemap_it CppRed::get_tilemap_location(unsigned x, unsigned y){
	if (x > tilemap_width || y > tilemap_height){
		std::stringstream stream;
		stream << "Invalid coordinates: " << x << ", " << y;
		throw std::runtime_error(stream.str());
	}
	return this->wram.wTileMap.begin() + y * tilemap_width + x;
}

unsigned CppRed::random(){
	return xorshift128(this->random_state);
}

void CppRed::start_new_game(){
	//Do not skip name selection.
	this->wram.wMainData.wd732.set_unknown(false);
	CppRedScripts::oak_speech(*this);
	this->delay_frames(20);
	//TODO: Confirm this. StartNewGame falls through to SpecialEnterMap, but I
	//don't know the state of the registers. I'm assuming this is what happens.
	this->special_enter_map(MapId::RedsHouse2f);
}

void CppRed::mass_initialization(){
	this->wram.wPlayerName.fill_bytes(0);
	this->wram.wPartyData.wPartyCount = 0;
	this->wram.wPartyData.wPartySpecies.fill_bytes(0);
	this->wram.wPartyData.wPartyMons.fill_bytes(0);
	this->wram.wPartyData.wPartyMonOT.fill_bytes(0);
	this->wram.wPartyData.wPartyMonNicks.fill_bytes(0);
	this->wram.wMainData.wPokedexOwned.fill_bytes(0);
	this->wram.wMainData.wPokedexSeen.fill_bytes(0);
	this->wram.wMainData.wNumBagItems = 0;
	this->wram.wMainData.wBagItems.fill_bytes(0);
	this->wram.wMainData.wBagItemsTerminator = 0;
	this->wram.wMainData.wPlayerMoney = 0;
	this->wram.wMainData.wRivalName.fill_bytes(0);
	this->wram.wMainData.wOptions.clear();
	this->wram.wMainData.wObtainedBadges = 0;
	this->wram.wMainData.wLetterPrintingDelayFlags.clear();
	this->wram.wMainData.wPlayerID = 0;
	this->wram.wMainData.wMapMusicSoundID = 0;
	this->wram.wMainData.wMapMusicROMBank = 0;
	this->wram.wMainData.wMapPalOffset = 0;
	this->wram.wMainData.wCurMap = MapId::PalletTown;
	this->wram.wMainData.wCurrentTileBlockMapViewPointer = 0;
	this->wram.wMainData.wYCoord = 0;
	this->wram.wMainData.wXCoord = 0;
	this->wram.wMainData.wYBlockCoord = 0;
	this->wram.wMainData.wXBlockCoord = 0;
	this->wram.wMainData.wLastMap = MapId::PalletTown;
	this->wram.wMainData.wUnusedD366 = 0;
	this->wram.wMainData.wCurMapTileset = 0;
	this->wram.wMainData.wCurMapHeight = 0;
	this->wram.wMainData.wCurMapWidth = 0;
	this->wram.wMainData.wMapDataPtr = 0;
	this->wram.wMainData.wMapTextPtr = 0;
	this->wram.wMainData.wMapScriptPtr = 0;
	this->wram.wMainData.wMapConnections = 0;
	this->wram.wMainData.wMapConn1Ptr = 0;
	this->wram.wMainData.wNorthConnectionStripSrc = 0;
	this->wram.wMainData.wNorthConnectionStripDest = 0;
	this->wram.wMainData.wNorthConnectionStripWidth = 0;
	this->wram.wMainData.wNorthConnectedMapWidth = 0;
	this->wram.wMainData.wNorthConnectedMapYAlignment = 0;
	this->wram.wMainData.wNorthConnectedMapXAlignment = 0;
	this->wram.wMainData.wNorthConnectedMapViewPointer = 0;
	this->wram.wMainData.wMapConn2Ptr = 0;
	this->wram.wMainData.wSouthConnectionStripSrc = 0;
	this->wram.wMainData.wSouthConnectionStripDest = 0;
	this->wram.wMainData.wSouthConnectionStripWidth = 0;
	this->wram.wMainData.wSouthConnectedMapWidth = 0;
	this->wram.wMainData.wSouthConnectedMapYAlignment = 0;
	this->wram.wMainData.wSouthConnectedMapXAlignment = 0;
	this->wram.wMainData.wSouthConnectedMapViewPointer = 0;
	this->wram.wMainData.wMapConn3Ptr = 0;
	this->wram.wMainData.wWestConnectionStripSrc = 0;
	this->wram.wMainData.wWestConnectionStripDest = 0;
	this->wram.wMainData.wWestConnectionStripHeight = 0;
	this->wram.wMainData.wWestConnectedMapWidth = 0;
	this->wram.wMainData.wWestConnectedMapYAlignment = 0;
	this->wram.wMainData.wWestConnectedMapXAlignment = 0;
	this->wram.wMainData.wWestConnectedMapViewPointer = 0;
	this->wram.wMainData.wMapConn4Ptr = 0;
	this->wram.wMainData.wEastConnectionStripSrc = 0;
	this->wram.wMainData.wEastConnectionStripDest = 0;
	this->wram.wMainData.wEastConnectionStripHeight = 0;
	this->wram.wMainData.wEastConnectedMapWidth = 0;
	this->wram.wMainData.wEastConnectedMapYAlignment = 0;
	this->wram.wMainData.wEastConnectedMapXAlignment = 0;
	this->wram.wMainData.wEastConnectedMapViewPointer = 0;
	this->wram.wMainData.wSpriteSet.fill_bytes(0);
	this->wram.wMainData.wSpriteSetID = 0;
	this->wram.wMainData.wObjectDataPointerTemp.fill_bytes(0);
	this->wram.wMainData.wMapBackgroundTile = 0;
	this->wram.wMainData.wNumberOfWarps = 0;
	this->wram.wMainData.wWarpEntries.fill_bytes(0);
	this->wram.wMainData.wDestinationWarpID = 0;
	this->wram.wMainData.wNumSigns = 0;
	this->wram.wMainData.wSignCoords.fill_bytes(0);
	this->wram.wMainData.wSignTextIDs.fill_bytes(0);
	this->wram.wMainData.wNumSprites = 0;
	this->wram.wMainData.wYOffsetSinceLastSpecialWarp = 0;
	this->wram.wMainData.wXOffsetSinceLastSpecialWarp = 0;
	this->wram.wMainData.wMapSpriteData.fill_bytes(0);
	this->wram.wMainData.wMapSpriteExtraData.fill_bytes(0);
	this->wram.wMainData.wCurrentMapHeight2 = 0;
	this->wram.wMainData.wCurrentMapWidth2 = 0;
	this->wram.wMainData.wMapViewVRAMPointer = 0;
	this->wram.wMainData.wPlayerMovingDirection = PlayerDirectionBitmap::Null;
	this->wram.wMainData.wPlayerLastStopDirection = 0;
	this->wram.wMainData.wPlayerDirection = PlayerDirectionBitmap::Null;
	this->wram.wMainData.wTilesetBank = 0;
	this->wram.wMainData.wTilesetBlocksPtr = 0;
	this->wram.wMainData.wTilesetGfxPtr = 0;
	this->wram.wMainData.wTilesetCollisionPtr = 0;
	this->wram.wMainData.wTilesetTalkingOverTiles.fill_bytes(0);
	this->wram.wMainData.wGrassTile = 0;
	this->wram.wMainData.wNumBoxItems = 0;
	this->wram.wMainData.wBoxItems.fill_bytes(0);
	this->wram.wMainData.wBoxItemsTerminator = 0;
	this->wram.wMainData.wCurrentBoxNum = 0;
	this->wram.wMainData.wNumHoFTeams = 0;
	this->wram.wMainData.wUnusedD5A3 = 0;
	this->wram.wMainData.wPlayerCoins = 0;
	this->wram.wMainData.wMissableObjectFlags.fill_bytes(0);
	this->wram.wMainData.wd5cd = 0;
	this->wram.wMainData.wMissableObjectList.fill_bytes(0);
	this->wram.wMainData.wOaksLabCurScript = 0;
	this->wram.wMainData.wGameProgressFlags.fill_bytes(0);
	this->wram.wMainData.wPalletTownCurScript = 0;
	this->wram.wMainData.wBluesHouseCurScript = 0;
	this->wram.wMainData.wViridianCityCurScript = 0;
	this->wram.wMainData.wPewterCityCurScript = 0;
	this->wram.wMainData.wRoute3CurScript = 0;
	this->wram.wMainData.wRoute4CurScript = 0;
	this->wram.wMainData.wViridianGymCurScript = 0;
	this->wram.wMainData.wPewterGymCurScript = 0;
	this->wram.wMainData.wCeruleanGymCurScript = 0;
	this->wram.wMainData.wVermilionGymCurScript = 0;
	this->wram.wMainData.wCeladonGymCurScript = 0;
	this->wram.wMainData.wRoute6CurScript = 0;
	this->wram.wMainData.wRoute8CurScript = 0;
	this->wram.wMainData.wRoute24CurScript = 0;
	this->wram.wMainData.wRoute25CurScript = 0;
	this->wram.wMainData.wRoute9CurScript = 0;
	this->wram.wMainData.wRoute10CurScript = 0;
	this->wram.wMainData.wMtMoon1CurScript = 0;
	this->wram.wMainData.wMtMoon3CurScript = 0;
	this->wram.wMainData.wSSAnne8CurScript = 0;
	this->wram.wMainData.wSSAnne9CurScript = 0;
	this->wram.wMainData.wRoute22CurScript = 0;
	this->wram.wMainData.wRedsHouse2CurScript = 0;
	this->wram.wMainData.wViridianMarketCurScript = 0;
	this->wram.wMainData.wRoute22GateCurScript = 0;
	this->wram.wMainData.wCeruleanCityCurScript = 0;
	this->wram.wMainData.wSSAnne5CurScript = 0;
	this->wram.wMainData.wViridianForestCurScript = 0;
	this->wram.wMainData.wMuseum1fCurScript = 0;
	this->wram.wMainData.wRoute13CurScript = 0;
	this->wram.wMainData.wRoute14CurScript = 0;
	this->wram.wMainData.wRoute17CurScript = 0;
	this->wram.wMainData.wRoute19CurScript = 0;
	this->wram.wMainData.wRoute21CurScript = 0;
	this->wram.wMainData.wSafariZoneEntranceCurScript = 0;
	this->wram.wMainData.wRockTunnel2CurScript = 0;
	this->wram.wMainData.wRockTunnel1CurScript = 0;
	this->wram.wMainData.wRoute11CurScript = 0;
	this->wram.wMainData.wRoute12CurScript = 0;
	this->wram.wMainData.wRoute15CurScript = 0;
	this->wram.wMainData.wRoute16CurScript = 0;
	this->wram.wMainData.wRoute18CurScript = 0;
	this->wram.wMainData.wRoute20CurScript = 0;
	this->wram.wMainData.wSSAnne10CurScript = 0;
	this->wram.wMainData.wVermilionCityCurScript = 0;
	this->wram.wMainData.wPokemonTower2CurScript = 0;
	this->wram.wMainData.wPokemonTower3CurScript = 0;
	this->wram.wMainData.wPokemonTower4CurScript = 0;
	this->wram.wMainData.wPokemonTower5CurScript = 0;
	this->wram.wMainData.wPokemonTower6CurScript = 0;
	this->wram.wMainData.wPokemonTower7CurScript = 0;
	this->wram.wMainData.wRocketHideout1CurScript = 0;
	this->wram.wMainData.wRocketHideout2CurScript = 0;
	this->wram.wMainData.wRocketHideout3CurScript = 0;
	this->wram.wMainData.wRocketHideout4CurScript = 0;
	this->wram.wMainData.wRoute6GateCurScript = 0;
	this->wram.wMainData.wRoute8GateCurScript = 0;
	this->wram.wMainData.wCinnabarIslandCurScript = 0;
	this->wram.wMainData.wMansion1CurScript = 0;
	this->wram.wMainData.wMansion2CurScript = 0;
	this->wram.wMainData.wMansion3CurScript = 0;
	this->wram.wMainData.wMansion4CurScript = 0;
	this->wram.wMainData.wVictoryRoad2CurScript = 0;
	this->wram.wMainData.wVictoryRoad3CurScript = 0;
	this->wram.wMainData.wFightingDojoCurScript = 0;
	this->wram.wMainData.wSilphCo2CurScript = 0;
	this->wram.wMainData.wSilphCo3CurScript = 0;
	this->wram.wMainData.wSilphCo4CurScript = 0;
	this->wram.wMainData.wSilphCo5CurScript = 0;
	this->wram.wMainData.wSilphCo6CurScript = 0;
	this->wram.wMainData.wSilphCo7CurScript = 0;
	this->wram.wMainData.wSilphCo8CurScript = 0;
	this->wram.wMainData.wSilphCo9CurScript = 0;
	this->wram.wMainData.wHallOfFameRoomCurScript = 0;
	this->wram.wMainData.wGaryCurScript = 0;
	this->wram.wMainData.wLoreleiCurScript = 0;
	this->wram.wMainData.wBrunoCurScript = 0;
	this->wram.wMainData.wAgathaCurScript = 0;
	this->wram.wMainData.wUnknownDungeon3CurScript = 0;
	this->wram.wMainData.wVictoryRoad1CurScript = 0;
	this->wram.wMainData.wLanceCurScript = 0;
	this->wram.wMainData.wSilphCo10CurScript = 0;
	this->wram.wMainData.wSilphCo11CurScript = 0;
	this->wram.wMainData.wFuchsiaGymCurScript = 0;
	this->wram.wMainData.wSaffronGymCurScript = 0;
	this->wram.wMainData.wCinnabarGymCurScript = 0;
	this->wram.wMainData.wCeladonGameCornerCurScript = 0;
	this->wram.wMainData.wRoute16GateCurScript = 0;
	this->wram.wMainData.wBillsHouseCurScript = 0;
	this->wram.wMainData.wRoute5GateCurScript = 0;
	this->wram.wMainData.wRoute7GateCurScript = 0;
	this->wram.wMainData.wPowerPlantCurScript = 0;
	this->wram.wMainData.wSSAnne2CurScript = 0;
	this->wram.wMainData.wSeafoamIslands4CurScript = 0;
	this->wram.wMainData.wRoute23CurScript = 0;
	this->wram.wMainData.wSeafoamIslands5CurScript = 0;
	this->wram.wMainData.wRoute18GateCurScript = 0;
	this->wram.wMainData.wObtainedHiddenItemsFlags.fill_bytes(0);
	this->wram.wMainData.wObtainedHiddenCoinsFlags.fill_bytes(0);
	this->wram.wMainData.wWalkBikeSurfState = 0;
	this->wram.wMainData.wTownVisitedFlag = 0;
	this->wram.wMainData.wSafariSteps = 0;
	this->wram.wMainData.wFossilItem = 0;
	this->wram.wMainData.wFossilMon = 0;
	this->wram.wMainData.wEnemyMonOrTrainerClass = 0;
	this->wram.wMainData.wPlayerJumpingYScreenCoordsIndex = 0;
	this->wram.wMainData.wRivalStarter = 0;
	this->wram.wMainData.wPlayerStarter = 0;
	this->wram.wMainData.wBoulderSpriteIndex = 0;
	this->wram.wMainData.wLastBlackoutMap = 0;
	this->wram.wMainData.wDestinationMap = MapId::PalletTown;
	this->wram.wMainData.wUnusedD71B = 0;
	this->wram.wMainData.wTileInFrontOfBoulderAndBoulderCollisionResult = 0;
	this->wram.wMainData.wDungeonWarpDestinationMap = 0;
	this->wram.wMainData.wWhichDungeonWarp = 0;
	this->wram.wMainData.wUnusedD71F = 0;
	this->wram.wMainData.wd728.clear();
	this->wram.wMainData.wBeatGymFlags.clear();
	this->wram.wMainData.wd72c.clear();
	this->wram.wMainData.wd72d_destinationMap = MapId::PalletTown;
	this->wram.wMainData.wd72d.clear();
	this->wram.wMainData.wd72e.clear();
	this->wram.wMainData.wd730.clear();
	this->wram.wMainData.wd732.clear();
	this->wram.wMainData.wFlags_D733.clear();
	this->wram.wMainData.wBeatLorelei.clear();
	this->wram.wMainData.wd736.clear();
	this->wram.wMainData.wCompletedInGameTradeFlags.fill_bytes(0);
	this->wram.wMainData.wWarpedFromWhichWarp = 0;
	this->wram.wMainData.wWarpedFromWhichMap = 0;
	this->wram.wMainData.wCardKeyDoorY = 0;
	this->wram.wMainData.wCardKeyDoorX = 0;
	this->wram.wMainData.wFirstLockTrashCanIndex = 0;
	this->wram.wMainData.wSecondLockTrashCanIndex = 0;
	this->wram.wMainData.wEventFlags.fill_bytes(0);
	this->wram.wMainData.wGrassRate = 0;
	this->wram.wMainData.wLinkEnemyTrainerName.fill_bytes(0);
	this->wram.wMainData.wGrassMons.fill_bytes(0);
	this->wram.wMainData.wSerialEnemyDataBlock.fill_bytes(0);
	this->wram.wMainData.wEnemyPartyCount = 0;
	this->wram.wMainData.wEnemyPartyMons.fill_bytes(0);
	this->wram.wMainData.wEnemyPartyMonsTerminator = 0;
	this->wram.wMainData.wEnemyMons.fill_bytes(0);
	this->wram.wMainData.wWaterRate = 0;
	this->wram.wMainData.wWaterMons = 0;
	this->wram.wMainData.wEnemyMonOT.fill_bytes(0);
	this->wram.wMainData.wEnemyMonNicks.fill_bytes(0);
	this->wram.wMainData.wTrainerHeaderPtr = 0;
	this->wram.wMainData.wUnusedDA38 = 0;
	this->wram.wMainData.wOpponentAfterWrongAnswer = 0;
	this->wram.wMainData.wCurMapScript = 0;
	this->wram.wMainData.wPlayTimeHours = 0;
	this->wram.wMainData.wPlayTimeMaxed = 0;
	this->wram.wMainData.wPlayTimeMinutes = 0;
	this->wram.wMainData.wPlayTimeSeconds = 0;
	this->wram.wMainData.wPlayTimeFrames = 0;
	this->wram.wMainData.wSafariZoneGameOver = 0;
	this->wram.wMainData.wNumSafariBalls = 0;
	this->wram.wMainData.wDayCareInUse = 0;
	this->wram.wMainData.wDayCareMonName.fill_bytes(0);
	this->wram.wMainData.wDayCareMonOT.fill_bytes(0);
	this->wram.wMainData.wDayCareMon.clear();
	this->wram.wBoxData.wNumInBox = 0;
	this->wram.wBoxData.wBoxSpecies.fill_bytes(0);
	this->wram.wBoxData.wBoxSpeciesTerminator = 0;
	this->wram.wBoxData.wBoxMons.clear();
	this->wram.wBoxData.wBoxMonOT.fill_bytes(0);
	this->wram.wBoxData.wBoxMonNicks.fill_bytes(0);
}

void CppRed::initialize_player_data(){
	auto wd732 = this->wram.wMainData.wd732.get_raw_value();
	auto wOptions = this->wram.wMainData.wOptions.get_raw_value();
	auto wLetterPrintingDelayFlags = this->wram.wMainData.wLetterPrintingDelayFlags.get_raw_value();

	this->mass_initialization();
	this->wram.wSpriteData.wSpriteStateData1.fill_bytes(0);
	this->wram.wSpriteData.wSpriteStateData2.fill_bytes(0);

	this->wram.wMainData.wd732.set_raw_value(wd732);
	this->wram.wMainData.wOptions.set_raw_value(wOptions);
	this->wram.wMainData.wLetterPrintingDelayFlags.set_raw_value(wLetterPrintingDelayFlags);

	if (!this->wram.wOptionsInitialized)
		this->initialize_options();

	this->wram.wPlayerName = ninten_text;
	this->wram.wMainData.wRivalName = sony_text;
}

void CppRed::initialize_options(){
	this->wram.wMainData.wLetterPrintingDelayFlags.set_raw_value(1);
	this->wram.wMainData.wOptions.clear();
	this->wram.wMainData.wOptions.set_text_speed(TextSpeed::Medium);
}

void CppRed::add_item_to_inventory(unsigned position, ItemId item, unsigned quantity){
	this->wram.wcf91 = (unsigned)item;
	this->wram.wItemQuantity = quantity;
	//TODO: complete me.
}

MapId CppRed::special_warp_in(){
	this->load_special_warp_data();
	this->load_tileset_header();
	bool fly_warp = this->wram.wMainData.wd732.get_fly_warp();
	this->wram.wMainData.wd732.set_fly_warp(false);
	MapId destination = MapId::PalletTown;
	if (fly_warp)
		destination = this->wram.wMainData.wDestinationMap;
	else
		destination = MapId::PalletTown;
	auto destination_map = this->wram.wMainData.wd72d_destinationMap.enum_value();
	if ((unsigned)destination_map)
		destination = destination_map;
	if (!this->wram.wMainData.wd732.get_hole_warp())
		this->wram.wMainData.wLastMap = destination;
	return destination;
}

void CppRed::load_special_warp_data(){
	//TODO
}

void CppRed::clear_save(){
	sram_t sram;
	std::fill(sram.begin(), sram.end(), 0xFF);
	this->save_sram(sram);
}

void CppRed::save_sram(const sram_t &sram) const{
	std::ofstream file("pokered.sav", std::ios::binary);
	file.write((const char *)sram.data(), sram.size());
}

CppRed::sram_t CppRed::load_sram(){
	sram_t memory;
	SRam sram(memory.data(), { { read_memory_u8, write_memory_u8 } });
	std::ifstream file("pokered.sav", std::ios::binary);
	if (!file){
		xorshift128_state state = {
			this->random(),
			this->random(),
			this->random(),
			this->random(),
		};
		sram.clear(state);
	}else{
		sram.clear();
		file.read((char *)memory.data(), memory.size());
	}
	return memory;
}

void CppRed::special_enter_map(MapId id){
	this->hram.hJoyPressed.clear();
	this->hram.hJoyHeld.clear();
	this->hram.hJoy5.clear();
	this->wram.wMainData.wd72d.clear();
	this->wram.wMainData.wd732.set_counting_play_time(true);
	this->reset_player_sprite_data();
	this->delay_frames(20);
	if (this->wram.wEnteringCableClub)
		return;
	this->enter_map(id);
}

void CppRed::enter_map(MapId){
	//TODO
}

void CppRed::reset_player_sprite_data(){
	{
		auto player = this->wram.wSpriteData.wSpriteStateData1[0];
		player.picture_id = 1;
		player.movement_status.clear();
		player.sprite_image_idx = 0;
		player.y_step_vector = 0;
		player.y_pixels = 60;
		player.x_step_vector = 0;
		player.x_pixels = 64;
		player.intra_anim_frame_counter = 0;
		player.anim_frame_counter = 0;
		player.facing_direction.value = 0;
		player.tile_position_y = 0;
		player.tile_position_x = 0;
		player.collision_bits = 0;
		player.unknown1 = 0;
		player.collision_bits2 = 0;
		player.collision_bits3 = 0;
	}
	this->wram.wSpriteData.wSpriteStateData2[0].clear();
	this->wram.wSpriteData.wSpriteStateData2[0].sprite_image_base_offset = 1;
}

void CppRed::load_save(){
	this->clear_screen();
	this->load_font_tile_patterns();
	this->load_textbox_tile_patterns();
	auto memory = this->load_sram();
	SRam sram(memory.data(), { { read_memory_u8, write_memory_u8 } });

	auto main_data = sram.player_name.get_memory();
	auto size = sram.main_data_checksum.get_memory() - main_data;
	auto checksum = calculate_checksum(main_data, size);
	if (checksum != sram.main_data_checksum){
		this->wram.wMainData.wd730.set_no_print_delay(true);
		this->text.print_text(this->text.FileDataDestroyedText);
		this->delay_frames(100);
		this->wram.wMainData.wd730.set_no_print_delay(false);
		this->wram.wSaveFileStatus = SaveFileStatus::NoSave;
	}else{
		this->wram.wPlayerName.copy_from(sram.player_name);
		this->wram.wMainData.copy_from(sram.main_data);
		this->wram.wSpriteData.copy_from(sram.sprite_data);
		this->wram.wPartyData.copy_from(sram.party_data);
		this->wram.wBoxData.copy_from(sram.current_box_data);
		this->hram.hTilesetType = sram.tileset_type;
		this->wram.wMainData.wCurMapTileset |= 1 << 7;
		this->wram.wSaveFileStatus = SaveFileStatus::SaveExists;
	}
}

void CppRed::clear_screen_area(unsigned w, unsigned h, const tilemap_it &location){
	for (unsigned y = 0; y < h; y++){
		auto i = location + y * tilemap_width;
		std::fill(i, i + w, SpecialCharacters::blank);
	}
}

void CppRed::copy_video_data(const BaseStaticImage &image, unsigned tiles, unsigned src_offset, unsigned destination, bool flipped){
	auto data = decode_image_data(image, flipped);

	auto copy_size = std::min<size_t>(tiles * tile_byte_size, data.size());
	this->copy_video_data(&data[src_offset * tile_byte_size], copy_size, destination);
}

void CppRed::copy_video_data(const BaseStaticImage &image, unsigned destination, bool flipped){
	this->copy_video_data(image, std::numeric_limits<unsigned>::max(), 0, destination, false);
}

void CppRed::copy_video_data(const void *data, size_t size, unsigned destination){
	this->vblank_copy_src = data;
	this->vblank_copy_size = size;
	this->vblank_copy_dst = destination;
	if (this->display_controller.get_display_enabled())
		this->delay_frame();
	else
		this->vblank_copy();
}

void CppRed::place_unfilled_arrow_menu_cursor(){
	this->write_character_at_menu_cursor(SpecialCharacters::arrow_white_right);
}

void CppRed::erase_menu_cursor(){
	this->write_character_at_menu_cursor(SpecialCharacters::blank);
}

void CppRed::write_character_at_menu_cursor(byte_t character){
	int x = this->wram.wMenuCursorLocationX;
	int y = this->wram.wMenuCursorLocationY;
	*this->get_tilemap_location(x, y) = character;
}

InputBitmap_struct CppRed::handle_menu_input(){
	this->wram.wPartyMenuAnimMonEnabled = 0;
	return this->handle_menu_input2();
}

InputBitmap_struct CppRed::handle_menu_input2(){
	auto counter1 = +this->hram.H_DOWNARROWBLINKCNT1;
	auto counter2 = +this->hram.H_DOWNARROWBLINKCNT2;
	this->hram.H_DOWNARROWBLINKCNT1 = 0;
	this->hram.H_DOWNARROWBLINKCNT2 = 6;

	InputBitmap_struct input;
	while (true){
		this->wram.wAnimCounter = 0;
		this->place_menu_cursor();
		this->delay3();

		while (true){
			if (this->wram.wPartyMenuAnimMonEnabled)
				this->animate_party_mon();
			//TODO: Verify that this works correctly.
			this->delay_frame();
			input = this->joypad_low_sensitivity();
			if (any_button_pressed(input))
				break;
			
			this->handle_down_arrow_blink_timing(this->get_tilemap_location(18, 11));
			if (this->wram.wMenuJoypadPollCount == 1)
				break;
		}

		this->wram.wCheckFor180DegreeTurn = 0;
		auto &menu_item = this->wram.wCurrentMenuItem;
		auto &max_menu_item = this->wram.wMaxMenuItem;
		auto &menu_wrapping = this->wram.wMenuWrappingEnabled;
		bool skip_checking_other_buttons = false;
		if (input.button_up){
			if (!menu_item){
				if (!menu_wrapping)
					skip_checking_other_buttons = !!this->wram.wMenuWatchMovingOutOfBounds;
				else
					menu_item = +max_menu_item;
			}else
				menu_item--;
		}else if (input.button_down){
			if (menu_item >= max_menu_item){
				if (!menu_wrapping)
					skip_checking_other_buttons = !!this->wram.wMenuWatchMovingOutOfBounds;
				else
					menu_item = 0;
			}else
				menu_item++;
		}

		if (!skip_checking_other_buttons && !any_button_pressed(input & this->wram.wMenuWatchedKeys))
			continue;

		if ((input.button_a || input.button_b) && !this->wram.wFlags_0xcd60.get_no_menu_sound())
			this->play_sound(Sound::SFX_Press_AB_1);
		break;
	}
	this->hram.H_DOWNARROWBLINKCNT1 = counter1;
	this->hram.H_DOWNARROWBLINKCNT2 = counter2;
	this->wram.wMenuWrappingEnabled = 0;
	return input;
}

void CppRed::joypad(){
	typedef InputBitmap_struct B;
	B input = this->hram.hJoyInput;
	if (input.button_a & input.button_b & input.button_select & input.button_start)
		throw SoftResetException();

	B last = this->hram.hJoyLast;
	this->hram.hJoyLast = input;

	if (this->wram.wMainData.wd730.get_ignore_input()){
		this->hram.hJoyHeld.clear();
		this->hram.hJoyPressed.clear();
		this->hram.hJoyReleased.clear();
		return;
	}

	auto ignore = ~(B)this->wram.wJoyIgnore;
	auto filtered = input & ignore;
	this->hram.hJoyHeld = filtered;
	this->hram.hJoyPressed = (last ^ input) & filtered;
	this->hram.hJoyReleased = (last ^ input) & last;
}

InputBitmap_struct CppRed::joypad_low_sensitivity(){
	typedef InputBitmap_struct B;
	this->joypad();
	bool flag6 = !!this->hram.hJoy6;
	bool flag7 = !!this->hram.hJoy7;
	B pressed = !flag7 ? this->hram.hJoyPressed : this->hram.hJoyHeld;
	this->hram.hJoy5 = pressed;
	if (any_button_pressed(this->hram.hJoyPressed)){
		this->hram.H_FRAMECOUNTER = 30;
		return pressed;
	}
	if (this->hram.H_FRAMECOUNTER){
		this->hram.hJoy5.clear();
		return this->hram.hJoy5;
	}
	B held = this->hram.hJoyHeld;
	if ((held.button_a || held.button_b) && !flag6)
		this->hram.hJoy5.clear();
	this->hram.H_FRAMECOUNTER = 5;
	return this->hram.hJoy5;
}

void CppRed::wait_for_text_scroll_button_press(){
	auto counter1 = +this->hram.H_DOWNARROWBLINKCNT1;
	auto counter2 = +this->hram.H_DOWNARROWBLINKCNT2;
	this->hram.H_DOWNARROWBLINKCNT1 = 0;
	this->hram.H_DOWNARROWBLINKCNT2 = 6;
	InputBitmap_struct joy5;
	do{
		if (this->wram.wTownMapSpriteBlinkingEnabled)
			this->town_map_sprite_blinking_animation();
		this->handle_down_arrow_blink_timing(this->get_tilemap_location(18, 16));
		this->joypad_low_sensitivity();
		this->cable_club_run();
		joy5 = this->hram.hJoy5;
	}while (!joy5.button_a && !joy5.button_b);
	this->hram.H_DOWNARROWBLINKCNT1 = counter1;
	this->hram.H_DOWNARROWBLINKCNT2 = counter2;
}

void CppRed::display_two_option_menu(TwoOptionMenuType type, unsigned x, unsigned y, bool default_to_second_option){
	this->wram.wTextBoxID = TextBoxId::TwoOptionMenu;
	this->wram.wTwoOptionMenuID.clear();
	this->wram.wTwoOptionMenuID.set_id(type);
	this->wram.wTwoOptionMenuID.set_select_second_item_by_default(default_to_second_option);
	this->display_textbox_id(x, y);
}

void CppRed::display_textbox_id(unsigned x, unsigned y){
	this->message_box.display_textbox_id(x, y);
}

void CppRed::load_textbox_tile_patterns(){
	this->copy_video_data(TextBoxGraphics, vChars2);
}

void CppRed::load_front_sprite(SpeciesId species, bool flipped, const tilemap_it &destination){
	auto data = pokemon_by_species_id[(int)species];
	if (!data->front){
		this->wram.wcf91 = (unsigned)SpeciesId::Rhydon;
		return;
	}
	auto &front = *data->front;
	auto image_data = decode_image_data(front, flipped);
	image_data = pad_tiles_for_mon_pic(image_data, front.get_width(), front.get_height(), flipped);
	write_mon_pic_tiles_to_buffer(destination, tilemap_width);
	this->copy_video_data(&image_data[0], image_data.size(), vFrontPic);
}

void CppRed::gb_fadeout_to_white(){
	for (int i = 0; i < 3; i++){
		auto &palette = fade_palettes[5 + i];
		this->BGP  = palette.background_palette;
		this->OBP0 = palette.obp0_palette;
		this->OBP1 = palette.obp1_palette;
		this->delay_frames(8);
	}
}

void CppRed::gb_fadein_from_white(){
	for (int i = 0; i < 3; i++){
		auto &palette = fade_palettes[6 - i];
		this->BGP = palette.background_palette;
		this->OBP0 = palette.obp0_palette;
		this->OBP1 = palette.obp1_palette;
		this->delay_frames(8);
	}
}

void CppRed::animate_party_mon(bool force_speed_1){
	static const byte_t animation_delays[] = { 6, 16, 32, };

	auto current_item = +this->wram.wCurrentMenuItem;
	unsigned delay = animation_delays[0];
	if (!force_speed_1){
		auto color = this->wram.wPartyMenuHPBarColors[current_item].enum_value();
		delay = animation_delays[(int)color % 3];
	}

	auto counter = +this->wram.wAnimCounter;
	assert(!!delay && !(!counter && counter == delay));
	if (counter != delay){
		if (!counter){
			size_t i = 0;
			for (auto &src : this->wram.wMonPartySpritesSavedOAM)
				this->wram.wOAMBuffer[i++].assign(src);
		}
		counter++;
		if (counter == delay * 2)
			counter = 0;
		this->wram.wAnimCounter = counter;
		this->delay_frame();
		return;
	}

	auto oam = this->wram.wOAMBuffer.begin() + tiles_per_pokemon_ow_sprite * current_item;
	auto tile = +oam[0].tile_number;

	if (tile == (int)PokemonOverworldSprite::Ball * tiles_per_pokemon_ow_sprite || tile == (int)PokemonOverworldSprite::Helix * tiles_per_pokemon_ow_sprite){
		for (unsigned i = 0; i < tiles_per_pokemon_ow_sprite; i++)
			--oam[i].y_position;
	}else{
		for (unsigned i = 0; i < tiles_per_pokemon_ow_sprite; i++)
			oam[i].tile_number += tiles_per_pokemon_ow_sprite * (int)PokemonOverworldSprite::Count;
	}
}

void CppRed::display_picture_centered_or_upper_right(const BaseStaticImage &image, Placing placing){
	auto image_data = decode_image_data(image);
	image_data = pad_tiles_for_mon_pic(image_data, image.get_width() / 16, image.get_height() / 16);
	this->copy_video_data(&image_data[0], image_data.size(), vFrontPic);
	tilemap_it position;
	switch (placing){
		case Placing::Centered:
			position = this->get_tilemap_location(15, 1);
			break;
		case Placing::TopLeft:
			position = this->get_tilemap_location(6, 4);
			break;
		default:
			throw std::runtime_error("CppRed::display_picture_centered_or_upper_right(): Invalid switch.");
	}
	write_mon_pic_tiles_to_buffer(position, tilemap_width);
}

std::string CppRed::display_naming_screen(NamingScreenType type){
	this->wram.wNamingScreenType = type;
	throw NotImplementedException();
}

bool CppRed::check_for_data_clear_request(InputBitmap_struct input){
	return input.button_up && input.button_select && input.button_b;
}

bool CppRed::check_for_user_interruption(unsigned max_frames){
	while (max_frames--){
		this->delay_frame();
		auto input = this->joypad_low_sensitivity();
		InputBitmap_struct held = this->hram.hJoyHeld;
		if (check_for_data_clear_request(held) || input.button_start || input.button_a)
			return true;
	}
	return false;
}

void CppRed::place_menu_cursor(){
	const std::pair<unsigned, unsigned> location(this->wram.wTopMenuItemX, this->wram.wTopMenuItemY);
	auto last_item = +this->wram.wLastMenuItem;

	{
		auto last_location = location;
		if (last_item)
			last_location.second += 2 - this->hram.hFlags_0xFFF6.get_menu_singly_spaced();

		auto it = this->get_tilemap_location(location);
		if (*it == SpecialCharacters::arrow_black_right)
			*it = +this->wram.wTileBehindCursor;
	}

	{
		auto current_location = location;
		auto current_item = +this->wram.wCurrentMenuItem;
		if (current_item){
			current_location.second += 2 - this->hram.hFlags_0xFFF6.get_menu_singly_spaced();
		}

		auto it = this->get_tilemap_location(location);
		if (*it != SpecialCharacters::arrow_black_right)
			this->wram.wTileBehindCursor = +*it;
		*it = SpecialCharacters::arrow_black_right;

		this->wram.wMenuCursorLocationX = location.first;
		this->wram.wMenuCursorLocationY = location.second;
	}

	this->wram.wLastMenuItem = +this->wram.wCurrentMenuItem;
}

void CppRed::handle_down_arrow_blink_timing(const tilemap_it &location){
	if (*location == SpecialCharacters::arrow_black_down){
		if (--this->hram.H_DOWNARROWBLINKCNT1)
			return;
		if (--this->hram.H_DOWNARROWBLINKCNT2)
			return;
		*location = SpecialCharacters::blank;
		this->hram.H_DOWNARROWBLINKCNT1 = 0xFF;
		this->hram.H_DOWNARROWBLINKCNT2 = 6;
		return;
	}

	if (!this->hram.H_DOWNARROWBLINKCNT1)
		return;
	if (--this->hram.H_DOWNARROWBLINKCNT1)
		return;
	--this->hram.H_DOWNARROWBLINKCNT1;
	if (--this->hram.H_DOWNARROWBLINKCNT2)
		return;
	this->hram.H_DOWNARROWBLINKCNT2 = 6;
	*location = SpecialCharacters::arrow_black_down;
}

RenderedFrame *CppRed::get_current_frame(){
	return this->display_controller.get_current_frame();
}

void CppRed::town_map_sprite_blinking_animation(){
	auto counter = this->wram.wAnimCounter + 1;
	if (counter != 25){
		if (counter == 50){
			size_t i = 0;
			for (auto src : this->wram.wOAMBufferBackup)
				this->wram.wOAMBuffer[i++].assign(src);
			counter = 0;
		}
	}else{
		for (auto sprite : this->wram.wOAMBuffer)
			sprite.y_position = lcd_height;
	}
	this->wram.wAnimCounter = counter;
	this->delay_frame();
}

void CppRed::vblank_irq(){
	if (!this->IF || !check_flag(this->IE, vblank_mask))
		return;
	this->SCX = +this->hram.hSCX;
	this->SCY = +this->hram.hSCY;

	if (!this->wram.wDisableVBlankWYUpdate)
		this->WY = +this->hram.hWY;
	
	this->auto_bg_map_transfer();
	this->vblank_copy_bg_map();
	this->redraw_row_or_column();
	this->vblank_copy();
	this->update_moving_bg_tiles();
	this->oam_dma();
	this->prepare_oam_data();

	this->hram.H_VBLANKOCCURRED = 0;

	if (this->hram.H_FRAMECOUNTER)
		--this->hram.H_FRAMECOUNTER;

	this->audio.fade_out_audio();
	this->audio.audio_update_music();
	this->audio.music_do_low_health_alert();

	this->track_play_time();

	if (!this->hram.hDisableJoypadPolling)
		this->read_joypad();
}

void CppRed::auto_bg_map_transfer(){
	if (!this->hram.H_AUTOBGTRANSFERENABLED)
		return;
	auto src = this->get_tilemap_location(0, 0);
	auto dst = &this->display_controller.access_vram(this->hram.H_AUTOBGTRANSFERDEST);
	std::copy(src, src + tilemap_width * tilemap_height, dst);
}

void CppRed::vblank_copy_bg_map(){
	auto src = +this->hram.H_VBCOPYBGSRC;
	if (!(src & 0xFF))
		return;
	this->hram.H_VBCOPYBGSRC = 0;
	auto dst = +this->hram.H_VBCOPYBGDEST;
	auto rows = +this->hram.H_VBCOPYBGNUMROWS;
	throw NotImplementedException();
}

void CppRed::redraw_row_or_column(){
	auto mode = this->hram.hRedrawRowOrColumnMode;
	if (!mode)
		return;
	this->hram.hRedrawRowOrColumnMode = 0;
	throw NotImplementedException();

	if (mode == 1){
		//redraw column
	}else{
		//redraw row
	}
}

void CppRed::vblank_copy(){
	auto src = this->vblank_copy_src;
	if (!src)
		return;
	this->vblank_copy_src = nullptr;
	auto dst = &this->display_controller.access_vram(this->vblank_copy_dst);
	memcpy(dst, src, this->vblank_copy_size);
}

void CppRed::update_moving_bg_tiles(){
	throw NotImplementedException();
}

void CppRed::oam_dma(){
	auto src = this->wram.wOAMBuffer.get_memory();
	auto dst = this->display_controller.get_oam();
	std::copy(src, src + gb_max_sprite_count * SpriteObject::size, dst);
}

typedef std::array<byte_t, 4> FAA_data;
struct OAM_CornerParameters{
	byte_t y_offset;
	byte_t x_offset;
	byte_t attributes;
};
typedef std::array<OAM_CornerParameters, 4> OAM_Parameters;

struct SpriteFacingAndAnimationElement{
	const FAA_data *data1;
	const OAM_Parameters *data2;
};

static const FAA_data SpriteFacingDownAndStanding = { 0x00, 0x01, 0x02, 0x03 };
static const FAA_data SpriteFacingUpAndStanding   = { 0x04, 0x05, 0x06, 0x07 };
static const FAA_data SpriteFacingLeftAndStanding = { 0x08, 0x09, 0x0A, 0x0B };
static const FAA_data SpriteFacingDownAndWalking  = { 0x80, 0x81, 0x82, 0x83 };
static const FAA_data SpriteFacingUpAndWalking    = { 0x84, 0x85, 0x86, 0x87 };
static const FAA_data SpriteFacingLeftAndWalking  = { 0x88, 0x89, 0x8A, 0x8B };

static const OAM_Parameters SpriteOAMParameters = {
	OAM_CornerParameters{ 0x00, 0x00, 0x00 },
	OAM_CornerParameters{ 0x00, 0x08, 0x00 },
	OAM_CornerParameters{ 0x08, 0x00, oamflag_canbemasked },
	OAM_CornerParameters{ 0x08, 0x08, oamflag_canbemasked },
};

static const OAM_Parameters SpriteOAMParametersFlipped = {
	OAM_CornerParameters{ 0x00, 0x08, oamflag_vflipped },
	OAM_CornerParameters{ 0x00, 0x00, oamflag_vflipped },
	OAM_CornerParameters{ 0x08, 0x00, oamflag_vflipped | oamflag_canbemasked },
	OAM_CornerParameters{ 0x08, 0x08, oamflag_vflipped | oamflag_canbemasked },
};

static const SpriteFacingAndAnimationElement SpriteFacingAndAnimationTable[] = {
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndWalking,  &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndWalking,  &SpriteOAMParametersFlipped },
	{ &SpriteFacingUpAndStanding,   &SpriteOAMParameters        },
	{ &SpriteFacingUpAndWalking,    &SpriteOAMParameters        },
	{ &SpriteFacingUpAndStanding,   &SpriteOAMParameters        },
	{ &SpriteFacingUpAndWalking,    &SpriteOAMParametersFlipped },
	{ &SpriteFacingLeftAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingLeftAndWalking,  &SpriteOAMParameters        },
	{ &SpriteFacingLeftAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingLeftAndWalking,  &SpriteOAMParameters        },
	{ &SpriteFacingLeftAndStanding, &SpriteOAMParametersFlipped },
	{ &SpriteFacingLeftAndWalking,  &SpriteOAMParametersFlipped },
	{ &SpriteFacingLeftAndStanding, &SpriteOAMParametersFlipped },
	{ &SpriteFacingLeftAndWalking,  &SpriteOAMParametersFlipped },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
	{ &SpriteFacingDownAndStanding, &SpriteOAMParameters        },
};

void CppRed::prepare_oam_data(){
	auto enabled = sign_extend(this->wram.wUpdateSpritesEnabled) - 1;
	if (enabled){
		if (enabled != -1)
			return;
		this->wram.wUpdateSpritesEnabled = reduce_sign(enabled);
		this->hide_sprites();
		return;
	}

	size_t last_sprite_unupdated = 0;

	for (size_t i = 0; i < this->wram.wSpriteData.wSpriteStateData1.length; i++){
		auto sprite1 = this->wram.wSpriteData.wSpriteStateData1[i];
		if (!sprite1.picture_id)
			continue;
		auto index = +sprite1.sprite_image_idx;
		if (index == 0xFF){
			//Invisible.
			this->get_sprite_screen_xy(sprite1);
			continue;
		}
		//Visible.
		if (index >= lcd_width)
			//Sprite is not animated.
			index = (index & 0x0F) | 0x10;
		else
			//Use facing.
			index &= 0x0F;

		auto sprite2 = this->wram.wSpriteData.wSpriteStateData2[i];
		auto priority = sprite2.grass_priority & 0x80;
		this->hram.hSpritePriority = priority;
		
		auto &sprite_data = SpriteFacingAndAnimationTable[index];
		auto location = this->get_sprite_screen_xy(sprite1);
		auto dst = this->wram.wOAMBuffer.begin() + i * 4;
		last_sprite_unupdated = (i + 1) * 4;

		for (int j = 0; j < 4; j++){
			auto tile_offset = (*sprite_data.data1)[j];
			auto param = (*sprite_data.data2)[j];
			auto oam_dst = dst[j];
			oam_dst.x_position = location.first + 8 + param.x_offset;
			oam_dst.y_position = location.second + 16 + param.y_offset;
			auto sprite_used = sprite1.sprite_image_idx >> 4;
			unsigned subsprite_used = 0;
			if (sprite_used == 11)
				subsprite_used = 10 * 12 + 4;
			else
				subsprite_used = sprite_used * 12;

			oam_dst.tile_number = tile_offset + subsprite_used;
			oam_dst.attributes = param.attributes;
			if (check_flag(param.attributes, oamflag_canbemasked))
				oam_dst.attributes |= priority;
		}
	}

	auto end = this->wram.wOAMBuffer.length;
	if (this->wram.wMainData.wd736.get_doing_animation())
		end -= 4;
	for (auto i = last_sprite_unupdated; i < end; i++){
		auto oam = this->wram.wOAMBuffer[i];
		oam.x_position = lcd_width;
		oam.y_position = lcd_width;
		oam.tile_number = lcd_width;
		oam.attributes = lcd_width;
	}
}

std::pair<unsigned, unsigned> CppRed::get_sprite_screen_xy(SpriteStateData1 &sprite){
	std::pair<unsigned, unsigned> ret(sprite.x_pixels, sprite.y_pixels);
	sprite.tile_position_x = ret.first & 0xF0;
	sprite.tile_position_y = (ret.second + 4) & 0xF0;
	this->hram.hSpriteScreenX = ret.first;
	this->hram.hSpriteScreenY = ret.second;
	return ret;
}

void CppRed::track_play_time(){
	//It's structured goto time!
	do{
		unsigned counter = +this->wram.wIgnoreInputCounter;
		counter = (counter + 0xFF) & 0xFF;
		this->wram.wIgnoreInputCounter = counter;
		if (counter)
			continue;
		auto wd730 = this->wram.wMainData.wd730;
		wd730.set_unknown1(false);
		wd730.set_unknown2(false);
		auto old = wd730.get_ignore_input();
		wd730.set_ignore_input(false);
		if (!old)
			continue;
		this->hram.hJoyPressed.clear();
		this->hram.hJoyHeld.clear();
	}while (false);

	auto main = this->wram.wMainData;
	if (!main.wd732.get_counting_play_time())
		return;
	if (main.wPlayTimeMaxed)
		return;

	if (++main.wPlayTimeFrames != 60)
		return;
	main.wPlayTimeFrames = 0;
	if (++main.wPlayTimeSeconds != 60)
		return;
	main.wPlayTimeSeconds = 0;
	if (++main.wPlayTimeMinutes != 60)
		return;
	main.wPlayTimeMinutes = 0;
	if (++main.wPlayTimeHours != 0xFF)
		return;
	main.wPlayTimeMaxed = 0xFF;
}

void CppRed::read_joypad(){
	const byte_t select_dpad = 1 << 5;
	const byte_t select_buttons = 1 << 4;
	const byte_t select_none = select_dpad | select_buttons;

	auto &ic = this->input_controller;
	ic.request_input_state(select_dpad);
	byte_t state;
	for (int i = 0; i < 6; i++)
		state = ic.get_requested_input_state();
	byte_t final_state = (state ^ 0xFF) << 4;

	ic.request_input_state(select_buttons);
	for (int i = 0; i < 10; i++)
		state = ic.get_requested_input_state();
	final_state |= (state ^ 0xFF) & 0x0F;

	this->hram.hJoyInput.set_raw_value(final_state);
	ic.request_input_state(select_none);
}

void CppRed::hide_sprites(){
	for (auto sprite : this->wram.wOAMBuffer)
		sprite.y_position = lcd_width;
}

void *CppRed::map_pointer(unsigned pointer){
	throw NotImplementedException();
}

void CppRed::return_used_frame(RenderedFrame *frame){
	this->display_controller.return_used_frame((RenderedFrame *)frame);
}

std::uint32_t CppRed::simulate_time(){
	if (this->time_simulation_lock)
		return 0;
	this->time_simulation_lock = true;

	if (!this->clock.lock_clock_value(get_timer_count() * this->real_time_multiplier))
		return 0;

	this->sound_controller.update();
	auto ret = this->display_controller.update();

	this->time_simulation_lock = false;
	return ret;
}

void CppRed::delay_frame(){
	while (!check_flag(this->simulate_time(), DisplayController::update_vsync_happened))
		this->periodic_notification.reset_and_wait_for(250);
}

void CppRed::wait_ly(unsigned value, bool metavalue){
	if ((this->display_controller.get_y_coordinate() == value) == metavalue)
		return;
	if (this->time_simulation_lock)
		throw std::runtime_error("CppRed::wait_for_ly(): Infinite loop detected!");
	while (true){
		std::uint32_t state;
		while (!check_flag(state = this->simulate_time(), DisplayController::update_ly_happened))
			this->periodic_notification.reset_and_wait_for(250);
		state &= DisplayController::update_ly_mask;
		state >>= DisplayController::update_ly_shift;
		if ((state == value) != metavalue)
			break;
	}
}

void CppRed::wait_for_ly(unsigned value){
	this->wait_ly(value, true);
}

void CppRed::wait_while_ly(unsigned value){
	this->wait_ly(value, false);
}

void CppRed::load_copyright_and_textbox_tiles(){
	this->hram.hWY = 0;
	this->clear_screen();
	this->load_textbox_tile_patterns();

	this->copy_video_data(NintendoCopyrightLogoGraphics, vChars2 + 0x600);

	static const byte_t strings[] = {
		/* (c)'95,'96,'98  Nintendo        */ 0x60, 0x61, 0x62, 0x61, 0x63, 0x61, 0x64, 0x7F, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x00,
		/* (c)'95,'96,'98  Creatures inc.  */ 0x60, 0x61, 0x62, 0x61, 0x63, 0x61, 0x64, 0x7F, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x00,
		/* (c)'95,'96,'98  GAME FREAK inc. */ 0x60, 0x61, 0x62, 0x61, 0x63, 0x61, 0x64, 0x7F, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B,
	};

	auto begin = this->get_tilemap_location(2, 7);
	auto it = begin;
	for (auto b : strings){
		if (!b){
			begin += 2 * tilemap_width;
			it = begin;
			continue;
		}
		*(it++) = b;
	}
}

void CppRed::init_player_data2(){
	throw NotImplementedException();
}

void CppRed::load_tileset_header(){
	throw NotImplementedException();
}

void CppRed::cable_club_run(){
	throw NotImplementedException();
}
