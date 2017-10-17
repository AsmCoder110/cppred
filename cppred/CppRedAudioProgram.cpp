#include "CppRedAudioProgram.h"
#include "CppRedData.h"
#include "utility.h"
#include "../common/calculate_frequency.h"
#include <set>

const byte_t command_parameter_counts[] = { 1, 2, 1, 1, 3, 0, 3, 1, 1, 2, 1, 2, 2, 1, 4, 3, 0, 2, 2, 1, 2, 2, 1, 1, 0, 0, 0, 0, };

#define DECLARE_COMMAND_FUNCTION_IN_ARRAY(x) &CppRedAudioProgram::Channel::command_##x

const CppRedAudioProgram::Channel::command_function CppRedAudioProgram::Channel::command_functions[28] = {
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Tempo),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Volume),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Duty),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(DutyCycle),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Vibrato),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(TogglePerfectPitch),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(NoteType),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Rest),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Octave),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Note),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(DSpeed),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Snare),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(MutedSnare),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(UnknownSfx10),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(UnknownSfx20),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(UnknownNoise20),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(ExecuteMusic),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(PitchBend),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Triangle),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(StereoPanning),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Cymbal),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Loop),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Call),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Goto),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(IfRed),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(Else),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(EndIf),
	DECLARE_COMMAND_FUNCTION_IN_ARRAY(End),
};

#define DEFINE_AC_STRUCT0(name)
#define DEFINE_AC_STRUCT1(name, p1)                  \
	struct name##AudioCommand{                       \
		std::uint32_t p1;                            \
		name##AudioCommand(const AudioCommand &cmd): \
			p1(cmd.params[0]){}                      \
	}
#define DEFINE_AC_STRUCT2(name, p1, p2)              \
	struct name##AudioCommand{                       \
		std::uint32_t p1;                            \
		std::uint32_t p2;                            \
		name##AudioCommand(const AudioCommand &cmd): \
			p1(cmd.params[0]),                       \
			p2(cmd.params[1]){}                      \
	}
#define DEFINE_AC_STRUCT3(name, p1, p2, p3)          \
	struct name##AudioCommand{                       \
		std::uint32_t p1;                            \
		std::uint32_t p2;                            \
		std::uint32_t p3;                            \
		name##AudioCommand(const AudioCommand &cmd): \
			p1(cmd.params[0]),                       \
			p2(cmd.params[1]),                       \
			p3(cmd.params[2]){}                      \
	}
#define DEFINE_AC_STRUCT4(name, p1, p2, p3, p4)      \
	struct name##AudioCommand{                       \
		std::uint32_t p1;                            \
		std::uint32_t p2;                            \
		std::uint32_t p3;                            \
		std::uint32_t p4;                            \
		name##AudioCommand(const AudioCommand &cmd): \
			p1(cmd.params[0]),                       \
			p2(cmd.params[1]),                       \
			p3(cmd.params[2]),                       \
			p4(cmd.params[3]){}                      \
	}

DEFINE_AC_STRUCT1(Tempo, tempo);
DEFINE_AC_STRUCT2(Volume, vol1, vol2);
DEFINE_AC_STRUCT1(Duty, duty);
DEFINE_AC_STRUCT1(DutyCycle, duty);
DEFINE_AC_STRUCT3(Vibrato, delay, rate, depth);
DEFINE_AC_STRUCT0(TogglePerfectPitch);
DEFINE_AC_STRUCT3(NoteType, speed, volume, fade);
DEFINE_AC_STRUCT1(Rest, length);
DEFINE_AC_STRUCT1(Octave, octave);
DEFINE_AC_STRUCT2(Note, pitch, length);
DEFINE_AC_STRUCT1(DSpeed, speed);
DEFINE_AC_STRUCT2(Snare, type, length);
DEFINE_AC_STRUCT2(MutedSnare, type, length);
DEFINE_AC_STRUCT1(UnknownSfx10, nr10);
DEFINE_AC_STRUCT3(UnknownSfx20, length, envelope, frequency);
DEFINE_AC_STRUCT3(UnknownNoise20, length, envelope, frequency);
DEFINE_AC_STRUCT0(ExecuteMusic);
DEFINE_AC_STRUCT2(PitchBend, length, frequency);
DEFINE_AC_STRUCT2(Triangle, type, length);
DEFINE_AC_STRUCT1(StereoPanning, value);
DEFINE_AC_STRUCT2(Cymbal, type, length);
DEFINE_AC_STRUCT2(Loop, times, dst);
DEFINE_AC_STRUCT1(Call, dst);
DEFINE_AC_STRUCT1(Goto, dst);
DEFINE_AC_STRUCT0(IfRed);
DEFINE_AC_STRUCT0(Else);
DEFINE_AC_STRUCT0(EndIf);
DEFINE_AC_STRUCT0(End);

const std::set<AudioResourceId> CppRedAudioProgram::Channel::pokemon_cries = {
	AudioResourceId::SFX_Cry00,
	AudioResourceId::SFX_Cry01,
	AudioResourceId::SFX_Cry02,
	AudioResourceId::SFX_Cry03,
	AudioResourceId::SFX_Cry04,
	AudioResourceId::SFX_Cry05,
	AudioResourceId::SFX_Cry06,
	AudioResourceId::SFX_Cry07,
	AudioResourceId::SFX_Cry08,
	AudioResourceId::SFX_Cry09,
	AudioResourceId::SFX_Cry0A,
	AudioResourceId::SFX_Cry0B,
	AudioResourceId::SFX_Cry0C,
	AudioResourceId::SFX_Cry0D,
	AudioResourceId::SFX_Cry0E,
	AudioResourceId::SFX_Cry0F,
	AudioResourceId::SFX_Cry10,
	AudioResourceId::SFX_Cry11,
	AudioResourceId::SFX_Cry12,
	AudioResourceId::SFX_Cry13,
	AudioResourceId::SFX_Cry14,
	AudioResourceId::SFX_Cry15,
	AudioResourceId::SFX_Cry16,
	AudioResourceId::SFX_Cry17,
	AudioResourceId::SFX_Cry18,
	AudioResourceId::SFX_Cry19,
	AudioResourceId::SFX_Cry1A,
	AudioResourceId::SFX_Cry1B,
	AudioResourceId::SFX_Cry1C,
	AudioResourceId::SFX_Cry1D,
	AudioResourceId::SFX_Cry1E,
	AudioResourceId::SFX_Cry1F,
	AudioResourceId::SFX_Cry20,
	AudioResourceId::SFX_Cry21,
	AudioResourceId::SFX_Cry22,
	AudioResourceId::SFX_Cry23,
	AudioResourceId::SFX_Cry24,
	AudioResourceId::SFX_Cry25,
};

static const byte_t disable_channel_masks[] = {
	BITMAP(11101110),
	BITMAP(11011101),
	BITMAP(10111011),
	BITMAP(01110111),
};

static const byte_t enable_channel_masks[] = {
	BITMAP(00010001),
	BITMAP(00100010),
	BITMAP(01000100),
	BITMAP(10001000),
};

CppRedAudioProgram::CppRedAudioProgram(){
	this->load_commands();
	this->load_resources();
}

void CppRedAudioProgram::load_commands(){
	static_assert(array_length(command_parameter_counts) == (size_t)AudioCommandType::End + 1, "");
	auto buffer = audio_sequence_data;
	size_t offset = 0;
	const size_t size = audio_sequence_data_size;
	this->commands.resize(read_varint(buffer, offset, size));
	for (auto &command : this->commands){
		auto type = read_varint(buffer, offset, size);
		if (type > (std::uint32_t)AudioCommandType::End)
			throw std::runtime_error("CppRedAudioProgram::load_commands(): Invalid data.");
		command.type = (AudioCommandType)type;
		std::fill(command.params, command.params + array_length(command.params), std::numeric_limits<std::uint32_t>::max());
		for (int i = 0; i < (int)command_parameter_counts[type]; i++)
			command.params[i] = read_varint(buffer, offset, size);
	}
	assert(offset == size);
}

void CppRedAudioProgram::load_resources(){
	auto buffer = audio_header_data;
	size_t offset = 0;
	const size_t size = audio_header_data_size;
	this->resources.resize(read_varint(buffer, offset, size));
	assert(this->resources.size() == (size_t)AudioResourceId::Stop - 1);
	for (auto &resource : this->resources){
		resource.name = read_string(buffer, offset, size);
		resource.bank = (byte_t)read_varint(buffer, offset, size);
		resource.channel_count = (byte_t)read_varint(buffer, offset, size);
		for (byte_t i = 0; i < resource.channel_count; i++){
			resource.channels[i].entry_point = read_varint(buffer, offset, size);
			resource.channels[i].channel = read_varint(buffer, offset, size);
		}
	}
}

const double CppRedAudioProgram::update_threshold = 4389.0 / 262144.0;

void CppRedAudioProgram::update(double now, AbstractAudioRenderer &renderer){
	auto delta = now - this->last_update;
	if (this->last_update > 0 || delta < update_threshold)
		return;
	delta *= 1.0 / update_threshold;
	int n = (int)(delta * (1.0 / update_threshold));
	for (int i = 0; i < n; i++){
		for (auto &c : this->channels){
			if (!c)
				continue;
			if (!c->update(renderer))
				c.reset();
		}
	}
	this->last_update = now - (delta - n * update_threshold);
}

bool CppRedAudioProgram::Channel::update(AbstractAudioRenderer &renderer){
	if (this->channel_no >= 4 || !this->program->mute_audio_and_pause_music)
		return this->apply_effects(renderer);
	if (this->program->mute_audio_and_pause_music & (1 << 7))
		return true;
	this->program->mute_audio_and_pause_music |= 1 << 7;
	renderer.set_NR51(0);
	renderer.set_NR30(0);
	renderer.set_NR30(0x80);
	return true;
}

bool CppRedAudioProgram::Channel::apply_effects(AbstractAudioRenderer &renderer){
	if (this->note_delay_counter == 1)
		return this->play_next_note(renderer);
	this->note_delay_counter--;
	if (this->channel_no < 4 && this->program->channels[4 + this->channel_no])
		return true;
	if (this->do_rotate_duty)
		this->apply_duty_cycle(renderer);
	if (!this->do_execute_music && !this->do_noise_or_sfx)
		return true;
	if (this->do_pitch_bend){
		this->apply_pitch_bend(renderer);
		return true;
	}
	this->apply_vibrato(renderer);
	return true;
}

void CppRedAudioProgram::Channel::apply_vibrato(AbstractAudioRenderer &renderer){
	if (this->vibrato_delay_counter){
		this->vibrato_delay_counter--;
		return;
	}
	if (!this->vibrato_extent)
		return;
	if (this->vibrato_counter){
		this->vibrato_counter--;
		return;
	}
	this->vibrato_counter = this->vibrato_length;
	int diff;
	if (this->vibrato_direction){
		this->vibrato_direction = false;
		diff = this->channel_frequency - this->vibrato_extent % 16;
		if (diff < 0)
			diff = 0;
	}else{
		this->vibrato_direction = true;
		diff = this->channel_frequency + this->vibrato_extent / 16;
		if (diff > 255)
			diff = 255;
	}
	this->program->get_register_pointer(RegisterId::FrequencyLow)(renderer, (byte_t)diff);
}

bool CppRedAudioProgram::Channel::play_next_note(AbstractAudioRenderer &renderer){
	this->vibrato_delay_counter = this->vibrato_delay_counter_reload_value;
	this->do_pitch_bend = false;
	this->pitch_bend_decreasing = false;
	return this->continue_execution(renderer);
}

bool CppRedAudioProgram::Channel::continue_execution(AbstractAudioRenderer &renderer){
	bool ret = true;
	while (true){
		auto &command = this->program->commands[this->program_counter++];
		this->program_counter %= this->program->commands.size();
		if (!this->ifred_execute_bit)
			continue;
		if (!(this->*this->command_functions[(int)command.type])(command, renderer, ret))
			break;
	}
	return ret;
}

#define DEFINE_COMMAND_FUNCTION(x) bool CppRedAudioProgram::Channel::command_##x(const AudioCommand &command_, AbstractAudioRenderer &renderer, bool &dont_stop_this_channel)

DEFINE_COMMAND_FUNCTION(Tempo){
	TempoAudioCommand command(command_);
	int offset;
	if (this->channel_no < 4){
		this->program->music_tempo = command.tempo;
		offset = 0;
	}else{
		this->program->sfx_tempo = command.tempo;
		offset = 4;
	}
	for (int i = 0; i < 4; i++){
		auto &channel = this->program->channels[i + offset];
		if (channel)
			channel->note_delay_counter_fractional_part = 0;
	}
	return true;
}

DEFINE_COMMAND_FUNCTION(Volume){
	VolumeAudioCommand command(command_);
	renderer.set_NR50((byte_t)((command.vol1 & 0x0F) | ((command.vol2 & 0x0F) << 4)));
	return true;
}

DEFINE_COMMAND_FUNCTION(Duty){
	DutyAudioCommand command(command_);
	this->duty = command.duty;
	return true;
}

DEFINE_COMMAND_FUNCTION(DutyCycle){
	DutyCycleAudioCommand command(command_);
	this->duty_cycle = command.duty;
	this->duty = command.duty & 0xC0;
	this->do_rotate_duty = true;
	return true;
}

DEFINE_COMMAND_FUNCTION(Vibrato){
	VibratoAudioCommand command(command_);
	this->vibrato_delay_counter_reload_value = this->vibrato_delay_counter = command.delay;
	this->vibrato_extent = (command.rate + 1) / 2;
	this->vibrato_extent |= this->vibrato_extent << 4;
	this->vibrato_depth_reload = this->vibrato_depth = command.depth;
	return true;
}

DEFINE_COMMAND_FUNCTION(TogglePerfectPitch){
	this->perfect_pitch = !this->perfect_pitch;
	return true;
}

DEFINE_COMMAND_FUNCTION(NoteType){
	NoteTypeAudioCommand command(command_);
	this->note_speed = command.speed;

	int *dst = nullptr;
	if (this->channel_no == 2)
		dst = &this->program->music_wave_instrument;
	else if (this->channel_no == 6)
		dst = &this->program->sfx_wave_instrument;
	if (dst){
		*dst = command.fade;
		this->volume = (command.volume * 2) % 16;
	}else{
		this->volume = command.volume;
		this->fade = command.fade;
	}
	return true;
}

DEFINE_COMMAND_FUNCTION(Rest){
	RestAudioCommand command(command_);
	this->set_delay_counters(renderer, command.length);
	if (this->channel_no < 4 && this->program->channels[4])
		return true;
	if (this->channel_no % 4 == 2){
		this->disable_this_hardware_channel(renderer);
	}else{
		this->program->get_register_pointer(RegisterId::VolumeEnvelope)(renderer, 0x08);
		this->program->get_register_pointer(RegisterId::VolumeEnvelopePlus1)(renderer, 0x80);
	}
	return true;
}

DEFINE_COMMAND_FUNCTION(Octave){
	OctaveAudioCommand command(command_);
	this->octave = command.octave;
	return true;
}

DEFINE_COMMAND_FUNCTION(Note){
	NoteAudioCommand command(command_);
	auto product = this->note_speed * command.length;
	std::uint32_t tempo;
	if (this->channel_no < 4)
		tempo = this->program->music_tempo;
	else{
		this->set_sfx_tempo();
		tempo = this->program->sfx_tempo;
	}
	product = this->note_delay_counter_fractional_part + tempo * product;
	this->note_delay_counter_fractional_part = product % 256;
	this->note_delay_counter = product / 256;
	if (this->do_execute_music || this->do_noise_or_sfx)
		this->note_pitch(renderer, command.length, command.pitch);
	return true;
}

DEFINE_COMMAND_FUNCTION(DSpeed){
	DSpeedAudioCommand command(command_);
	this->note_speed = command.speed;
	return true;
}

//DEFINE_COMMAND_FUNCTION(Snare)
//DEFINE_COMMAND_FUNCTION(MutedSnare)

DEFINE_COMMAND_FUNCTION(UnknownSfx10){
	if (this->channel_no < 4 || this->do_execute_music)
		return true;
	UnknownSfx10AudioCommand command(command_);
	renderer.set_NR10((byte_t)command.nr10);
	return true;
}

DEFINE_COMMAND_FUNCTION(UnknownSfx20){
	return this->unknown20(command_, renderer, dont_stop_this_channel, false);
}

DEFINE_COMMAND_FUNCTION(UnknownNoise20){
	return this->unknown20(command_, renderer, dont_stop_this_channel, true);
}

bool CppRedAudioProgram::Channel::unknown20(const AudioCommand &command_, AbstractAudioRenderer &renderer, bool &dont_stop_this_channel, bool noise){
	if (this->channel_no < 3 || this->do_execute_music)
		return true;
	UnknownSfx20AudioCommand command(command_);
	this->note_length(renderer, command.length, 2);
	this->program->get_register_pointer(RegisterId::DutySoundLength)(renderer, this->duty | this->note_delay_counter);
	this->program->get_register_pointer(RegisterId::VolumeEnvelope)(renderer, command.envelope);
	this->apply_duty_and_sound_length(renderer);
	this->enable_channel_output(renderer);
	this->apply_wave_pattern_and_frequency(renderer, command.frequency);
	return true;
}

DEFINE_COMMAND_FUNCTION(ExecuteMusic){
	this->do_execute_music = true;
	return true;
}

DEFINE_COMMAND_FUNCTION(PitchBend){
	PitchBendAudioCommand command(command_);
	this->pitch_bend_length = command.length;
	this->pitch_bend_target_frequency = command.frequency;
	this->do_pitch_bend = true;
	return true;
}

//DEFINE_COMMAND_FUNCTION(Triangle)

DEFINE_COMMAND_FUNCTION(StereoPanning){
	StereoPanningAudioCommand command(command_);
	this->program->stereo_panning = command.value;
	return true;
}

//DEFINE_COMMAND_FUNCTION(Cymbal)

DEFINE_COMMAND_FUNCTION(Loop){
	LoopAudioCommand command(command_);
	if (!command.times){
		this->program_counter = command.dst;
		return true;
	}
	if (this->loop_counter < command.times){
		this->loop_counter++;
		this->program_counter = command.dst;
		return true;
	}
	this->loop_counter = 1;
	return true;
}

DEFINE_COMMAND_FUNCTION(Call){
	CallAudioCommand command(command_);
	if (this->call_stack.size())
		std::cout << "Assumption in audio program might have been invalidated.\n";
	this->call_stack.push_back(this->program_counter);
	this->program_counter = command.dst;
	return true;
}

DEFINE_COMMAND_FUNCTION(Goto){
	GotoAudioCommand command(command_);
	this->program_counter = command.dst;
	return true;
}

DEFINE_COMMAND_FUNCTION(IfRed){
#if POKEMON_VERSION == RED
	this->ifred_execute_bit = true;
#elif POKEMON_VERSION == BLUE
	this->ifred_execute_bit = false;
#endif
	return true;
}

DEFINE_COMMAND_FUNCTION(Else){
	this->ifred_execute_bit = !ifred_execute_bit;
	return true;
}

DEFINE_COMMAND_FUNCTION(EndIf){
	this->ifred_execute_bit = true;
	return true;
}

DEFINE_COMMAND_FUNCTION(End){
	if (this->call_stack.size()){
		this->program_counter = this->call_stack.back();
		this->call_stack.pop_back();
		return true;
	}

	if (this->channel_no < 3){
		dont_stop_this_channel = !this->disable_channel_output(renderer);
		return dont_stop_this_channel;
	}
	this->do_noise_or_sfx = false;
	this->do_execute_music = false;
	if (this->channel_no == 6){
		renderer.set_NR30(0);
		renderer.set_NR30(0x80);
		if (this->program->disable_channel_output_when_sfx_ends){
			this->program->disable_channel_output_when_sfx_ends = 0;
			dont_stop_this_channel = !this->disable_channel_output(renderer);
			return dont_stop_this_channel;
		}
	}
	dont_stop_this_channel = !this->disable_channel_output_sub(renderer);
	return dont_stop_this_channel;
}

bool CppRedAudioProgram::Channel::disable_channel_output(AbstractAudioRenderer &renderer){
	this->disable_this_hardware_channel(renderer);
	return this->disable_channel_output_sub(renderer);
}

bool CppRedAudioProgram::Channel::disable_channel_output_sub(AbstractAudioRenderer &renderer){
	if (this->program->channels[4] && this->pokemon_cries.find(this->program->channels[4]->sound_id) != this->pokemon_cries.end()){
		if (this->channel_no == 4 && this->go_back_one_command_if_cry(renderer))
			return false;
		renderer.set_NR50(this->program->saved_volume);
		this->program->saved_volume = 0;
	}
	return true;
}

bool CppRedAudioProgram::Channel::go_back_one_command_if_cry(AbstractAudioRenderer &renderer){
	if (this->pokemon_cries.find(this->sound_id) == this->pokemon_cries.end())
		return false;
	this->program_counter--;
	return true;
}

void CppRedAudioProgram::Channel::note_length(AbstractAudioRenderer &renderer, std::uint32_t length_parameter, std::uint32_t note_parameter){
	this->set_delay_counters(renderer, length_parameter);
	if (this->do_execute_music | this->do_noise_or_sfx)
		this->note_pitch(renderer, length_parameter, note_parameter);
}

void CppRedAudioProgram::Channel::set_delay_counters(AbstractAudioRenderer &renderer, std::uint32_t length_parameter){
	auto length = this->note_speed * length_parameter;
	std::uint32_t tempo;
	if (this->channel_no < 4)
		tempo = this->program->music_tempo;
	else if (this->channel_no != 7)
		tempo = this->program->sfx_tempo;
	else
		tempo = 0x0100;
	auto i = tempo * length + this->note_delay_counter_fractional_part;
	this->note_delay_counter_fractional_part = i & 0xFF;
	this->note_delay_counter = (i >> 8) & 0xFF;
}

void CppRedAudioProgram::Channel::disable_this_hardware_channel(AbstractAudioRenderer &renderer){
	auto r = renderer.get_NR51();
	r &= disable_channel_masks[this->channel_no % 4];
	renderer.set_NR51(r);
}

void CppRedAudioProgram::Channel::note_pitch(AbstractAudioRenderer &renderer, std::uint32_t length_parameter, std::uint32_t note_parameter){
	int frequency = calculate_frequency(note_parameter, this->octave);
	if (this->do_pitch_bend)
		this->init_pitch_bend_variables(frequency);
	if (this->channel_no < 4 && this->program->channels[4])
		return;
	this->program->get_register_pointer(RegisterId::VolumeEnvelope)(renderer, this->volume);
	this->apply_duty_and_sound_length(renderer);
	this->enable_channel_output(renderer);
	if (this->perfect_pitch)
		frequency++;
	this->channel_frequency = frequency;
	this->apply_wave_pattern_and_frequency(renderer, frequency);
}

void CppRedAudioProgram::Channel::init_pitch_bend_variables(int frequency){
	this->pitch_bend_current_frequency = frequency;
	this->pitch_bend_length = this->note_delay_counter - this->pitch_bend_length;
	if (this->pitch_bend_length < 0)
		this->pitch_bend_length = 1;
	int delta = this->pitch_bend_target_frequency - this->pitch_bend_current_frequency;
	if (delta >= 0)
		this->pitch_bend_decreasing = false;
	else{
		this->pitch_bend_decreasing = true;
		delta = -delta;
	}
	int quotient = 0;
	int remainder = delta;
	assert(this->pitch_bend_length >= 0 && this->pitch_bend_length < 0x100);
	do{
		quotient++;
		remainder -= this->pitch_bend_length;
	}while (remainder >= 0x100);
	this->pitch_bend_frequency_steps = quotient;
	this->pitch_bend_current_frequency_fractional_part =
		this->pitch_bend_frequency_steps_fractional_part =
			(remainder + this->pitch_bend_length) & 0xFF;
}

void CppRedAudioProgram::Channel::apply_duty_and_sound_length(AbstractAudioRenderer &renderer){
	auto temp = this->note_delay_counter;
	if (this->channel_no % 4 != 2)
		temp = (temp % 64) | this->duty;
	this->program->get_register_pointer(RegisterId::DutySoundLength)(renderer, (byte_t)temp);
}

void CppRedAudioProgram::Channel::enable_channel_output(AbstractAudioRenderer &renderer){
	auto nr51 = renderer.get_NR51();
	byte_t value;
	if (this->channel_no >= 4 || this->program->channels[4])
		value = nr51 | enable_channel_masks[this->channel_no % 4];
	else
		value = nr51 & disable_channel_masks[this->channel_no % 4]
			| this->program->stereo_panning & enable_channel_masks[this->channel_no % 4];
	renderer.set_NR51(value);
}

void CppRedAudioProgram::Channel::set_sfx_tempo(){
	int tempo;
	if (this->is_cry()){
		tempo = this->program->tempo_modifier + 0x80;
		int d = tempo >= 256;
		tempo &= 0xFF;
		tempo = tempo | (tempo << 8);
	}else{
		tempo = 0x100;
	}
	this->program->sfx_tempo = tempo;
}