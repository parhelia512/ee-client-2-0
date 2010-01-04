//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/types.h"
#include "console/consoleTypes.h"
#include "console/console.h"
#include "gui/core/guiTypes.h"
#include "gfx/gFont.h"
#include "gfx/bitmap/gBitmap.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"
#include "sfx/sfxProfile.h"

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- //
IMPLEMENT_CONOBJECT(GuiCursor);


GFX_ImplementTextureProfile(GFXGuiCursorProfile,
                            GFXTextureProfile::DiffuseMap, 
                            GFXTextureProfile::PreserveSize |
                            GFXTextureProfile::Static, 
                            GFXTextureProfile::None);
GFX_ImplementTextureProfile(GFXDefaultGUIProfile,
                            GFXTextureProfile::DiffuseMap, 
                            GFXTextureProfile::PreserveSize |
                            GFXTextureProfile::Static |
                            GFXTextureProfile::NoPadding, 
                            GFXTextureProfile::None);


GuiCursor::GuiCursor()
{
   mHotSpot.set(0,0);
   mRenderOffset.set(0.0f,0.0f);
   mExtent.set(1,1);
   mTextureObject = NULL;
   mTexturePicked = NULL;
   mStatu = Cursor_Normal;
}

GuiCursor::~GuiCursor()
{
}

void GuiCursor::initPersistFields()
{
   addField("hotSpot",     TypePoint2I,   Offset(mHotSpot, GuiCursor));
   addField("renderOffset",TypePoint2F,   Offset(mRenderOffset, GuiCursor));
   addField("bitmapName",  TypeFilename,  Offset(mBitmapName, GuiCursor));
   Parent::initPersistFields();
}

bool GuiCursor::onAdd()
{
   if(!Parent::onAdd())
      return false;

   Sim::getGuiDataGroup()->addObject(this);

   return true;
}

void GuiCursor::onRemove()
{
   Parent::onRemove();
}
void GuiCursor::setPickedBmp(StringTableEntry path)
{
	if (mTexturePicked.set(path,&GFXDefaultGUIProfile,"gui cursor"))
		mStatu = Cursor_Picked;
	else
		mStatu = Cursor_Normal;
}
void GuiCursor::clearPickedBmp()
{
	mStatu = Cursor_Normal;
}
void GuiCursor::render(const Point2I &pos)
{
   if (!mTextureObject && mBitmapName && mBitmapName[0])
   {
      mTextureObject.set( mBitmapName, &GFXGuiCursorProfile, avar("%s() - mTextureObject (line %d)", __FUNCTION__, __LINE__));
      if(!mTextureObject)
         return;
      mExtent.set(mTextureObject->getWidth(), mTextureObject->getHeight());
   }

   // Render the cursor centered according to dimensions of texture
   S32 texWidth = mTextureObject.getWidth();
   S32 texHeight = mTextureObject.getHeight();

   Point2I renderPos = pos;
   /*
   renderPos.x -= (S32)( texWidth  * mRenderOffset.x );
   renderPos.y -= (S32)( texHeight * mRenderOffset.y );

   GFX->getDrawUtil()->clearBitmapModulation();
   GFX->getDrawUtil()->drawBitmap(mTextureObject, renderPos);
   */
   GFX->getDrawUtil()->clearBitmapModulation();
   switch (mStatu)
   {
   case Cursor_Picked:
	   {
		   Point2I bg = renderPos;
		   bg.x -= (S32)( (F32)(mTexturePicked.getWidth()) / 2.f + 0.5f );
		   bg.y -= (S32)( (F32)(mTexturePicked.getHeight()) / 2.f + 0.5f );
		   GFX->getDrawUtil()->drawBitmap(mTexturePicked, bg);			
		   break;
	   }
   case Cursor_Normal:
	   //renderPos.x -= (S32)( texWidth  * mRenderOffset.x );
	   //renderPos.y -= (S32)( texHeight * mRenderOffset.y );
	   //GFX->getDrawUtil()->drawBitmap(mTextureObject, renderPos);			
	   break;
   }
}

//------------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiControlProfile);

static const EnumTable::Enums alignEnums[] =
{
   { GuiControlProfile::LeftJustify,          "left"      },
   { GuiControlProfile::CenterJustify,        "center"    },
   { GuiControlProfile::RightJustify,         "right"     }
};
static const EnumTable gAlignTable(3, &alignEnums[0]);

static const EnumTable::Enums charsetEnums[]=
{
    { TGE_ANSI_CHARSET,         "ANSI" },
    { TGE_SYMBOL_CHARSET,       "SYMBOL" },
    { TGE_SHIFTJIS_CHARSET,     "SHIFTJIS" },
    { TGE_HANGEUL_CHARSET,      "HANGEUL" },
    { TGE_HANGUL_CHARSET,       "HANGUL" },
    { TGE_GB2312_CHARSET,       "GB2312" },
    { TGE_CHINESEBIG5_CHARSET,  "CHINESEBIG5" },
    { TGE_OEM_CHARSET,          "OEM" },
    { TGE_JOHAB_CHARSET,        "JOHAB" },
    { TGE_HEBREW_CHARSET,       "HEBREW" },
    { TGE_ARABIC_CHARSET,       "ARABIC" },
    { TGE_GREEK_CHARSET,        "GREEK" },
    { TGE_TURKISH_CHARSET,      "TURKISH" },
    { TGE_VIETNAMESE_CHARSET,   "VIETNAMESE" },
    { TGE_THAI_CHARSET,         "THAI" },
    { TGE_EASTEUROPE_CHARSET,   "EASTEUROPE" },
    { TGE_RUSSIAN_CHARSET,      "RUSSIAN" },
    { TGE_MAC_CHARSET,          "MAC" },
    { TGE_BALTIC_CHARSET,       "BALTIC" },
};

#define NUMCHARSETENUMS     (sizeof(charsetEnums) / sizeof(EnumTable::Enums))

static const EnumTable gCharsetTable(NUMCHARSETENUMS, &charsetEnums[0]);

StringTableEntry GuiControlProfile::sFontCacheDirectory = "";

void GuiControlProfile::setBitmapHandle(GFXTexHandle handle)
{
   mTextureObject = handle;

   mBitmapName = StringTable->insert("texhandle");
}

bool GuiControlProfile::protectedSetBitmap( void *obj, const char *data)
{
   GuiControlProfile *profile = static_cast<GuiControlProfile*>(obj);
   
   profile->mBitmapName = StringTable->insert(data);

   if ( !profile->isProperlyAdded() )
      return false;

   profile->mBitmapArrayRects.clear();
   profile->mTextureObject = NULL;

   //verify the bitmap
   if (profile->mBitmapName && profile->mBitmapName[0] && dStricmp(profile->mBitmapName, "texhandle") != 0 &&
      !profile->mTextureObject.set( profile->mBitmapName, &GFXDefaultPersistentProfile, avar("%s() - mTextureObject (line %d)", __FUNCTION__, __LINE__) ))
      Con::errorf("Failed to load profile bitmap (%s)",profile->mBitmapName);

   // If we've got a special border, make sure it's usable.
   //if( profile->mBorder == -1 || profile->mBorder == -2 )
   profile->constructBitmapArray();

   return false;
}

GuiControlProfile::GuiControlProfile(void) :
   mFillColor(255,0,255,255),
   mFillColorHL(255,0,255,255),
   mFillColorNA(255,0,255,255),
   mFillColorSEL(255,0,255,255),
   mBorderColor(255,0,255,255),
   mBorderColorHL(255,0,255,255),
   mBorderColorNA(255,0,255,255),
   mBevelColorHL(255,0,255,255),
   mBevelColorLL(255,0,255,255),
   // initialize these references to locations in the font colors array
   // the array is initialized below.
   mFontColor(mFontColors[BaseColor]),
   mFontColorHL(mFontColors[ColorHL]),
   mFontColorNA(mFontColors[ColorNA]),
   mFontColorSEL(mFontColors[ColorSEL]),
   mCursorColor(255,0,255,255),
	mYPositionOffset(0),
   mTextOffset(0,0),
   mBitmapArrayRects(0)
{
   // profiles are reference counted
   mRefCount = 0;
   
   // event focus behavior
   mTabable = false;
   mCanKeyFocus = false;
   mModal = false;

   // fill and border   
   mOpaque = false;
   mBorder = 1;
   mBorderThickness = 1;

   // font members
   mFontType = "Arial";
   mFontSize = 10;

   for(U32 i = 0; i < 10; i++)
      mFontColors[i].set(255,0,255,255);

   mFontCharset = TGE_ANSI_CHARSET;

   // sizing and alignment
   mAlignment = LeftJustify;
   mAutoSizeWidth = false;
   mAutoSizeHeight= false;
   mReturnTab     = false;
   mNumbersOnly   = false;
   mMouseOverSelected = false;
   
   // bitmap members
   mBitmapName = NULL;
   mUseBitmapArray = false;
   mTextureObject = NULL; // initialized in incRefCount()

   // sound members
   mSoundButtonDown = NULL;
   mSoundButtonOver = NULL;

   mChildrenProfileName = NULL;
   mChildrenProfile = NULL;

   // inherit/copy values from GuiDefaultProfile
   GuiControlProfile *def = dynamic_cast<GuiControlProfile*>(Sim::findObject("GuiDefaultProfile"));
   if (def)
   {
      mTabable       = def->mTabable;
      mCanKeyFocus   = def->mCanKeyFocus;

      mOpaque        = def->mOpaque;
      mFillColor     = def->mFillColor;
      mFillColorHL   = def->mFillColorHL;
      mFillColorNA   = def->mFillColorNA;
      mFillColorSEL  = def->mFillColorSEL;

      mBorder        = def->mBorder;
      mBorderThickness = def->mBorderThickness;
      mBorderColor   = def->mBorderColor;
      mBorderColorHL = def->mBorderColorHL;
      mBorderColorNA = def->mBorderColorNA;

      mBevelColorHL = def->mBevelColorHL;
      mBevelColorLL = def->mBevelColorLL;

      // default font
      mFontType      = def->mFontType;
      mFontSize      = def->mFontSize;
      mFontCharset   = def->mFontCharset;

      for(U32 i = 0; i < 10; i++)
         mFontColors[i] = def->mFontColors[i];
		
		// default offset 
		mYPositionOffset = def->mYPositionOffset;

      // default bitmap
      mBitmapName     = def->mBitmapName;
      mUseBitmapArray = def->mUseBitmapArray;
      mTextOffset     = def->mTextOffset;

      // default sound
      mSoundButtonDown = def->mSoundButtonDown;
      mSoundButtonOver = def->mSoundButtonOver;

      //used by GuiTextCtrl
      mModal         = def->mModal;
      mAlignment     = def->mAlignment;
      mAutoSizeWidth = def->mAutoSizeWidth;
      mAutoSizeHeight= def->mAutoSizeHeight;
      mReturnTab     = def->mReturnTab;
      mNumbersOnly   = def->mNumbersOnly;
      mCursorColor   = def->mCursorColor;
      mChildrenProfileName = def->mChildrenProfileName;
      setChildrenProfile(def->mChildrenProfile);
   }
}

GuiControlProfile::~GuiControlProfile()
{
}


void GuiControlProfile::initPersistFields()
{
   addField("tab",           TypeBool,       Offset(mTabable, GuiControlProfile));
   addField("canKeyFocus",   TypeBool,       Offset(mCanKeyFocus, GuiControlProfile));
   addField("mouseOverSelected", TypeBool,   Offset(mMouseOverSelected, GuiControlProfile));

   addField("modal",         TypeBool,       Offset(mModal, GuiControlProfile));
   addField("opaque",        TypeBool,       Offset(mOpaque, GuiControlProfile));
   addField("fillColor",     TypeColorI,     Offset(mFillColor, GuiControlProfile));
   addField("fillColorHL",   TypeColorI,     Offset(mFillColorHL, GuiControlProfile));
   addField("fillColorNA",   TypeColorI,     Offset(mFillColorNA, GuiControlProfile));
   addField("fillColorSEL",  TypeColorI,     Offset(mFillColorSEL, GuiControlProfile));
   addField("border",        TypeS32,        Offset(mBorder, GuiControlProfile));
   addField("borderThickness",TypeS32,       Offset(mBorderThickness, GuiControlProfile));
   addField("borderColor",   TypeColorI,     Offset(mBorderColor, GuiControlProfile));
   addField("borderColorHL", TypeColorI,     Offset(mBorderColorHL, GuiControlProfile));
   addField("borderColorNA", TypeColorI,     Offset(mBorderColorNA, GuiControlProfile));

   addField("bevelColorHL", TypeColorI,     Offset(mBevelColorHL, GuiControlProfile));
   addField("bevelColorLL", TypeColorI,     Offset(mBevelColorLL, GuiControlProfile));

   addField("fontType",             TypeString,     Offset(mFontType, GuiControlProfile));
   addField("fontSize",             TypeS32,        Offset(mFontSize, GuiControlProfile));
   addField("fontCharset",          TypeEnum,       Offset(mFontCharset, GuiControlProfile), 1, &gCharsetTable, "");
   addField("fontColors",           TypeColorI,     Offset(mFontColors, GuiControlProfile), 10);
   addField("fontColor",            TypeColorI,     Offset(mFontColors[BaseColor], GuiControlProfile));
   addField("fontColorHL",          TypeColorI,     Offset(mFontColors[ColorHL], GuiControlProfile));
   addField("fontColorNA",          TypeColorI,     Offset(mFontColors[ColorNA], GuiControlProfile));
   addField("fontColorSEL",         TypeColorI,     Offset(mFontColors[ColorSEL], GuiControlProfile));
   addField("fontColorLink",        TypeColorI,     Offset(mFontColors[ColorUser0], GuiControlProfile));
   addField("fontColorLinkHL",      TypeColorI,     Offset(mFontColors[ColorUser1], GuiControlProfile));

	addField("yPositionOffset",      TypeS32,        Offset(mYPositionOffset, GuiControlProfile));
	
   addField("justify",       TypeEnum,       Offset(mAlignment, GuiControlProfile), 1, &gAlignTable);
   addField("textOffset",    TypePoint2I,    Offset(mTextOffset, GuiControlProfile));
   addField("autoSizeWidth", TypeBool,       Offset(mAutoSizeWidth, GuiControlProfile));
   addField("autoSizeHeight",TypeBool,       Offset(mAutoSizeHeight, GuiControlProfile));
   addField("returnTab",     TypeBool,       Offset(mReturnTab, GuiControlProfile));
   addField("numbersOnly",   TypeBool,       Offset(mNumbersOnly, GuiControlProfile));
   addField("cursorColor",   TypeColorI,     Offset(mCursorColor, GuiControlProfile));

   addProtectedField( "bitmap", TypeFilename,  Offset(mBitmapName, GuiControlProfile), &GuiControlProfile::protectedSetBitmap, &defaultProtectedGetFn, "" );
   addField("hasBitmapArray", TypeBool,      Offset(mUseBitmapArray, GuiControlProfile));

   addField("soundButtonDown", TypeSFXProfilePtr,  Offset(mSoundButtonDown, GuiControlProfile));
   addField("soundButtonOver", TypeSFXProfilePtr,  Offset(mSoundButtonOver, GuiControlProfile));
   addField("profileForChildren", TypeString,      Offset(mChildrenProfileName, GuiControlProfile));

   Parent::initPersistFields();
}

bool GuiControlProfile::onAdd()
{
   if(!Parent::onAdd())
      return false;

   Sim::getGuiDataGroup()->addObject(this);
   
   // Make sure we have an up-to-date children profile
   getChildrenProfile();

   return true;
}

void GuiControlProfile::onStaticModified(const char* slotName, const char* newValue)
{
   if ( mRefCount > 0 )
   {
      if ( !dStricmp(slotName, "fontType") || 
           !dStricmp(slotName, "fontCharset") ||
           !dStricmp(slotName, "fontSize" ) )
      {
         // Reload the font
         mFont = GFont::create(mFontType, mFontSize, sFontCacheDirectory, mFontCharset);
         if ( mFont == NULL )
            Con::errorf("Failed to load/create profile font (%s/%d)", mFontType, mFontSize);
      }
   }
}

void GuiControlProfile::onDeleteNotify(SimObject *object)
{
   if (object == mChildrenProfile)
      mChildrenProfile = NULL;
}

GuiControlProfile* GuiControlProfile::getChildrenProfile()
{
   // We can early out if we still have a valid profile
   if (mChildrenProfile)
      return mChildrenProfile;

   // Attempt to find the profile specified
   if (mChildrenProfileName)
   {
      GuiControlProfile *profile = dynamic_cast<GuiControlProfile*>(Sim::findObject( mChildrenProfileName ));

      if( profile )
         setChildrenProfile(profile);
   }

   return mChildrenProfile;
}

void GuiControlProfile::setChildrenProfile(GuiControlProfile *prof)
{
   if(prof == mChildrenProfile)
      return;

   // Clear the delete notification we previously set up
   if (mChildrenProfile)
      clearNotify(mChildrenProfile);

   mChildrenProfile = prof;

   // Make sure that the new profile will notify us when it is deleted
   if (mChildrenProfile)
      deleteNotify(mChildrenProfile);
}

RectI GuiControlProfile::getBitmapArrayRect(U32 i)
{
   if(!mBitmapArrayRects.size())
      constructBitmapArray();
   
   if( i >= mBitmapArrayRects.size())
      return RectI(0,0,0,0);

   return mBitmapArrayRects[i];
}

S32 GuiControlProfile::constructBitmapArray()
{
   if(mBitmapArrayRects.size())
      return mBitmapArrayRects.size();

   if( mTextureObject.isNull() )
   {   
      if ( !mBitmapName || !mBitmapName[0] || !mTextureObject.set( mBitmapName, &GFXDefaultPersistentProfile, avar("%s() - mTextureObject (line %d)", __FUNCTION__, __LINE__) ))
         return 0;
   }

   GBitmap *bmp = mTextureObject->getBitmap();

   //get the separator color
   ColorI sepColor;
   if ( !bmp || !bmp->getColor( 0, 0, sepColor ) )
	{
      Con::errorf("Failed to create bitmap array from %s for profile %s - couldn't ascertain seperator color!", mBitmapName, getName());
      AssertFatal( false, avar("Failed to create bitmap array from %s for profile %s - couldn't ascertain seperator color!", mBitmapName, getName()));
      return 0;
	}

   //now loop through all the scroll pieces, and find the bounding rectangle for each piece in each state
   S32 curY = 0;

   // ascertain the height of this row...
   ColorI color;
   mBitmapArrayRects.clear();
   while(curY < bmp->getHeight())
   {
      // skip any sep colors
      bmp->getColor( 0, curY, color);
      if(color == sepColor)
      {
         curY++;
         continue;
      }
      // ok, process left to right, grabbing bitmaps as we go...
      S32 curX = 0;
      while(curX < bmp->getWidth())
      {
         bmp->getColor(curX, curY, color);
         if(color == sepColor)
         {
            curX++;
            continue;
         }
         S32 startX = curX;
         while(curX < bmp->getWidth())
         {
            bmp->getColor(curX, curY, color);
            if(color == sepColor)
               break;
            curX++;
         }
         S32 stepY = curY;
         while(stepY < bmp->getHeight())
         {
            bmp->getColor(startX, stepY, color);
            if(color == sepColor)
               break;
            stepY++;
         }
         mBitmapArrayRects.push_back(RectI(startX, curY, curX - startX, stepY - curY));
      }
      // ok, now skip to the next separation color on column 0
      while(curY < bmp->getHeight())
      {
         bmp->getColor(0, curY, color);
         if(color == sepColor)
            break;
         curY++;
      }
   }
   return mBitmapArrayRects.size();
}

void GuiControlProfile::incRefCount()
{
   if(!mRefCount++)
   {
      sFontCacheDirectory = Con::getVariable("$GUI::fontCacheDirectory");

      //verify the font
      mFont = GFont::create(mFontType, mFontSize, sFontCacheDirectory, mFontCharset);
      if (mFont == NULL)
         Con::errorf("Failed to load/create profile font (%s/%d)", mFontType, mFontSize);

      //verify the bitmap
      if (mBitmapName && mBitmapName[0] && dStricmp(mBitmapName, "texhandle") != 0 &&
         !mTextureObject.set( mBitmapName, &GFXDefaultPersistentProfile, avar("%s() - mTextureObject (line %d)", __FUNCTION__, __LINE__) ))
         Con::errorf("Failed to load profile bitmap (%s)",mBitmapName);

      // If we've got a special border, make sure it's usable.
      if( mBorder == -1 || mBorder == -2 )
         constructBitmapArray();

   }

   // Quick check to make sure our children profile is up-to-date
   getChildrenProfile();
}

void GuiControlProfile::decRefCount()
{
   AssertFatal(mRefCount, "GuiControlProfile::decRefCount: zero ref count");
   if(!mRefCount)
      return;

   if(!--mRefCount)
   {
      if (!mBitmapName || !mBitmapName[0] ||
          (mBitmapName && mBitmapName[0] && dStricmp(mBitmapName, "texhandle") != 0))
         mTextureObject = NULL;
   }
}

ConsoleMethod( GuiControlProfile, getStringWidth, S32, 3, 3, "( pString )" )
{
    return object->mFont->getStrNWidth( argv[2], dStrlen( argv[2] ) );
}

ConsoleType( GuiProfile, TypeGuiProfile, GuiControlProfile* )

ConsoleSetType( TypeGuiProfile )
{
   // Reference counting is now handled by the GuiControl.
   // In fact this method should never be called because GuiControl uses
   // a protected field set method.   

   /*
   GuiControlProfile *profile = NULL;
   if(argc == 1)
      Sim::findObject(argv[0], profile);

   AssertWarn(profile != NULL, avar("GuiControlProfile: requested gui profile (%s) does not exist.", argv[0]));
   if(!profile)
      profile = dynamic_cast<GuiControlProfile*>(Sim::findObject("GuiDefaultProfile"));

   AssertFatal(profile != NULL, avar("GuiControlProfile: unable to find specified profile (%s) and GuiDefaultProfile does not exist!", argv[0]));

   GuiControlProfile **obj = (GuiControlProfile **)dptr;
   if((*obj) == profile)
      return;

   if(*obj)
      (*obj)->decRefCount();

   *obj = profile;
   (*obj)->incRefCount();
   */
}

ConsoleGetType( TypeGuiProfile )
{
   static char returnBuffer[256];

   GuiControlProfile **obj = (GuiControlProfile**)dptr;
   dSprintf(returnBuffer, sizeof(returnBuffer), "%s", *obj ? (*obj)->getName() ? (*obj)->getName() : (*obj)->getIdString() : "");
   return returnBuffer;
}

//-----------------------------------------------------------------------------
// TypeRectSpacingI
//-----------------------------------------------------------------------------
ConsoleType( RectSpacingI, TypeRectSpacingI, RectSpacingI )

ConsoleGetType( TypeRectSpacingI )
{
   RectSpacingI *rect = (RectSpacingI *) dptr;
   char* returnBuffer = Con::getReturnBuffer(256);
   dSprintf(returnBuffer, 256, "%d %d %d %d", rect->top, rect->bottom,
      rect->left, rect->right);
   return returnBuffer;
}

ConsoleSetType( TypeRectSpacingI )
{
   if(argc == 1)
      dSscanf(argv[0], "%d %d %d %d", &((RectSpacingI *) dptr)->top, &((RectSpacingI *) dptr)->bottom,
      &((RectSpacingI *) dptr)->left, &((RectSpacingI *) dptr)->right);
   else if(argc == 4)
      *((RectSpacingI *) dptr) = RectSpacingI(dAtoi(argv[0]), dAtoi(argv[1]), dAtoi(argv[2]), dAtoi(argv[3]));
   else
      Con::printf("RectSpacingI must be set as { t, b, l, r } or \"t b l r\"");
}
