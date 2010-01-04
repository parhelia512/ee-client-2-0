
#include "platform/platform.h"
#include "lightFlareData.h"

#include "sim/processList.h"
#include "core/stream/bitStream.h"
#include "sceneGraph/sceneState.h"
#include "renderInstance/renderPassManager.h"
#include "math/mathUtils.h"
#include "math/mathIO.h"
#include "T3D/gameConnection.h"
#include "gfx/gfxOcclusionQuery.h"
#include "gfx/gfxDrawUtil.h"

namespace 
{
   void vectorRotateZAxis( Point3F &vec, F32 radians )
   {
      VectorF newVec;
      F32 newX = mCos(radians) * vec.x - mSin(radians) * vec.y;
      F32 newY = mSin(radians) * vec.x + mCos(radians) * vec.y;

      vec.x = newX;
      vec.y = newY;
   }
}

LightFlareState::~LightFlareState()
{
   delete occlusionQuery;
   delete fullPixelQuery;
}

void LightFlareState::clear()
{
   visChangedTime = 0;
   visible = false;
   scale = 1.0f;
   fullBrightness = 1.0f;
   lightMat = MatrixF::Identity;
   lightInfo = NULL;
   worldRadius = -1.0f;
   occlusion = -1.0f;
   occlusionQuery = NULL;
   fullPixelQuery = NULL;
}

LightFlareData::LightFlareData()
 : mFlareEnabled( true ),
   mElementCount( 0 ),
   mScale( 1.0f ),
   mOcclusionRadius( 0.5f )
{
   dMemset( mElementRect, 0, sizeof( RectF ) * MAX_ELEMENTS );   
   dMemset( mElementScale, 0, sizeof( F32 ) * MAX_ELEMENTS );
   dMemset( mElementTint, 0, sizeof( ColorF ) * MAX_ELEMENTS );
   dMemset( mElementRotate, 0, sizeof( bool ) * MAX_ELEMENTS );
   dMemset( mElementUseLightColor, 0, sizeof( bool ) * MAX_ELEMENTS );   

   for ( U32 i = 0; i < MAX_ELEMENTS; i++ )   
      mElementDist[i] = -1.0f;   
}

LightFlareData::~LightFlareData()
{
}

IMPLEMENT_CO_DATABLOCK_V1( LightFlareData );

void LightFlareData::initPersistFields()
{
   addGroup( "LightFlareData" );

      addField( "overallScale", TypeF32, Offset( mScale, LightFlareData ) );
      addField( "occlusionRadius", TypeF32, Offset( mOcclusionRadius, LightFlareData ), 
         "Radius in world units to test for occlusion if supported by hardware, disable by setting radius non-positive." );

   endGroup( "LightFlareData" );

   addGroup( "FlareElements" );

      addField( "flareEnabled", TypeBool, Offset( mFlareEnabled, LightFlareData ) );
      addField( "flareTexture", TypeImageFilename, Offset( mFlareTextureName, LightFlareData ) );

      addArray( "Elements", MAX_ELEMENTS );

         addField( "elementRect", TypeRectF, Offset( mElementRect, LightFlareData ), MAX_ELEMENTS );
         addField( "elementDist", TypeF32, Offset( mElementDist, LightFlareData ), MAX_ELEMENTS );
         addField( "elementScale", TypeF32, Offset( mElementScale, LightFlareData ), MAX_ELEMENTS );
         addField( "elementTint", TypeColorF, Offset( mElementTint, LightFlareData ), MAX_ELEMENTS );
         addField( "elementRotate", TypeBool, Offset( mElementRotate, LightFlareData ), MAX_ELEMENTS );
         addField( "elementUseLightColor", TypeBool, Offset( mElementUseLightColor, LightFlareData ), MAX_ELEMENTS );

      endArray( "FlareElements" );

   endGroup( "Flares" );

   Parent::initPersistFields();
}

void LightFlareData::inspectPostApply()
{
   Parent::inspectPostApply();

   // Hack to allow changing properties in game.
   // Do the same work as preload.
   
   String str;
   _preload( false, str );
}

bool LightFlareData::preload( bool server, String &errorStr )
{
   if ( !Parent::preload( server, errorStr ) )
      return false;

   return _preload( server, errorStr );
}

void LightFlareData::packData( BitStream *stream )
{
   Parent::packData( stream );

   stream->writeFlag( mFlareEnabled );
   stream->write( mFlareTextureName );   
   stream->write( mScale );
   stream->write( mOcclusionRadius );

   stream->write( mElementCount );

   for ( U32 i = 0; i < mElementCount; i++ )
   {
      mathWrite( *stream, mElementRect[i] );
      stream->write( mElementDist[i] );
      stream->write( mElementScale[i] );
      stream->write( mElementTint[i] );
      stream->writeFlag( mElementRotate[i] );
      stream->writeFlag( mElementUseLightColor[i] );
   }
}

void LightFlareData::unpackData( BitStream *stream )
{
   Parent::unpackData( stream );

   mFlareEnabled = stream->readFlag();
   stream->read( &mFlareTextureName );   
   stream->read( &mScale );
   stream->read( &mOcclusionRadius );

   stream->read( &mElementCount );

   for ( U32 i = 0; i < mElementCount; i++ )
   {
      mathRead( *stream, &mElementRect[i] );
      stream->read( &mElementDist[i] );
      stream->read( &mElementScale[i] );
      stream->read( &mElementTint[i] );
      mElementRotate[i] = stream->readFlag();
      mElementUseLightColor[i] = stream->readFlag();
   }
}

void LightFlareData::prepRender( SceneState *state, LightFlareState *flareState )
{    
   PROFILE_SCOPE( LightFlareData_prepRender );

   // No elements then nothing to render.
   if ( mElementCount == 0 )
      return;

   // We need these all over the place later.
   const Point3F &camPos = state->getCameraPosition();
   const RectI &viewport = GFX->getViewport();
   const Point3F &lightPos = flareState->lightMat.getPosition();
   LightInfo *lightInfo = flareState->lightInfo;

   bool isVectorLight = lightInfo->getType() == LightInfo::Vector;

   // Perform visibility testing on the light...
   // Project the light position from world to screen space, we need this
   // position later, and it tells us if it is actually onscreen.   
   
   Point3F lightPosSS;
   bool onscreen = MathUtils::mProjectWorldToScreen( lightPos, &lightPosSS, viewport, GFX->getWorldMatrix(), gClientSceneGraph->getNonClipProjection() );  

   U32 visDelta = U32_MAX;
   U32 fadeOutTime = 20;
   U32 fadeInTime = 125;    

   // Fade factor based on amount of occlusion.
   F32 occlusionFade = 1.0f;

   bool lightVisible = true;
   
   if ( !state->isReflectPass() )
   {
      // It is onscreen, so raycast as a simple occlusion test.

      U32 losMask =	STATIC_COLLISION_MASK |
                     ShapeBaseObjectType |
                     StaticTSObjectType |
                     ItemObjectType |
                     PlayerObjectType;

      GameConnection *conn = GameConnection::getConnectionToServer();
      if ( !conn )
         return;

      bool needsRaycast = true;

      // NOTE: if hardware does not support HOQ it will return NULL
      // and we will retry every time but there is not currently a good place
      // for one-shot initialization of LightFlareState
      if ( flareState->occlusionQuery == NULL )
         flareState->occlusionQuery = GFX->createOcclusionQuery();
      if ( flareState->fullPixelQuery == NULL )
         flareState->fullPixelQuery = GFX->createOcclusionQuery();

      if ( flareState->occlusionQuery && 
           ( ( isVectorLight && flareState->worldRadius > 0.0f ) || 
             ( !isVectorLight && mOcclusionRadius > 0.0f ) ) )
      {
         // Always treat light as onscreen if using HOQ
         // it will be faded out if offscreen anyway.
         onscreen = true;

         U32 pixels = -1;
         GFXOcclusionQuery::OcclusionQueryStatus status = flareState->occlusionQuery->getStatus( true, &pixels );

         String str = flareState->occlusionQuery->statusToString( status );
         Con::setVariable( "$Flare::OcclusionStatus", str.c_str() );
         Con::setIntVariable( "$Flare::OcclusionVal", pixels );
         
         if ( status == GFXOcclusionQuery::Occluded )
            occlusionFade = 0.0f;

         if ( status != GFXOcclusionQuery::Unset )         
            needsRaycast = false;

         RenderPassManager *pass = state->getRenderPass();

         OccluderRenderInst *ri = pass->allocInst<OccluderRenderInst>();   

         Point3F scale( Point3F::One );

         if ( isVectorLight && flareState->worldRadius > 0.0f )         
            scale *= flareState->worldRadius;
         else
            scale *= mOcclusionRadius;
         
         ri->type = RenderPassManager::RIT_Occluder;
         ri->query = flareState->occlusionQuery;   
         ri->query2 = flareState->fullPixelQuery;
         ri->position = lightPos;
         ri->scale = scale;
         ri->orientation = pass->allocUniqueXform( lightInfo->getTransform() );         
         ri->isSphere = true;
         state->getRenderPass()->addInst( ri );

         if ( status == GFXOcclusionQuery::NotOccluded )
         {
            U32 fullPixels;
            flareState->fullPixelQuery->getStatus( true, &fullPixels );

            occlusionFade = (F32)pixels / (F32)fullPixels;

            // Approximation of the full pixel count rather than doing
            // two queries, but it is not very accurate.
            /*
            F32 dist = ( camPos - lightPos ).len();
            F32 radius = scale.x;
            radius = ( radius / dist ) * state->getWorldToScreenScale().y;

            occlusionFade = (F32)pixels / (4.0f * radius * radius);
            occlusionFade = mClampF( occlusionFade, 0.0f, 1.0f );            
            */
         }
      }

      Con::setFloatVariable( "$Flare::OcclusionFade", occlusionFade );

      if ( needsRaycast )
      {
         // Use a raycast to determine occlusion.

         bool fps = conn->isFirstPerson();

         GameBase *control = conn->getControlObject();
         if ( control && fps )
            control->disableCollision();

         RayInfo rayInfo;

         if ( gClientContainer.castRayRendered( camPos, lightPos, losMask, &rayInfo ) )
            occlusionFade = 0.0f;

         if ( control && fps )
            control->enableCollision();
      }

      lightVisible = onscreen && occlusionFade > 0.0f;

      // To perform a fade in/out when we gain or lose visibility
      // we must update/store the visibility state and time.

      U32 currentTime = Sim::getCurrentTime();

      if ( lightVisible != flareState->visible )
      {
         flareState->visible = lightVisible;
         flareState->visChangedTime = currentTime;
      }      

      // Save this in the state so that we have it during the reflect pass.
      flareState->occlusion = occlusionFade;

      visDelta = currentTime - flareState->visChangedTime;      
   }
   else // state->isReflectPass()
   {
      occlusionFade = flareState->occlusion;
      lightVisible = flareState->visible;
      visDelta = Sim::getCurrentTime() - flareState->visChangedTime;
   }

   // We can only skip rendering if the light is not visible, and it
   // has elapsed the fadeOutTime.
   if ( !lightVisible && visDelta > fadeOutTime )
      return;

   // In a reflection we only render the elements with zero distance.   
   U32 elementCount = mElementCount;
   if ( state->isReflectPass()  )
   {
      elementCount = 0;
      for ( ; elementCount < mElementCount; elementCount++ )
      {
         if ( mElementDist[elementCount] > 0.0f )
            break;
      }
   }

   if ( elementCount == 0 )
      return;

   // A bunch of preparatory math before generating verts...

   const Point2I &vpExtent = viewport.extent;
   Point3F viewportExtent( vpExtent.x, vpExtent.y, 1.0f );
   Point2I halfViewportExtentI( viewport.extent / 2 );
   Point3F halfViewportExtentF( (F32)halfViewportExtentI.x * 0.5f, (F32)halfViewportExtentI.y, 0.0f );
   Point3F screenCenter( 0,0,0 );
   Point3F oneOverViewportExtent( 1.0f / viewportExtent.x, 1.0f / viewportExtent.y, 1.0f );

   lightPosSS.y -= viewport.point.y;
   lightPosSS *= oneOverViewportExtent;
   lightPosSS = ( lightPosSS * 2.0f ) - Point3F::One;
   lightPosSS.y = -lightPosSS.y;
   lightPosSS.z = 0.0f;

   Point3F flareVec( screenCenter - lightPosSS );
   F32 flareLength = flareVec.len();   
   flareVec.normalizeSafe();

   Point3F basePoints[4];
   basePoints[0] = Point3F( -0.5, 0.5, 0.0 );  
   basePoints[1] = Point3F( -0.5, -0.5, 0.0 );   
   basePoints[2] = Point3F( 0.5, -0.5, 0.0 );   
   basePoints[3] = Point3F( 0.5, 0.5, 0.0 );

   Point3F rotatedBasePoints[4];
   rotatedBasePoints[0] = basePoints[0];
   rotatedBasePoints[1] = basePoints[1];
   rotatedBasePoints[2] = basePoints[2];
   rotatedBasePoints[3] = basePoints[3];

   Point3F fvec( -1, 0, 0 );   
   F32 rot = mAcos( mDot( fvec, flareVec ) );
   Point3F rvec( 0, -1, 0 );
   rot *= mDot( rvec, flareVec ) > 0.0f ? 1.0f : -1.0f;

   vectorRotateZAxis( rotatedBasePoints[0], rot );
   vectorRotateZAxis( rotatedBasePoints[1], rot );
   vectorRotateZAxis( rotatedBasePoints[2], rot );
   vectorRotateZAxis( rotatedBasePoints[3], rot );

   // Here we calculate a the light source's influence on the effect's size
   // and brightness...

   // Scale based on the current light brightness compared to its normal output.
   F32 lightSourceBrightnessScale = lightInfo->getBrightness() / flareState->fullBrightness;
   // Scale based on world space distance from camera to light source.
   F32 lightSourceWSDistanceScale = ( isVectorLight ) ? 1.0f : getMin( 10.0f / ( lightPos - camPos ).len(), 1.5f );   
   // Scale based on screen space distance from screen position of light source to the screen center.
   F32 lightSourceSSDistanceScale = ( 1.5f - ( lightPosSS - screenCenter ).len() ) / 1.5f;

   // Scale based on recent visibility changes, fading in or out.
   F32 fadeInOutScale = 1.0f;
   if ( lightVisible && visDelta < fadeInTime && flareState->occlusion )
      fadeInOutScale = (F32)visDelta / (F32)fadeInTime;
   else if ( !lightVisible && visDelta < fadeOutTime )
      fadeInOutScale = 1.0f - (F32)visDelta / (F32)fadeOutTime;

   // This combined scale influences the size of all elements this effect renders.
   // Note we also add in a scale that is user specified in the Light.
   F32 lightSourceIntensityScale = lightSourceBrightnessScale * 
                                   lightSourceWSDistanceScale * 
                                   lightSourceSSDistanceScale * 
                                   fadeInOutScale * 
                                   flareState->scale *
                                   occlusionFade;

   // The baseColor which modulates the color of all elements.
   ColorF baseColor;
   if ( flareState->fullBrightness == 0.0f )
      baseColor = ColorF::BLACK;
   else
      // These are the factors which affect the "alpha" of the flare effect.
      // Modulate more in as appropriate.
      baseColor = ColorF::WHITE * lightSourceBrightnessScale * occlusionFade;

   // Fill in the vertex buffer...
   const U32 vertCount = 4 * elementCount;
   if (  flareState->vertBuffer.isNull() || 
         flareState->vertBuffer->mNumVerts != vertCount )
         flareState->vertBuffer.set( GFX, vertCount, GFXBufferTypeDynamic );

   GFXVertexPCT *pVert = flareState->vertBuffer.lock();

   const Point2I &widthHeightI = mFlareTexture.getWidthHeight();
   Point2F oneOverTexSize( 1.0f / (F32)widthHeightI.x, 1.0f / (F32)widthHeightI.y );

   for ( U32 i = 0; i < elementCount; i++ )
   {      
      Point3F *basePos = mElementRotate[i] ? rotatedBasePoints : basePoints;

      ColorF elementColor( baseColor * mElementTint[i] );
      if ( mElementUseLightColor[i] )
         elementColor *= lightInfo->getColor();

      Point3F elementPos;
      elementPos = lightPosSS + flareVec * mElementDist[i] * flareLength;      
      elementPos.z = 0.0f;

      F32 maxDist = 1.5f;
      F32 elementDist = mSqrt( ( elementPos.x * elementPos.x ) + ( elementPos.y * elementPos.y ) );
      F32 distanceScale = ( maxDist - elementDist ) / maxDist;
      distanceScale = 1.0f;

      const RectF &elementRect = mElementRect[i];
      Point3F elementSize( elementRect.extent.x, elementRect.extent.y, 1.0f );
      elementSize *= mElementScale[i] * distanceScale * mScale * lightSourceIntensityScale;

      if ( elementSize.x < 100.0f )
      {
         F32 alphaScale = mPow( elementSize.x / 100.0f, 2 );
         elementColor *= alphaScale;
      }

      elementColor.clamp();

      Point2F texCoordMin, texCoordMax;
      texCoordMin = elementRect.point * oneOverTexSize;
      texCoordMax = ( elementRect.point + elementRect.extent ) * oneOverTexSize;          

      pVert->color = elementColor;
      pVert->point = ( basePos[0] * elementSize * oneOverViewportExtent ) + elementPos;      
      pVert->texCoord.set( texCoordMin.x, texCoordMax.y );
      pVert++;

      pVert->color = elementColor;
      pVert->point = ( basePos[1] * elementSize * oneOverViewportExtent ) + elementPos;
      pVert->texCoord.set( texCoordMax.x, texCoordMax.y );
      pVert++;

      pVert->color = elementColor;
      pVert->point = ( basePos[2] * elementSize * oneOverViewportExtent ) + elementPos;
      pVert->texCoord.set( texCoordMax.x, texCoordMin.y );
      pVert++;

      pVert->color = elementColor;
      pVert->point = ( basePos[3] * elementSize * oneOverViewportExtent ) + elementPos;
      pVert->texCoord.set( texCoordMin.x, texCoordMin.y );
      pVert++;
   }   

   flareState->vertBuffer.unlock();   

   // Create and submit the render instance...
   
   RenderPassManager *renderManager = state->getRenderPass();
   ParticleRenderInst *ri = renderManager->allocInst<ParticleRenderInst>();

   ri->vertBuff = &flareState->vertBuffer;
   ri->primBuff = &mFlarePrimBuffer;
   ri->translucentSort = true;
   ri->type = RenderPassManager::RIT_Particle;
   ri->sortDistSq = ( lightPos - camPos ).lenSquared();

   ri->modelViewProj = &MatrixF::Identity;
   ri->bbModelViewProj = ri->modelViewProj;

   ri->count = elementCount;

   // Only draw the light flare in high-res mode, never off-screen mode
   ri->systemState = ParticleRenderInst::AwaitingHighResDraw;

   ri->blendStyle = ParticleRenderInst::BlendGreyscale;

   ri->diffuseTex = &*(mFlareTexture);

   ri->softnessDistance = 1.0f; 

   // Sort by texture too.
   ri->defaultKey = ri->diffuseTex ? (U32)ri->diffuseTex : (U32)ri->vertBuff;

   renderManager->addInst( ri );
}

bool LightFlareData::_preload( bool server, String &errorStr )
{
   mElementCount = 0;
   for ( U32 i = 0; i < MAX_ELEMENTS; i++ )
   {
      if ( mElementDist[i] == -1 )
         break;
      mElementCount = i + 1;
   }   

   if ( mElementCount > 0 )
      _makePrimBuffer( &mFlarePrimBuffer, mElementCount );

   if ( !server )
   {
      if ( mFlareTextureName.isNotEmpty() )      
         mFlareTexture.set( mFlareTextureName, &GFXDefaultStaticDiffuseProfile, "FlareTexture" );  
   }

   return true;
}

void LightFlareData::_makePrimBuffer( GFXPrimitiveBufferHandle *pb, U32 count )
{
   // create index buffer based on that size
   U32 indexListSize = count * 6; // 6 indices per particle
   U16 *indices = new U16[ indexListSize ];

   for ( U32 i = 0; i < count; i++ )
   {
      // this index ordering should be optimal (hopefully) for the vertex cache
      U16 *idx = &indices[i*6];
      volatile U32 offset = i * 4;  // set to volatile to fix VC6 Release mode compiler bug
      idx[0] = 0 + offset;
      idx[1] = 1 + offset;
      idx[2] = 3 + offset;
      idx[3] = 1 + offset;
      idx[4] = 3 + offset;
      idx[5] = 2 + offset; 
   }

   U16 *ibIndices;
   GFXBufferType bufferType = GFXBufferTypeStatic;

#ifdef TORQUE_OS_XENON
   // Because of the way the volatile buffers work on Xenon this is the only
   // way to do this.
   bufferType = GFXBufferTypeVolatile;
#endif
   pb->set( GFX, indexListSize, 0, bufferType );
   pb->lock( &ibIndices );
   dMemcpy( ibIndices, indices, indexListSize * sizeof(U16) );
   pb->unlock();

   delete [] indices;
}

IMPLEMENT_CONSOLETYPE( LightFlareData )
IMPLEMENT_GETDATATYPE( LightFlareData )
IMPLEMENT_SETDATATYPE( LightFlareData )


ConsoleMethod( LightFlareData, apply, void, 2, 2, "force an inspectPostApply for the benefit of tweaking via the console" )
{
   object->inspectPostApply();
}