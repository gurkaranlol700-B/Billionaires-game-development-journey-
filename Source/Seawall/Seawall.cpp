#include "Seawall.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogSeawall);

// Registers this module as the project's primary game module. Without this the
// engine loads the project but none of our C++ classes exist.
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, Seawall, "Seawall");
