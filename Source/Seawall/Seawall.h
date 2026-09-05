#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// Our own log category. Everything Seawall prints goes through this, so the
// game's own messages can be filtered out of Unreal's very noisy log with
// `find_in_log("LogSeawall")` or, in the editor console, `Log LogSeawall`.
//
// Usage:  UE_LOG(LogSeawall, Log, TEXT("Noise event at %s"), *Location.ToString());
DECLARE_LOG_CATEGORY_EXTERN(LogSeawall, Log, All);
