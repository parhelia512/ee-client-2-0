//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

FMOD_FUNCTION( FMOD_Channel_GetPaused, (FMOD_CHANNEL *channel, FMOD_BOOL *paused))
FMOD_FUNCTION( FMOD_Channel_SetPaused, (FMOD_CHANNEL *channel, FMOD_BOOL paused));
FMOD_FUNCTION( FMOD_Channel_IsPlaying, (FMOD_CHANNEL *channel, FMOD_BOOL *isplaying))
FMOD_FUNCTION( FMOD_Channel_Set3DAttributes, (FMOD_CHANNEL *channel, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel))
FMOD_FUNCTION( FMOD_Channel_SetFrequency, (FMOD_CHANNEL *channel, float frequency))
FMOD_FUNCTION( FMOD_Channel_SetLoopCount, (FMOD_CHANNEL *channel, int loopcount))
FMOD_FUNCTION( FMOD_Channel_SetPosition, (FMOD_CHANNEL *channel, unsigned int position, FMOD_TIMEUNIT postype))
FMOD_FUNCTION( FMOD_Channel_GetPosition, (FMOD_CHANNEL* channel, unsigned int* position, FMOD_TIMEUNIT postype))
FMOD_FUNCTION( FMOD_Channel_SetVolume, (FMOD_CHANNEL *channel, float volume))
FMOD_FUNCTION( FMOD_Channel_GetVolume, (FMOD_CHANNEL *channel, float *volume));
FMOD_FUNCTION( FMOD_Channel_Stop, (FMOD_CHANNEL *channel))
FMOD_FUNCTION( FMOD_Channel_SetMode, (FMOD_CHANNEL *channel, FMOD_MODE mode));
FMOD_FUNCTION( FMOD_Channel_Set3DMinMaxDistance, (FMOD_CHANNEL *channel, float mindistance, float maxdistance));
FMOD_FUNCTION( FMOD_Channel_Set3DConeSettings, (FMOD_CHANNEL *channel, float insideconeangle, float outsideconeangle, float outsidevolume))
FMOD_FUNCTION( FMOD_Channel_Set3DConeOrientation, (FMOD_CHANNEL *channel, FMOD_VECTOR *orientation))
FMOD_FUNCTION( FMOD_Channel_SetReverbProperties, ( FMOD_CHANNEL* channel, const FMOD_REVERB_CHANNELPROPERTIES* prop ) )

FMOD_FUNCTION( FMOD_Sound_Lock, (FMOD_SOUND *sound, unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2))
FMOD_FUNCTION( FMOD_Sound_Unlock, (FMOD_SOUND *sound, void *ptr1, void *ptr2, unsigned int len1, unsigned int len2))
FMOD_FUNCTION( FMOD_Sound_Release, (FMOD_SOUND *sound))
FMOD_FUNCTION( FMOD_Sound_Set3DMinMaxDistance, (FMOD_SOUND *sound, float min, float max))
FMOD_FUNCTION( FMOD_Sound_SetMode, (FMOD_SOUND *sound, FMOD_MODE mode))
FMOD_FUNCTION( FMOD_Sound_GetMode, (FMOD_SOUND *sound, FMOD_MODE *mode))
FMOD_FUNCTION( FMOD_Sound_SetLoopCount, (FMOD_SOUND *sound, int loopcount))
FMOD_FUNCTION( FMOD_Sound_GetFormat, ( FMOD_SOUND* sound, FMOD_SOUND_TYPE* type, FMOD_SOUND_FORMAT* format, int* channels, int* bits ) )
FMOD_FUNCTION( FMOD_Sound_GetLength, ( FMOD_SOUND* sound, unsigned int* length, FMOD_TIMEUNIT lengthtype ) )
FMOD_FUNCTION( FMOD_Sound_GetDefaults, ( FMOD_SOUND* sound, float* frequency, float* volume, float* pan, int* priority ) ) 
FMOD_FUNCTION( FMOD_Sound_GetMemoryInfo, ( FMOD_SOUND* sound, unsigned int memorybits, unsigned int event_memorybits, unsigned int* memoryused, unsigned int* memoryused_array ) );

FMOD_FUNCTION( FMOD_Geometry_AddPolygon, ( FMOD_GEOMETRY* geometry, float directocclusion, float reverbocclusion, FMOD_BOOL doublesided, int numvertices, const FMOD_VECTOR* vertices, int* polygonindex ) )
FMOD_FUNCTION( FMOD_Geometry_SetPosition, ( FMOD_GEOMETRY* geometry, const FMOD_VECTOR* position ) )
FMOD_FUNCTION( FMOD_Geometry_SetRotation, ( FMOD_GEOMETRY* geometry, const FMOD_VECTOR* forward, const FMOD_VECTOR* up ) )
FMOD_FUNCTION( FMOD_Geometry_SetScale, ( FMOD_GEOMETRY* geometry, const FMOD_VECTOR* scale ) )
FMOD_FUNCTION( FMOD_Geometry_Release, ( FMOD_GEOMETRY* geometry ) )

FMOD_FUNCTION( FMOD_System_Close, (FMOD_SYSTEM *system))
FMOD_FUNCTION( FMOD_System_Create, (FMOD_SYSTEM **system))
FMOD_FUNCTION( FMOD_System_Release, (FMOD_SYSTEM *system))
FMOD_FUNCTION( FMOD_System_CreateSound, (FMOD_SYSTEM *system, const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, FMOD_SOUND **sound))
FMOD_FUNCTION( FMOD_System_CreateGeometry, ( FMOD_SYSTEM* system, int maxpolygons, int maxvertices, FMOD_GEOMETRY** geometry ) )
FMOD_FUNCTION( FMOD_System_SetDriver, (FMOD_SYSTEM *system, int driver))
FMOD_FUNCTION( FMOD_System_GetDriverCaps, (FMOD_SYSTEM *system, int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode))
FMOD_FUNCTION( FMOD_System_GetDriverInfo, (FMOD_SYSTEM *system, int id, char *name, int namelen, FMOD_GUID *GUID))
FMOD_FUNCTION( FMOD_System_GetNumDrivers, (FMOD_SYSTEM *system, int *numdrivers))
FMOD_FUNCTION( FMOD_System_GetVersion, (FMOD_SYSTEM *system, unsigned int *version))
FMOD_FUNCTION( FMOD_System_Init, (FMOD_SYSTEM *system, int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata))
FMOD_FUNCTION( FMOD_System_PlaySound, (FMOD_SYSTEM *system, FMOD_CHANNELINDEX channelid, FMOD_SOUND *sound, FMOD_BOOL paused, FMOD_CHANNEL **channel))
FMOD_FUNCTION( FMOD_System_Set3DListenerAttributes, (FMOD_SYSTEM *system, int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel, const FMOD_VECTOR *forward, const FMOD_VECTOR *up))
FMOD_FUNCTION( FMOD_System_Set3DNumListeners, ( FMOD_SYSTEM* system, int numlisteners ) )
FMOD_FUNCTION( FMOD_System_SetGeometrySettings, ( FMOD_SYSTEM* system, float maxworldsize ) )
FMOD_FUNCTION( FMOD_System_Get3DSettings, (FMOD_SYSTEM* system, float* dopplerFactor, float* distanceFactor, float* rolloffFactor))
FMOD_FUNCTION( FMOD_System_Set3DSettings, (FMOD_SYSTEM *system, float dopplerscale, float distancefactor, float rolloffscale))
FMOD_FUNCTION( FMOD_System_SetDSPBufferSize, (FMOD_SYSTEM *system, unsigned int bufferlength, int numbuffers))
FMOD_FUNCTION( FMOD_System_SetSpeakerMode, (FMOD_SYSTEM *system, FMOD_SPEAKERMODE speakermode))
FMOD_FUNCTION( FMOD_System_SetReverbProperties, ( FMOD_SYSTEM* system, const FMOD_REVERB_PROPERTIES* prop ) )
FMOD_FUNCTION( FMOD_System_Update, (FMOD_SYSTEM *system))
FMOD_FUNCTION( FMOD_Memory_GetStats, (int*, int*))
FMOD_FUNCTION( FMOD_Memory_Initialize,(void *poolmem, int poollen, FMOD_MEMORY_ALLOCCALLBACK useralloc, FMOD_MEMORY_REALLOCCALLBACK userrealloc, FMOD_MEMORY_FREECALLBACK userfree))
