//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "core/util/zip/compressor.h"

#include "core/util/zip/zipSubStream.h"

namespace Zip
{

ImplementCompressor(Deflate, Deflated);

CompressorCreateReadStream(Deflate)
{
   ZipSubRStream *stream = new ZipSubRStream;
   stream->attachStream(zipStream);
   stream->setUncompressedSize(cdir->mUncompressedSize);

   return stream;
}

CompressorCreateWriteStream(Deflate)
{
   ZipSubWStream *stream = new ZipSubWStream;
   stream->attachStream(zipStream);

   return stream;
}

} // end namespace Zip
