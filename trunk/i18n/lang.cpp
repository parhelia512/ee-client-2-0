//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "core/stream/stream.h"
#include "core/stream/fileStream.h"
#include "console/console.h"
#include "console/consoleInternal.h"
#include "console/ast.h"
#include "console/compiler.h"
#include "core/util/safeDelete.h"

#include "i18n/lang.h"

//-----------------------------------------------------------------------------
// LangFile Class
//-----------------------------------------------------------------------------

LangFile::LangFile(const UTF8 *langName /* = NULL */)
{
   VECTOR_SET_ASSOCIATION( mStringTable );

	if(langName)
	{
		mLangName = new UTF8 [dStrlen(langName) + 1];
		dStrcpy(mLangName, langName);
	}
	else
		mLangName = NULL;

	mLangFile = NULL;
}

LangFile::~LangFile()
{
	// [tom, 3/1/2005] Note: If this is freed in FreeTable() then when the file
	// is loaded, the language name will be blitzed.
	// Programming after 36 hours without sleep != good.

   SAFE_DELETE_ARRAY(mLangName);
   SAFE_DELETE_ARRAY(mLangFile);
	freeTable();
}

void LangFile::freeTable()
{
	U32 i;
	for(i = 0;i < mStringTable.size();i++)
	{
		if(mStringTable[i])
			delete [] mStringTable[i];
	}
	mStringTable.clear();
}

bool LangFile::load(const UTF8 *filename)
{
	FileStream * stream;
   if((stream = FileStream::createAndOpen( filename, Torque::FS::File::Read )) == NULL)
      return false;

	bool ret = load(stream);
	delete stream;
   return ret;
}

bool LangFile::load(Stream *s)
{
	freeTable();
	
	while(s->getStatus() != Stream::EOS)
	{
		char buf[256];
		s->readString(buf);
		addString((const UTF8*)buf);
	}
	return true;
}

bool LangFile::save(const UTF8 *filename)
{
	FileStream *fs;
	
	if(!isLoaded())
		return false;

   if((fs = FileStream::createAndOpen( filename, Torque::FS::File::Write )) == NULL)
      return false;

	bool ret = save(fs);
	delete fs;

   return ret;
}

bool LangFile::save(Stream *s)
{
	if(!isLoaded())
		return false;
	
	U32 i;
	for(i = 0;i < mStringTable.size();i++)
	{
		s->writeString((char*)mStringTable[i]);
	}
	return true;
}

const UTF8 * LangFile::getString(U32 id)
{
	if(id == LANG_INVALID_ID || id >= mStringTable.size())
		return NULL;
	return mStringTable[id];
}

U32 LangFile::addString(const UTF8 *str)
{
	UTF8 *newstr = new UTF8 [dStrlen(str) + 1];
	dStrcpy(newstr, str);
	mStringTable.push_back(newstr);
	return mStringTable.size() - 1;
}

void LangFile::setString(U32 id, const UTF8 *str)
{
	if(id >= mStringTable.size())
		mStringTable.setSize(id+1);

	UTF8 *newstr = new UTF8 [dStrlen(str) + 1];
	dStrcpy(newstr, str);
	mStringTable[id] = newstr;
}

void LangFile::setLangName(const UTF8 *newName)
{
	if(mLangName)
		delete [] mLangName;
	
	mLangName = new UTF8 [dStrlen(newName) + 1];
	dStrcpy(mLangName, newName);
}

void LangFile::setLangFile(const UTF8 *langFile)
{
	if(mLangFile)
		delete [] mLangFile;
	
	mLangFile = new UTF8 [dStrlen(langFile) + 1];
	dStrcpy(mLangFile, langFile);
}

bool LangFile::activateLanguage()
{
	if(isLoaded())
		return true;

	if(mLangFile)
	{
		return load(mLangFile);
	}
	return false;
}

void LangFile::deactivateLanguage()
{
	if(mLangFile && isLoaded())
		freeTable();
}

//-----------------------------------------------------------------------------
// LangTable Class
//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(LangTable);

LangTable::LangTable() : mDefaultLang(-1), mCurrentLang(-1)
{
   VECTOR_SET_ASSOCIATION( mLangTable );
}

LangTable::~LangTable()
{
	S32 i;

	for(i = 0;i < mLangTable.size();i++)
	{
		if(mLangTable[i])
			delete mLangTable[i];
	}
	mLangTable.clear();
}

S32 LangTable::addLanguage(LangFile *lang, const UTF8 *name /* = NULL */)
{
	if(name)
		lang->setLangName(name);

	mLangTable.push_back(lang);

	if(mDefaultLang == -1)
		setDefaultLanguage(mLangTable.size() - 1);
	if(mCurrentLang == -1)
		setCurrentLanguage(mLangTable.size() - 1);

	return mLangTable.size() - 1;
}

S32 LangTable::addLanguage(const UTF8 *filename, const UTF8 *name /* = NULL */)
{
	LangFile * lang = new LangFile(name);
	if(lang != NULL)
	{
      if(Torque::FS::IsFile(filename))
		{
			lang->setLangFile(filename);
			
      	S32 ret = addLanguage(lang);
			if(ret >= 0)
				return ret;
		}
		delete lang;
	}

	return -1;
}

const UTF8 *LangTable::getString(const U32 id) const
{
	const UTF8 *s = NULL;

	if(mCurrentLang >= 0)
		s = mLangTable[mCurrentLang]->getString(id);
	if(s == NULL && mDefaultLang >= 0 && mDefaultLang != mCurrentLang)
		s = mLangTable[mDefaultLang]->getString(id);

	return s;
}

const U32 LangTable::getStringLength(const U32 id) const
{
	const UTF8 *s = getString(id);
	if(s)
		return dStrlen(s);
	
	return 0;
}

void LangTable::setDefaultLanguage(S32 langid)
{
	if(langid >= 0 && langid < mLangTable.size())
	{
		if(mLangTable[langid]->activateLanguage())
		{
			if(mDefaultLang >= 0)
				mLangTable[mDefaultLang]->deactivateLanguage();
			
			mDefaultLang = langid;
		}
	}
}

void LangTable::setCurrentLanguage(S32 langid)
{
	if(langid >= 0 && langid < mLangTable.size())
	{
		if(mLangTable[langid]->activateLanguage())
		{
			Con::printf("Language %s [%s] activated.", mLangTable[langid]->getLangName(), mLangTable[langid]->getLangFile());

			if(mCurrentLang >= 0 && mCurrentLang != mDefaultLang)
			{
				mLangTable[mCurrentLang]->deactivateLanguage();
				Con::printf("Language %s [%s] deactivated.", mLangTable[mCurrentLang]->getLangName(), mLangTable[mCurrentLang]->getLangFile());
			}
			mCurrentLang = langid;
		}
	}
}

//-----------------------------------------------------------------------------
// LangTable Console Methods
//-----------------------------------------------------------------------------

ConsoleMethod(LangTable, addLanguage, S32, 3, 4, "(string filename, [string languageName])")
{
	UTF8 scriptFilenameBuffer[1024];
	
	Con::expandScriptFilename((char*)scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[2]);
	return object->addLanguage(scriptFilenameBuffer, argc == 4 ? (const UTF8*)argv[3] : NULL);
}

ConsoleMethod(LangTable, getString, const char *, 3, 3, "(string filename)")
{
	const char * str =	(const char*)object->getString(dAtoi(argv[2]));
	if(str != NULL)
	{
		char * ret = Con::getReturnBuffer(dStrlen(str) + 1);
		dStrcpy(ret, str);
		return ret;
	}
	
	return "";
}

ConsoleMethod(LangTable, setDefaultLanguage, void, 3, 3, "(int language)")
{
	object->setDefaultLanguage(dAtoi(argv[2]));
}

ConsoleMethod(LangTable, setCurrentLanguage, void, 3, 3, "(int language)")
{
	object->setCurrentLanguage(dAtoi(argv[2]));
}

ConsoleMethod(LangTable, getCurrentLanguage, S32, 2, 2, "()")
{
	return object->getCurrentLanguage();
}

ConsoleMethod(LangTable, getLangName, const char *, 3, 3, "(int language)")
{
	const char * str = (const char*)object->getLangName(dAtoi(argv[2]));
	if(str != NULL)
	{
		char * ret = Con::getReturnBuffer(dStrlen(str) + 1);
		dStrcpy(ret, str);
		return ret;
	}
	
	return "";
}

ConsoleMethod(LangTable, getNumLang, S32, 2, 2, "()")
{
	return object->getNumLang();
}

//-----------------------------------------------------------------------------
// Support Functions
//-----------------------------------------------------------------------------

UTF8 *sanitiseVarName(const UTF8 *varName, UTF8 *buffer, U32 bufsize)
{
	if(! varName || bufsize < 10)	// [tom, 3/3/2005] bufsize check gives room to be lazy below
	{
		*buffer = 0;
		return NULL;
	}
	
	dStrcpy(buffer, (const UTF8*)"I18N::");
	
	UTF8 *dptr = buffer + 6;
	const UTF8 *sptr = varName;
	while(*sptr)
	{
		if(dIsalnum(*sptr))
			*dptr++ = *sptr++;
		else
		{
			if(*(dptr - 1) != '_')
				*dptr++ = '_';
			sptr++;
		}
		
		if((dptr - buffer) >= (bufsize - 1))
			break;
	}
	*dptr = 0;
	
	return buffer;
}

UTF8 *getCurrentModVarName(UTF8 *buffer, U32 bufsize)
{
	char varName[256];
	StringTableEntry cbName = CodeBlock::getCurrentCodeBlockName();
	
	const UTF8 *slash = (const UTF8*)dStrchr(cbName, '/');
	if (slash == NULL)
	{
		Con::errorf("Illegal CodeBlock path detected in sanitiseVarName() (no mod directory): %s", cbName);
		return NULL;
	}
	
	dStrncpy(varName, cbName, slash - (const UTF8*)cbName);
	varName[slash - (const UTF8*)cbName] = 0;

	return sanitiseVarName((UTF8*)varName, buffer, bufsize);
}

const LangTable *getCurrentModLangTable()
{
	UTF8 saneVarName[256];
	
	if(getCurrentModVarName(saneVarName, sizeof(saneVarName)))
	{
		const LangTable *lt = dynamic_cast<LangTable *>(Sim::findObject(Con::getIntVariable((const char*)saneVarName)));
		return lt;
	}
	return NULL;
}

const LangTable *getModLangTable(const UTF8 *mod)
{
	UTF8 saneVarName[256];

	if(sanitiseVarName(mod, saneVarName, sizeof(saneVarName)))
	{
		const LangTable *lt = dynamic_cast<LangTable *>(Sim::findObject(Con::getIntVariable((const char*)saneVarName)));
		return lt;
	}
	return NULL;
}
