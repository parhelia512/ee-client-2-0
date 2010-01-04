#ifndef __EFX_ANIMCLIP__
#define __EFX_ANIMCLIP__

#include "T3D/gameBase.h"
#include "../EffectWrapper/EffectWrapper.h"
#include "../EffectWrapper/EffectWrapperDesc.h"

class EFX_AnimClip_Data : public GameBaseData
{
	typedef GameBaseData	Parent;
public:
	StringTableEntry				_anim_name;
	EFX_AnimClip_Data();
	void						packData(BitStream*);
	void						unpackData(BitStream*);
	static void			initPersistFields();
	bool						preload(bool server, String &errorStr);
	DECLARE_CONOBJECT(EFX_AnimClip_Data);
};

class EFX_AnimClip_Wrapper : public EffectWrapper
{
	typedef EffectWrapper	Parent;
	EFX_AnimClip_Data	*		mDataBlock;
public:
	EFX_AnimClip_Wrapper();
	virtual bool						ea_SetData(SimDataBlock * pData , ERRORS & error);
	virtual bool						ea_Start(ERRORS & error);
	virtual void						ea_Update(const U32 & dt);
	virtual void						ea_End();
	DECLARE_CONOBJECT(EFX_AnimClip_Wrapper);
};

class EFX_AnimClip_Desc : public EffectWrapperDesc
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