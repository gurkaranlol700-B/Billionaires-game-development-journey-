#include "Audio/SWAudioSettings.h"

const USWAudioSettings& USWAudioSettings::Get()
{
	// GetDefault never returns null for a UDeveloperSettings: the CDO is created
	// with the class and holds the values loaded from DefaultGame.ini.
	return *GetDefault<USWAudioSettings>();
}
