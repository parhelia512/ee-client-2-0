//-----------------------------------------------------------------------------
// Torque 3D - ImageList Control
// Copyright (C) 2005 Justin DuJardin.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/consoleTypes.h"
#include "console/console.h"
#include "gfx/gfxDevice.h"
#include "gui/editor/guiImageList.h"

IMPLEMENT_CONOBJECT(GuiImageList);

GuiImageList::GuiImageList()
{
  VECTOR_SET_ASSOCIATION(mTextures);
  mTextures.clear();
  mUniqueId = 0;
}

U32 GuiImageList::Insert( const char* texturePath, GFXTextureProfile *Type )
{
  TextureEntry *t = new TextureEntry;

  if ( ! t ) return -1;

  t->TexturePath = StringTable->insert(texturePath);
  if ( *t->TexturePath ) 
  {
    t->Handle = GFXTexHandle(t->TexturePath, Type, avar("%s() - t->Handle (line %d)", __FUNCTION__, __LINE__));

    if ( t->Handle )
    {
      t->id = ++mUniqueId;

      mTextures.push_back( t );

      return t->id;

    }
  }

  // Free Texture Entry.
  delete t;

  // Return Failure.
  return -1;

}

bool GuiImageList::Clear()
{
  while ( mTextures.size() )
    FreeTextureEntry( mTextures[0] );
  mTextures.clear();

  mUniqueId = 0;

  return true;
}

bool GuiImageList::FreeTextureEntry( U32 Index )
{
  U32 Id = IndexFromId( Index );
  if ( Id != -1 )
     return FreeTextureEntry( mTextures[ Id ] );
  else
    return false;
}

bool GuiImageList::FreeTextureEntry( PTextureEntry Entry )
{
  if ( ! Entry )
    return false;

  U32 id = IndexFromId( Entry->id );

  delete Entry;

  mTextures.erase ( id );

  return true;
}

U32 GuiImageList::IndexFromId ( U32 Id )
{
  if ( !mTextures.size() ) return -1;
  Vector<PTextureEntry>::iterator i = mTextures.begin();
  U32 j = 0;
  for ( ; i != mTextures.end(); i++ )
  {
    if ( i )
    {
    if ( (*i)->id == Id )
      return j;
    j++;
    }
  }

  return -1;
}

U32 GuiImageList::IndexFromPath ( const char* Path )
{
  if ( !mTextures.size() ) return -1;
  Vector<PTextureEntry>::iterator i = mTextures.begin();
  for ( ; i != mTextures.end(); i++ )
  {
    if ( dStricmp( Path, (*i)->TexturePath ) == 0 )
      return (*i)->id;
  }

  return -1;
}

void GuiImageList::initPersistFields()
{
  Parent::initPersistFields();
}

ConsoleMethod(GuiImageList, getImage,const char *, 3, 3, "(int index) Get a path to the texture at the specified index")
{
  return object->GetTexturePath(dAtoi(argv[2]));
}


ConsoleMethod(GuiImageList, clear,bool, 2, 2, "clears the imagelist")
{
  return object->Clear();
}

ConsoleMethod(GuiImageList, count,S32, 2, 2, "gets the number of images in the list")
{
  return object->Count();
}


ConsoleMethod(GuiImageList, remove, bool, 3,3, "(image index) removes an image from the list by index")
{
  return object->FreeTextureEntry( dAtoi(argv[2] ) );
}

ConsoleMethod(GuiImageList, getIndex, S32, 3,3, "(image path) retrieves the imageindex of a specified texture in the list")
{
  return object->IndexFromPath( argv[2] );
}


ConsoleMethod(GuiImageList, insert, S32, 3, 3, "(image path) insert an image into imagelist- returns the image index or -1 for failure")
{
  return object->Insert( argv[2] );
}

GFXTexHandle GuiImageList::GetTextureHandle( U32 Index )
{
  U32 ItemIndex = IndexFromId(Index);
  if ( ItemIndex != -1 )
    return mTextures[ItemIndex]->Handle;
  else
    return NULL;

}
GFXTexHandle GuiImageList::GetTextureHandle( const char* TexturePath )
{
  Vector<PTextureEntry>::iterator i = mTextures.begin();
  for ( ; i != mTextures.end(); i++ )
  {
    if ( dStricmp( TexturePath, (*i)->TexturePath ) == 0 )
      return (*i)->Handle;
  }

  return NULL;
}


const char *GuiImageList::GetTexturePath( U32 Index )
{
  U32 ItemIndex = IndexFromId(Index);
  if ( ItemIndex != -1 )
    return mTextures[ItemIndex]->TexturePath;
  else
    return "";
}
