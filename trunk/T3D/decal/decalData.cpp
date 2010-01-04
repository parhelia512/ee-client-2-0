
#include "platform/platform.h"
#include "T3D/decal/decalData.h"

#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "math/mathIO.h"
#include "materials/materialManager.h"
#include "materials/baseMatInstance.h"
#include "T3D/objectTypes.h"

GFXImplementVertexFormat( DecalVertex )
{
   addElement( GFXSemantic::POSITION, GFXDeclType_Float3 );
   addElement( GFXSemantic::NORMAL, GFXDeclType_Float3 );
   addElement( GFXSemantic::TANGENT, GFXDeclType_Float3 );   
   addElement( GFXSemantic::COLOR, GFXDeclType_Color );
   addElement( GFXSemantic::TEXCOORD, GFXDeclType_Float2, 0 );   
}

IMPLEMENT_CO_DATABLOCK_V1( DecalData );
IMPLEMENT_CONSOLETYPE( DecalData )
IMPLEMENT_GETDATATYPE( DecalData )
IMPLEMENT_SETDATATYPE( DecalData )

//-------------------------------------------------------------------------
// DecalData
//-------------------------------------------------------------------------

DecalData::DecalData()
{
   size = 5;
   materialName = "";

   lifeSpan = 5000;
   fadeTime = 1000;

	frame = 0;
	randomize = false;
	texRows = 1;
	texCols = 1;

   startPixRadius = 2.0f;
   endPixRadius = 1.0f;

   material = NULL;
   matInst = NULL;

   renderPriority = 10;
   clippingMasks = STATIC_COLLISION_MASK;

	texCoordCount = 1;

   for ( S32 i = 0; i < 16; i++ )
   {
      texRect[i].point.set( 0.0f, 0.0f );
      texRect[i].extent.set( 1.0f, 1.0f );
   }
}

DecalData::~DecalData()
{
   SAFE_DELETE( matInst );
}

bool DecalData::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   if (size < 0.0) {
      Con::warnf("DecalData::onAdd: size < 0");
      size = 0;
   }   
   
   getSet()->addObject( this );

	if( texRows > 1 || texCols > 1 )
		reloadRects();

   return true;
}

void DecalData::onRemove()
{
   Parent::onRemove();
}

void DecalData::initPersistFields()
{
   Parent::initPersistFields();

	addGroup("AdvCoordManipulation");
	addField( "textureCoordCount", TypeS32, Offset( texCoordCount, DecalData ) );
   addField( "textureCoords", TypeRectF,  Offset( texRect, DecalData ), MAX_TEXCOORD_COUNT, NULL, 
      "A RectF in uv space - eg ( topleft.x topleft.y extent.x extent.y )");
	endGroup("AdvCoordManipulation");

   addField( "size", TypeF32, Offset( size, DecalData ) );
   addField( "material", TypeMaterialName, Offset( materialName, DecalData ) );
   addField( "lifeSpan", TypeS32, Offset( lifeSpan, DecalData ) );
   addField( "fadeTime", TypeS32, Offset( fadeTime, DecalData ) );

	addField( "frame", TypeS32, Offset( frame, DecalData ) );
	addField( "randomize", TypeBool, Offset( randomize, DecalData ) );
	addField( "texRows", TypeS32, Offset( texRows, DecalData ) );
   addField( "texCols", TypeS32, Offset( texCols, DecalData ) );

   addField( "screenStartRadius", TypeF32, Offset( startPixRadius, DecalData ) );
   addField( "screenEndRadius", TypeF32, Offset( endPixRadius, DecalData ) );

   addField( "renderPriority", TypeS8, Offset( renderPriority, DecalData ), "Default renderPriority for decals of this type." );
}

void DecalData::onStaticModified( const char *slotName, const char *newValue )
{
   Parent::onStaticModified( slotName, newValue );

   if ( !isProperlyAdded() )
      return;

   // To allow changing materials live.
   if ( dStricmp( slotName, "material" ) == 0 )
   {
      materialName = newValue;
      _updateMaterial();
   }
   // To allow changing name live.
   else if ( dStricmp( slotName, "name" ) == 0 )
   {
      lookupName = getName();
   }
   else if ( dStricmp( slotName, "renderPriority" ) == 0 )
   {
      renderPriority = getMax( renderPriority, (U8)1 );
   }
}

bool DecalData::preload( bool server, String &errorStr )
{
   if (Parent::preload(server, errorStr) == false)
      return false;

   // Server assigns name to lookupName,
   // client assigns lookupName in unpack.
   if ( server )
      lookupName = getName();

   return true;
}

void DecalData::packData( BitStream *stream )
{
   Parent::packData( stream );

   stream->write( lookupName );
   stream->write( size );
   stream->write( materialName );
   stream->write( lifeSpan );
   stream->write( fadeTime );
	stream->write( texCoordCount );

   for (S32 i = 0; i < texCoordCount; i++)
      mathWrite( *stream, texRect[i] );

   stream->write( startPixRadius );
   stream->write( endPixRadius );
   stream->write( renderPriority );
   stream->write( clippingMasks );

	stream->write( texRows );
   stream->write( texCols );
	stream->write( frame );
	stream->write( randomize );
}

void DecalData::unpackData( BitStream *stream )
{
   Parent::unpackData( stream );

   stream->read( &lookupName );
   stream->read( &size );  
   stream->read( &materialName );
   _updateMaterial();
   stream->read( &lifeSpan );
   stream->read( &fadeTime );
	stream->read( &texCoordCount );

   for (S32 i = 0; i < texCoordCount; i++)
      mathRead(*stream, &texRect[i]);

   stream->read( &startPixRadius );
   stream->read( &endPixRadius );
   stream->read( &renderPriority );
   stream->read( &clippingMasks );

	stream->read( &texRows );
   stream->read( &texCols );
	stream->read( &frame );
	stream->read( &randomize );
}

void DecalData::_initMaterial()
{
   SAFE_DELETE( matInst );

   if ( material )
      matInst = material->createMatInstance();
   else
      matInst = MATMGR->createMatInstance( "WarningMaterial" );

   GFXStateBlockDesc desc;
   desc.setZReadWrite( true, false );
   //desc.zFunc = GFXCmpLess;
   matInst->addStateBlockDesc( desc );

   matInst->init( MATMGR->getDefaultFeatures(), getGFXVertexFormat<DecalVertex>() );
}

void DecalData::_updateMaterial()
{
   if ( materialName.isEmpty() )
      return;

   Material *pMat = NULL;
   if ( !Sim::findObject( materialName, pMat ) )
   {
      Con::printf( "DecalData::unpackUpdate, failed to find Material of name &s!", materialName.c_str() );
      return;
   }

   material = pMat;

   // Only update material instance if we have one allocated.
   if ( matInst )
      _initMaterial();
}

Material* DecalData::getMaterial()
{
   if ( !material )
   {
      _updateMaterial();
      if ( !material )
         material = static_cast<Material*>( Sim::findObject("WarningMaterial") );
   }

   return material;
}

BaseMatInstance* DecalData::getMaterialInstance()
{
   if ( !material || !matInst || matInst->getMaterial() != material )
      _initMaterial();

   return matInst;
}

DecalData* DecalData::findDatablock( String searchName )
{
   StringTableEntry className = DecalData::getStaticClassRep()->getClassName();
   DecalData *pData;
   SimSet *set = getSet();
   SimSetIterator iter( set );

   for ( ; *iter; ++iter )
   {
      if ( (*iter)->getClassName() != className )
      {
         Con::errorf( "DecalData::findDatablock - found a class %s object in DecalDataSet!", (*iter)->getClassName() );
         continue;
      }

      pData = static_cast<DecalData*>( *iter );
      if ( pData->lookupName.equal( searchName, String::NoCase ) )
         return pData;
   }

   return NULL;
}

void DecalData::inspectPostApply()
{ 
	reloadRects();
}

void DecalData::reloadRects()
{ 
	F32 rowsBase = 0;
	F32 colsBase = 0;
	bool canRenderRowsByFrame = false;
	bool canRenderColsByFrame = false;
	S32 id = 0;
	
	texRect[id].point.x = 0.f;
	texRect[id].extent.x = 1.f;
	texRect[id].point.y = 0.f;
	texRect[id].extent.y = 1.f;
	
	texCoordCount = (texRows * texCols) - 1;

	if( texCoordCount > 16 )
	{
		Con::warnf("Coordinate max must be lower than 16 to be a valid decal !");
		texRows = 1;
		texCols = 1;
		texCoordCount = 1;
	}

	// use current datablock information in order to build a template to extract
	// coordinates from. 
	if( texRows > 1 )
	{
		rowsBase = ( 1.f / texRows );
		canRenderRowsByFrame = true;
	}
	if( texCols > 1 )
	{
		colsBase = ( 1.f / texCols );
		canRenderColsByFrame = true;
	}

	// if were able, lets enter the loop
   if( frame >= 0 && (canRenderRowsByFrame || canRenderColsByFrame) )
	{
		// columns first then rows
		for ( S32 colId = 1; colId <= texCols; colId++ )
		{
			for ( S32 rowId = 1; rowId <= texRows; rowId++, id++ )
			{
				// if were over the coord count, lets go
				if(id > texCoordCount)
					return;

				// keep our dimensions correct
				if(rowId > texRows)
					rowId = 1;

				if(colId > texCols)
					colId = 1;

				// start setting our rect values per frame
				if( canRenderRowsByFrame )
				{
					texRect[id].point.x = rowsBase * ( rowId - 1 );
					texRect[id].extent.x = rowsBase;
				}
				
				if( canRenderColsByFrame )
				{
					texRect[id].point.y = colsBase * ( colId - 1 );
					texRect[id].extent.y = colsBase;
				}
			}
		}
	}
}

ConsoleMethod( DecalData, postApply, void, 2, 2, "")
{
	object->inspectPostApply();
}