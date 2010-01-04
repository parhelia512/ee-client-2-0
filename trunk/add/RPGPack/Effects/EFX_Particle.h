#ifndef __EFX_PARTICLE__
#define __EFX_PARTICLE__

#include "T3D/gameBase.h"
#include "../EffectWrapper/EffectWrapper.h"
#include "../EffectWrapper/EffectWrapperDesc.h"
#include "T3D/fx/particleEmitter.h"


class EFX_Particle_Data : public ParticleEmitterData
{
	typedef ParticleEmitterData	Parent;
public:
	EFX_Particle_Data();
	void						packData(BitStream*);
	void						unpackData(BitStream*);
	static void			initPersistFields();
	bool						preload(bool server, String &errorStr);
	DECLARE_CONOBJECT(EFX_Particle_Data);
};

class EFX_Particle_Wrapper : public EffectWrapper
{
	typedef EffectWrapper				Parent;
	EFX_Particle_Data	*						mDataBlock;
	SimObjectPtr<ParticleEmitter>	mEmitter;
public:
	EFX_Particle_Wrapper();
	virtual bool						ea_SetData(SimDataBlock * pData , ERRORS & error);
	virtual bool						ea_Start(ERRORS & error);
	virtual void						ea_Update(const U32 & dt);
	virtual void						ea_End();
	DECLARE_CONOBJECT(EFX_Particle_Wrapper);
};

class EFX_Particle_Desc : public EffectWrapperDesc
{
	typedef EffectWrapperDesc Parent;
public:
	virtual bool																canRunOnServer();
	virtual bool																canRunOnClient();
	virtual bool																isMatch( SimDataBlock * pData);
	virtual EffectWrapper *											createEffectWrapper(SimDataBlock * pData);
	virtual EFFECTRUN													getEffectRunsOn();
};

#endif