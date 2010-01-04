//-----------------------------------------------------------------------------
// TGA Loader
// Copyright 2009 3D Interactive Inc. All Rights Reserved
//
// Only known to work with Purelight's lightmap tga files.
//-----------------------------------------------------------------------------

#include "core/stream/stream.h"

#include "gfx/bitmap/gBitmap.h"


static bool sReadTGA(Stream &stream, GBitmap *bitmap);
static bool sWriteTGA(GBitmap *bitmap, Stream &stream, U32 compressionLevel);

static struct _privateRegisterTGA
{
   _privateRegisterTGA()
   {
      GBitmap::Registration reg;

      reg.extensions.push_back( "tga" );

      reg.readFunc = sReadTGA;
      reg.writeFunc = sWriteTGA;

      GBitmap::sRegisterFormat( reg );
   }
} sStaticRegisterTGA;


//------------------------------------------------------------------------------
//-------------------------------------- Supplementary I/O (Partially located in
//                                                          bitmapPng.cc)
//

static bool sReadTGA(Stream &stream, GBitmap *bitmap)
{
	U8 idLength;
	stream.read(&idLength);
	
	U8 colormapType;
	stream.read(&colormapType);

	U8 imageType; //2 = rgb, not sure what the other types are
	stream.read(&imageType);

	//Not sure what these are, none of our tgas use them
	U16 colormapStart;
	stream.read(&colormapStart);
	U16 colormapLength;
	stream.read(&colormapLength);
	U8 colormapDepth;
	stream.read(&colormapDepth);

	//Never seen these actually used
	U16 xOrigin;
	stream.read(&xOrigin);
	U16 yOrigin;
	stream.read(&yOrigin);

	U16 width;
	stream.read(&width);
	U16 height;
	stream.read(&height);

	U8 colorDepth;
	stream.read(&colorDepth);

	U8 imageDescriptor; //always 0 for us
	stream.read(&imageDescriptor);


	GFXFormat fmt;
	if(colorDepth == 24)
	{
		fmt = GFXFormatR8G8B8;
	}
	else if(colorDepth == 32)
	{
		fmt = GFXFormatR8G8B8A8;
	}
	else
	{
		//not sure how to handle other color depths
		fmt = GFXFormatR8G8B8;
	}
  
   bitmap->allocateBitmap(width, height, false, fmt);
   U32 bmpWidth  = bitmap->getWidth();
   U32 bmpHeight = bitmap->getHeight();
   U32 bmpBytesPerPixel = bitmap->getBytesPerPixel();

   for(U32 row = 0; row < bmpHeight; row++)
   {
	    U8* rowDest = bitmap->getAddress(0, bmpHeight - row - 1);
		stream.read(bmpBytesPerPixel * bmpWidth, rowDest);
   }

   if(bmpBytesPerPixel == 3 || bmpBytesPerPixel == 4) // do BGR swap
   {
      U8* ptr = bitmap->getAddress(0, 0);
      for(int i = 0; i < bmpWidth * bmpHeight; i++)
      {
         U8 tmp = ptr[0];
         ptr[0] = ptr[2];
         ptr[2] = tmp;
         ptr += bmpBytesPerPixel;
      }
   }

   //32 bit tgas have an alpha channel
   bitmap->setHasTransparency(colorDepth == 32);

   return true;
}

static bool sWriteTGA(GBitmap *bitmap, Stream &stream, U32 compressionLevel)
{
	AssertISV(false, "GBitmap::writeTGA - doesn't support writing tga files!")

   return false;
}
