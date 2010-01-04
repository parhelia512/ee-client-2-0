//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _DYNAMIC_CONSOLETYPES_H_
#define _DYNAMIC_CONSOLETYPES_H_

#ifndef _SIMBASE_H_
#include "console/simBase.h"
#endif

class ConsoleBaseType
{
protected:
   /// This is used to generate unique IDs for each type.
   static S32 smConsoleTypeCount;

   /// We maintain a linked list of all console types; this is its head.
   static ConsoleBaseType *smListHead;

   /// Next item in the list of all console types.
   ConsoleBaseType *mListNext;

   /// Destructor is private to avoid people mucking up the list.
   ~ConsoleBaseType();

   S32      mTypeID;
   dsize_t  mTypeSize;
   const char *mTypeName;
   const char *mInspectorFieldType;

public:

   /// @name cbt_list List Interface
   ///
   /// Interface for accessing/traversing the list of types.

   /// Get the head of the list.
   static ConsoleBaseType *getListHead();

   /// Get the item that follows this item in the list.
   ConsoleBaseType *getListNext() const
   {
      return mListNext;
   }

   /// Called once to initialize the console type system.
   static void initialize();

   /// Call me to get a pointer to a type's info.
   static ConsoleBaseType *getType(const S32 typeID);

   /// Call to get a pointer to a type's info
   static ConsoleBaseType *getTypeByName( const char *typeName );
   static ConsoleBaseType *getTypeByClassName( const char *typeName );
   /// @}

   /// The constructor is responsible for linking an element into the
   /// master list, registering the type ID, etc.
   ConsoleBaseType(const S32 size, S32 *idPtr, const char *aTypeName);

   const S32 getTypeID() const { return mTypeID; }
   const S32 getTypeSize() const { return mTypeSize; }
   const char *getTypeName() const { return mTypeName; }
   
   void setInspectorFieldType(const char *type) { mInspectorFieldType = type; }
   const char *getInspectorFieldType() { return mInspectorFieldType; }

   virtual void setData(void *dptr, S32 argc, const char **argv, const EnumTable *tbl, BitSet32 flag)=0;
   virtual const char *getData(void *dptr, const EnumTable *tbl, BitSet32 flag )=0;
   virtual const char *getTypeClassName()=0;
   virtual void       *getNativeVariable()=0;
   virtual void        deleteNativeVariable(void* var) = 0;
   virtual const bool isDatablock() { return false; };
   virtual const char *prepData(const char *data, char *buffer, U32 bufferLen) { return data; };
};

#define DefineConsoleType( type, nativeType ) \
   extern S32 type; \
   extern const char *castConsoleTypeToString( nativeType arg ); \
   extern bool castConsoleTypeFromString( nativeType &arg, const char *str );

#define ConsoleType( typeName, type, nativeType ) \
   class ConsoleType##type : public ConsoleBaseType \
   { \
   public: \
      ConsoleType##type (const S32 aSize, S32 *idPtr, const char *aTypeName) : ConsoleBaseType(aSize, idPtr, aTypeName) { nativeType *make_Sure_You_Dont_Pass_SizeOf_Here = (nativeType*)0; (void)make_Sure_You_Dont_Pass_SizeOf_Here; } \
      virtual void setData(void *dptr, S32 argc, const char **argv, const EnumTable *tbl, BitSet32 flag); \
      virtual const char *getData(void *dptr, const EnumTable *tbl, BitSet32 flag ); \
      virtual const char *getTypeClassName() { return #typeName ; } \
      virtual void       *getNativeVariable() { nativeType* var = new nativeType; return (void*)var; } \
      virtual void        deleteNativeVariable(void* var) { nativeType* nativeVar = reinterpret_cast<nativeType*>(var); delete nativeVar; }\
   }; \
   S32 type = -1; \
   ConsoleType##type gConsoleType##type##Instance(sizeof(nativeType),&type,#type); \

#define ImplementConsoleTypeCasters(type, nativeType) \
   const char *castConsoleTypeToString( nativeType arg ) { return Con::getData(type, &arg, 0); } \
   bool castConsoleTypeFromString( nativeType &arg, const char *str ) { Con::setData(type, &arg, 0, 1, &str); return true; }


#define ConsolePrepType( typeName, type, nativeType ) \
   class ConsoleType##type : public ConsoleBaseType \
   { \
   public: \
      ConsoleType##type (const S32 aSize, S32 *idPtr, const char *aTypeName) : ConsoleBaseType(aSize, idPtr, aTypeName) { } \
      virtual void setData(void *dptr, S32 argc, const char **argv, const EnumTable *tbl, BitSet32 flag); \
      virtual const char *getData(void *dptr, const EnumTable *tbl, BitSet32 flag ); \
      virtual const char *getTypeClassName() { return #typeName; }; \
      virtual void       *getNativeVariable() { nativeType* var = new nativeType; return (void*)var; } \
      virtual void        deleteNativeVariable(void* var) { nativeType* nativeVar = reinterpret_cast<nativeType*>(var); delete nativeVar; }\
      virtual const char *prepData(const char *data, char *buffer, U32 bufferLen); \
   }; \
   S32 type = -1; \
   ConsoleType##type gConsoleType##type##Instance(sizeof( nativeType ),&type,#type); \

#define ConsoleSetType( type ) \
   void ConsoleType##type::setData(void *dptr, S32 argc, const char **argv, const EnumTable *tbl, BitSet32 flag)

#define ConsoleGetType( type ) \
   const char *ConsoleType##type::getData(void *dptr, const EnumTable *tbl, BitSet32 flag)

#define ConsoleProcessData( type ) \
   const char *ConsoleType##type::prepData(const char *data, char *buffer, U32 bufferSz)

#define DatablockConsoleType( typeName, type, size, className ) \
   class ConsoleType##type : public ConsoleBaseType \
   { \
   public: \
      ConsoleType##type (const S32 aSize, S32 *idPtr, const char *aTypeName) : ConsoleBaseType(aSize, idPtr, aTypeName) { } \
      virtual void setData(void *dptr, S32 argc, const char **argv, const EnumTable *tbl, BitSet32 flag); \
      virtual const char *getData(void *dptr, const EnumTable *tbl, BitSet32 flag ); \
      virtual const char *getTypeClassName() { return #className; }; \
      virtual void       *getNativeVariable() { className* var = new className; return (void*)var; } \
      virtual void        deleteNativeVariable(void* var) { className* nativeVar = reinterpret_cast<className*>(var); delete nativeVar; } \
      virtual const bool isDatablock() { return true; }; \
   }; \
   S32 type = -1; \
   ConsoleType##type gConsoleType##type##Instance(size,&type,#type); \


#endif
