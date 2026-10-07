#ifndef MENUAUDIO_H
#define MENUAUDIO_H

#include <string>

class GuiSound;

// Owns WSM Player's non-banner System Menu audio. The files are rendered from
// the installed Wii System Menu NAND and streamed beside boot.dol.
class MenuAudio
{
public:
	static MenuAudio *Instance();

	void StartMusic();
	void PauseMusic();
	void ApplySettings();
	void Reset();

private:
	MenuAudio();
	~MenuAudio();
	MenuAudio( const MenuAudio & );
	MenuAudio &operator=( const MenuAudio & );

	bool EnsureMusic();
	bool LocateAsset( const char *name, std::string &path ) const;

	static MenuAudio *instance;
	GuiSound *music;
};

#endif
