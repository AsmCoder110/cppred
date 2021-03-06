#include "CppRedEntryPoint.h"
#include "Engine.h"
#include "Renderer.h"
#include "CppRedEngine.h"
#include "CppRedIntro.h"
#include "CppRedTitleScreen.h"
#include "CppRedMainMenu.h"
#include "CppRedClearSave.h"
#include "CppRedOakSpeech.h"

namespace CppRedScripts{

static MainMenuResult initial_sequence(CppRedEngine &cppred){
	auto &engine = cppred.get_engine();
	while (true){
		intro(cppred);
		while (title_screen(cppred) == TitleScreenResult::GoToMainMenu){
			auto main_menu_result = main_menu(cppred);
			if (main_menu_result != MainMenuResult::GoToTitleScreen)
				return main_menu_result;
		}
		//assert(title_screen_result == TitleScreenResult::GoToClearSaveDialog);
		//clear_save_dialog(cppred);
	}
}

void entry_point(Engine &engine, PokemonVersion version, CppRedAudioProgram &program){
	CppRedEngine cppred(engine, version, program);
	if (initial_sequence(cppred) == MainMenuResult::ContinueGame){
		//Continue game.
	}else{
		auto names = oak_speech(cppred);
	}
}

}
