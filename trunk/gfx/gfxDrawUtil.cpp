//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gfx/gfxDrawUtil.h"

#include "core/strings/stringFunctions.h"
#include "core/strings/unicode.h"
#include "math/util/frustum.h"
#include "math/util/sphereMesh.h"
#include "collision/polyhedron.h"
#include "math/mathUtils.h"
#include "gfx/gfxFontRenderBatcher.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/primBuilder.h"


GFXDrawUtil::GFXDrawUtil( GFXDevice * d)
{
   mDevice = d;
   mBitmapModulation.set(0xFF, 0xFF, 0xFF, 0xFF);
   mTextAnchorColor.set(0xFF, 0xFF, 0xFF, 0xFF);
   mFontRenderBatcher = new FontRenderBatcher();

   _setupStateBlocks();   
}

GFXDrawUtil::~GFXDrawUtil()
{
   delete mFontRenderBatcher;
}

void GFXDrawUtil::_setupStateBlocks()
{
   // DrawBitmapStretchSR
   GFXStateBlockDesc bitmapStretchSR;
   bitmapStretchSR.setCullMode(GFXCullNone);
   bitmapStretchSR.setZReadWrite(false);
   bitmapStretchSR.setBlend(true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
   bitmapStretchSR.samplersDefined = true;

   // Linear: Create wrap SB
   bitmapStretchSR.samplers[0] = GFXSamplerStateDesc::getWrapLinear();
   mBitmapStretchWrapLinearSB = mDevice->createStateBlock(bitmapStretchSR);

   // Linear: Create clamp SB
   bitmapStretchSR.samplers[0] = GFXSamplerStateDesc::getClampLinear();
   mBitmapStretchLinearSB = mDevice->createStateBlock(bitmapStretchSR);

   // Point:
   bitmapStretchSR.samplers[0].minFilter = GFXTextureFilterPoint;
   bitmapStretchSR.samplers[0].mipFilter = GFXTextureFilterPoint;
   bitmapStretchSR.samplers[0].magFilter = GFXTextureFilterPoint;

   // Point: Create clamp SB, last created clamped so no work required here
   mBitmapStretchSB = mDevice->createStateBlock(bitmapStretchSR);

   // Point: Create wrap SB, have to do this manually because getWrapLinear doesn't
   bitmapStretchSR.samplers[0].addressModeU = GFXAddressWrap;
   bitmapStretchSR.samplers[0].addressModeV = GFXAddressWrap;
   bitmapStretchSR.samplers[0].addressModeW = GFXAddressWrap;
   mBitmapStretchWrapSB = mDevice->createStateBlock(bitmapStretchSR);

   GFXStateBlockDesc rectFill;
   rectFill.setCullMode(GFXCullNone);
   rectFill.setZReadWrite(false);
   rectFill.setBlend(true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
   mRectFillSB = mDevice->createStateBlock(rectFill);
}

//-----------------------------------------------------------------------------
// Color Modulation
//-----------------------------------------------------------------------------
void GFXDrawUtil::setBitmapModulation( const ColorI &modColor )
{
   mBitmapModulation = modColor;
}

void GFXDrawUtil::clearBitmapModulation()
{
   mBitmapModulation.set( 255, 255, 255, 255 );
}

void GFXDrawUtil::getBitmapModulation( ColorI *color )
{
   mBitmapModulation.getColor( color );
}

void GFXDrawUtil::setTextAnchorColor( const ColorI &ancColor )
{
   mTextAnchorColor = ancColor;
}

//-----------------------------------------------------------------------------
// Draw Text
//-----------------------------------------------------------------------------
U32 GFXDrawUtil::drawText( GFont *font, const Point2I &ptDraw, const UTF16 *in_string, 
                          const ColorI *colorTable, const U32 maxColorIndex, F32 rot )
{
   return drawTextN( font, ptDraw, in_string, dStrlen(in_string), colorTable, maxColorIndex, rot );
}

U32 GFXDrawUtil::drawText( GFont *font, const Point2I &ptDraw, const UTF8 *in_string, 
                          const ColorI *colorTable, const U32 maxColorIndex, F32 rot )
{
   return drawTextN( font, ptDraw, in_string, dStrlen(in_string), colorTable, maxColorIndex, rot );
}

U32 GFXDrawUtil::drawText( GFont *font, const Point2F &ptDraw, const UTF8 *in_string, const ColorI *colorTable /*= NULL*/, const U32 maxColorIndex /*= 9*/, F32 rot /*= 0.f */ )
{
   return drawText(font,Point2I((S32)ptDraw.x,(S32)ptDraw.y),in_string,colorTable,maxColorIndex,rot);
}

U32 GFXDrawUtil::drawText( GFont *font, const Point2F &ptDraw, const UTF16 *in_string, const ColorI *colorTable /*= NULL*/, const U32 maxColorIndex /*= 9*/, F32 rot /*= 0.f */ )
{
   return drawText(font,Point2I((S32)ptDraw.x,(S32)ptDraw.y),in_string,colorTable,maxColorIndex,rot);
}

U32 GFXDrawUtil::drawTextN( GFont *font, const Point2I &ptDraw, const UTF8 *in_string, U32 n,
                           const ColorI *colorTable, const U32 maxColorIndex, F32 rot )
{
   // return on zero length strings
   if( n == 0 )
      return ptDraw.x;

   // Convert to UTF16 temporarily.
   n++; // space for null terminator
   FrameTemp<UTF16> ubuf( n * sizeof(UTF16) );
   convertUTF8toUTF16(in_string, ubuf, n);

   return drawTextN( font, ptDraw, ubuf, n, colorTable, maxColorIndex, rot );
}

U32 GFXDrawUtil::drawTextN( GFont *font, const Point2I &ptDraw, const UTF16 *in_string, 
                           U32 n, const ColorI *colorTable, const U32 maxColorIndex, F32 rot )
{
   // return on zero length strings
   if( n == 0 )
      return ptDraw.x;

   // If it's over about 4000 verts we want to break it up
   if( n > 666 )
   {
      U32 left = drawTextN(font, ptDraw, in_string, 666, colorTable, maxColorIndex, rot);

      Point2I newDrawPt(left, ptDraw.y);
      const UTF16* str = (const UTF16*)in_string;

      return drawTextN(font, newDrawPt, &(str[666]), n - 666, colorTable, maxColorIndex, rot);
   }

   PROFILE_START(GFXDevice_drawTextN);

   const PlatformFont::CharInfo *tabci = NULL;

   S32 ptX = 0;

   // Queue everything for render.   
   mFontRenderBatcher->init(font, n);

   U32 i;
   UTF16 c;   
   for(i = 0, c = in_string[i]; in_string[i] && i < n; i++, c = in_string[i])
   {
      switch(c)
      {
         // We have to do a little dance here since \t = 0x9, \n = 0xa, and \r = 0xd
      case 1: case 2: case 3: case 4: case 5: case 6: case 7:
      case 11: case 12:
      case 14:
         {
            // Color code
            if (colorTable) 
            {
               static U8 remap[15] = 
               { 
                  0x0, // 0 special null terminator
                  0x0, // 1 ascii start-of-heading??
                  0x1, 
                  0x2, 
                  0x3, 
                  0x4, 
                  0x5, 
                  0x6, 
                  0x0, // 8 special backspace
                  0x0, // 9 special tab
                  0x0, // a special \n
                  0x7, 
                  0x8,
                  0x0, // a special \r
                  0x9 
               };

               U8 remapped = remap[c];

               // Ignore if the color is greater than the specified max index:
               if ( remapped <= maxColorIndex )
               {
                  const ColorI &clr = colorTable[remapped];
                  mBitmapModulation = clr;
               }
            }

            // And skip rendering this character.
            continue;
         }

         // reset color?
      case 15:
         {
            mBitmapModulation = mTextAnchorColor;

            // And skip rendering this character.
            continue;
         }

         // push color:
      case 16:
         {
            mTextAnchorColor = mBitmapModulation;

            // And skip rendering this character.
            continue;
         }

         // pop color:
      case 17:
         {
            mBitmapModulation = mTextAnchorColor;

            // And skip rendering this character.
            continue;
         }

         // Tab character
      case dT('\t'): 
         {
            if ( tabci == NULL )
               tabci = &(font->getCharInfo( dT(' ') ));

            const U32	fontTabIncrement = tabci->xIncrement * GFont::TabWidthInSpaces;

            ptX += fontTabIncrement;

            // And skip rendering this character.
            continue;
         }

         // Don't draw invalid characters.
      default:
         {
            if( !font->isValidChar( c ) ) 
               continue;
         }
      }

      // Queue char for rendering..
      mFontRenderBatcher->queueChar(c, ptX, mBitmapModulation);
   }


   mFontRenderBatcher->render(rot, Point2F((F32)ptDraw.x, (F32)ptDraw.y));

   PROFILE_END();

   return ptX - ptDraw.x;
}

U32 GFXDrawUtil::drawTextN( GFont *font, const Point2F &ptDraw, const UTF8 *in_string, U32 n, const ColorI *colorTable /*= NULL*/, const U32 maxColorIndex /*= 9*/, F32 rot /*= 0.f */ )
{
   return drawTextN(font,Point2I((S32)ptDraw.x,(S32)ptDraw.y),in_string,n,colorTable,maxColorIndex,rot);
}

U32 GFXDrawUtil::drawTextN( GFont *font, const Point2F &ptDraw, const UTF16 *in_string, U32 n, const ColorI *colorTable /*= NULL*/, const U32 maxColorIndex /*= 9*/, F32 rot /*= 0.f */ )
{
   return drawTextN(font,Point2I((S32)ptDraw.x,(S32)ptDraw.y),in_string,n,colorTable,maxColorIndex,rot);
}

//-----------------------------------------------------------------------------
// Draw Bitmaps
//-----------------------------------------------------------------------------
void GFXDrawUtil::drawBitmap( GFXTextureObject* texture, const Point2I &in_rAt, const GFXBitmapFlip in_flip, const GFXTextureFilterType filter , bool in_wrap /*= true*/ )
{
   drawBitmap(texture,Point2F((F32)in_rAt.x,(F32)in_rAt.y),in_flip,filter,in_wrap);
}

void GFXDrawUtil::drawBitmapStretch( GFXTextureObject* texture, const RectI &dstRect, const GFXBitmapFlip in_flip, const GFXTextureFilterType filter , bool in_wrap /*= true*/ )
{
   drawBitmapStretch(texture,RectF((F32)dstRect.point.x,(F32)dstRect.point.y,(F32)dstRect.extent.x,(F32)dstRect.extent.y),in_flip,filter,in_wrap);
}

void GFXDrawUtil::drawBitmapSR( GFXTextureObject* texture, const Point2I &in_rAt, const RectI &srcRect, const GFXBitmapFlip in_flip, const GFXTextureFilterType filter , bool in_wrap /*= true*/ )
{
   drawBitmapSR(texture,Point2F((F32)in_rAt.x,(F32)in_rAt.y),RectF((F32)srcRect.point.x,(F32)srcRect.point.y,(F32)srcRect.extent.x,(F32)srcRect.extent.y),in_flip,filter,in_wrap);
}

void GFXDrawUtil::drawBitmapStretchSR( GFXTextureObject *texture, const RectI &dstRect, const RectI &srcRect, const GFXBitmapFlip in_flip, const GFXTextureFilterType filter , bool in_wrap /*= true*/ ) 
{
   RectF dstRectF = RectF((F32)dstRect.point.x,(F32)dstRect.point.y,(F32)dstRect.extent.x,(F32)dstRect.extent.y);
   RectF srcRectF = RectF((F32)srcRect.point.x,(F32)srcRect.point.y,(F32)srcRect.extent.x,(F32)srcRect.extent.y);
   drawBitmapStretchSR(texture,dstRectF,srcRectF,in_flip,filter,in_wrap);
}

void GFXDrawUtil::drawBitmap( GFXTextureObject*texture, const Point2F &in_rAt, const GFXBitmapFlip in_flip /*= GFXBitmapFlip_None*/, const GFXTextureFilterType filter /*= GFXTextureFilterPoint */ , bool in_wrap /*= true*/ )
{
   AssertFatal( texture != 0, "No texture specified for drawBitmap()" );

   RectI subRegion( 0, 0, texture->mBitmapSize.x, texture->mBitmapSize.y );
   RectI stretch( in_rAt.x, in_rAt.y, texture->mBitmapSize.x, texture->mBitmapSize.y );
   drawBitmapStretchSR( texture, stretch, subRegion, in_flip, filter, in_wrap );
}

void GFXDrawUtil::drawBitmapStretch( GFXTextureObject*texture, const RectF &dstRect, const GFXBitmapFlip in_flip /*= GFXBitmapFlip_None*/, const GFXTextureFilterType filter /*= GFXTextureFilterPoint */ , bool in_wrap /*= true*/ )
{
   AssertFatal( texture != 0, "No texture specified for drawBitmapStretch()" );

   RectF subRegion( 0.f, 0.f, (F32)texture->mBitmapSize.x, (F32)texture->mBitmapSize.y );
   drawBitmapStretchSR( texture, dstRect, subRegion, in_flip, filter, in_wrap );
}

void GFXDrawUtil::drawBitmapSR( GFXTextureObject*texture, const Point2F &in_rAt, const RectF &srcRect, const GFXBitmapFlip in_flip /*= GFXBitmapFlip_None*/, const GFXTextureFilterType filter /*= GFXTextureFilterPoint */ , bool in_wrap /*= true*/ )
{
   AssertFatal( texture != 0, "No texture specified for drawBitmapSR()" );

   RectF stretch( in_rAt.x, in_rAt.y, srcRect.len_x(), srcRect.len_y() );
   drawBitmapStretchSR( texture, stretch, srcRect, in_flip, filter, in_wrap );
}

void GFXDrawUtil::drawBitmapStretchSR( GFXTextureObject* texture, const RectF &dstRect, const RectF &srcRect, const GFXBitmapFlip in_flip /*= GFXBitmapFlip_None*/, const GFXTextureFilterType filter /*= GFXTextureFilterPoint */ , bool in_wrap /*= true*/ )
{
   // Sanity if no texture is specified.
   if(!texture)
      return;   

   GFXVertexBufferHandle<GFXVertexPCT> verts(mDevice, 4, GFXBufferTypeVolatile );
   verts.lock();

   F32 texLeft   = (srcRect.point.x)                    / (texture->mTextureSize.x);
   F32 texRight  = (srcRect.point.x + srcRect.extent.x) / (texture->mTextureSize.x);
   F32 texTop    = (srcRect.point.y)                    / (texture->mTextureSize.y);
   F32 texBottom = (srcRect.point.y + srcRect.extent.y) / (texture->mTextureSize.y);

   F32 screenLeft   = dstRect.point.x;
   F32 screenRight  = (dstRect.point.x + dstRect.extent.x);
   F32 screenTop    = dstRect.point.y;
   F32 screenBottom = (dstRect.point.y + dstRect.extent.y);

   if( in_flip & GFXBitmapFlip_X ) 
   {
      F32 temp = texLeft;
      texLeft = texRight;
      texRight = temp;
   }
   if( in_flip & GFXBitmapFlip_Y ) 
   {
      F32 temp = texTop;
      texTop = texBottom;
      texBottom = temp;
   }

   const F32 fillConv = mDevice->getFillConventionOffset();
   verts[0].point.set( screenLeft  - fillConv, screenTop    - fillConv, 0.f );
   verts[1].point.set( screenRight - fillConv, screenTop    - fillConv, 0.f );
   verts[2].point.set( screenLeft  - fillConv, screenBottom - fillConv, 0.f );
   verts[3].point.set( screenRight - fillConv, screenBottom - fillConv, 0.f );

   verts[0].color = verts[1].color = verts[2].color = verts[3].color = mBitmapModulation;

   verts[0].texCoord.set( texLeft,  texTop );
   verts[1].texCoord.set( texRight, texTop );
   verts[2].texCoord.set( texLeft,  texBottom );
   verts[3].texCoord.set( texRight, texBottom );

   verts.unlock();

   mDevice->setVertexBuffer( verts );

   switch (filter)
   {
   case GFXTextureFilterPoint :
      mDevice->setStateBlock(in_wrap ? mBitmapStretchWrapSB : mBitmapStretchSB);
      break;
   case GFXTextureFilterLinear :
      mDevice->setStateBlock(in_wrap ? mBitmapStretchWrapLinearSB : mBitmapStretchLinearSB);
      break;
   default:
      AssertFatal(false, "No GFXDrawUtil state block defined for this filter type!");
      mDevice->setStateBlock(mBitmapStretchSB);
      break;
   }   
   mDevice->setTexture( 0, texture );
   mDevice->setupGenericShaders( GFXDevice::GSModColorTexture );

   mDevice->drawPrimitive( GFXTriangleStrip, 0, 2 );
}
void GFXDrawUtil::drawBitmapStretchSRCircle( GFXTextureObject*texture, const RectI &dstRectConst, const RectI &srcRect,const GFXBitmapFlip in_flip /* = GFXBitmapFlip_None */ , const GFXTextureFilterType filter/* = GFXTextureFilterPoint */, bool in_wrap/* = true */)
{
	//目标矩形如果不是正方形，将取较小的边
	RectI dstRect = dstRectConst;
	if (dstRect.extent.x != dstRect.extent.y)
	{
		int i = dstRect.extent.x > dstRect.extent.y ? dstRect.extent.y : dstRect.extent.x;
		dstRect.extent.x = i;
		dstRect.extent.y = i;
	}
	//画三角扇
	//mDevice->setBaseRenderState();

	int nDivided = 24;
	F32 angelStep = M_2PI_F / nDivided;
	GFXVertexBufferHandle<GFXVertexPCT> verts(mDevice, nDivided + 2, GFXBufferTypeVolatile );
	verts.lock();

	F32 texLeft   = F32(srcRect.point.x)                    / F32(texture->mTextureSize.x);
	F32 texRight  = F32(srcRect.point.x + srcRect.extent.x) / F32(texture->mTextureSize.x);
	F32 texTop    = F32(srcRect.point.y)                    / F32(texture->mTextureSize.y);
	F32 texBottom = F32(srcRect.point.y + srcRect.extent.y) / F32(texture->mTextureSize.y);
	F32 texHalfWidth = (texRight - texLeft) / 2;
	F32 texHalfHeight = (texBottom - texTop) / 2;

	//设置顶点和纹理
	F32 radius = (F32)(dstRect.extent.x) / 2;
	F32 angle,cos,sin;
	Point3F pt(0,0,0);
	Point2F pt2(0,0);
	verts[0].point.set(dstRect.point.x + radius,dstRect.point.y + radius,0.f);
	verts[0].color = mBitmapModulation;
	verts[0].texCoord.set(texLeft / 2 + texRight / 2 , texTop / 2 + texBottom  / 2);
	for (int i = 0 ; i < nDivided ; i++)
	{
		angle = -angelStep * i;
		cos = mCos(angle);
		sin = mSin(angle);
		pt.x = verts[0].point.x + radius * cos;
		pt.y = verts[0].point.y - radius * sin;
		pt2.x = verts[0].texCoord.x + texHalfWidth * cos;
		pt2.y = verts[0].texCoord.y - texHalfHeight * sin;
		verts[i + 1].point.set(pt);
		verts[i + 1].color = mBitmapModulation;
		verts[i + 1].texCoord.set(pt2.x,pt2.y);
	}
	verts[nDivided + 1] = verts[1];

	verts.unlock();

	mDevice->setVertexBuffer( verts );
	switch (filter)
	{
	case GFXTextureFilterPoint :
		mDevice->setStateBlock(in_wrap ? mBitmapStretchWrapSB : mBitmapStretchSB);
		break;
	case GFXTextureFilterLinear :
		mDevice->setStateBlock(in_wrap ? mBitmapStretchWrapLinearSB : mBitmapStretchLinearSB);
		break;
	default:
		AssertFatal(false, "No GFXDrawUtil state block defined for this filter type!");
		mDevice->setStateBlock(mBitmapStretchSB);
		break;
	}   
	mDevice->setTexture( 0, texture );
	mDevice->setupGenericShaders( GFXDevice::GSModColorTexture );

	mDevice->drawPrimitive( GFXTriangleFan, 0, nDivided);
}
void GFXDrawUtil::drawCDRectFill(const Point2I &center , const Point2I &size , const F32 angle, const ColorI &color)
{
	if (angle < 0.01 || angle > M_2PI_F - 0.01)
	{
		drawRectFill(Point2I(center.x - size.x/2,center.y - size.y/2),Point2I(center.x + size.x/2,center.y + size.y/2),color);
		return;
	}

// 	mDevice->setBaseRenderState();

	int nVerts = 0;	
	//45度以下7个点
	if (angle < M_PI_F/4)
	{
		nVerts = 7;
	}
	//45 to 135 need 6 point
	else if (angle >= M_PI_F/4 && angle < 3*M_PI_F/4)
	{
		nVerts = 6;
	}
	//135 to 225 need 5 point
	else if (angle >= 3*M_PI_F/4 && angle < 5*M_PI_F/4)
	{
		nVerts = 5;
	}
	//225 to 315 need 4 point
	else if (angle >= 5*M_PI_F/4 && angle < 7*M_PI_F/4)
	{
		nVerts = 4;
	}
	//
	else if (angle >= 7*M_PI_F/4)
	{
		nVerts = 3;
	}

	if (nVerts == 0)
		return;

	GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, nVerts, GFXBufferTypeVolatile);
	verts.lock();

	Point2I ptConers[4];
	Point2I ptMiddles[4];
	Point2I ptHalfSize = size;
	ptHalfSize.x = ptHalfSize.x/2;
	ptHalfSize.y = ptHalfSize.y/2;

	ptConers[0].x = center.x + ptHalfSize.x;
	ptConers[0].y = center.y - ptHalfSize.y;

	ptConers[1].x = center.x + ptHalfSize.x;
	ptConers[1].y = center.y + ptHalfSize.y;

	ptConers[2].x = center.x - ptHalfSize.x;
	ptConers[2].y = center.y + ptHalfSize.y;

	ptConers[3].x = center.x - ptHalfSize.x;
	ptConers[3].y = center.y - ptHalfSize.y;

	ptMiddles[0].x = center.x;
	ptMiddles[0].y = center.y - ptHalfSize.y;

	ptMiddles[1].x = center.x + ptHalfSize.x;
	ptMiddles[1].y = center.y;

	ptMiddles[2].x = center.x;
	ptMiddles[2].y = center.y + ptHalfSize.y;

	ptMiddles[3].x = center.x - ptHalfSize.x;
	ptMiddles[3].y = center.y;

	verts[0].point.set(center.x,center.y,0.0f);
	switch (nVerts)
	{
	case 7:
		verts[1].point.set(center.x + ptHalfSize.y * mTan(angle),ptMiddles[0].y,0.0f);
		verts[2].point.set(ptConers[0].x,ptConers[0].y,0.0f);
		verts[3].point.set(ptConers[1].x,ptConers[1].y,0.0f);
		verts[4].point.set(ptConers[2].x,ptConers[2].y,0.0f);
		verts[5].point.set(ptConers[3].x,ptConers[3].y,0.0f);
		verts[6].point.set(ptMiddles[0].x,ptMiddles[0].y,0.0f);
		break;
	case 6:
		verts[1].point.set(ptMiddles[1].x,(angle < M_PI_F/2) ? center.y - ptHalfSize.x * (M_PI_F/2 - angle): center.y + ptHalfSize.x * mTan(angle - M_PI_F/2),0.0f);
		verts[2].point.set(ptConers[1].x,ptConers[1].y,0.0f);
		verts[3].point.set(ptConers[2].x,ptConers[2].y,0.0f);
		verts[4].point.set(ptConers[3].x,ptConers[3].y,0.0f);
		verts[5].point.set(ptMiddles[0].x,ptMiddles[0].y,0.0f);
		break;
	case 5:
		verts[1].point.set( (angle - 3*M_PI_F/4) < M_PI_F/4 ? center.x + ptHalfSize.y * mTan(M_PI_F/4 - (angle - 3*M_PI_F/4)) : center.x - ptHalfSize.y * mTan((angle - 3*M_PI_F/4) - M_PI_F/4),
			ptMiddles[2].y,0.0f);
		verts[2].point.set(ptConers[2].x,ptConers[2].y,0.0f);
		verts[3].point.set(ptConers[3].x,ptConers[3].y,0.0f);
		verts[4].point.set(ptMiddles[0].x,ptMiddles[0].y,0.0f);
		break;
	case 4:
		verts[1].point.set(ptMiddles[3].x,
			(angle < 3*M_PI_F/2) ? center.y + ptHalfSize.x * mTan(M_PI_F/4 - (angle - 5*M_PI_F/4)) : center.y - ptHalfSize.x * ((angle - 5*M_PI_F/4) - M_PI_F/4) ,
			0.0f);
		verts[2].point.set(ptConers[3].x,ptConers[3].y,0.0f);
		verts[3].point.set(ptMiddles[0].x,ptMiddles[0].y,0.0f);
		break;
	case 3:
		verts[1].point.set(center.x - ptHalfSize.y * mTan(M_PI_F/4 - (angle - 7*M_PI_F/4)),ptMiddles[0].y,0.0f);
		verts[2].point.set(ptMiddles[0].x,ptMiddles[0].y,0.0f);
		break;
	}


	for (int i=0; i<nVerts; i++)
		verts[i].color = color;

	verts.unlock();

	mDevice->setStateBlock(mRectFillSB);
	mDevice->setVertexBuffer( verts );
	mDevice->setupGenericShaders();

	mDevice->drawPrimitive( GFXTriangleFan, 0, nVerts - 2 );
}
void GFXDrawUtil::drawBitmapStretchRotate( GFXTextureObject*texture, const RectI &dstRect,F32 spinAngle, const GFXBitmapFlip in_flip /* = GFXBitmapFlip_None */ , const GFXTextureFilterType filter/* = GFXTextureFilterPoint */, bool in_wrap/* = true */)
{
	AssertFatal( texture != 0, "No texture specified for drawBitmapStretch()" );

	RectI subRegion( 0, 0, texture->mBitmapSize.x, texture->mBitmapSize.y );
	drawBitmapStretchSRRotate(texture,dstRect,subRegion,spinAngle,in_flip,filter,in_wrap);
}
void GFXDrawUtil::drawBitmapStretchSRRotate( GFXTextureObject*texture, const RectI &dstRect, const RectI &srcRect,F32 spinAngle, const GFXBitmapFlip in_flip /* = GFXBitmapFlip_None */ , const GFXTextureFilterType filter/* = GFXTextureFilterPoint */, bool in_wrap/* = true */)
{
	// Sanity if no texture is specified.
	if(!texture)
		return;

// 	mDevice->setBaseRenderState();

	GFXVertexBufferHandle<GFXVertexPCT> verts(mDevice, 4, GFXBufferTypeVolatile );
	verts.lock();

	F32 texLeft   = F32(srcRect.point.x)                    / F32(texture->mTextureSize.x);
	F32 texRight  = F32(srcRect.point.x + srcRect.extent.x) / F32(texture->mTextureSize.x);
	F32 texTop    = F32(srcRect.point.y)                    / F32(texture->mTextureSize.y);
	F32 texBottom = F32(srcRect.point.y + srcRect.extent.y) / F32(texture->mTextureSize.y);


	const F32 fillConv = mDevice->getFillConventionOffset();
	Point3F offsetCenter(0,0,0);
	offsetCenter.x = dstRect.point.x + dstRect.extent.x/2 - fillConv;
	offsetCenter.y = dstRect.point.y + dstRect.extent.y/2 - fillConv;


	if( in_flip & GFXBitmapFlip_X ) 
	{
		F32 temp = texLeft;
		texLeft = texRight;
		texRight = temp;
	}
	if( in_flip & GFXBitmapFlip_Y ) 
	{
		F32 temp = texTop;
		texTop = texBottom;
		texBottom = temp;
	}


	verts[0].point.set(-dstRect.extent.x/2,-dstRect.extent.y/2,0.f);
	verts[1].point.set(-dstRect.extent.x/2,dstRect.extent.y/2,0.f);
	verts[2].point.set(dstRect.extent.x/2,-dstRect.extent.y/2,0.f);
	verts[3].point.set(dstRect.extent.x/2,dstRect.extent.y/2,0.f);
	// 	if(spinAngle != 0.f)
	// 	{
	MatrixF rotMatrix( EulerF( 0.0, 0.0, -spinAngle - M_PI/2 ));

	for( S32 i = 0; i < 4; i++ )
	{
		rotMatrix.mulP( verts[i].point );
		verts[i].point += offsetCenter;
	}
	/*	}*/



	verts[0].color = verts[1].color = verts[2].color = verts[3].color = mBitmapModulation;

	verts[0].texCoord.set( texLeft,  texTop );
	verts[1].texCoord.set( texRight, texTop );
	verts[2].texCoord.set( texLeft,  texBottom );
	verts[3].texCoord.set( texRight, texBottom );


	verts.unlock();

	mDevice->setVertexBuffer( verts );
	switch (filter)
	{
	case GFXTextureFilterPoint :
		mDevice->setStateBlock(in_wrap ? mBitmapStretchWrapSB : mBitmapStretchSB);
		break;
	case GFXTextureFilterLinear :
		mDevice->setStateBlock(in_wrap ? mBitmapStretchWrapLinearSB : mBitmapStretchLinearSB);
		break;
	default:
		AssertFatal(false, "No GFXDrawUtil state block defined for this filter type!");
		mDevice->setStateBlock(mBitmapStretchSB);
		break;
	}   
	mDevice->setTexture( 0, texture );
	mDevice->setupGenericShaders( GFXDevice::GSModColorTexture );

	mDevice->drawPrimitive( GFXTriangleStrip, 0, 2 );
}


//-----------------------------------------------------------------------------
// Draw Rectangle
//-----------------------------------------------------------------------------
void GFXDrawUtil::drawRect( const Point2I &upperLeft, const Point2I &lowerRight, const ColorI &color ) 
{
   drawRect( Point2F((F32)upperLeft.x,(F32)upperLeft.y),Point2F((F32)lowerRight.x,(F32)lowerRight.y),color);
}

void GFXDrawUtil::drawRect( const RectI &rect, const ColorI &color )
{
   drawRect( rect.point, Point2I(rect.point.x + rect.extent.x - 1, rect.point.y + rect.extent.y - 1), color );
}

void GFXDrawUtil::drawRect( const RectF &rect, const ColorI &color )
{
   drawRect( rect.point, Point2F(rect.point.x + rect.extent.x - 1, rect.point.y + rect.extent.y - 1), color );
}

void GFXDrawUtil::drawRect( const Point2F &upperLeft, const Point2F &lowerRight, const ColorI &color )
{
   //
   // Convert Box   a----------x
   //               |          |
   //               x----------b
   //
   // Into Triangle-Strip Outline
   //               v0-----------v2
   //               | a         x |
   //					  |  v1-----v3  |
   //               |   |     |   |
   //               |  v7-----v5  |
   //               | x         b |
   //               v6-----------v4
   //

   // NorthWest and NorthEast facing offset vectors
   // These adjust the thickness of the line, it'd be neat if one day
   // they were passed in as arguments.
   Point2F nw(-0.5f,-0.5f); /*  \  */
   Point2F ne(0.5f,-0.5f); /*  /  */

   GFXVertexBufferHandle<GFXVertexPC> verts (mDevice, 10, GFXBufferTypeVolatile );
   verts.lock();

   F32 ulOffset = 0.5f - mDevice->getFillConventionOffset();

   verts[0].point.set( upperLeft.x + ulOffset + nw.x, upperLeft.y + ulOffset + nw.y, 0.0f );
   verts[1].point.set( upperLeft.x + ulOffset - nw.x, upperLeft.y + ulOffset - nw.y, 0.0f );
   verts[2].point.set( lowerRight.x + ne.x, upperLeft.y + ulOffset + ne.y, 0.0f );
   verts[3].point.set( lowerRight.x - ne.x, upperLeft.y + ulOffset - ne.y, 0.0f );
   verts[4].point.set( lowerRight.x - nw.x, lowerRight.y - nw.y, 0.0f );
   verts[5].point.set( lowerRight.x + nw.x, lowerRight.y + nw.y, 0.0f );
   verts[6].point.set( upperLeft.x + ulOffset - ne.x, lowerRight.y - ne.y, 0.0f );
   verts[7].point.set( upperLeft.x + ulOffset + ne.x, lowerRight.y + ne.y, 0.0f );
   verts[8].point.set( upperLeft.x + ulOffset + nw.x, upperLeft.y + ulOffset + nw.y, 0.0f ); // same as 0
   verts[9].point.set( upperLeft.x + ulOffset - nw.x, upperLeft.y + ulOffset - nw.y, 0.0f ); // same as 1

   for (int i=0; i<10; i++)
      verts[i].color = color;

   verts.unlock();
   mDevice->setVertexBuffer( verts );

   mDevice->setStateBlock(mRectFillSB);
   mDevice->setupGenericShaders();
   mDevice->drawPrimitive( GFXTriangleStrip, 0, 8 );
}

//-----------------------------------------------------------------------------
// Draw Rectangle Fill
//-----------------------------------------------------------------------------
void GFXDrawUtil::drawRectFill( const RectF &rect, const ColorI &color )
{
   drawRectFill(rect.point, Point2F(rect.extent.x + rect.point.x - 1, rect.extent.y + rect.point.y - 1), color );
}

void GFXDrawUtil::drawRectFill( const Point2I &upperLeft, const Point2I &lowerRight, const ColorI &color ) 
{   
   drawRectFill(Point2F((F32)upperLeft.x, (F32)upperLeft.y), Point2F((F32)lowerRight.x, (F32)lowerRight.y), color);
}

void GFXDrawUtil::drawRectFill( const RectI &rect, const ColorI &color )
{
   drawRectFill(rect.point, Point2I(rect.extent.x + rect.point.x - 1, rect.extent.y + rect.point.y - 1), color );
}

void GFXDrawUtil::drawRectFill( const Point2F &upperLeft, const Point2F &lowerRight, const ColorI &color )
{
   //
   // Convert Box   a----------x
   //               |          |
   //               x----------b
   // Into Quad
   //               v0---------v1
   //               | a       x |
   //					  |           |
   //               | x       b |
   //               v2---------v3
   //

   // NorthWest and NorthEast facing offset vectors
   Point2F nw(-0.5,-0.5); /*  \  */
   Point2F ne(0.5,-0.5); /*  /  */

   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 4, GFXBufferTypeVolatile);
   verts.lock();

   F32 ulOffset = 0.5f - mDevice->getFillConventionOffset();
   
   verts[0].point.set( upperLeft.x+nw.x+ulOffset, upperLeft.y+nw.y+ulOffset, 0.0f );
   verts[1].point.set( lowerRight.x+ne.x, upperLeft.y+ne.y+ulOffset, 0.0f );
   verts[2].point.set( upperLeft.x-ne.x+ulOffset, lowerRight.y-ne.y, 0.0f );
   verts[3].point.set( lowerRight.x-nw.x, lowerRight.y-nw.y, 0.0f );

   for (int i=0; i<4; i++)
      verts[i].color = color;

   verts.unlock();

   mDevice->setStateBlock(mRectFillSB);
   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();
   mDevice->drawPrimitive( GFXTriangleStrip, 0, 2 );
}

void GFXDrawUtil::draw2DSquare( const Point2F &screenPoint, F32 width, F32 spinAngle )
{
   width *= 0.5;

   Point3F offset( screenPoint.x, screenPoint.y, 0.0 );

   GFXVertexBufferHandle<GFXVertexPC> verts( mDevice, 4, GFXBufferTypeVolatile );
   verts.lock();

   verts[0].point.set( -width, -width, 0.0f );
   verts[1].point.set( -width, width, 0.0f );
   verts[2].point.set( width,  -width, 0.0f );
   verts[3].point.set( width,  width, 0.0f );

   verts[0].color = verts[1].color = verts[2].color = verts[3].color = mBitmapModulation;

   if(spinAngle != 0.f)
   {
      MatrixF rotMatrix( EulerF( 0.0, 0.0, spinAngle ) );

      for( S32 i = 0; i < 4; i++ )
      {
         rotMatrix.mulP( verts[i].point );
         verts[i].point += offset;
      }
   }

   verts.unlock();
   mDevice->setVertexBuffer( verts );

   mDevice->setStateBlock(mRectFillSB);
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXTriangleStrip, 0, 2 );
}

//-----------------------------------------------------------------------------
// Draw Line
//-----------------------------------------------------------------------------
void GFXDrawUtil::drawLine( const Point3F &startPt, const Point3F &endPt, const ColorI &color )
{
   drawLine( startPt.x, startPt.y, startPt.z, endPt.x, endPt.y, endPt.z, color );
}

void GFXDrawUtil::drawLine( const Point2F &startPt, const Point2F &endPt, const ColorI &color )
{
   drawLine( startPt.x, startPt.y, 0.0f, endPt.x, endPt.y, 0.0f, color );
}

void GFXDrawUtil::drawLine( const Point2I &startPt, const Point2I &endPt, const ColorI &color )
{
   drawLine( startPt.x, startPt.y, 0.0f, endPt.x, endPt.y, 0.0f, color );
}

void GFXDrawUtil::drawLine( F32 x1, F32 y1, F32 x2, F32 y2, const ColorI &color )
{
   drawLine( x1, y1, 0.0f, x2, y2, 0.0f, color );
}

void GFXDrawUtil::drawLine( F32 x1, F32 y1, F32 z1, F32 x2, F32 y2, F32 z2, const ColorI &color )
{
   //
   // Convert Line   a----------b
   //
   // Into Quad      v0---------v1
   //                 a         b
   //                v2---------v3
   //

   Point2F start(x1, y1);
   Point2F end(x2, y2);
   Point2F perp, lineVec;

   // handle degenerate case where point a = b
   if(x1 == x2 && y1 == y2)
   {
      perp.set(0.0f, 0.5f);
      lineVec.set(0.1f, 0.0f);
   }
   else
   {
      perp.set(start.y - end.y, end.x - start.x);
      lineVec.set(end.x - start.x, end.y - start.y);
      perp.normalize(0.5f);
      lineVec.normalize(0.1f);
   }
   start -= lineVec;
   end   += lineVec;

   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 4, GFXBufferTypeVolatile);
   verts.lock();

   verts[0].point.set( start.x+perp.x, start.y+perp.y, z1 );
   verts[1].point.set( end.x+perp.x, end.y+perp.y, z2 );
   verts[2].point.set( start.x-perp.x, start.y-perp.y, z1 );
   verts[3].point.set( end.x-perp.x, end.y-perp.y, z2 );

   verts[0].color = color;
   verts[1].color = color;
   verts[2].color = color;
   verts[3].color = color;

   verts.unlock();
   mDevice->setVertexBuffer( verts );
   mDevice->setStateBlock(mRectFillSB);
   mDevice->drawPrimitive( GFXTriangleStrip, 0, 2 );
}

//-----------------------------------------------------------------------------
// 3D World Draw Misc
//-----------------------------------------------------------------------------

static SphereMesh gSphere;

void GFXDrawUtil::drawSphere( const GFXStateBlockDesc &desc, F32 radius, const Point3F &pos, const ColorI &color, bool drawTop, bool drawBottom, const MatrixF *xfm )
{
   MatrixF mat;
   if ( xfm )
      mat = *xfm;
   else
      mat = MatrixF::Identity;

   mat.scale(Point3F(radius,radius,radius));
   mat.setPosition(pos);
   GFX->pushWorldMatrix();
   GFX->multWorld(mat);

   const SphereMesh::TriangleMesh * sphereMesh = gSphere.getMesh(2);
   S32 numPoly = sphereMesh->numPoly;
   S32 totalPoly = 0;
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, numPoly*3, GFXBufferTypeVolatile);
   verts.lock();
   S32 vertexIndex = 0;
   for (S32 i=0; i<numPoly; i++)
   {
      if (!drawBottom)
      {
         if (sphereMesh->poly[i].pnt[0].z < -0.01f || sphereMesh->poly[i].pnt[1].z < -0.01f || sphereMesh->poly[i].pnt[2].z < -0.01f)
            continue;
      }
      if (!drawTop)
      {
         if (sphereMesh->poly[i].pnt[0].z > 0.01f || sphereMesh->poly[i].pnt[1].z > 0.01f || sphereMesh->poly[i].pnt[2].z > 0.01f)
            continue;
      }
      totalPoly++;

      verts[vertexIndex].point = sphereMesh->poly[i].pnt[0];
      verts[vertexIndex].color = color;
      vertexIndex++;

      verts[vertexIndex].point = sphereMesh->poly[i].pnt[1];
      verts[vertexIndex].color = color;
      vertexIndex++;

      verts[vertexIndex].point = sphereMesh->poly[i].pnt[2];
      verts[vertexIndex].color = color;
      vertexIndex++;
   }
   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXTriangleList, 0, totalPoly );

   GFX->popWorldMatrix();
}

//-----------------------------------------------------------------------------

static const Point3F cubePoints[8] = 
{
   Point3F(-1, -1, -1), Point3F(-1, -1,  1), Point3F(-1,  1, -1), Point3F(-1,  1,  1),
   Point3F( 1, -1, -1), Point3F( 1, -1,  1), Point3F( 1,  1, -1), Point3F( 1,  1,  1)
};

static const U32 cubeFaces[6][4] = 
{
   { 0, 4, 6, 2 }, { 0, 2, 3, 1 }, { 0, 1, 5, 4 },
   { 3, 2, 6, 7 }, { 7, 6, 4, 5 }, { 3, 7, 5, 1 }
};

void GFXDrawUtil::drawTriangle( const GFXStateBlockDesc &desc, const Point3F &p0, const Point3F &p1, const Point3F &p2, const ColorI &color, const MatrixF *xfm )
{
   if ( desc.fillMode == GFXFillWireframe )
      _drawWireTriangle( desc, p0, p1, p2, color, xfm );
   else
      _drawSolidTriangle( desc, p0, p1, p2, color, xfm );
}

void GFXDrawUtil::_drawWireTriangle( const GFXStateBlockDesc &desc, const Point3F &p0, const Point3F &p1, const Point3F &p2, const ColorI &color, const MatrixF *xfm )
{
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 4, GFXBufferTypeVolatile);
   verts.lock();

   // Set up the line strip
   verts[0].point = p0;
   verts[0].color = color;
   verts[1].point = p1;
   verts[1].color = color;
   verts[2].point = p2;
   verts[2].color = color;
   verts[3].point = p0;
   verts[3].color = color;

   // Apply xfm if we were passed one.
   if ( xfm != NULL )
   {
      for ( U32 i = 0; i < 4; i++ )
         xfm->mulP( verts[i].point );
   }

   verts.unlock();

   GFXStateBlockRef sb = mDevice->createStateBlock( desc );
   mDevice->setStateBlock( sb );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXLineStrip, 0, 3 );
}

void GFXDrawUtil::_drawSolidTriangle( const GFXStateBlockDesc &desc, const Point3F &p0, const Point3F &p1, const Point3F &p2, const ColorI &color, const MatrixF *xfm )
{
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 3, GFXBufferTypeVolatile);
   verts.lock();

   // Set up the line strip
   verts[0].point = p0;
   verts[0].color = color;
   verts[1].point = p1;
   verts[1].color = color;
   verts[2].point = p2;
   verts[2].color = color;

   // Apply xfm if we were passed one.
   if ( xfm != NULL )
   {
      for ( U32 i = 0; i < 3; i++ )
         xfm->mulP( verts[i].point );
   }

   verts.unlock();

   GFXStateBlockRef sb = mDevice->createStateBlock( desc );
   mDevice->setStateBlock( sb );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXTriangleList, 0, 1 );
}

void GFXDrawUtil::drawCube( const GFXStateBlockDesc &desc, const Box3F &box, const ColorI &color, const MatrixF *xfm )
{
   drawCube( desc, box.getExtents(), box.getCenter(), color, xfm );
}

void GFXDrawUtil::drawCube( const GFXStateBlockDesc &desc, const Point3F &size, const Point3F &pos, const ColorI &color, const MatrixF *xfm )
{
   if ( desc.fillMode == GFXFillWireframe )
      _drawWireCube( desc, size, pos, color, xfm );
   else
      _drawSolidCube( desc, size, pos, color, xfm );
}

void GFXDrawUtil::_drawWireCube( const GFXStateBlockDesc &desc, const Point3F &size, const Point3F &pos, const ColorI &color, const MatrixF *xfm )
{
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 30, GFXBufferTypeVolatile);
   verts.lock();

   Point3F halfSize = size * 0.5f;

   // setup 6 line loops
   U32 vertexIndex = 0;
   for(int i = 0; i < 6; i++)
   {
      for(int j = 0; j < 5; j++)
      {
         int idx = cubeFaces[i][j%4];

         verts[vertexIndex].point = cubePoints[idx] * halfSize;
         verts[vertexIndex].color = color;
         vertexIndex++;
      }
   }

   // Apply xfm if we were passed one.
   if ( xfm != NULL )
   {
      for ( U32 i = 0; i < 30; i++ )
         xfm->mulP( verts[i].point );
   }

   // Apply position offset
   for ( U32 i = 0; i < 30; i++ )
      verts[i].point += pos;

   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   for( U32 i=0; i<6; i++ )
      mDevice->drawPrimitive( GFXLineStrip, i*5, 4 );
}

void GFXDrawUtil::_drawSolidCube( const GFXStateBlockDesc &desc, const Point3F &size, const Point3F &pos, const ColorI &color, const MatrixF *xfm )
{
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 36, GFXBufferTypeVolatile);
   verts.lock();

   Point3F halfSize = size * 0.5f;

   // setup 6 line loops
   U32 vertexIndex = 0;
   U32 idx;
   for(int i = 0; i < 6; i++)
   {
      idx = cubeFaces[i][0];
      verts[vertexIndex].point = cubePoints[idx] * halfSize;      
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][1];
      verts[vertexIndex].point = cubePoints[idx] * halfSize;
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][3];
      verts[vertexIndex].point = cubePoints[idx] * halfSize;
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][1];
      verts[vertexIndex].point = cubePoints[idx] * halfSize;
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][2];
      verts[vertexIndex].point = cubePoints[idx] * halfSize;
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][3];
      verts[vertexIndex].point = cubePoints[idx] * halfSize;
      verts[vertexIndex].color = color;
      vertexIndex++;
   }

   // Apply xfm if we were passed one.
   if ( xfm != NULL )
   {
      for ( U32 i = 0; i < 36; i++ )
         xfm->mulV( verts[i].point );
   }

   // Apply position offset
   for ( U32 i = 0; i < 36; i++ )
      verts[i].point += pos;

   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXTriangleList, 0, 12 );
}

void GFXDrawUtil::drawPolyhedron( const GFXStateBlockDesc &desc, const Polyhedron &poly, const ColorI &color, const MatrixF *xfm )
{
   if ( desc.fillMode == GFXFillWireframe )
      _drawWirePolyhedron( desc, poly, color, xfm );
   else
      _drawSolidPolyhedron( desc, poly, color, xfm );
}

// maps vertices from the cubeFaces array to a polyhedron object
static const U32 polyFaceMap[] = { 2, 6, 0, 3, 4, 7, 1, 5 };

void GFXDrawUtil::_drawWirePolyhedron( const GFXStateBlockDesc &desc, const Polyhedron &poly, const ColorI &color, const MatrixF *xfm )
{
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 30, GFXBufferTypeVolatile);
   verts.lock();

   // setup 6 line loops
   U32 vertexIndex = 0;
   for(int i = 0; i < 6; i++)
   {
      for(int j = 0; j < 5; j++)
      {
         int idx = cubeFaces[i][j%4];

         verts[vertexIndex].point = poly.pointList[polyFaceMap[idx]];
         verts[vertexIndex].color = color;
         vertexIndex++;
      }
   }

   // Apply xfm if we were passed one.
   if ( xfm != NULL )
   {
      for ( U32 i = 0; i < 30; i++ )
         xfm->mulP( verts[i].point );
   }

   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   for( U32 i=0; i<6; i++ )
      mDevice->drawPrimitive( GFXLineStrip, i*5, 4 );
}

void GFXDrawUtil::_drawSolidPolyhedron( const GFXStateBlockDesc &desc, const Polyhedron &poly, const ColorI &color, const MatrixF *xfm )
{
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 36, GFXBufferTypeVolatile);
   verts.lock();

   // setup 6 line loops
   U32 vertexIndex = 0;
   U32 idx;
   for(int i = 0; i < 6; i++)
   {
      idx = cubeFaces[i][0];
      verts[vertexIndex].point = poly.pointList[polyFaceMap[idx]];      
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][1];
      verts[vertexIndex].point = poly.pointList[polyFaceMap[idx]];
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][3];
      verts[vertexIndex].point = poly.pointList[polyFaceMap[idx]];
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][1];
      verts[vertexIndex].point = poly.pointList[polyFaceMap[idx]];
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][2];
      verts[vertexIndex].point = poly.pointList[polyFaceMap[idx]];
      verts[vertexIndex].color = color;
      vertexIndex++;

      idx = cubeFaces[i][3];
      verts[vertexIndex].point = poly.pointList[polyFaceMap[idx]];
      verts[vertexIndex].color = color;
      vertexIndex++;
   }

   // Apply xfm if we were passed one.
   if ( xfm != NULL )
   {
      for ( U32 i = 0; i < 36; i++ )
         xfm->mulV( verts[i].point );
   }

   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXTriangleList, 0, 12 );
}

void GFXDrawUtil::drawObjectBox( const GFXStateBlockDesc &desc, const Point3F &size, const Point3F &pos, const MatrixF &objMat, const ColorI &color )
{
   GFXTransformSaver saver;

   mDevice->setStateBlockByDesc( desc );

   MatrixF scaledObjMat( true );
   scaledObjMat = objMat;

   scaledObjMat.scale( size );
   scaledObjMat.setPosition( pos );

   PrimBuild::color( color );
   PrimBuild::begin( GFXLineList, 48 );

   static const Point3F cubePoints[8] = 
   {
      Point3F(-0.5, -0.5, -0.5), Point3F(-0.5, -0.5,  0.5), Point3F(-0.5,  0.5, -0.5), Point3F(-0.5,  0.5,  0.5),
      Point3F( 0.5, -0.5, -0.5), Point3F( 0.5, -0.5,  0.5), Point3F( 0.5,  0.5, -0.5), Point3F( 0.5,  0.5,  0.5)
   };

   // 8 corner points of the box   
   for ( U32 i = 0; i < 8; i++ )
   {
      //const Point3F &start = cubePoints[i];  

      // 3 lines per corner point
      for ( U32 j = 0; j < 3; j++ )
      {
         Point3F start = cubePoints[i];
         Point3F end = start;
         end[j] *= 0.8f;

         scaledObjMat.mulP(start);
         PrimBuild::vertex3fv(start);
         scaledObjMat.mulP(end);
         PrimBuild::vertex3fv(end);            
      }
   }

   PrimBuild::end();
}

static const Point2F circlePoints[] = 
{
   Point2F(0.707107f, 0.707107f),
   Point2F(0.923880f, 0.382683f),
   Point2F(1.000000f, 0.000000f),
   Point2F(0.923880f, -0.382684f),
   Point2F(0.707107f, -0.707107f),
   Point2F(0.382683f, -0.923880f),
   Point2F(0.000000f, -1.000000f),
   Point2F(-0.382683f, -0.923880f),
   Point2F(-0.707107f, -0.707107f),
   Point2F(-0.923880f, -0.382684f),
   Point2F(-1.000000f, 0.000000f),
   Point2F(-0.923879f, 0.382684f),
   Point2F(-0.707107f, 0.707107f),
   Point2F(-0.382683f, 0.923880f),
   Point2F(0.000000f, 1.000000f),
   Point2F(0.382684f, 0.923879f)
};

void GFXDrawUtil::drawCapsule( const GFXStateBlockDesc &desc, const Point3F &center, F32 radius, F32 height, const ColorI &color, const MatrixF *xfm )
{
   if ( desc.fillMode == GFXFillWireframe )
      _drawWireCapsule( desc, center, radius, height, color, xfm );
   else
      _drawSolidCapsule( desc, center, radius, height, color, xfm );
}

void GFXDrawUtil::_drawSolidCapsule( const GFXStateBlockDesc &desc, const Point3F &center, F32 radius, F32 height, const ColorI &color, const MatrixF *xfm )
{	
   MatrixF mat;
   if ( xfm )
      mat = *xfm;      
   else
      mat = MatrixF::Identity;

   S32 numPoints = sizeof(circlePoints)/sizeof(Point2F);
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, numPoints * 2 + 2, GFXBufferTypeVolatile);
   verts.lock();

   for (S32 i=0; i<numPoints + 1; i++)
   {
      S32 imod = i % numPoints;      
      verts[2 * i].point = Point3F( circlePoints[imod].x * radius, circlePoints[imod].y * radius, height );
      verts[2 * i].color = color;
      verts[2 * i + 1].point = Point3F( circlePoints[imod].x * radius, circlePoints[imod].y * radius, -height );
      verts[2 * i + 1].color = color;
   }

   S32 totalNumPnts = numPoints * 2 + 2;

   // Apply xfm if we were passed one.
   for ( U32 i = 0; i < totalNumPnts; i++ )
      mat.mulP( verts[i].point );

   // Apply position offset
   for ( U32 i = 0; i < totalNumPnts; i++ )
      verts[i].point += center;

   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXTriangleStrip, 0, 2 * numPoints );

   Point3F sphereCenter;
   MatrixF sphereMat;

   if ( xfm )
      sphereMat = *xfm;
   else
      sphereMat = MatrixF::Identity;   

   sphereCenter.set( 0, 0, 0.5f * height );
   mat.mulV( sphereCenter );
   sphereCenter += center;

   drawSphere( desc, radius, sphereCenter, color, true, false, &sphereMat );

   sphereCenter.set( 0, 0, -0.5f * height );
   mat.mulV( sphereCenter );
   sphereCenter += center;

   drawSphere( desc, radius, sphereCenter, color, false, true, &sphereMat );
}

void GFXDrawUtil::_drawWireCapsule( const GFXStateBlockDesc &desc, const Point3F &center, F32 radius, F32 height, const ColorI &color, const MatrixF *xfm )
{
   MatrixF mat;
   if ( xfm )
      mat = *xfm;
   else
      mat = MatrixF::Identity;

   mat.scale( Point3F(radius,radius,height*0.5f) );
   mat.setPosition(center);
   mDevice->pushWorldMatrix();
   mDevice->multWorld(mat);

   S32 numPoints = sizeof(circlePoints)/sizeof(Point2F);
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, numPoints, GFXBufferTypeVolatile);
   verts.lock();
   for (S32 i=0; i< numPoints; i++)
   {
      S32 idx = i & (~1); // just draw the even ones
      F32 z = i & 1 ? 1.0f : -1.0f;
      verts[i].point = Point3F(circlePoints[idx].x,circlePoints[idx].y, z);
      verts[i].color = color;
   }
   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   for (S32 i=0; i<numPoints; i += 2)
      mDevice->drawPrimitive(GFXLineStrip, i, 1);

   mDevice->popWorldMatrix();

   Point3F sphereCenter;
   sphereCenter.z = center.z + 0.5f * height;
   drawSphere( desc, radius,sphereCenter,color,true,false);
   sphereCenter.z = center.z - 0.5f * height;
   drawSphere( desc, radius,sphereCenter,color,false,true);
}

void GFXDrawUtil::drawCone( const GFXStateBlockDesc &desc, const Point3F &basePnt, const Point3F &tipPnt, F32 baseRadius, const ColorI &color )
{   
   VectorF uvec = tipPnt - basePnt;
   F32 height = uvec.len();
   uvec.normalize();
   MatrixF mat( true );
   MathUtils::getMatrixFromUpVector( uvec, &mat );   
   mat.setPosition(basePnt);

   Point3F scale( baseRadius, baseRadius, height );
   mat.scale(scale);

   GFXTransformSaver saver;

   mDevice->pushWorldMatrix();
   mDevice->multWorld(mat);

   S32 numPoints = sizeof(circlePoints)/sizeof(Point2F);
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, numPoints + 2, GFXBufferTypeVolatile);
   verts.lock();
   verts[0].point = Point3F(0.0f,0.0f,1.0f);
   verts[0].color = color;
   for (S32 i=0; i<numPoints + 1; i++)
   {
      S32 imod = i % numPoints;
      verts[i + 1].point = Point3F(circlePoints[imod].x,circlePoints[imod].y, 0.0f);
      verts[i + 1].color = color;
   }
   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders( GFXDevice::GSModColorTexture );

   mDevice->drawPrimitive( GFXTriangleFan, 0, numPoints );
   mDevice->drawPrimitive( GFXTriangleFan, 1, numPoints-1 );

   mDevice->popWorldMatrix();
}

void GFXDrawUtil::drawCylinder( const GFXStateBlockDesc &desc, const Point3F &basePnt, const Point3F &tipPnt, F32 radius, const ColorI &color )
{
   VectorF uvec = tipPnt - basePnt;
   F32 height = uvec.len();
   uvec.normalize();
   MatrixF mat( true );
   MathUtils::getMatrixFromUpVector( uvec, &mat );   
   mat.setPosition(basePnt);

   Point3F scale( radius/2, radius/2, height * 2 );
   mat.scale(scale);
   GFXTransformSaver saver;

   mDevice->pushWorldMatrix();
   mDevice->multWorld(mat);

   S32 numPoints = sizeof(circlePoints)/sizeof(Point2F);
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, numPoints * 4 + 4, GFXBufferTypeVolatile);
   verts.lock();
   for (S32 i=0; i<numPoints + 1; i++)
   {
      S32 imod = i % numPoints;
      verts[i].point = Point3F(circlePoints[imod].x,circlePoints[imod].y, 0.5f);
      verts[i].color = color;
      verts[i + numPoints + 1].point = Point3F(circlePoints[imod].x,circlePoints[imod].y, 0);
      verts[i + numPoints + 1].color = color;

      verts[2*numPoints + 2 + 2 * i].point = Point3F(circlePoints[imod].x,circlePoints[imod].y, 0.5f);
      verts[2*numPoints + 2 + 2 * i].color = color;
      verts[2*numPoints + 2 + 2 * i + 1].point = Point3F(circlePoints[imod].x,circlePoints[imod].y, 0);
      verts[2*numPoints + 2 + 2 * i + 1].color = color;
   }
   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders( GFXDevice::GSModColorTexture );

   mDevice->drawPrimitive( GFXTriangleFan, 0, numPoints );
   mDevice->drawPrimitive( GFXTriangleFan, numPoints + 1, numPoints );
   mDevice->drawPrimitive( GFXTriangleStrip, 2 * numPoints + 2, 2 * numPoints);

   mDevice->popWorldMatrix();
}

void GFXDrawUtil::drawArrow( const GFXStateBlockDesc &desc, const Point3F &start, const Point3F &end, const ColorI &color )
{   
   GFXTransformSaver saver;

   // Direction and length of the arrow.
   VectorF dir = end - start;
   F32 len = dir.len();
   dir.normalize();   
   len *= 0.2f;      

   // Base of the cone will be a distance back from the end of the arrow
   // proportional to the total distance of the arrow... 0.3f looks about right.
   Point3F coneBase = end - dir * len * 0.3f;

   // Calculate the radius of the cone given that we want the cone to have
   // an angle of 25 degrees (just because it looks good).
   F32 coneLen = ( end - coneBase ).len();
   F32 coneDiameter = mTan( mDegToRad(25.0f) ) * coneLen;

   // Draw the cone on at the arrow's tip.
   drawCone( desc, coneBase, end, coneDiameter / 2.0f, color );

   // Get the difference in length from
   // the start of the cone to the end
   // of the cylinder so we can put the
   // end of the cylinder right against where
   // the cone starts.
   Point3F coneDiff = end - coneBase;

   // Draw the cylinder.
   F32 stickRadius = len * 0.025f;   
   drawCylinder( desc, start, end - coneDiff, stickRadius, color );
}

void GFXDrawUtil::drawFrustum( const Frustum &f, const ColorI &color )
{
   const Point3F  *points = f.getPoints();

   // Draw near and far planes.
   for (U32 offset = 0; offset < 8; offset+=4)
   {      
      drawLine(points[offset+0], points[offset+1], color);
      drawLine(points[offset+2], points[offset+3], color);
      drawLine(points[offset+0], points[offset+2], color);
      drawLine(points[offset+1], points[offset+3], color);            
   }

   // connect the near and far planes
   drawLine(points[Frustum::NearTopLeft], points[Frustum::FarTopLeft], color);
   drawLine(points[Frustum::NearTopRight], points[Frustum::FarTopRight], color);
   drawLine(points[Frustum::NearBottomLeft], points[Frustum::FarBottomLeft], color);
   drawLine(points[Frustum::NearBottomRight], points[Frustum::FarBottomRight], color);
}

void GFXDrawUtil::drawSolidPlane( const GFXStateBlockDesc &desc, const Point3F &pos, const Point2F &size, const ColorI &color )
{
   GFXVertexBufferHandle<GFXVertexPC> verts(mDevice, 4, GFXBufferTypeVolatile);
   verts.lock();

   verts[0].point = pos + Point3F( -size.x / 2.0f, -size.y / 2.0f, 0 );
   verts[0].color = color;
   verts[1].point = pos + Point3F( -size.x / 2.0f, size.y / 2.0f, 0 );
   verts[1].color = color;
   verts[2].point = pos + Point3F( size.x / 2.0f, size.y / 2.0f, 0 );
   verts[2].color = color;
   verts[3].point = pos + Point3F( size.x / 2.0f, -size.y / 2.0f, 0 );
   verts[3].color = color;

   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXTriangleFan, 0, 2 );
}

void GFXDrawUtil::drawPlaneGrid( const GFXStateBlockDesc &desc, const Point3F &pos, const Point2F &size, const Point2F &step, const ColorI &color )
{
   // Note that when calculating the number of steps, we +0.5 to round up,
   // and +1 for the last line (ie. 4 steps needs 5 lines to be rendered)
   U32 xSteps = 0;
   if ( step.x > 0 )
      xSteps = size.x / step.x + 0.5 + 1;

   U32 ySteps = 0;
   if ( step.y > 0 )
      ySteps = size.y / step.y + 0.5 + 1;

   if ( xSteps <= 1 || ySteps <= 1 )
      return;

   GFXVertexBufferHandle<GFXVertexPC> verts( mDevice, ( xSteps * 2 ) + ( ySteps * 2 ), GFXBufferTypeVolatile );
   verts.lock();

   U32 vertCount = 0;

   Point3F origin( pos.x - ( size.x / 2.0f ), pos.y - ( size.y / 2.0f ), pos.z );

   F32 start = mFloor( origin.x / step.x + 0.5f ) * step.x;

   for ( U32 i = 0; i < xSteps; i++ )
   {
      verts[vertCount].point = Point3F( start + step.x * i, origin.y, origin.z );
      verts[vertCount].color = color;
      ++vertCount;

      verts[vertCount].point = Point3F( start + step.x * i, origin.y + size.y, origin.z );
      verts[vertCount].color = color;
      ++vertCount;
   }

   start = mFloor( origin.y / step.y + 0.5f ) * step.y;

   for ( U32 i = 0; i < ySteps; i++ )
   {
      verts[vertCount].point = Point3F( origin.x, start + step.y * i, origin.z );
      verts[vertCount].color = color;
      ++vertCount;

      verts[vertCount].point = Point3F( origin.x + size.x, start + step.y * i, origin.z );
      verts[vertCount].color = color;
      ++vertCount;
   }

   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();

   mDevice->drawPrimitive( GFXLineList, 0, xSteps + ySteps );
}

void GFXDrawUtil::drawTransform( const GFXStateBlockDesc &desc, const MatrixF &mat, const Point3F &scale, const ColorI &color )
{
   GFXTransformSaver saver;

   GFX->multWorld( mat );

   GFXVertexBufferHandle<GFXVertexPC> verts( mDevice, 6, GFXBufferTypeVolatile );
   verts.lock();

   verts[0].point = Point3F::Zero;
   verts[0].color = color;
   verts[1].point = Point3F( 1, 0, 0 );
   verts[1].color = color;
   verts[2].point = Point3F::Zero;
   verts[2].color = color;
   verts[3].point = Point3F( 0, 1, 0 );
   verts[3].color = color;
   verts[4].point = Point3F::Zero;
   verts[4].color = color;
   verts[5].point = Point3F( 0, 0, 1 );
   verts[5].color = color;

   verts.unlock();

   mDevice->setStateBlockByDesc( desc );

   mDevice->setVertexBuffer( verts );
   mDevice->setupGenericShaders();
   mDevice->drawPrimitive( GFXLineList, 0, 6 );
}