
#ifndef _GFX_PC_D3D9DEVICE_H_
#define _GFX_PC_D3D9DEVICE_H_

#include "gfx/D3D9/gfxD3D9Device.h"

class PlatformWindow;

class GFXPCD3D9Device : public GFXD3D9Device
{
   typedef GFXD3D9Device Parent;

public:
   // Set to true to force nvperfhud device creation
   static bool mEnableNVPerfHUD;

   GFXPCD3D9Device( LPDIRECT3D9 d3d, U32 index ) : GFXD3D9Device( d3d, index ) {};

   static GFXDevice *createInstance( U32 adapterIndex );

   virtual GFXFormat selectSupportedFormat(GFXTextureProfile *profile,
	   const Vector<GFXFormat> &formats, bool texture, bool mustblend, bool mustfilter);
   
   static void enumerateAdapters( Vector<GFXAdapter*> &adapterList );

   virtual void enumerateVideoModes();

   virtual GFXWindowTarget *allocWindowTarget(PlatformWindow *window);
   virtual GFXTextureTarget *allocRenderToTextureTarget();
   virtual bool beginSceneInternal();

   virtual void init( const GFXVideoMode &mode, PlatformWindow *window = NULL );

   virtual void enterDebugEvent(ColorI color, const char *name);
   virtual void leaveDebugEvent();
   virtual void setDebugMarker(ColorI color, const char *name);

   virtual void setMatrix( GFXMatrixType mtype, const MatrixF &mat );

   virtual void initStates();
   virtual void reset( D3DPRESENT_PARAMETERS &d3dpp );
   virtual D3DPRESENT_PARAMETERS setupPresentParams( const GFXVideoMode &mode, const HWND &hwnd ) const;
protected:   
   static GFXAdapter::CreateDeviceInstanceDelegate mCreateDeviceInstance;

   virtual void _setTextureStageState( U32 stage, U32 state, U32 value );      
   void _validateMultisampleParams(D3DFORMAT format, D3DMULTISAMPLE_TYPE & aatype, DWORD & aalevel) const;
};

#endif