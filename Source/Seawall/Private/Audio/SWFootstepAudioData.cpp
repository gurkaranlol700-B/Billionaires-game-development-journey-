#include "Audio/SWFootstepAudioData.h"

const FSWSurfaceFootsteps& USWFootstepAudioData::Resolve(EPhysicalSurface Surface) const
{
	if (const FSWSurfaceFootsteps* Found = Surfaces.Find(Surface))
	{
		return *Found;
	}
	return Default;
}
