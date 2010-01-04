//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/debris.h"

#include "core/stream/bitStream.h"
#include "math/mathUtils.h"
#include "console/consoleTypes.h"
#include "console/consoleObject.h"
#include "sim/netConnection.h"
#include "sceneGraph/sceneState.h"
#include "sceneGraph/sceneGraph.h"
#include "ts/tsShapeInstance.h"
#include "ts/tsPartInstance.h"
#include "T3D/fx/particleEmitter.h"
#include "T3D/fx/explosion.h"
#include "T3D/gameProcess.h"
#include "core/resourceManager.h"
#include "gfx/gfxTransformSaver.h"


const U32 csmStaticCollisionMask =  TerrainObjectType  |
                                    InteriorObjectType;

const U32 csmDynamicCollisionMask = StaticShapeObjectType;


IMPLEMENT_CO_DATABLOCK_V1(DebrisData);


DebrisData::DebrisData()
{
   dMemset( emitterList, 0, sizeof( emitterList ) );
   dMemset( emitterIDList, 0, sizeof( emitterIDList ) );

   explosion = NULL;
   explosionId = 0;

   velocity = 0.0f;
   velocityVariance = 0.0;
   elasticity = 0.3f;
   friction   = 0.2f;
   numBounces = 0;
   bounceVariance = 0;
   minSpinSpeed = maxSpinSpeed = 0.0;
   render2D = false;
   staticOnMaxBounce = false;
   explodeOnMaxBounce = false;
   snapOnMaxBounce = false;
   lifetime = 3.0f;
   lifetimeVariance = 0.0f;
   minSpinSpeed = 0.0f;
   maxSpinSpeed = 0.0f;
   textureName = NULL;
   mTypeMask |= DebrisObjectType;
   shapeName = NULL;
   fade = true;
   useRadiusMass = false;
   baseRadius = 1.0f;
   gravModifier = 1.0f;
   terminalVelocity = 0.0f;
   ignoreWater = true;
}

bool DebrisData::onAdd()
{
   if(!Parent::onAdd())
      return false;

   for( int i=0; i<DDC_NUM_EMITTERS; i++ )
   {
      if( !emitterList[i] && emitterIDList[i] != 0 )
      {
         if( Sim::findObject( emitterIDList[i], emitterList[i] ) == false)
         {
            Con::errorf( ConsoleLogEntry::General, "DebrisData::onAdd: Invalid packet, bad datablockId(emitter): 0x%x", emitterIDList[i]);
         }
      }
   }

   if (!explosion && explosionId != 0)
   {
      if (!Sim::findObject( SimObjectId( explosionId ), explosion ))
            Con::errorf( ConsoleLogEntry::General, "DebrisData::onAdd: Invalid packet, bad datablockId(particle emitter): 0x%x", explosionId);
   }

   // validate data

   if( velocityVariance > velocity )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: velocityVariance invalid", getName());
      velocityVariance = velocity;
   }
   if( friction < -10.0f || friction > 10.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: friction invalid", getName());
      friction = 0.2f;
   }
   if( elasticity < -10.0f || elasticity > 10.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: elasticity invalid", getName());
      elasticity = 0.2f;
   }
   if( lifetime < 0.0f || lifetime > 1000.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: lifetime invalid", getName());
      lifetime = 3.0f;
   }
   if( lifetimeVariance < 0.0f || lifetimeVariance > lifetime )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: lifetimeVariance invalid", getName());
      lifetimeVariance = 0.0f;
   }
   if( numBounces < 0 || numBounces > 10000 )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: numBounces invalid", getName());
      numBounces = 3;
   }
   if( bounceVariance < 0 || bounceVariance > numBounces )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: bounceVariance invalid", getName());
      bounceVariance = 0;
   }
   if( minSpinSpeed < -10000.0f || minSpinSpeed > 10000.0f || minSpinSpeed > maxSpinSpeed )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: minSpinSpeed invalid", getName());
      minSpinSpeed = maxSpinSpeed - 1.0f;
   }
   if( maxSpinSpeed < -10000.0f || maxSpinSpeed > 10000.0f )
   {
      Con::warnf(ConsoleLogEntry::General, "DebrisData(%s)::onAdd: maxSpinSpeed invalid", getName());
      maxSpinSpeed = 0.0f;
   }

   return true;
}

bool DebrisData::preload(bool server, String &errorStr)
{
   if (Parent::preload(server, errorStr) == false)
      return false;

   if( server ) return true;

   if( shapeName && shapeName[0] != '\0' && !bool(shape) )
   {
      shape = ResourceManager::get().load(shapeName);
      if( bool(shape) == false )
      {
         errorStr = String::ToString("DebrisData::load: Couldn't load shape \"%s\"", shapeName);
         return false;
      }
      else
      {
         TSShapeInstance* pDummy = new TSShapeInstance(shape, !server);
         delete pDummy;
      }

   }

   return true;
}

IMPLEMENT_CONSOLETYPE(DebrisData)
IMPLEMENT_SETDATATYPE(DebrisData)
IMPLEMENT_GETDATATYPE(DebrisData)

void DebrisData::initPersistFields()
{
   addGroup("Display");
   addField("texture",              TypeString,                  Offset(textureName,         DebrisData));
   addField("shapeFile",            TypeFilename,                Offset(shapeName,           DebrisData));
   addField("render2D",             TypeBool,                    Offset(render2D,            DebrisData));
   endGroup("Display");

   addGroup("Datablocks");
   addField("emitters",             TypeParticleEmitterDataPtr,  Offset(emitterList,         DebrisData), DDC_NUM_EMITTERS);
   addField("explosion",            TypeExplosionDataPtr,        Offset(explosion,           DebrisData));
   endGroup("Datablocks");

   addGroup("Physical Properties");
   addField("elasticity",           TypeF32,                     Offset(elasticity,          DebrisData));
   addField("friction",             TypeF32,                     Offset(friction,            DebrisData));
   addField("numBounces",           TypeS32,                     Offset(numBounces,          DebrisData));
   addField("bounceVariance",       TypeS32,                     Offset(bounceVariance,      DebrisData));
   addField("minSpinSpeed",         TypeF32,                     Offset(minSpinSpeed,        DebrisData));
   addField("maxSpinSpeed",         TypeF32,                     Offset(maxSpinSpeed,        DebrisData));
   addField("gravModifier",         TypeF32,                     Offset(gravModifier,        DebrisData));
   addField("terminalVelocity",     TypeF32,                     Offset(terminalVelocity,    DebrisData));
   addField("velocity",             TypeF32,                     Offset(velocity,            DebrisData));
   addField("velocityVariance",     TypeF32,                     Offset(velocityVariance,    DebrisData));
   addField("lifetime",             TypeF32,                     Offset(lifetime,            DebrisData));
   addField("lifetimeVariance",     TypeF32,                     Offset(lifetimeVariance,    DebrisData));
   addField("useRadiusMass",        TypeBool,                    Offset(useRadiusMass,       DebrisData));
   addField("baseRadius",           TypeF32,                     Offset(baseRadius,          DebrisData));
   endGroup("Physical Properties");

   addGroup("Behavior");
   addField("explodeOnMaxBounce",   TypeBool,                    Offset(explodeOnMaxBounce,  DebrisData));
   addField("staticOnMaxBounce",    TypeBool,                    Offset(staticOnMaxBounce,   DebrisData));
   addField("snapOnMaxBounce",      TypeBool,                    Offset(snapOnMaxBounce,     DebrisData));
   addField("fade",                 TypeBool,                    Offset(fade,                DebrisData));
   addField("ignoreWater",          TypeBool,                    Offset(ignoreWater,         DebrisData));
   endGroup("Behavior");

   Parent::initPersistFields();
}

void DebrisData::packData(BitStream* stream)
{
   Parent::packData(stream);

   stream->write(elasticity);
   stream->write(friction);
   stream->write(numBounces);
   stream->write(bounceVariance);
   stream->write(minSpinSpeed);
   stream->write(maxSpinSpeed);
   stream->write(render2D);
   stream->write(explodeOnMaxBounce);
   stream->write(staticOnMaxBounce);
   stream->write(snapOnMaxBounce);
   stream->write(lifetime);
   stream->write(lifetimeVariance);
   stream->write(velocity);
   stream->write(velocityVariance);
   stream->write(fade);
   stream->write(useRadiusMass);
   stream->write(baseRadius);
   stream->write(gravModifier);
   stream->write(terminalVelocity);
   stream->write(ignoreWater);

   stream->writeString( textureName );
   stream->writeString( shapeName );

   for( int i=0; i<DDC_NUM_EMITTERS; i++ )
   {
      if( stream->writeFlag( emitterList[i] != NULL ) )
      {
         stream->writeRangedU32( emitterList[i]->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
      }
   }

   if( stream->writeFlag( explosion ) )
   {
      stream->writeRangedU32(packed? SimObjectId(explosion):
         explosion->getId(),DataBlockObjectIdFirst,DataBlockObjectIdLast);
   }

}

void DebrisData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

   stream->read(&elasticity);
   stream->read(&friction);
   stream->read(&numBounces);
   stream->read(&bounceVariance);
   stream->read(&minSpinSpeed);
   stream->read(&maxSpinSpeed);
   stream->read(&render2D);
   stream->read(&explodeOnMaxBounce);
   stream->read(&staticOnMaxBounce);
   stream->read(&snapOnMaxBounce);
   stream->read(&lifetime);
   stream->read(&lifetimeVariance);
   stream->read(&velocity);
   stream->read(&velocityVariance);
   stream->read(&fade);
   stream->read(&useRadiusMass);
   stream->read(&baseRadius);
   stream->read(&gravModifier);
   stream->read(&terminalVelocity);
   stream->read(&ignoreWater);

   textureName = stream->readSTString();
   shapeName   = stream->readSTString();

   for( int i=0; i<DDC_NUM_EMITTERS; i++ )
   {
      if( stream->readFlag() )
      {
         emitterIDList[i] = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );
      }
   }

   if(stream->readFlag())
   {
      explosionId = (S32)stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }
   else
   {
      explosionId = 0;
   }

}


IMPLEMENT_CO_NETOBJECT_V1(Debris);

ConsoleMethod( Debris, init, bool, 4, 4, "(Point3F position, Point3F velocity)"
              "Set this piece of debris at the given position with the given velocity.")
{
   Point3F pos;
   dSscanf( argv[2], "%f %f %f", &pos.x, &pos.y, &pos.z );

   Point3F vel;
   dSscanf( argv[3], "%f %f %f", &vel.x, &vel.y, &vel.z );

   object->init( pos, vel );

   return true;
}

Debris::Debris()
{
   mTypeMask |= DebrisObjectType;

   mVelocity = Point3F( 0.0f, 0.0f, 4.0f );
   mLifetime = gRandGen.randF( 1.0f, 10.0f );
   mLastPos =  getPosition();
   mNumBounces = gRandGen.randI( 0, 1 );
   mSize = 2.0f;
   mElapsedTime = 0.0f;
   mShape = NULL;
   mPart = NULL;
   mXRotSpeed = 0.0f;
   mZRotSpeed = 0.0f;
   mInitialTrans.identity();
   mRadius = 0.2f;
   mStatic = false;

   dMemset( mEmitterList, 0, sizeof( mEmitterList ) );
}

Debris::~Debris()
{
   if( mShape )
   {
      delete mShape;
      mShape = NULL;
   }

   if( mPart )
   {
      delete mPart;
      mPart = NULL;
   }
}

void Debris::initPersistFields()
{
   addGroup("Misc");	
   addField("lifetime", TypeF32, Offset(mLifetime, Debris));
   endGroup("Misc");	
}

void Debris::init( const Point3F &position, const Point3F &velocity )
{
   setPosition( position );
   setVelocity( velocity );
}

bool Debris::onNewDataBlock( GameBaseData* dptr )
{
   mDataBlock = dynamic_cast< DebrisData* >( dptr );
   if( !mDataBlock || !Parent::onNewDataBlock( dptr ) )
      return false;

   scriptOnNewDataBlock();
   return true;

}

bool Debris::onAdd()
{
   if( !Parent::onAdd() )
   {
      return false;
   }

   // create emitters
   for( int i=0; i<DebrisData::DDC_NUM_EMITTERS; i++ )
   {
      if( mDataBlock->emitterList[i] != NULL )
      {
         ParticleEmitter * pEmitter = new ParticleEmitter;
         pEmitter->onNewDataBlock( mDataBlock->emitterList[i] );
         if( !pEmitter->registerObject() )
         {
            Con::warnf( ConsoleLogEntry::General, "Could not register emitter for particle of class: %s", mDataBlock->getName() );
            delete pEmitter;
            pEmitter = NULL;
         }
         mEmitterList[i] = pEmitter;
      }
   }

   // set particle sizes based on debris size
   F32 sizeList[ParticleData::PDC_NUM_KEYS];

   if( mEmitterList[0] )
   {
      sizeList[0] = mSize * 0.5;
      sizeList[1] = mSize;
      sizeList[2] = mSize * 1.5;

      mEmitterList[0]->setSizes( sizeList );
   }

   if( mEmitterList[1] )
   {
      sizeList[0] = 0.0;
      sizeList[1] = mSize * 0.5;
      sizeList[2] = mSize;

      mEmitterList[1]->setSizes( sizeList );
   }

   S32 bounceVar = (S32)mDataBlock->bounceVariance;
   bounceVar = gRandGen.randI( -bounceVar, bounceVar );
   mNumBounces = mDataBlock->numBounces + bounceVar;

   F32 lifeVar = (mDataBlock->lifetimeVariance * 2.0f * gRandGen.randF(-1.0,1.0)) - mDataBlock->lifetimeVariance;
   mLifetime = mDataBlock->lifetime + lifeVar;

   F32 xRotSpeed = gRandGen.randF( mDataBlock->minSpinSpeed, mDataBlock->maxSpinSpeed );
   F32 zRotSpeed = gRandGen.randF( mDataBlock->minSpinSpeed, mDataBlock->maxSpinSpeed );
   zRotSpeed *= gRandGen.randF( 0.1f, 0.5f );

   mRotAngles.set( xRotSpeed, 0.0f, zRotSpeed );

   mElasticity = mDataBlock->elasticity;
   mFriction = mDataBlock->friction;

   // Setup our bounding box
   if( mDataBlock->shape )
   {
      mObjBox = mDataBlock->shape->bounds;
   }
   else
   {
      mObjBox = Box3F(Point3F(-1, -1, -1), Point3F(1, 1, 1));
   }

   if( mDataBlock->shape )
   {
      mShape = new TSShapeInstance( mDataBlock->shape, true);
   }

   if( mPart )
   {
      // use half radius becuase we want debris to stick in ground
      mRadius = mPart->getRadius() * 0.5;
      mObjBox = mPart->getBounds();
   }

   resetWorldBox();

   mInitialTrans = getTransform();

   if( mDataBlock->velocity != 0.0 )
   {
      F32 velocity = mDataBlock->velocity + gRandGen.randF( -mDataBlock->velocityVariance, mDataBlock->velocityVariance );

      mVelocity.normalizeSafe();
      mVelocity *= velocity;
   }

   // mass calculations
   if( mDataBlock->useRadiusMass )
   {
      if( mRadius < mDataBlock->baseRadius )
      {
         mRadius = mDataBlock->baseRadius;
      }

      // linear falloff
      F32 multFactor = mDataBlock->baseRadius / mRadius;

      mElasticity *= multFactor;
      mFriction *= multFactor;
      mRotAngles *= multFactor;
   }


   // tell engine the debris exists
   gClientContainer.addObject(this);
   gClientSceneGraph->addObjectToScene(this);

   removeFromProcessList();
   gClientProcessList.addObject(this);

   NetConnection* pNC = NetConnection::getConnectionToServer();
   AssertFatal(pNC != NULL, "Error, must have a connection to the server!");
   pNC->addObject(this);

   return true;
}

void Debris::onRemove()
{
   for( int i=0; i<DebrisData::DDC_NUM_EMITTERS; i++ )
   {
      if( mEmitterList[i] )
      {
         mEmitterList[i]->deleteWhenEmpty();
         mEmitterList[i] = NULL;
      }
   }

   if( mPart )
   {
      TSShapeInstance *ss = mPart->getSourceShapeInstance();
      if( ss )
      {
         ss->decDebrisRefCount();
         if( ss->getDebrisRefCount() == 0 )
         {
            delete ss;
         }
      }
   }

   mSceneManager->removeObjectFromScene(this);
   getContainer()->removeObject(this);

   Parent::onRemove();
}

void Debris::processTick(const Move*)
{
   if (mLifetime <= 0.0)
      deleteObject();
}

void Debris::advanceTime( F32 dt )
{
   mElapsedTime += dt;

   mLifetime -= dt;
   if( mLifetime <= 0.0 )
   {
      mLifetime = 0.0;
      return;
   }

   mLastPos = getPosition();

   if( !mStatic )
   {
      rotate( dt );

      Point3F nextPos = getPosition();
      computeNewState( nextPos, mVelocity, dt );

      if( bounce( nextPos, dt ) )
      {
         --mNumBounces;
         if( mNumBounces <= 0 )
         {
            if( mDataBlock->explodeOnMaxBounce )
            {
               explode();
               mLifetime = 0.0;
            }
            if( mDataBlock->snapOnMaxBounce )
            {
               // orient debris so it's flat
               MatrixF stat = getTransform();

               Point3F dir;
               stat.getColumn( 1, &dir );
               dir.z = 0.0;

               MatrixF newTrans = MathUtils::createOrientFromDir( dir );

               // hack for shell casings to get them above ground.  Need something better - bramage
               newTrans.setPosition( getPosition() + Point3F( 0.0f, 0.0f, 0.10f ) );

               setTransform( newTrans );
            }
            if( mDataBlock->staticOnMaxBounce )
            {
               mStatic = true;
            }
         }
      }
      else
      {
         setPosition( nextPos );
      }
   }

   Point3F pos( getPosition( ) );
   updateEmitters( pos, mVelocity, (U32)(dt * 1000.0));

}

void Debris::rotate( F32 dt )
{
   MatrixF curTrans = getTransform();
   curTrans.setPosition( Point3F(0.0f, 0.0f, 0.0f) );

   Point3F curAngles = mRotAngles * dt * M_PI_F/180.0f;
   MatrixF rotMatrix( EulerF( curAngles.x, curAngles.y, curAngles.z ) );

   curTrans.mul( rotMatrix );
   curTrans.setPosition( getPosition() );
   setTransform( curTrans );
}

bool Debris::bounce( const Point3F &nextPos, F32 dt )
{
   Point3F curPos = getPosition();

   Point3F dir = nextPos - curPos;
   if( dir.magnitudeSafe() == 0.0f ) return false;
   dir.normalizeSafe();
   Point3F extent = nextPos + dir * mRadius;
   F32 totalDist = Point3F( extent - curPos ).magnitudeSafe();
   F32 moveDist = Point3F( nextPos - curPos ).magnitudeSafe();
   F32 movePercent = (moveDist / totalDist);

   RayInfo rayInfo;
   U32 collisionMask = csmStaticCollisionMask;
   if( !mDataBlock->ignoreWater )
   {
      collisionMask |= WaterObjectType;
   }

   if( getContainer()->castRay( curPos, extent, collisionMask, &rayInfo ) )
   {

      Point3F reflection = mVelocity - rayInfo.normal * (mDot( mVelocity, rayInfo.normal ) * 2.0f);
      mVelocity = reflection;

      Point3F tangent = reflection - rayInfo.normal * mDot( reflection, rayInfo.normal );
      mVelocity -= tangent * mFriction;

      Point3F velDir = mVelocity;
      velDir.normalizeSafe();

      mVelocity *= mElasticity;

      Point3F bouncePos = curPos + dir * rayInfo.t * movePercent;
      bouncePos += mVelocity * dt;

      setPosition( bouncePos );

      mRotAngles *= mElasticity;

      return true;

   }

   return false;

}

void Debris::explode()
{

   if( !mDataBlock->explosion ) return;

   Point3F explosionPos = getPosition();

   Explosion* pExplosion = new Explosion;
   pExplosion->onNewDataBlock(mDataBlock->explosion);

   MatrixF trans( true );
   trans.setPosition( getPosition() );

   pExplosion->setTransform( trans );
   pExplosion->setInitialState( explosionPos, VectorF(0,0,1), 1);
   if (!pExplosion->registerObject())
      delete pExplosion;
}

void Debris::computeNewState( Point3F &newPos, Point3F &newVel, F32 dt )
{
   // apply gravity
   Point3F force = Point3F(0, 0, -9.81 * mDataBlock->gravModifier );

   if( mDataBlock->terminalVelocity > 0.0001 )
   {
      if( newVel.magnitudeSafe() > mDataBlock->terminalVelocity )
      {
         newVel.normalizeSafe();
         newVel *= mDataBlock->terminalVelocity;
      }
      else
      {
         newVel += force * dt;
      }
   }
   else
   {
      newVel += force * dt;
   }

   newPos += newVel * dt;

}

void Debris::updateEmitters( Point3F &pos, Point3F &vel, U32 ms )
{

   Point3F axis = -vel;

   if( axis.magnitudeSafe() == 0.0 )
   {
      axis = Point3F( 0.0, 0.0, 1.0 );
   }
   axis.normalizeSafe();


   Point3F lastPos = mLastPos;

   for( int i=0; i<DebrisData::DDC_NUM_EMITTERS; i++ )
   {
      if( mEmitterList[i] )
      {
         mEmitterList[i]->emitParticles( lastPos, pos, axis, vel, ms );
      }
   }

}

bool Debris::prepRenderImage(SceneState *state, const U32 stateKey,
                             const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
   if (isLastState(state, stateKey))
      return false;
   setLastState(state, stateKey);

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   if( state->isObjectRendered(this) && (mPart || mShape) )
   {
      Point3F cameraOffset;
      mObjToWorld.getColumn(3,&cameraOffset);
      cameraOffset -= state->getDiffuseCameraPosition();
      F32 dist = cameraOffset.len();
      F32 invScale = (1.0f/getMax(getMax(mObjScale.x,mObjScale.y),mObjScale.z));

      if( mShape )
      {
         mShape->setDetailFromDistance( state, dist * invScale );
         if( mShape->getCurrentDetail() < 0 )
            return false;
      }

      if( mPart )
         mShape->setDetailFromDistance( state, dist * invScale );

      prepBatchRender( state );
   }

   return false;
}

void Debris::prepBatchRender( SceneState *state )
{
   if ( !mShape && !mPart )
      return;

   GFXTransformSaver saver;

   F32 alpha = 1.0;
   if( mDataBlock->fade )
   {
      if( mLifetime < 1.0 ) alpha = mLifetime;
   }

   Point3F cameraOffset;
   mObjToWorld.getColumn(3,&cameraOffset);
   cameraOffset -= state->getCameraPosition();
      
   // Set up our TS render state.
   TSRenderState rdata;
   rdata.setSceneState( state );

   LightManager *lm = gClientSceneGraph->getLightManager();  
   if ( !state->isShadowPass() ) 
      lm->setupLights( this, getWorldSphere() );  

   if( mShape )
   {
      MatrixF mat = getRenderTransform();
      GFX->setWorldMatrix( mat );

      rdata.setFadeOverride( alpha );
      mShape->render( rdata );
   }
   else
   {
      if (mPart->getCurrentObjectDetail() != -1)
      {
         MatrixF mat = getRenderTransform();
         GFX->setWorldMatrix( mat );
           
         rdata.setFadeOverride( alpha );
         mPart->render( rdata );
      }
   }

   render2D();
}

void Debris::render2D()
{
}

void Debris::setSize( F32 size )
{
   mSize = size;
}
