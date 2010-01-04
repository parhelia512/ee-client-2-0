//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "core/util/zip/compressor.h"

#include "core/resizeStream.h"

namespace Zip
{

ImplementCompressor(Stored, Stored);

CompressorCreateReadStream(Stored)
{
   ResizeFilterStream *resStream = new ResizeFilterStream;
   resStream->attachStream(zipStream);
   resStream->setStreamOffset(zipStream->getPosition(), cdir->mCompressedSize);

   return resStream;
}

CompressorCreateWriteStream(Stored)
{
   return zipStream;
}

} // end namespace Zip
