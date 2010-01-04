#ifndef __EFX_DECAL__
#define __EFX_DECAL__

#include "T3D/gameBase.h"
#include "../EffectWrapper/EffectWrapper.h"
#include "../EffectWrapper/EffectWrapperDesc.h"
#include "T3D/decal/decalData.h"

class DecalInstance;

class EFX_Decal_Data : public DecalData
{
	typedef DecalData	Parent;
public:
	F32						mSpinSpeed;						//旋转速度，弧度每秒
	EFX_Decal_Data();
	void						packData(BitStream*);
	void						unpackData(BitStream*);
	static void			initPersistFields();
	bool						preload(bool server, String &errorStr);
	DECLARE_CONOBJECT(EFX_Decal_Data);
};

class EFX_Decal_Wrapper : public EffectWrapper
{
	typedef EffectWrapper	Parent;
	EFX_Decal_Data	*			mDataBlock;
	DecalInstance *				mDecalInstance;
public:
	EFX_Decal_Wrapper();
	virtual bool						ea_SetData(SimDataBlock * pData , ERRORS & error);
	virtual bool						ea_Start(ERRORS & error);
	virtual void						ea_Update(const U32 & dt);
	virtual void						ea_End();
	DECLARE_CONOBJECT(EFX_Decal_Wrapper);
};

class EFX_Decal_Desc : public EffectWrapperDesc
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