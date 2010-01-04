//-----------------------------------------------------------------------------
// Torque Game Engine Advanced
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "environment/sky.h"
#include "math/mMath.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "sceneGraph/sceneObject.h"
#include "math/mathIO.h"
#include "sceneGraph/windingClipper.h"
#include "platform/profiler.h"
#include "gfx/primBuilder.h"
#include "T3D/fx/particleEmitter.h"
#include "renderInstance/renderPassManager.h"
#include "core/stream/fileStream.h"

//
#define HORIZON         0.0f
#define FOG_BAN_DETAIL  8
#define RAD             (2.0f * M_PI_F)


IMPLEMENT_CO_NETOBJECT_V1(Sky);

//Static Sky variables
bool Sky::smCloudsOn       = true;
bool Sky::smCloudOutlineOn = false;
bool Sky::smSkyOn          = true;
S32  Sky::smNumCloudsOn    = MAX_NUM_LAYERS;

//Static Cloud variables
StormInfo Cloud::mGStormData;
F32 Cloud::mRadius;


//---------------------------------------------------------------------------
Sky::Sky()
{
   mNumCloudLayers = 0;
   mTypeMask |= EnvironmentObjectType;
   mNetFlags.set(Ghostable | ScopeAlways);

   mSkyTexturesOn   = true;
   mRenderBoxBottom = false;
   mSolidFillColor.set(0.0f, 1.0f, 0.0f, 0.0f);
   mWindVelocity.set(1.0f, 0.0f, 0.0f);
   mWindDir.set(0.f, 0.f);
   mNoRenderBans = false;

   mSkyGlow = false;

   mLastVisDisMod = -1;

   for(S32 i = 0; i < MAX_NUM_LAYERS; ++i)
   {
      mCloudSpeed[i]  = 0.0001f * (1.0f + (i * 1.0f));
      mCloudHeight[i] = 0.0f;
   }

   mStormCloudData.state = isDone;
   mStormCloudData.speed = 0.0f;
   mStormCloudData.time  = 0.0f;

   mSkyVB = NULL;
   mBanOffsetHeight = 50.0f;
}

//------------------------------------------------------------------------------
Sky::~Sky()
{
}

//---------------------------------------------------------------------------
bool Sky::onAdd()
{
   if(!Parent::onAdd())
      return false;

   mObjBox.minExtents.set(-1e9f, -1e9f, -1e9f);
   mObjBox.maxExtents.set( 1e9f,  1e9f,  1e9f);
   resetWorldBox();

   if(isClientObject())
   {
      if(!loadDml())
         return false;

      loadVBPoints();

      initSkyData();
      setupStateBlocks();
   }
   else
   {
      setWindVelocity(mWindVelocity);
   }

   addToScene();
   setSkyColor();
   return true;
}

//---------------------------------------------------------------------------
void Sky::initSkyData()
{
   calcPoints();
   mWindDir = Point2F(mWindVelocity.x, -mWindVelocity.y);
   mWindDir.normalize();
   for(S32 i = 0; i < MAX_NUM_LAYERS; ++i)
   {
      mCloudLayer[i].setHeights(mCloudHeight[i], mCloudHeight[i]-0.05f, 0.00f);
      mCloudLayer[i].setSpeed(mWindDir * mCloudSpeed[i]);
      mCloudLayer[i].setPoints();
   }
   setSkyColor();
}

//---------------------------------------------------------------------------
void Sky::setSkyColor()
{
   if(mSceneManager)
   {
      mRealSkyColor.red   = S32(mSolidFillColor.red * 255.0f);
      mRealSkyColor.green = S32(mSolidFillColor.green * 255.0f);
      mRealSkyColor.blue  = S32(mSolidFillColor.blue * 255.0f);
   }
}

//---------------------------------------------------------------------------
void Sky::setWindVelocity(const Point3F & vel)
{
   mWindVelocity = vel;
   ParticleEmitter::setWindVelocity(vel);
   if(isServerObject())
      setMaskBits(WindMask);
}

const Point3F &Sky::getWindVelocity() const
{
   return(mWindVelocity);
}

//---------------------------------------------------------------------------
void Sky::onRemove()
{
   mSkyVB = NULL;

   removeFromScene();
   Parent::onRemove();
}

//---------------------------------------------------------------------------
ConsoleMethod( Sky, stormClouds, void, 4, 4, "(bool show, float duration)")
{
   Sky *ctrl = static_cast<Sky*>(object);
   ctrl->stormCloudsOn(dAtoi(argv[2]), dAtof(argv[3]));
}

ConsoleMethod( Sky, getWindVelocity, const char *, 2, 2, "()")
{
   Sky * sky = static_cast<Sky*>(object);
   char * retBuf = Con::getReturnBuffer(128);

   Point3F vel = sky->getWindVelocity();
   dSprintf(retBuf, 128, "%f %f %f", vel.x, vel.y, vel.z);
   return(retBuf);
}

ConsoleMethod( Sky, applySkyChanges, void, 2, 2, "() - Apply any changes.")
{
	object->applySkyChanges();
}

//---------------------------------------------------------------------------

ConsoleMethod( Sky, setWindVelocity, void, 5, 5, "(float x, float y, float z)")
{
   Sky * sky = static_cast<Sky*>(object);
   if(sky->isClientObject())
      return;

   Point3F vel(dAtof(argv[2]), dAtof(argv[3]), dAtof(argv[4]));
   sky->setWindVelocity(vel);
}

ConsoleMethod( Sky, stormCloudsShow, void, 3, 3, "(bool showClouds)")
{
   Sky *ctrl = static_cast<Sky*>(object);
   ctrl->stormCloudsShow(dAtob(argv[2]));
}

//---------------------------------------------------------------------------
void Sky::initPersistFields()
{
   addGroup("Media");	
   addField("materialList",            TypeStringFilename,  Offset(mMaterialListName,Sky));
   endGroup("Media");	

   addGroup("Clouds");	
   // This is set from the DML.
   addField("cloudHeightPer",          TypeF32,             Offset(mCloudHeight,Sky),MAX_NUM_LAYERS);
   addField("cloudSpeed1",             TypeF32,             Offset(mCloudSpeed[0],Sky));
   addField("cloudSpeed2",             TypeF32,             Offset(mCloudSpeed[1],Sky));
   addField("cloudSpeed3",             TypeF32,             Offset(mCloudSpeed[2],Sky));
   endGroup("Clouds");	

   addGroup("Wind");	
   addField("windVelocity",            TypePoint3F,         Offset(mWindVelocity, Sky));
   endGroup("Wind");	

   addGroup("Misc");	
   addField("SkySolidColor",           TypeColorF,          Offset(mSolidFillColor, Sky));
   addField("useSkyTextures",          TypeBool,            Offset(mSkyTexturesOn, Sky));
   addField("renderBottomTexture",     TypeBool,            Offset(mRenderBoxBottom, Sky));
   addField("noRenderBans",            TypeBool,            Offset(mNoRenderBans, Sky));
   addField("renderBanOffsetHeight",   TypeF32,             Offset(mBanOffsetHeight, Sky));

   addField("skyGlow",                 TypeBool,            Offset(mSkyGlow, Sky));
   addField("skyGlowColor",            TypeColorF,          Offset(mSkyGlowColor, Sky));
   endGroup("Misc");

   Parent::initPersistFields();
}

void Sky::consoleInit()
{
#if defined(TORQUE_DEBUG)
   Con::addVariable("pref::CloudOutline", TypeBool, &smCloudOutlineOn);
#endif
   Con::addVariable("pref::CloudsOn",    TypeBool, &smCloudsOn);

   Con::addVariable("pref::NumCloudLayers", TypeS32, &smNumCloudsOn);
   Con::addVariable("pref::SkyOn",          TypeBool, &smSkyOn);
}
//---------------------------------------------------------------------------
void Sky::unpackUpdate(NetConnection *, BitStream *stream)
{
   if(stream->readFlag()) // InitMask
   {
      stream->read(&mMaterialListName);
      loadDml();

      stream->read(&mSkyTexturesOn);
      stream->read(&mRenderBoxBottom);
      stream->read(&mSolidFillColor.red);
      stream->read(&mSolidFillColor.green);
      stream->read(&mSolidFillColor.blue);
      mNoRenderBans = stream->readFlag();
      stream->read(&mBanOffsetHeight);

      for (U32 i = 0; i < MAX_NUM_LAYERS; i++)
      {
         stream->read(&mCloudHeight[i]);
         stream->read(&mCloudSpeed[i]);
      }

      initSkyData();
      Point3F vel;
      if(mathRead(*stream, &vel))
         setWindVelocity(vel);

   } // InitMask

   if(stream->readFlag()) // WindMask
   {
      Point3F vel;
      if(mathRead(*stream, &vel))
         setWindVelocity(vel);
   }

   if(stream->readFlag()) // SkyGlowMask
   {
      mSkyGlow = stream->readFlag();
      if(mSkyGlow)
      {
         stream->read(&mSkyGlowColor.red);
         stream->read(&mSkyGlowColor.green);
         stream->read(&mSkyGlowColor.blue);
      }
   }

}

//---------------------------------------------------------------------------
U32 Sky::packUpdate(NetConnection *, U32 mask, BitStream *stream)
{
   if(stream->writeFlag(mask & InitMask))
   {
      stream->write(mMaterialListName);
      stream->write(mSkyTexturesOn);
      stream->write(mRenderBoxBottom);
      stream->write(mSolidFillColor.red);
      stream->write(mSolidFillColor.green);
      stream->write(mSolidFillColor.blue);
      stream->writeFlag(mNoRenderBans);
      stream->write(mBanOffsetHeight);

      for (U32 i = 0; i < MAX_NUM_LAYERS; i++)
      {
         stream->write(mCloudHeight[i]);
         stream->write(mCloudSpeed[i]);
      }
      mathWrite(*stream, mWindVelocity);
   }

   if(stream->writeFlag(mask & WindMask))
      mathWrite(*stream, mWindVelocity);

   if(stream->writeFlag(mask & SkyGlowMask))
   {
      if(stream->writeFlag(mSkyGlow))
      {
         stream->write(mSkyGlowColor.red);
         stream->write(mSkyGlowColor.green);
         stream->write(mSkyGlowColor.blue);
      }
   }

   return 0;
}

//---------------------------------------------------------------------------
void Sky::inspectPostApply()
{
   setMaskBits(InitMask | SkyGlowMask);
}

void Sky::setupStateBlocks()
{
   GFXStateBlockDesc clear;
   clear.cullDefined = true;
   clear.cullMode = GFXCullNone;
   clear.zDefined = true;
   clear.zWriteEnable = false;
   mClearSB = GFX->createStateBlock(clear);

   GFXStateBlockDesc skybox;
   skybox.cullDefined = true;
   skybox.cullMode = GFXCullNone;
   skybox.zDefined = true;
   skybox.zEnable = false;
   skybox.zWriteEnable = false;
   skybox.samplersDefined = true;
   skybox.samplers[0] = GFXSamplerStateDesc::getClampLinear();
   mSkyBoxSB = GFX->createStateBlock(skybox);

   GFXStateBlockDesc renderbans;
   renderbans.cullDefined = true;
   renderbans.cullMode = GFXCullNone;
   renderbans.zDefined = true;
   renderbans.zEnable = false;
   renderbans.zWriteEnable = false;
   renderbans.blendDefined = true;
   renderbans.blendEnable = true;
   renderbans.blendSrc = GFXBlendSrcAlpha;
   renderbans.blendDest = GFXBlendInvSrcAlpha;
   mRenderBansSB = GFX->createStateBlock(renderbans);
}

//---------------------------------------------------------------------------
void Sky::renderObject(ObjectRenderInst *ri, SceneState *state, BaseMatInstance* overrideMat)
{
   if (overrideMat)
      return;

   GFX->disableShaders();

   for(U32 i = 0; i < GFX->getNumSamplers(); i++)
      GFX->setTexture(i, NULL);

   RectI viewport = GFX->getViewport();

   // Clear the objects viewport to the fog color.  This is something of a dirty trick,
   //  since we want an identity projection matrix here...
   MatrixF proj = GFX->getProjectionMatrix();
   GFX->setProjectionMatrix( MatrixF( true ) );

   GFX->pushWorldMatrix();
   GFX->setWorldMatrix( MatrixF( true ) );

   GFX->setStateBlock(mClearSB);

   ColorI fogColor(200, 200, 200, 255);

   if (state->getSceneManager())
      fogColor = state->getSceneManager()->getFogData().color;

   PrimBuild::color3i(U8(fogColor.red),U8(fogColor.green),U8(fogColor.blue));

   GFX->setupGenericShaders( GFXDevice::GSColor );

   PrimBuild::begin(GFXTriangleFan, 4);
      PrimBuild::vertex3f(-1, -1, 1);
      PrimBuild::vertex3f(-1,  1, 1);
      PrimBuild::vertex3f( 1,  1, 1);
      PrimBuild::vertex3f( 1, -1, 1);
   PrimBuild::end();

   // this fixes oblique frustum clip prob on planar reflections
   if( state->isInvertedCull() )
      GFX->setProjectionMatrix( gClientSceneGraph->getNonClipProjection() );
   else
      GFX->setProjectionMatrix( proj );

   GFX->popWorldMatrix();
   GFX->pushWorldMatrix();

   Point3F camPos = state->getCameraPosition();

   MatrixF tMat(1);
   tMat.setPosition(camPos);

   GFX->multWorld(tMat);

   render(state);

   GFX->setProjectionMatrix(proj);

   GFX->popWorldMatrix();

   GFX->setViewport(viewport);
}

//---------------------------------------------------------------------------
bool Sky::prepRenderImage(SceneState* state, const U32 stateKey,
                          const U32 startZone, const bool modifyBaseState)
{
   TORQUE_UNUSED(startZone); TORQUE_UNUSED(modifyBaseState);
   AssertFatal(modifyBaseState == false, "Error, should never be called with this parameter set");
   AssertFatal(startZone == 0xFFFFFFFF, "Error, startZone should indicate -1");

   PROFILE_START(Sky_prepRenderImage);

   if (isLastState(state, stateKey))
   {
      PROFILE_END();
      return false;
   }
   setLastState(state, stateKey);

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if (state->isObjectRendered(this)) 
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &Sky::renderObject );
      ri->type = RenderPassManager::RIT_Sky;
      ri->defaultKey = 10;
      ri->defaultKey2 = 0;
      state->getRenderPass()->addInst( ri );      
   }

   PROFILE_END();
   return false;
}

//---------------------------------------------------------------------------
void Sky::render(SceneState *state)
{
   PROFILE_START(SkyRender);

   F32 banHeights[2] = {-(mSpherePt.z-1),-(mSpherePt.z-1)};
   F32 alphaBan[2]   = {0.0f, 0.0f};
   Point3F camPos;

   if(gClientSceneGraph)
   {
      F32 currentVisDis = gClientSceneGraph->getVisibleDistance();
      if(mLastVisDisMod != currentVisDis)
      {
         calcPoints();
         for(S32 i = 0; i < MAX_NUM_LAYERS; ++i)
            mCloudLayer[i].setPoints();

         mLastVisDisMod = currentVisDis;
      }
   }

   ColorI fogColor(200, 200, 200, 255);

   if (state->getSceneManager())
      fogColor = state->getSceneManager()->getFogData().color;

   // Setup default values
   alphaBan[0] = 0.0f;
   alphaBan[1] = 0.0f;
   banHeights[0] = HORIZON;
   banHeights[1] = banHeights[0] + mBanOffsetHeight;

   // if lower ban is at top of box then no clipping plane is needed
   if(banHeights[0] >= mSpherePt.z)
      banHeights[0] = banHeights[1] = mSpherePt.z;

   //Renders the 6 sides of the sky box
   if(alphaBan[1] < 1.0f)
      renderSkyBox(banHeights[0], alphaBan[1]);

   // if completely fogged out then no need to render
   if(alphaBan[1] < 1.0f)
   {
      if(smCloudsOn && mStormCloudsOn && smSkyOn)
      {
         F32 ang   = mAtan2(banHeights[0],mSkyBoxPt.x);
         F32 xyval = mSin(ang);
         F32 zval  = mCos(ang);
         PlaneF planes[4];
         planes[0] = PlaneF(xyval,  0.0f,   zval, 0.0f);
         planes[1] = PlaneF(-xyval, 0.0f,   zval, 0.0f);
         planes[2] = PlaneF(0.0f,   xyval,  zval, 0.0f);
         planes[3] = PlaneF(0.0f,   -xyval, zval, 0.0f);

         S32 numRender = (smNumCloudsOn > mNumCloudLayers) ? mNumCloudLayers : smNumCloudsOn;
         for(S32 x = 0; x < numRender; ++x)
            mCloudLayer[x].render(
                              Sim::getCurrentTime(), x, 
                              smCloudOutlineOn, mNumCloudLayers, 
                              planes);
      }
      
      if(!mNoRenderBans)
      {
         Point3F banPoints[2][MAX_BAN_POINTS];
         Point3F cornerPoints[MAX_BAN_POINTS];

         // Calculate upper, lower, and corner ban points
         calcBans(banHeights, banPoints, cornerPoints);

         GFX->setTexture(0, NULL);

         // Renders the side, top, and corner bans
         renderBans(alphaBan, banHeights, banPoints, cornerPoints, fogColor);
      }
   }

   PROFILE_END();
}

void Sky::setRenderPoints(Point3F* renderPoints, S32 index)
{
   renderPoints[0].set(mPoints[index  ].x, mPoints[index  ].y, mPoints[index  ].z);
   renderPoints[1].set(mPoints[index+1].x, mPoints[index+1].y, mPoints[index+1].z);
   renderPoints[2].set(mPoints[index+6].x, mPoints[index+6].y, mPoints[index+6].z);
   renderPoints[3].set(mPoints[index+5].x, mPoints[index+5].y, mPoints[index+5].z);
}

void Sky::calcTexCoords(Point2F* texCoords, Point3F* renderPoints, S32 index, F32 lowerBanHeight)
{

   for(S32 x = 0; x < 4; ++x)
      texCoords[x].set(mTexCoord[x].x, mTexCoord[x].y);
   S32 length = (S32)(mFabs(mPoints[index].z) + mFabs(mPoints[index + 5].z));
   F32 per = mPoints[index].z - renderPoints[3].z;   
    
   texCoords[3].y = texCoords[2].y = (per / length);
}

//---------------------------------------------------------------------------
void Sky::renderSkyBox(F32 lowerBanHeight, F32 alphaBanUpper)
{
   S32 side, index=0, val;
   U32 numPoints;
   Point3F renderPoints[8];
   Point2F texCoords[8];

   GFX->setStateBlock(mSkyBoxSB);

   PrimBuild::color4f( 1, 1, 1, 1 );

   if(!mSkyTexturesOn || !smSkyOn)
   {
      GFX->setTexture(0, NULL);
      PrimBuild::color3i(U8(mRealSkyColor.red), U8(mRealSkyColor.green), U8(mRealSkyColor.blue));
   }

   for(side = 0; side < ((mRenderBoxBottom) ? 6 : 5); ++side)
   {
      if((lowerBanHeight != mSpherePt.z  || (side == 4 && alphaBanUpper < 1.0f)) && mSkyHandle[side])
      {         
         if(!mSkyTexturesOn || !smSkyOn)
         {
            GFX->setTexture(0, NULL);
            PrimBuild::color3i(U8(mRealSkyColor.red), U8(mRealSkyColor.green), U8(mRealSkyColor.blue));
         }
         else
         {
            GFX->setTexture(0, mSkyHandle[side]);
         }

         // If it's one of the sides...
         if(side < 4)
         {
            numPoints = 4;
            setRenderPoints(renderPoints, index);

            if(!mNoRenderBans)
               sgUtil_clipToPlane(renderPoints, numPoints, PlaneF(0.0f, 0.0f, 1.0f, -lowerBanHeight));
            // Need this assert since above method can change numPoints
            AssertFatal(sizeof(Point3F) * numPoints <= sizeof(renderPoints), "Exceeding size of renderPoints array");

            if(numPoints)
            {
               calcTexCoords(texCoords, renderPoints, index, lowerBanHeight);
               
               GFX->setupGenericShaders( GFXDevice::GSModColorTexture );

               PrimBuild::begin(GFXTriangleFan, numPoints);
               for (S32 p = 0; p < numPoints; p++)
               {
                  PrimBuild::texCoord2f(texCoords[p].x,    texCoords[p].y);
                  PrimBuild::vertex3f(  renderPoints[p].x, renderPoints[p].y, renderPoints[p].z);
               }
               PrimBuild::end();

            }
            index++;
         }
         else
         {
            index = 3;
            val = -1;
            if(side == 5)
            {
               index = 5;
               val = 1;
            }

            GFX->setupGenericShaders( GFXDevice::GSModColorTexture );

            PrimBuild::begin(GFXTriangleFan, 4);
               PrimBuild::texCoord2f(mTexCoord[0].x, mTexCoord[0].y);
               PrimBuild::vertex3f(mPoints[index].x, mPoints[index].y, mPoints[index].z);
               PrimBuild::texCoord2f(mTexCoord[1].x, mTexCoord[1].y);
               PrimBuild::vertex3f(mPoints[index+(1*val)].x, mPoints[index+(1*val)].y, mPoints[index+(1*val)].z);
               PrimBuild::texCoord2f(mTexCoord[2].x, mTexCoord[2].y);
               PrimBuild::vertex3f(mPoints[index+(2*val)].x, mPoints[index+(2*val)].y, mPoints[index+(2*val)].z);
               PrimBuild::texCoord2f(mTexCoord[3].x, mTexCoord[3].y);
               PrimBuild::vertex3f(mPoints[index+(3*val)].x, mPoints[index+(3*val)].y, mPoints[index+(3*val)].z);
            PrimBuild::end();
         }

      }
   }
}

//---------------------------------------------------------------------------
void Sky::calcBans(F32 *banHeights, Point3F banPoints[][MAX_BAN_POINTS], Point3F *cornerPoints)
{
   F32 incRad = RAD / F32(FOG_BAN_DETAIL*2);
   MatrixF ban;
   Point4F point;
   S32 index, x;
   F32 height = banHeights[0];

   F32 value = banHeights[0] / mSkyBoxPt.z;
   F32 mulVal = -(mSqrt(1-(value*value))); // lowerBan Multiple
   index=0;

   // Calculates the upper and lower bans
   for(x=0; x < 2; ++x)
   {
      for(F32 angle=0.0f; angle <= RAD+incRad ; angle+=incRad)
      {
         ban.set(Point3F(0.0f, 0.0f, angle));
         point.set(mulVal*mSkyBoxPt.x,0.0f,0.0f,1.0f);
         ban.mul(point);
         banPoints[x][index].set(point.x,point.y,height);
         index++;
      }
      height = banHeights[1];
      value  = banHeights[1] / mSkyBoxPt.x;
      value = mClampF( value, 0.0, 1.0 );
      mulVal = -(mSqrt(1.0-(value*value))); // upperBan Multiple
      index = 0;
   }

   // Calculates the filler points needed between the lower ban and the clipping plane
   index = 2;
   cornerPoints[0].set(mPoints[3].x, mPoints[3].y, banHeights[0]-1);
   cornerPoints[1].set(mPoints[3].x, 0.0f, banHeights[0]-1);

   for(x = 0; x < (FOG_BAN_DETAIL/2.0f) + 1.0f; ++x)
      cornerPoints[index++].set(banPoints[0][x].x, banPoints[0][x].y, banPoints[0][x].z);
   cornerPoints[index].set(0.0f, mPoints[3].y, banHeights[0]-1 );
}

//---------------------------------------------------------------------------
void Sky::renderBans(const F32 *alphaBan, const F32 *banHeights, const Point3F banPoints[][MAX_BAN_POINTS], const Point3F *cornerPoints, const ColorI& fogColor)
{
   S32 side, x, index = 0;
   F32 angle;
   U8 UalphaIn = U8(alphaBan[1]*255);
   U8 UalphaOut = U8(alphaBan[0]*255);

   GFX->setStateBlock(mRenderBansSB);

   //Renders the side bans
   if(banHeights[0] < mSpherePt.z)
   {
      GFX->setupGenericShaders( GFXDevice::GSColor );
      PrimBuild::begin(GFXTriangleStrip, 2*(FOG_BAN_DETAIL*2+1));
         for(x=0;x<FOG_BAN_DETAIL*2+1;++x)
         {
            PrimBuild::color4i(U8(fogColor.red), U8(fogColor.green), U8(fogColor.blue), 255);
            PrimBuild::vertex3f(banPoints[0][index].x,banPoints[0][index].y,banPoints[0][index].z);

            PrimBuild::color4i(U8(fogColor.red), U8(fogColor.green), U8(fogColor.blue), UalphaOut);
            PrimBuild::vertex3f(banPoints[1][index].x,banPoints[1][index].y,banPoints[1][index].z);
            ++index;
         }
      PrimBuild::end();
   }

   //Renders the top ban
   GFX->setupGenericShaders( GFXDevice::GSColor );
   PrimBuild::begin(GFXTriangleFan, 2*(FOG_BAN_DETAIL*2+1));

      PrimBuild::color4i(U8(fogColor.red), U8(fogColor.green), U8(fogColor.blue), UalphaIn);
      PrimBuild::vertex3f(mTopCenterPt.x, mTopCenterPt.y, mTopCenterPt.z);

      for(x=0;x<FOG_BAN_DETAIL*2+1;++x)
      {
         PrimBuild::color4i(U8(fogColor.red), U8(fogColor.green), U8(fogColor.blue), UalphaOut);
         PrimBuild::vertex3f(banPoints[1][x].x, banPoints[1][x].y, banPoints[1][x].z);
      }

   PrimBuild::end();

   GFX->pushWorldMatrix();

   angle = 0.0f;

   //Renders the filler
   for(side=0;side<4;++side)
   {
      // Rotate stuff
      AngAxisF rotAAF( Point3F(0,0,1), angle);
      MatrixF m;
      rotAAF.setMatrix(&m);
      GFX->multWorld(m);

      GFX->setupGenericShaders( GFXDevice::GSColor );

      PrimBuild::begin(GFXTriangleFan, FOG_BAN_DETAIL);
         for(x=0;x<FOG_BAN_DETAIL;++x)
         {
            PrimBuild::color4i(U8(fogColor.red), U8(fogColor.green), U8(fogColor.blue), 255);
            PrimBuild::vertex3f(cornerPoints[x].x, cornerPoints[x].y, cornerPoints[x].z);
         }
      PrimBuild::end();

      angle += M_PI_F/2.0f; // 90 degrees
   }

   GFX->popWorldMatrix();
}

//---------------------------------------------------------------------------
void Sky::startStorm()
{
   mStormCloudsOn = true;
   Cloud::startStorm(mStormCloudData.state);
   for(int i = 0; i < mNumCloudLayers; ++i)
      mCloudLayer[i].calcStorm(mStormCloudData.speed, mStormCloudData.fadeSpeed);
}

void Sky::stormCloudsShow(bool show)
{
   mStormCloudsOn = show;
   setMaskBits(StormCloudsOnMask);
}

//---------------------------------------------------------------------------
// Load vertex buffer points
//---------------------------------------------------------------------------
void Sky::loadVBPoints()
{
   mSkyVB.set(GFX, 24, GFXBufferTypeStatic );
   mSkyVB.lock();

   Point3F points[8];

   #define fillPoints( PointIndex, x, y, z ){\
   points[PointIndex].set( x, y, z ); }

   #define fillVerts( PointIndex, SkyIndex, TU, TV ){\
   dMemcpy( &mSkyVB[SkyIndex], points[PointIndex], sizeof(Point3F) );\
   mSkyVB[SkyIndex].color.set( 255, 255, 255, 255 );\
   mSkyVB[SkyIndex].texCoord.x = TU;\
   mSkyVB[SkyIndex].texCoord.y = TV;}

   fillPoints( 0, -1.0, -1.0,  1.0 );
   fillPoints( 1,  1.0, -1.0,  1.0 );
   fillPoints( 2,  1.0,  1.0,  1.0 );
   fillPoints( 3, -1.0,  1.0,  1.0 );
   fillPoints( 4, -1.0, -1.0, -1.0 );
   fillPoints( 5,  1.0, -1.0, -1.0 );
   fillPoints( 6,  1.0,  1.0, -1.0 );
   fillPoints( 7, -1.0,  1.0, -1.0 );


   fillVerts( 0, 0, 0.0, 1.0 );
   fillVerts( 1, 1, 1.0, 1.0 );
   fillVerts( 2, 2, 1.0, 0.0 );
   fillVerts( 3, 3, 0.0, 0.0 );

   fillVerts( 4, 4, 0.0, 0.0 );
   fillVerts( 5, 5, 1.0, 0.0 );
   fillVerts( 6, 6, 1.0, 1.0 );
   fillVerts( 7, 7, 0.0, 1.0 );

   fillVerts( 0, 8,  0.0, 0.0 );
   fillVerts( 1, 9,  1.0, 0.0 );
   fillVerts( 5, 10, 1.0, 1.0 );
   fillVerts( 4, 11, 0.0, 1.0 );

   fillVerts( 2, 12, 0.0, 0.0 );
   fillVerts( 3, 13, 1.0, 0.0 );
   fillVerts( 7, 14, 1.0, 1.0 );
   fillVerts( 6, 15, 0.0, 1.0 );

   fillVerts( 3, 16, 0.0, 0.0 );
   fillVerts( 0, 17, 1.0, 0.0 );
   fillVerts( 4, 18, 1.0, 1.0 );
   fillVerts( 7, 19, 0.0, 1.0 );

   fillVerts( 1, 20, 0.0, 0.0 );
   fillVerts( 2, 21, 1.0, 0.0 );
   fillVerts( 6, 22, 1.0, 1.0 );
   fillVerts( 5, 23, 0.0, 1.0 );

   mSkyVB.unlock();
}

//---------------------------------------------------------------------------
void Sky::calcPoints()
{
   S32 x, xval = 1, yval = -1;
   F32 textureDim;

   F32 visDisMod = 1000.0f;
   if(gClientSceneGraph)
      visDisMod = gClientSceneGraph->getVisibleDistance();
   mRadius = visDisMod * 0.20f;

   Cloud::setRadius(mRadius);

   Point3F tpt(1,1,1);
   tpt.normalize(mRadius);

   mPoints[0] = mPoints[4] = Point3F(-tpt.x, -tpt.y,  tpt.z);
   mPoints[5] = mPoints[9] = Point3F(-tpt.x, -tpt.y, -tpt.z);

   for(x = 1; x < 4; ++x)
   {
      mPoints[x]   = Point3F(tpt.x * xval, tpt.y * yval,  tpt.z);
      mPoints[x+5] = Point3F(tpt.x * xval, tpt.y * yval, -tpt.z);

      if(yval > 0 && xval > 0)
         xval *= -1;
      if(yval < 0)
         yval *= -1;
   }

   textureDim = 512.0f;
   if(mSkyHandle[0])
      textureDim = (F32)mSkyHandle[0].getWidth();

   mTexCoord[0].set( 0.f, 0.f );
   mTexCoord[1].set( 1.f, 0.f );
   mTexCoord[2].set( 1.f, 1.f );
   mTexCoord[3].set( 0.f, 1.f );

   for(U32 i = 0; i < 4 ; i++)
   {
      mTexCoord[i] *= (textureDim-1.0f)/textureDim;
      mTexCoord[i] += Point2F(0.5 / textureDim, 0.5 / textureDim);
   }

   mSpherePt = mSkyBoxPt = mPoints[1];
   mSpherePt.set(mSpherePt.x,0.0f,mSpherePt.z);
   mSpherePt.normalize(mSkyBoxPt.x);
   mTopCenterPt.set(0.0f,0.0f,mSkyBoxPt.z);
}

//---------------------------------------------------------------------------

bool Sky::loadDml()
{
   // Reset cloud layers.
   mNumCloudLayers = 0;

   FileStream  *stream = FileStream::createAndOpen( mMaterialListName, Torque::FS::File::Read );

   if (stream == NULL)
   {
		Con::errorf("Sky material list is missing: %s", mMaterialListName.c_str());
      return false;
   }

   mMaterialList.read(*stream);
   stream->close();
   
   delete stream;

   const Torque::Path  thePath( mMaterialListName );

   if(!mMaterialList.load(thePath.getPath()))
   {
      Con::errorf("Sky material list failed to load properly: %s", mMaterialListName.c_str());
      return false;
   }

   // Finally, assign our various texture handles.
   for(S32 x = 0; x < 6; ++x)
      mSkyHandle[x] = mMaterialList.getMaterial(x);

   for(S32 x = 0; x < mMaterialList.size() - CloudMaterialOffset; ++x, ++mNumCloudLayers)
      mCloudLayer[x].setTexture(mMaterialList.getMaterial(x + CloudMaterialOffset));

   if(mNumCloudLayers>3)
      Con::warnf("Sky::loadDml - got more than 3 cloud layers, may not be able to control all the layers properly!");

   return true;
}

//---------------------------------------------------------------------------
void Sky::stormCloudsOn(S32 state, F32 time)
{
   mStormCloudData.state = (state) ? comingIn : goingOut;
   mStormCloudData.time = time;

   setMaskBits(StormCloudMask);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Cloud Code
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
Cloud::Cloud()
{
   mDown=5;
   mOver=1;
   mBaseOffset.set(0, 0);
   mTextureScale.set(1, 1);
   mCenterHeight=0.5f;
   mInnerHeight=0.45f;
   mEdgeHeight=0.4f;
   mLastTime = 0;
   mOffset=0;
   mSpeed.set(1,1);
   mGStormData.currentCloud = MAX_NUM_LAYERS;
   mGStormData.fadeSpeed = 0.0f;
   mGStormData.StormOn = false;
   mGStormData.stormState = isDone;
   for(int i = 0; i < 25; ++i)
      stormAlpha[i] = 1.0f;
   mRadius = 1.0f;
}

//---------------------------------------------------------------------------
Cloud::~Cloud()
{
}

//---------------------------------------------------------------------------
void Cloud::updateCoord()
{
   mBaseOffset += mSpeed*mOffset;
   if(mSpeed.x < 0)
      mBaseOffset.x -= mCeil(mBaseOffset.x);
   else
      mBaseOffset.x -= mFloor(mBaseOffset.x);
   if(mSpeed.y < 0)
      mBaseOffset.y -= mCeil(mBaseOffset.y);
   else
      mBaseOffset.y -= mFloor(mBaseOffset.y);
}

//---------------------------------------------------------------------------
void Cloud::setHeights(F32 cHeight, F32 iHeight, F32 eHeight)
{
   mCenterHeight = cHeight;
   mInnerHeight  = iHeight;
   mEdgeHeight   = eHeight;
}

//---------------------------------------------------------------------------

void Cloud::setTexture(GFXTexHandle textHand)
{
   mCloudHandle = textHand;
}

//---------------------------------------------------------------------------
void Cloud::setSpeed(const Point2F &speed)
{
   mSpeed = speed;
}

//---------------------------------------------------------------------------
void Cloud::setPoints()
{
   S32 x, y;
   F32 xyDiff = mRadius/2;
   F32 cDis  = mRadius*mCenterHeight,
       upDis = mRadius*mInnerHeight,
       edgeZ = mRadius*mEdgeHeight;

   // We're dealing with a hemisphere so calculate some heights.
   F32 zValue[25] = {
                        edgeZ,   edgeZ,   edgeZ,   edgeZ,   edgeZ,
                        edgeZ,   upDis,   upDis,   upDis,   edgeZ,
                        edgeZ,   upDis,   cDis,    upDis,   edgeZ,
                        edgeZ,   upDis,   upDis,   upDis,   edgeZ,
                        edgeZ,   edgeZ,   edgeZ,   edgeZ,   edgeZ
                     };

   for(y = 0; y < 5; ++y)
      for(x = 0; x < 5; ++x)
         mPoints[y*5+x].set(-mRadius+(xyDiff*x),mRadius - (xyDiff*y),zValue[y*5+x]);

   // 0, 4, 20, 24 are the four corners of the grid...
   // the goal here is to make the cloud layer more "spherical"?

   /*Point3F vec = (mPoints[5]  + ((mPoints[1]  - mPoints[5])  * 0.5f)) - mPoints[6];
   mPoints[0] =   mPoints[6]  + (vec * 2.0f);

   vec =         (mPoints[9]  + ((mPoints[3]  - mPoints[9])  * 0.5f)) - mPoints[8];
   mPoints[4] =   mPoints[8]  + (vec * 2.0f);

   vec =         (mPoints[21] + ((mPoints[15] - mPoints[21]) * 0.5f)) - mPoints[16];
   mPoints[20] =  mPoints[16] + (vec * 2.0f);

   vec =         (mPoints[23] + ((mPoints[19] - mPoints[23]) * 0.5f)) - mPoints[18];
   mPoints[24] =  mPoints[18] + (vec * 2.0f); */

   calcAlpha();
}

//---------------------------------------------------------------------------
void Cloud::calcAlpha()
{
   for(S32 i = 0; i < 25; ++i)
   {
      mAlpha[i] = 1.3f - ((mPoints[i] - Point3F(0, 0, mPoints[i].z)).len())/mRadius;
      if(mAlpha[i] < 0.4f)
         mAlpha[i]=0.0f;
      else if(mAlpha[i] > 0.8f)
         mAlpha[i] = 1.0f;
   }
}

//---------------------------------------------------------------------------
void Cloud::render(U32 currentTime, U32 cloudLayer, bool outlineOn, S32 numLayers, PlaneF* planes)
{
//  if(cloudLayer != Con::getIntVariable("onlyCloudLayer", 2))
//      return;

   mGStormData.numCloudLayers = numLayers;
   mOffset = 1.0f;
   U32 numPoints;
   Point3F renderPoints[128];
   Point2F renderTexPoints[128];
   F32 renderAlpha[128];
   F32 renderSAlpha[128];

   if(mLastTime != 0)
      mOffset = (currentTime - mLastTime)/32.0f;
   mLastTime=currentTime;

   if(!mCloudHandle || (mGStormData.StormOn && mGStormData.currentCloud < cloudLayer))
      return;

   S32 start=0, i, j, k;
   updateCoord();
   for(S32 x = 0; x < 5; x++)
      for(S32 y = 0; y < 5; y++)
         mTexCoords[y * 5 + x].set ( x * mTextureScale.x + mBaseOffset.x,
                                     y * mTextureScale.y + mBaseOffset.y);

   if(mGStormData.StormOn && mGStormData.currentCloud == cloudLayer)
      updateStorm();

   if(!outlineOn)
   {
      if ( mCloudSB.isNull() )
      {
         GFXStateBlockDesc clouddesc;
         clouddesc.samplersDefined = true;
         clouddesc.samplers[0] = GFXSamplerStateDesc::getWrapLinear();
         clouddesc.zDefined = true;
         clouddesc.zEnable = false;
         clouddesc.zWriteEnable = false;
         clouddesc.blendDefined = true;
         clouddesc.blendEnable = true;
         clouddesc.blendSrc = GFXBlendSrcAlpha;
         clouddesc.blendDest = GFXBlendInvSrcAlpha;
         mCloudSB = GFX->createStateBlock(clouddesc);
      }
      GFX->setStateBlock(mCloudSB);
      GFX->setTexture(0,mCloudHandle);
   }

   for(i = 0; i < 4; ++i)
   {
      start = i * 5;
      for(j = 0; j < 4; ++j )
      {
         numPoints = 4;
         setRenderPoints(renderPoints, renderTexPoints, renderAlpha, renderSAlpha, start);

         for(S32 i = 0; i < 4; ++i)
            clipToPlane(renderPoints, renderTexPoints, renderAlpha, renderSAlpha,
                        numPoints, planes[i]);

         if(numPoints)
         {
            GFX->setupGenericShaders( GFXDevice::GSModColorTexture );
            PrimBuild::begin(GFXTriangleFan, numPoints);

            for(k = 0; k < numPoints; ++k)
            {
               PrimBuild::color4f   (1.0,1.0,1.0, renderAlpha[k]*renderSAlpha[k]);

               PrimBuild::texCoord2f(renderTexPoints[k].x, renderTexPoints[k].y);
               PrimBuild::vertex3f  (renderPoints[k].x,    renderPoints[k].y,     renderPoints[k].z);
            }

            PrimBuild::end();
         }

         ++start;
      }
   }
}

void Cloud::setRenderPoints(Point3F* renderPoints, Point2F* renderTexPoints,
                            F32* renderAlpha, F32* renderSAlpha, S32 index)
{
   S32 offset[4] = {0,5,6,1};
   for(S32 x = 0; x < 4; ++x)
   {
      renderPoints[x].set(
         mPoints[index+offset[x]].x,
         mPoints[index+offset[x]].y,
         mPoints[index+offset[x]].z
      );
      renderTexPoints[x].set(
         mTexCoords[index+offset[x]].x,
         mTexCoords[index+offset[x]].y
      );

      renderAlpha[x]  = mAlpha    [index+offset[x]];
      renderSAlpha[x] = stormAlpha[index+offset[x]];
   }
}


//---------------------------------------------------------------------------
void Cloud::setTextPer(F32 cloudTextPer)
{
   mTextureScale.set(cloudTextPer / 4.0, cloudTextPer / 4.0);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//    Storm Code
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void Cloud::updateStorm()
{
   if(!mGStormData.FadeOut && !mGStormData.FadeIn) {
      alphaCenter += (stormUpdate * mOffset);
      F32 update, center;
      if(mGStormData.stormDir == 'x') {
         update = stormUpdate.x;
         center = alphaCenter.x;
      }
      else {
         update = stormUpdate.y;
         center = alphaCenter.y;
      }

      if(mGStormData.stormState == comingIn) {
         if((update > 0 && center > 0) || (update < 0 && center < 0))
            mGStormData.FadeIn = true;
      }
      else
         if((update > 0 && center > mRadius*2) || (update < 0 && center < -mRadius*2)) {
//            Con::printf("Cloud %d is done.", mGStormData.currentCloud);
            mGStormData.StormOn = --mGStormData.currentCloud >= 0;
            if(mGStormData.StormOn) {
               mGStormData.FadeOut = true;
               return;
            }
         }
   }
   calcStormAlpha();
}

//---------------------------------------------------------------------------
void Cloud::calcStormAlpha()
{
   if(mGStormData.FadeIn)
   {
      bool done = true;
      for(int i = 0; i < 25; ++i)
      {
         stormAlpha[i] += (mGStormData.fadeSpeed * mOffset);
         if(stormAlpha[i] >= 1.0f)
            stormAlpha[i] = 1.0f;
         else
            done = false;
      }
      if(done)
      {
//         Con::printf("Cloud %d is done.", mGStormData.currentCloud);
         mGStormData.StormOn = ++mGStormData.currentCloud < mGStormData.numCloudLayers;
         mGStormData.FadeIn = false;
      }
   }
   else if(mGStormData.FadeOut)
   {
      bool done = true;
      for(int i = 0; i < 25; ++i)
      {
         stormAlpha[i] -= (mGStormData.fadeSpeed * mOffset);
         if(stormAlpha[i] <= mAlphaSave[i])
            stormAlpha[i] = mAlphaSave[i];
         else
            done = false;
      }
      if(done)
         mGStormData.FadeOut = false;
   }
   else
      for(int i = 0; i < 25; ++i)
      {
         stormAlpha[i] = 1.0f -((Point3F(mPoints[i].x-alphaCenter.x, mPoints[i].y-alphaCenter.y, mPoints[i].z).len())/mRadius);
         if(stormAlpha[i] < 0.0f)
            stormAlpha[i]=0.0f;
         else if(stormAlpha[i] > 1.0f)
            stormAlpha[i] = 1.0f;
      }
}

//---------------------------------------------------------------------------
void Cloud::calcStorm(F32 speed, F32 fadeSpeed)
{
   F32 tempX, tempY;
   F32 windSlop = 0.0f;

   if(mSpeed.x != 0)
      windSlop = mSpeed.y/mSpeed.x;

   tempX = (mSpeed.x < 0) ? -mSpeed.x : mSpeed.x;
   tempY = (mSpeed.y < 0) ? -mSpeed.y : mSpeed.y;

   if(tempX >= tempY)
   {
      alphaCenter.x =(mSpeed.x < 0) ? mRadius * -2 : mRadius * 2;
      alphaCenter.y = windSlop*alphaCenter.x;

      stormUpdate.x = alphaCenter.x > 0.0f ? -speed : speed;
      stormUpdate.y = alphaCenter.y > 0.0f ? -speed * windSlop : speed * windSlop;
      mGStormData.stormDir = 'x';
   }
   else
   {
      alphaCenter.y = (mSpeed.y < 0) ? mRadius * 2 : mRadius * -2;
      alphaCenter.x = windSlop * alphaCenter.y;

/*      if(windSlop != 0)
         alphaCenter.x = (1/windSlop)*alphaCenter.y;
      else
         alphaCenter.x = 0.0f;
*/
      stormUpdate.y = alphaCenter.y > 0.0f ? -speed : speed;
      stormUpdate.x = alphaCenter.x > 0.0f ? -speed * (1/windSlop) : speed * (1/windSlop);

      mGStormData.stormDir = 'y';
   }

   mGStormData.fadeSpeed = fadeSpeed;

   for(int i = 0; i < 25; ++i)
   {
      mAlphaSave[i] = 1.0f - (mPoints[i].len()/mRadius);
      if(mAlphaSave[i] < 0.0f)
         mAlphaSave[i]=0.0f;
      else if(mAlphaSave[i] > 1.0f)
         mAlphaSave[i] = 1.0f;
   }
   if(mGStormData.stormState == goingOut)
      alphaCenter.set(0.0f, 0.0f);
}

//---------------------------------------------------------------------------
void Cloud::startStorm(SkyState state)
{
   mGStormData.StormOn = true;
   mGStormData.stormState = state;
   if(state == goingOut)
   {
      mGStormData.FadeOut= true;
      mGStormData.FadeIn = false;
      mGStormData.currentCloud = mGStormData.numCloudLayers - 1;
   }
   else
   {
      mGStormData.FadeIn = false;
      mGStormData.FadeOut= false;
      mGStormData.currentCloud = 0;
   }
}

void Cloud::clipToPlane(Point3F* points, Point2F* texPoints, F32* alphaPoints,
                        F32* sAlphaPoints, U32& rNumPoints, const PlaneF& rPlane)
{
   S32 start = -1;
   for (U32 i = 0; i < rNumPoints; i++) 
   {
      if (rPlane.whichSide(points[i]) == PlaneF::Front) 
      {
         start = i;
         break;
      }
   }

   // Nothing was in front of the plane...
   if (start == -1) 
   {
      rNumPoints = 0;
      return;
   }

   U32     numFinalPoints = 0;
   Point3F finalPoints[128];
   Point2F finalTexPoints[128];
   F32     finalAlpha[128];
   F32     finalSAlpha[128];

   U32 baseStart = start;
   U32 end       = (start + 1) % rNumPoints;

   while (end != baseStart)
   {
      const Point3F& rStartPoint = points[start];
      const Point3F& rEndPoint   = points[end];

      const Point2F& rStartTexPoint = texPoints[start];
      const Point2F& rEndTexPoint   = texPoints[end];

      PlaneF::Side fSide = rPlane.whichSide(rStartPoint);
      PlaneF::Side eSide = rPlane.whichSide(rEndPoint);

      S32 code = fSide * 3 + eSide;
      switch (code) {
        case 4:   // f f
        case 3:   // f o
        case 1:   // o f
        case 0:   // o o
         // No Clipping required

         //Alpha
         finalAlpha[numFinalPoints] = alphaPoints[start];
         finalSAlpha[numFinalPoints] = sAlphaPoints[start];

         //Points
         finalPoints[numFinalPoints] = points[start];
         finalTexPoints[numFinalPoints++] = texPoints[start];

         start = end;
         end   = (end + 1) % rNumPoints;
         break;


        case 2: { // f b
            // In this case, we emit the front point, Insert the intersection,
            //  and advancing to point to first point that is in front or on...

            //Alpha
            finalAlpha[numFinalPoints] = alphaPoints[start];
            finalSAlpha[numFinalPoints] = sAlphaPoints[start];

            //Points
            finalPoints[numFinalPoints] = points[start];
            finalTexPoints[numFinalPoints++] = texPoints[start];

            Point3F vector = rEndPoint - rStartPoint;
            F32 t        = -(rPlane.distToPlane(rStartPoint) / mDot(rPlane, vector));

            //Alpha
            finalAlpha[numFinalPoints] = alphaPoints[start]+ ((alphaPoints[end] - alphaPoints[start]) * t);
            finalSAlpha[numFinalPoints] = sAlphaPoints[start]+ ((sAlphaPoints[end] - sAlphaPoints[start]) * t);

            //Polygon Points
            Point3F intersection = rStartPoint + (vector * t);
            finalPoints[numFinalPoints] = intersection;

            //Texture Points
            Point2F texVec = rEndTexPoint - rStartTexPoint;

            Point2F texIntersection = rStartTexPoint + (texVec * t);
            finalTexPoints[numFinalPoints++] = texIntersection;

            U32 endSeek = (end + 1) % rNumPoints;
            while (rPlane.whichSide(points[endSeek]) == PlaneF::Back)
               endSeek = (endSeek + 1) % rNumPoints;

            end   = endSeek;
            start = (end + (rNumPoints - 1)) % rNumPoints;

            const Point3F& rNewStartPoint = points[start];
            const Point3F& rNewEndPoint   = points[end];

            const Point2F& rNewStartTexPoint = texPoints[start];
            const Point2F& rNewEndTexPoint   = texPoints[end];

            vector = rNewEndPoint - rNewStartPoint;
            t = -(rPlane.distToPlane(rNewStartPoint) / mDot(rPlane, vector));

            //Alpha
            alphaPoints[start] = alphaPoints[start]+ ((alphaPoints[end] - alphaPoints[start]) * t);
            sAlphaPoints[start] = sAlphaPoints[start]+ ((sAlphaPoints[end] - sAlphaPoints[start]) * t);

            //Polygon Points
            intersection = rNewStartPoint + (vector * t);
            points[start] = intersection;

            //Texture Points
            texVec = rNewEndTexPoint - rNewStartTexPoint;

            texIntersection = rNewStartTexPoint + (texVec * t);
            texPoints[start] = texIntersection;
         }
         break;

        case -1: {// o b
            // In this case, we emit the front point, and advance to point to first
            //  point that is in front or on...
            //

            //Alpha
            finalAlpha[numFinalPoints] = alphaPoints[start];
            finalSAlpha[numFinalPoints] = sAlphaPoints[start];

            //Points
            finalPoints[numFinalPoints] = points[start];
            finalTexPoints[numFinalPoints++] = texPoints[start];

            U32 endSeek = (end + 1) % rNumPoints;
            while (rPlane.whichSide(points[endSeek]) == PlaneF::Back)
               endSeek = (endSeek + 1) % rNumPoints;

            end   = endSeek;
            start = (end + (rNumPoints - 1)) % rNumPoints;

            const Point3F& rNewStartPoint = points[start];
            const Point3F& rNewEndPoint   = points[end];

            const Point2F& rNewStartTexPoint = texPoints[start];
            const Point2F& rNewEndTexPoint   = texPoints[end];

            Point3F vector = rNewEndPoint - rNewStartPoint;
            F32 t        = -(rPlane.distToPlane(rNewStartPoint) / mDot(rPlane, vector));

            //Alpha
            alphaPoints[start]  = alphaPoints[start]  + ((alphaPoints[end]  - alphaPoints[start]) * t);
            sAlphaPoints[start] = sAlphaPoints[start] + ((sAlphaPoints[end] - sAlphaPoints[start]) * t);

            //Polygon Points
            Point3F intersection = rNewStartPoint + (vector * t);
            points[start] = intersection;

            //Texture Points
            Point2F texVec = rNewEndTexPoint - rNewStartTexPoint;

            Point2F texIntersection = rNewStartTexPoint + (texVec * t);
            texPoints[start] = texIntersection;
         }
         break;

        case -2:  // b f
        case -3:  // b o
        case -4:  // b b
         // In the algorithm used here, this should never happen...
         AssertISV(false, "SGUtil::clipToPlane: error in polygon clipper");
         break;

        default:
         AssertFatal(false, "SGUtil::clipToPlane: bad outcode");
         break;
      }
   }

   // Emit the last point.

   //Alpha
   finalAlpha[numFinalPoints] = alphaPoints[start];
   finalSAlpha[numFinalPoints] = sAlphaPoints[start];

   //Points
   finalPoints[numFinalPoints] = points[start];
   finalTexPoints[numFinalPoints++] = texPoints[start];
   AssertFatal(numFinalPoints >= 3, avar("Error, this shouldn't happen!  Invalid winding in clipToPlane: %d", numFinalPoints));

   // Copy the new rWinding, and we're set!

   //Alpha
   dMemcpy(alphaPoints, finalAlpha, numFinalPoints * sizeof(F32));
   dMemcpy(sAlphaPoints, finalSAlpha, numFinalPoints * sizeof(F32));

   //Points
   dMemcpy(points, finalPoints, numFinalPoints * sizeof(Point3F));
   dMemcpy(texPoints, finalTexPoints, numFinalPoints * sizeof(Point2F));

   rNumPoints = numFinalPoints;
   AssertISV(rNumPoints <= 128, "MaxWindingPoints exceeded in scenegraph.  Fatal error.");
}
