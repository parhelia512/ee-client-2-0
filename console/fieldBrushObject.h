//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _FIELDBRUSHOBJECT_H_
#define _FIELDBRUSHOBJECT_H_

#ifndef _CONSOLEINTERNAL_H_
#include "console/consoleInternal.h"
#endif

#ifndef CORE_TDICTIONARY_H
#include "core/util/tDictionary.h"
#endif

//-----------------------------------------------------------------------------
// Field Brush Object.
//-----------------------------------------------------------------------------

/// FieldBrushObject for static-field copying/pasting.
///
class FieldBrushObject : public SimObject
{
private:
    typedef SimObject Parent;

    // Destroy Fields.
    void destroyFields( void );

    StringTableEntry    mDescription;                   ///< Description.
    StringTableEntry    mSortName;                      ///< Sort Name.

public:
    FieldBrushObject();

    void copyFields( SimObject* pSimObject, const char* fieldList );
    void pasteFields( SimObject* pSimObject );
    
    static bool setDescription(void* obj, const char* data) { static_cast<FieldBrushObject*>(obj)->setDescription(data); return false; };
    void setDescription( const char* description )  { mDescription = StringTable->insert(description); }
    StringTableEntry getDescription(void) const     { return mDescription; }

    static bool setSortName(void* obj, const char* data) { static_cast<FieldBrushObject*>(obj)->setSortName(data); return false; };
    void setSortName( const char* sortName )  { mSortName = StringTable->insert(sortName); }
    StringTableEntry getSortName(void) const     { return mSortName; }

    static void initPersistFields();                    ///< Persist Fields.
    virtual void onRemove();                            ///< Called when the object is removed from the sim.

    DECLARE_CONOBJECT(FieldBrushObject);
};

#endif