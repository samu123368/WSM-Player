#ifndef MIIPROFILES_H_
#define MIIPROFILES_H_

#include <string>
#include <vector>

// Read-only access to the real Mii Channel database on NAND.  WSM Player only
// stores the chosen display name in its own settings and never writes RFL_DB.
namespace MiiProfiles
{
	const std::vector<std::string> &Names();
	std::string Step( const std::string &current, int direction );
	// Wii Fit's capture has eight profile cells. Return the selected NAND Mii's
	// stable Mii Channel order so the renderer can choose the matching cell.
	int IndexOf( const std::string &name );
	void Reload();
}

#endif
