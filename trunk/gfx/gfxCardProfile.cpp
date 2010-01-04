//-----------------------------------------------------------------------------
// Torque Shader Engine 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/volume.h"

#include "gfx/gfxCardProfile.h"
#include "console/console.h"


void GFXCardProfiler::loadProfileScript(const char* aScriptName)
{
   const String   profilePath = Con::getVariable( "$Pref::Video::ProfilePath" );
   String scriptName = !profilePath.isEmpty() ? profilePath.c_str() : "profile";
   scriptName += "/";
   scriptName += aScriptName;
   
   void  *data = NULL;
   U32   dataSize = 0;

   Torque::FS::ReadFile( scriptName.c_str(), data, dataSize, true );

   if(data == NULL)
   {
      Con::warnf("      - No card profile %s exists", scriptName.c_str());
      return;
   }

   const char  *script = static_cast<const char *>(data);

   Con::printf("      - Loaded card profile %s", scriptName.c_str());

   Con::executef("eval", script);
   delete[] script;
}

void GFXCardProfiler::loadProfileScripts(const String& render, const String& vendor, const String& card, const String& version)
{
   String script = render + ".cs";
   loadProfileScript(script);

   script = render + "." + vendor + ".cs";
   loadProfileScript(script);

   script = render + "." + vendor + "." + card + ".cs";
   loadProfileScript(script);

   script = render + "." + vendor + "." + card + "." + version + ".cs";
   loadProfileScript(script);
}

GFXCardProfiler::GFXCardProfiler() : mVideoMemory( 0 )
{
}

GFXCardProfiler::~GFXCardProfiler()
{
   mCapDictionary.clear();
}

String GFXCardProfiler::strippedString(const char *string)
{
   String res = "";

   // And fill it with the stripped string...
   const char *a=string;
   while(*a)
   {
      if(isalnum(*a))
      {
         res += *a;
      }
      a++;
   }

   return res;
}

void GFXCardProfiler::init()
{
   // Spew a bit...
   Con::printf("Initializing GFXCardProfiler (%s)", getRendererString().c_str());
   Con::printf("   o Chipset : '%s'", getChipString().c_str());
   Con::printf("   o Card    : '%s'", getCardString().c_str());
   Con::printf("   o Version : '%s'", getVersionString().c_str());

   // Do card-specific setup...
   Con::printf("   - Scanning card capabilities...");

   setupCardCapabilities();

   // And finally, load stuff up...
   String render  = strippedString(getRendererString());
   String chipset  = strippedString(getChipString());
   String card    = strippedString(getCardString());
   String version = strippedString(getVersionString());

   Con::printf("   - Loading card profiles...");
   loadProfileScripts(render, chipset, card, version);
}

U32 GFXCardProfiler::queryProfile(const String &cap)
{
   U32 res;
   if( _queryCardCap( cap, res ) )
      return res;

   if(mCapDictionary.contains(cap))
      return mCapDictionary[cap];

   Con::errorf( "GFXCardProfiler (%s) - Unknown capability '%s'.", getRendererString().c_str(), cap.c_str() );
   return 0;
}

U32 GFXCardProfiler::queryProfile(const String &cap, U32 defaultValue)
{
   U32 res;
   if( _queryCardCap( cap, res ) )
      return res;

   if( mCapDictionary.contains( cap ) )
      return mCapDictionary[cap];
   else
      return defaultValue;
}

void GFXCardProfiler::setCapability(const String &cap, U32 value)
{
   // Check for dups.
   if( mCapDictionary.contains( cap ) )
   {
      Con::warnf( "GFXCardProfiler (%s) - Setting capability '%s' multiple times.", getRendererString().c_str(), cap.c_str() );
      mCapDictionary[cap] = value;
      return;
   }

   // Insert value as necessary.
   Con::printf( "GFXCardProfiler (%s) - Setting capability '%s' to %d.", getRendererString().c_str(), cap.c_str(), value );
   mCapDictionary.insert( cap, value );
}

bool GFXCardProfiler::checkFormat( const GFXFormat fmt, const GFXTextureProfile *profile, bool &inOutAutogenMips )
{
   return _queryFormat( fmt, profile, inOutAutogenMips );
}

ConsoleMethodGroupBegin(GFXCardProfiler, Core, "Functions relating to the card profiler functionality.");

ConsoleStaticMethod( GFXCardProfiler, getVersion, const char*, 1, 1, "() - Returns the driver version (59.72).")
{
	return GFX->getCardProfiler()->getVersionString();
}

ConsoleStaticMethod( GFXCardProfiler, getCard, const char*, 1, 1, "() - Returns the card name (GeforceFX 5950 Ultra).")
{
	return GFX->getCardProfiler()->getCardString();
}

ConsoleStaticMethod( GFXCardProfiler, getVendor, const char*, 1, 1, "() - Returns the vendor name (nVidia, ATI).")
{
   // TODO: Fix all of this vendor crap, it's not consistent
	return GFX->getCardProfiler()->getChipString();
}

ConsoleStaticMethod( GFXCardProfiler, getRenderer, const char*, 1, 1, "() - Returns the renderer name (D3D9, for instance).")
{
	return GFX->getCardProfiler()->getRendererString();
}

ConsoleStaticMethod( GFXCardProfiler, setCapability, void, 3, 3, "setCapability(name, true/false) - Set a specific card capability.")
{
	TORQUE_UNUSED(argc);
   
	bool val = dAtob(argv[2]);
	GFX->getCardProfiler()->setCapability(argv[1], val);
	return;
}

ConsoleMethodGroupEnd(GFXCardProfiler, Core);
