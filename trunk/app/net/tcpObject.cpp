//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "app/net/tcpObject.h"

#include "platform/platform.h"
#include "platform/event.h"
#include "console/simBase.h"
#include "console/consoleInternal.h"

TCPObject *TCPObject::table[TCPObject::TableSize] = {0, };

IMPLEMENT_CONOBJECT(TCPObject);

TCPObject *TCPObject::find(NetSocket tag)
{
   for(TCPObject *walk = table[U32(tag) & TableMask]; walk; walk = walk->mNext)
      if(walk->mTag == tag)
         return walk;
   return NULL;
}

void TCPObject::addToTable(NetSocket newTag)
{
   removeFromTable();
   mTag = newTag;
   mNext = table[U32(mTag) & TableMask];
   table[U32(mTag) & TableMask] = this;
}

void TCPObject::removeFromTable()
{
   for(TCPObject **walk = &table[U32(mTag) & TableMask]; *walk; walk = &((*walk)->mNext))
   {
      if(*walk == this)
      {
         *walk = mNext;
         return;
      }
   }
}

void processConnectedReceiveEvent(NetSocket sock, RawData incomingData);
void processConnectedAcceptEvent(NetSocket listeningPort, NetSocket newConnection, NetAddress originatingAddress);
void processConnectedNotifyEvent( NetSocket sock, U32 state );

S32 gTCPCount = 0;

TCPObject::TCPObject()
{
   mBuffer = NULL;
   mBufferSize = 0;
   mPort = 0;
   mTag = InvalidSocket;
   mNext = NULL;
   mState = Disconnected;

   gTCPCount++;

   if(gTCPCount == 1)
   {
      Net::smConnectionAccept.notify(processConnectedAcceptEvent);
      Net::smConnectionReceive.notify(processConnectedReceiveEvent);
      Net::smConnectionNotify.notify(processConnectedNotifyEvent);
   }
}

TCPObject::~TCPObject()
{
   disconnect();
   dFree(mBuffer);

   gTCPCount--;

   if(gTCPCount == 0)
   {
      Net::smConnectionAccept.remove(processConnectedAcceptEvent);
      Net::smConnectionReceive.remove(processConnectedReceiveEvent);
      Net::smConnectionNotify.remove(processConnectedNotifyEvent);
   }
}

bool TCPObject::processArguments(S32 argc, const char **argv)
{
   if(argc == 0)
      return true;
   else if(argc == 1)
   {
      addToTable(U32(dAtoi(argv[0])));
      return true;
   }
   return false;
}

bool TCPObject::onAdd()
{
   if(!Parent::onAdd())
      return false;

   const char *name = getName();

   if(name && name[0] && getClassRep())
   {
      Namespace *parent = getClassRep()->getNameSpace();
      Con::linkNamespaces(parent->mName, name);
      mNameSpace = Con::lookupNamespace(name);

   }

   Sim::getTCPGroup()->addObject(this);

   return true;
}

U32 TCPObject::onReceive(U8 *buffer, U32 bufferLen)
{
   // we got a raw buffer event
   // default action is to split the buffer into lines of text
   // and call processLine on each
   // for any incomplete lines we have mBuffer
   U32 start = 0;
   parseLine(buffer, &start, bufferLen);
   return start;
}

void TCPObject::parseLine(U8 *buffer, U32 *start, U32 bufferLen)
{
   // find the first \n in buffer
   U32 i;
   U8 *line = buffer + *start;

   for(i = *start; i < bufferLen; i++)
      if(buffer[i] == '\n' || buffer[i] == 0)
         break;
   U32 len = i - *start;

   if(i == bufferLen || mBuffer)
   {
      // we've hit the end with no newline
      mBuffer = (U8 *) dRealloc(mBuffer, mBufferSize + len + 1);
      dMemcpy(mBuffer + mBufferSize, line, len);
      mBufferSize += len;
      *start = i;

      // process the line
      if(i != bufferLen)
      {
         mBuffer[mBufferSize] = 0;
         if(mBufferSize && mBuffer[mBufferSize-1] == '\r')
            mBuffer[mBufferSize - 1] = 0;
         U8 *temp = mBuffer;
         mBuffer = 0;
         mBufferSize = 0;

         processLine((UTF8*)temp);
         dFree(temp);
      }
   }
   else if(i != bufferLen)
   {
      line[len] = 0;
      if(len && line[len-1] == '\r')
         line[len-1] = 0;
      processLine((UTF8*)line);
   }
   if(i != bufferLen)
      *start = i + 1;
}

void TCPObject::onConnectionRequest(const NetAddress *addr, U32 connectId)
{
   char idBuf[16];
   char addrBuf[256];
   Net::addressToString(addr, addrBuf);
   dSprintf(idBuf, sizeof(idBuf), "%d", connectId);
   Con::executef(this, "onConnectRequest", addrBuf, idBuf);
}

bool TCPObject::processLine(UTF8 *line)
{
   Con::executef(this, "onLine", line);
   return true;
}

void TCPObject::onDNSResolved()
{
   mState = DNSResolved;
   Con::executef(this, "onDNSResolved");
}

void TCPObject::onDNSFailed()
{
   mState = Disconnected;
   Con::executef(this, "onDNSFailed");
}

void TCPObject::onConnected()
{
   mState = Connected;
   Con::executef(this, "onConnected");
}

void TCPObject::onConnectFailed()
{
   mState = Disconnected;
   Con::executef(this, "onConnectFailed");
}

void TCPObject::finishLastLine()
{
   if(mBufferSize)
   {
      mBuffer[mBufferSize] = 0;
      processLine((UTF8*)mBuffer);
      dFree(mBuffer);
      mBuffer = 0;
      mBufferSize = 0;
   }
}

void TCPObject::onDisconnect()
{
   finishLastLine();
   mState = Disconnected;
   Con::executef(this, "onDisconnect");
}

void TCPObject::listen(U16 port)
{
   mState = Listening;
   U32 newTag = Net::openListenPort(port);
   addToTable(newTag);
}

void TCPObject::connect(const char *address)
{
   NetSocket newTag = Net::openConnectTo(address);
   addToTable(newTag);
}

void TCPObject::disconnect()
{
   if( mTag != InvalidSocket ) {
      Net::closeConnectTo(mTag);
   }
   removeFromTable();
}

void TCPObject::send(const U8 *buffer, U32 len)
{
   Net::sendtoSocket(mTag, buffer, S32(len));
}

ConsoleMethod( TCPObject, send, void, 3, 0, "(...)"
              "Parameters are transmitted as strings, one at a time.")
{
   for(S32 i = 2; i < argc; i++)
      object->send((const U8 *) argv[i], dStrlen(argv[i]));
}

ConsoleMethod( TCPObject, listen, void, 3, 3, "(int port)"
              "Start listening on the specified ports for connections.")
{
   object->listen(U32(dAtoi(argv[2])));
}

ConsoleMethod( TCPObject, connect, void, 3, 3, "(string addr)"
              "Connect to the given address.")
{
   object->connect(argv[2]);
}

ConsoleMethod( TCPObject, disconnect, void, 2, 2, "Disconnect from whatever we're connected to, if anything.")
{
   object->disconnect();
}

void processConnectedReceiveEvent(NetSocket sock, RawData incomingData)
{
   TCPObject *tcpo = TCPObject::find(sock);
   if(!tcpo)
   {
      Con::printf("Got bad connected receive event.");
      return;
   }

   U32 size = incomingData.size;
   U8 *buffer = (U8*)incomingData.data;

   while(size)
   {
      U32 ret = tcpo->onReceive(buffer, size);
      AssertFatal(ret <= size, "Invalid return size");
      size -= ret;
      buffer += ret;
   }
}

void processConnectedAcceptEvent(NetSocket listeningPort, NetSocket newConnection, NetAddress originatingAddress)
{
   TCPObject *tcpo = TCPObject::find(listeningPort);
   if(!tcpo)
      return;

   tcpo->onConnectionRequest(&originatingAddress, newConnection);
}

void processConnectedNotifyEvent( NetSocket sock, U32 state )
{
   TCPObject *tcpo = TCPObject::find(sock);
   if(!tcpo)
      return;

   switch(state)
   {
      case Net::DNSResolved:
         tcpo->onDNSResolved();
         break;
      case Net::DNSFailed:
         tcpo->onDNSFailed();
         break;
      case Net::Connected:
         tcpo->onConnected();
         break;
      case Net::ConnectFailed:
         tcpo->onConnectFailed();
         break;
      case Net::Disconnected:
         tcpo->onDisconnect();
         break;
   }
}
