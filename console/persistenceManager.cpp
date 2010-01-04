//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "persistenceManager.h"
#include "console/simSet.h"
#include "console/consoleTypes.h"
#include "core/stream/fileStream.h"
#include "gui/core/guiTypes.h"
#include "materials/customMaterialDefinition.h"

// Helper function
char* packU32List(Vector<U32>& values)
{
   // First determine how big the buffer needs to be
   U32 size = 0;

   for (U32 i = 0; i < values.size(); i++)
   {
      if (values[i] < 10)
         size += 2;
      else if (values[i] < 100)
         size += 3;
      else if (values[i] < 1000)
         size += 4;
      else if (values[i] < 10000)
         size += 5;
      else if (values[i] < 100000)
         size += 6;
      else if (values[i] < 1000000)
         size += 7;
      else if (values[i] < 10000000)
         size += 8;
      else if (values[i] < 100000000)
         size += 9;
      else if (values[i] < 1000000000)
         size += 10;
      else
         Con::errorf("This number is far too high: %d", values[i]);
   }

   // Now create our return buffer
   char* buff = Con::getReturnBuffer(size + 1);

   char* walk = buff;

   for (U32 i = 0; i < values.size(); i++)
   {
      U32 valueSize = 0;

      if (values[i] < 10)
         valueSize += 2;
      else if (values[i] < 100)
         valueSize += 3;
      else if (values[i] < 1000)
         valueSize += 4;
      else if (values[i] < 10000)
         valueSize += 5;
      else if (values[i] < 100000)
         valueSize += 6;
      else if (values[i] < 1000000)
         valueSize += 7;
      else if (values[i] < 10000000)
         valueSize += 8;
      else if (values[i] < 100000000)
         valueSize += 9;
      else if (values[i] < 1000000000)
         valueSize += 10;

      if (valueSize > 0)
         dSprintf(walk, valueSize + 1, "%d ", values[i]);

      walk += valueSize;

      *walk = 0;
   }

   // trim off the final space
   if (walk && *walk == ' ')
      *(walk - 1) = 0;

   return buff;
}

IMPLEMENT_CONOBJECT(PersistenceManager);

PersistenceManager::PersistenceManager()
{
   mCurrentObject = NULL;
   mCurrentFile = NULL;

   VECTOR_SET_ASSOCIATION(mLineBuffer);

   mLineBuffer.reserve(2048);
}

PersistenceManager::~PersistenceManager()
{
   mDirtyObjects.clear();
}

bool PersistenceManager::onAdd()
{
   if (!Parent::onAdd())
      return false;

   return true;
}

void PersistenceManager::onRemove()
{
   Parent::onRemove();
}

void PersistenceManager::clearLineBuffer()
{
   for (U32 i = 0; i < mLineBuffer.size(); i++)
      SAFE_DELETE_ARRAY(mLineBuffer[i]);

   mLineBuffer.clear();
}

void PersistenceManager::deleteObject(ParsedObject* object)
{
   if (object)
   {
      // Clear up used property memory
      for (U32 j = 0; j < object->properties.size(); j++)
      {
         ParsedProperty& prop = object->properties[j];

         if (prop.value)
            SAFE_DELETE(prop.value);
      }

      object->properties.clear();

      // Delete the parsed object
      SAFE_DELETE(object);
   }
}

void PersistenceManager::clearObjects()
{
   // Clean up the object buffer
   for (U32 i = 0; i < mObjectBuffer.size(); i++)
      deleteObject(mObjectBuffer[i]);

   mObjectBuffer.clear();

   // We shouldn't have anything in the object stack
   // but let's clean it up just in case
   // Clean up the object buffer
   for (U32 i = 0; i < mObjectStack.size(); i++)
      deleteObject(mObjectStack[i]);

   mObjectStack.clear();

   // Finally make sure there isn't a current object
   deleteObject(mCurrentObject);
}

void PersistenceManager::clearFileData()
{
   // Clear the active file name
   if (mCurrentFile)
      SAFE_DELETE(mCurrentFile);

   // Clear the file objects
   clearObjects();

   // Clear the line buffer
   clearLineBuffer();

   // Clear the tokenizer data
   mParser.clear();
}

void PersistenceManager::clearAll()
{
   // Clear the file data in case it hasn't cleared yet
   clearFileData();

   // Clear the dirty object list
   mDirtyObjects.clear();

   // Clear the remove field list
   mRemoveFields.clear();
}

bool PersistenceManager::readFile(const char* fileName)
{
   // Clear our previous file buffers just in
   // case saveDirtyFile() didn't catch it
   clearFileData();

   // Handle an object writing out to a new file
   if ( !Torque::FS::IsFile( fileName ) )
   {
      // Set our current file
      mCurrentFile = dStrdup(fileName);

      return true;
   }

   // Try to open the file
   FileStream  stream;
   stream.open( fileName, Torque::FS::File::Read );

   if ( stream.getStatus() != Stream::Ok )
   {
      Con::errorf("PersistenceManager::readFile() - Failed to open %s", fileName);

      return false;
   }

   // The file is good so read it in
   mCurrentFile = dStrdup(fileName);

   while(stream.getStatus() != Stream::EOS)
   {
      U8* buffer = new U8[2048];
      dMemset(buffer, 0, 2048);

      stream.readLine(buffer, 2048);

      mLineBuffer.push_back((const char*)buffer);
   }

   // Because of the way that writeLine() works we need to
   // make sure we don't have an empty last line or else
   // we will get an extra line break
   if (mLineBuffer.size() > 0)
   {
      if (mLineBuffer.last() && mLineBuffer.last()[0] == 0)
      {
         SAFE_DELETE_ARRAY(mLineBuffer.last());

         mLineBuffer.pop_back();
      }
   }

   stream.close();

   //Con::printf("Successfully opened and read %s", mCurrentFile);

   return true;
}

void PersistenceManager::killObject()
{
   // Don't save this object
   SAFE_DELETE(mCurrentObject);

   // If there is an object in the stack restore it
   if (mObjectStack.size() > 0)
   {
      mCurrentObject = mObjectStack.last();
      mObjectStack.pop_back();
   }
}

void PersistenceManager::saveObject()
{
   // Now that we have all of the data attempt to
   // find the corresponding SimObject
   mCurrentObject->simObject = Sim::findObject(mCurrentFile, mCurrentObject->endLine + 1);

   // Save this object
   mObjectBuffer.push_back(mCurrentObject);

   mCurrentObject = NULL;

   // If there is an object in the stack restore it
   if (mObjectStack.size() > 0)
   {
      mCurrentObject = mObjectStack.last();
      mObjectStack.pop_back();
   }
}

void PersistenceManager::parseObject()
{
   // We *should* already be in position but just in case...
   if (!mParser.tokenICmp("new") &&
       !mParser.tokenICmp("singleton") &&
       !mParser.tokenICmp("datablock"))
   {
      Con::errorf("PersistenceManager::parseObject() - handed a position that doesn't point to an object \
         creation keyword (new, singleton, datablock)");

      return;
   }

   // If there is an object already being parsed then
   // push it into the stack to finish later
   if (mCurrentObject)
      mObjectStack.push_back(mCurrentObject);

   mCurrentObject = new ParsedObject;

   //// If this object declaration is being assigned to a variable then
   //// consider that the "start" of the declaration (otherwise we could
   //// get a script compile error if we delete the object declaration)
   mParser.regressToken(true);

   if (mParser.tokenICmp("="))
   {
      // Ok, we are at an '='...back up to the beginning of that variable
      mParser.regressToken(true);

      // Get the startLine and startPosition
      mCurrentObject->startLine = mParser.getCurrentLine();
      mCurrentObject->startPosition = mParser.getTokenLineOffset();

      // Advance back to the object declaration
      mParser.advanceToken(true);
      mParser.advanceToken(true);
   }
   else
   {
      // Advance back to the object declaration
      mParser.advanceToken(true);

      // Get the startLine and startPosition
      mCurrentObject->startLine = mParser.getCurrentLine();
      mCurrentObject->startPosition = mParser.getTokenLineOffset();
   }

   if (mObjectStack.size() > 0)
      mCurrentObject->parentObject = mObjectStack.last();

   // The next token should be the className
   mCurrentObject->className = StringTable->insert(mParser.getNextToken());

   // Advance to '('
   mParser.advanceToken(true);

   if (!mParser.tokenICmp("("))
   {
      Con::errorf("PersistenceManager::parseObject() - badly formed object \
         declaration on line %d - was expecting a '(' character", mParser.getCurrentLine());

      // Remove this object without saving it
      killObject();

      return;
   }

   // The next token should either be the object name or ')'
   mParser.advanceToken(true);

   if (mParser.tokenICmp(")"))
   {
      mCurrentObject->name = StringTable->insert("");

      mCurrentObject->nameLine = mParser.getCurrentLine();
      mCurrentObject->namePosition = mParser.getTokenLineOffset();
   }
   else
   {
      mCurrentObject->name = StringTable->insert(mParser.getToken());

      mCurrentObject->nameLine = mParser.getCurrentLine();
      mCurrentObject->namePosition = mParser.getTokenLineOffset();

      // Advance to either ')' or ':'
      mParser.advanceToken(true);

      if (mParser.tokenICmp(":"))
      {
         // Advance past the object we are copying from
         mParser.advanceToken(true);

         // Advance to ')'
         mParser.advanceToken(true);
      }

      if (!mParser.tokenICmp(")"))
      {
         Con::errorf("PersistenceManager::parseObject() - badly formed object \
            declaration on line %d - was expecting a ')' character", mParser.getCurrentLine());

         // Remove this object without saving it
         killObject();

         return;
      }
   }

   // The next token should either be a ';' or a '{'
   mParser.advanceToken(true);

   if (mParser.tokenICmp(";"))
   {
      // Save the end line number
      mCurrentObject->endLine = mParser.getCurrentLine();

      // Save the end position
      mCurrentObject->endPosition = mParser.getTokenLineOffset();

      // Flag this object as not having braces
      mCurrentObject->hasBraces = false;

      saveObject(); // Object has no fields

      return;
   }
   else if (!mParser.tokenICmp("{"))
   {
      Con::errorf("PersistenceManager::parseObject() - badly formed object \
         declaration on line %d - was expecting a '{' character", mParser.getCurrentLine());

      // Remove this object without saving it
      killObject();

      return;
   }

   while (mParser.advanceToken(true))
   {
      // Check for a subobject
      if (mParser.tokenICmp("new") ||
          mParser.tokenICmp("singleton") ||
          mParser.tokenICmp("datablock"))
      {
         parseObject();
      }

      // Check to see if we have a property
      if (mParser.tokenICmp("="))
      {
         // Ok, we are at an '='...back up to find out
         // what variable is getting assigned
         mParser.regressToken(true);

         const char* variable = mParser.getToken();

         if (variable && dStrlen(variable) > 0)
         {
            // See if it is a global or a local variable
            if (variable[0] == '%' || variable[0] == '$')
            {
               // We ignore this variable and go
               // back to our previous place
               mParser.advanceToken(true);
            }
            // Could also potentially be a <object>.<variable>
            // assignment which we don't care about either
            else if (dStrchr(variable, '.'))
            {
               // We ignore this variable and go
               // back to our previous place
               mParser.advanceToken(true);
            }
            // If we made it to here assume it is a variable
            // for the current object
            else
            {
               // Create our new property
               mCurrentObject->properties.increment();

               ParsedProperty& prop = mCurrentObject->properties.last();

               // Check to see if this is an array variable
               if (dStrlen(variable) > 3 && variable[dStrlen(variable) - 1] == ']')
               {
                  // The last character is a ']' which *should* mean
                  // there is also a corresponding '['
                  const char* arrayPosStart = dStrrchr(variable, '[');

                  if (!arrayPosStart)
                  {
                     Con::errorf("PersistenceManager::parseObject() - error parsing array position - \
                        was expecting a '[' character");
                  }
                  else
                  {
                     // Parse the array position for the variable name
                     S32 arrayPos = -1;

                     dSscanf(arrayPosStart, "[%d]", &arrayPos);

                     // If we got a valid array position then set it
                     if (arrayPos > -1)
                        prop.arrayPos = arrayPos;

                     // Trim off the [<pos>] from the variable name
                     char* variableName = dStrdup(variable);
                     variableName[arrayPosStart - variable] = 0;

                     // Set the variable name to our new shortened name
                     variable = StringTable->insert(variableName, true);

                     // Cleanup our variableName buffer
                     delete variableName;  
                  }
               }


               // Set back the variable name
               prop.name = StringTable->insert(variable, true);

               // Store the start position for this variable
               prop.startLine     = mParser.getCurrentLine();
               prop.startPosition = mParser.getTokenLineOffset();

               // Advance back to the '='
               mParser.advanceToken(true);

               // Sanity check
               if (!mParser.tokenICmp("="))
                  Con::errorf("PersistenceManager::parseObject() - somehow we aren't \
                               pointing at the expected '=' character");
               else
               {
                  // The next token should be the value
                  // being assigned to the variable
                  mParser.advanceToken(true);

                  const char* value = mParser.getToken();

                  // TODO: make sure this doesn't leak
                  prop.value = dStrdup(value);

                  // Store the line number for this value
                  prop.valueLine = mParser.getCurrentLine();

                  // Store the values beginning position
                  prop.valuePosition = mParser.getTokenLineOffset();

                  // The next token should be a ';'
                  mParser.advanceToken(true);

                  if (!mParser.tokenICmp(";"))
                     Con::errorf("PersistenceManager::parseObject() - badly formed variable \
                                  assignment on line %d - was expecting a ';' character", mParser.getCurrentLine());

                  // Store the end position for this variable
                  prop.endLine     = mParser.getCurrentLine();
                  prop.endPosition = mParser.getTokenLineOffset();
               }
            }
         }
      }

      // Check for the end of the object declaration
      if (mParser.tokenICmp("}"))
      {
         // See if the next token is a ';'
         mParser.advanceToken(true);

         if (mParser.tokenICmp(";"))
         {
            // Save the end line number
            mCurrentObject->endLine = mParser.getCurrentLine();

            // Save the end position
            mCurrentObject->endPosition = mParser.getTokenLineOffset();

            saveObject();

            break;
         }
      }
   }
}

bool PersistenceManager::parseFile(const char* fileName)
{
   // Read the file into the line buffer
   if (!readFile(fileName))
      return false;

   // Load it into our Tokenizer parser
   if (!mParser.openFile(fileName))
   {
      // Handle an object writing out to a new file
      if ( !Torque::FS::IsFile( fileName ) )
         return true;

      return false;
   }

   // Set our reserved "single" tokens
   mParser.setSingleTokens("(){};=:");

   // Search object declarations
   while (mParser.advanceToken(true))
   {
      if (mParser.tokenICmp("new") ||
          mParser.tokenICmp("singleton") ||
          mParser.tokenICmp("datablock"))
      {
         parseObject();
      }
   }

   // If we had an object that didn't end properly
   // then we could have objects on the stack
   while (mCurrentObject)
      saveObject();

   //Con::errorf("Parsed Results:");

   //for (U32 i = 0; i < mObjectBuffer.size(); i++)
   //{
   //   ParsedObject* parsedObject = mObjectBuffer[i];

   //   Con::warnf("   mObjectBuffer[%d]:", i);
   //   Con::warnf("      name = %s",      parsedObject->name);
   //   Con::warnf("      className = %s", parsedObject->className);
   //   Con::warnf("      startLine = %d", parsedObject->startLine + 1);
   //   Con::warnf("      endLine   = %d", parsedObject->endLine + 1);

   //   //if (mObjectBuffer[i]->properties.size() > 0)
   //   //{
   //   //   Con::warnf("   properties:");
   //   //   for (U32 j = 0; j < mObjectBuffer[i]->properties.size(); j++)
   //   //      Con::warnf("      %s = %s;", mObjectBuffer[i]->properties[j].name,
   //   //                                   mObjectBuffer[i]->properties[j].value);
   //   //}

   //   if (!parsedObject->simObject.isNull())
   //   {
   //      SimObject* simObject = parsedObject->simObject;

   //      Con::warnf("      SimObject(%s) %d:", simObject->getName(), simObject->getId());
   //      Con::warnf("         declaration line = %d", simObject->getDeclarationLine());
   //   }
   //}

   return true;
}

S32 PersistenceManager::getPropertyIndex(ParsedObject* parsedObject, const char* fieldName, U32 arrayPos)
{
   S32 propertyIndex = -1;

   if (!parsedObject)
      return propertyIndex;

   for (U32 i = 0; i < parsedObject->properties.size(); i++)
   {
      if (dStricmp(fieldName, parsedObject->properties[i].name) == 0 &&
          parsedObject->properties[i].arrayPos == arrayPos)
      {
         propertyIndex = i;
         break;
      }
   }

   return propertyIndex;
}

char* PersistenceManager::getObjectIndent(ParsedObject* object)
{
   char* indent = Con::getReturnBuffer(2048);
   indent[0] = 0;

   if (!object)
      return indent;

   if (object->startLine < 0 || object->startLine >= mLineBuffer.size())
      return indent;

   const char* line = mLineBuffer[object->startLine];

   if (line)
   {
      const char* nonSpace = line;

      U32 strLen = dStrlen(line);

      for (U32 i = 0; i < strLen; i++)
      {
         if (*nonSpace != ' ')
            break;

         nonSpace++;
      }

      dStrncpy(indent, line, nonSpace - line);

      indent[nonSpace - line] = 0;
   }

   return indent;
}

void PersistenceManager::updatePositions(U32 lineNumber, U32 startPos, S32 diff)
{
   if (diff == 0)
      return;

   for (U32 i = 0; i < mObjectBuffer.size(); i++)
   {
      ParsedObject* object = mObjectBuffer[i];

      if (object->nameLine == lineNumber && object->namePosition > startPos)
         object->namePosition += diff;

      if (object->endLine == lineNumber && object->endPosition > startPos)
         object->endPosition += diff;

      if (lineNumber >= object->startLine && lineNumber <= object->endLine)
      {
         for (U32 j = 0; j < object->properties.size(); j++)
         {
            ParsedProperty& prop = object->properties[j];

            S32 propStartPos = prop.startPosition;
            S32 endPos       = prop.endPosition;
            S32 valuePos     = prop.valuePosition;

            if (lineNumber == prop.startLine && propStartPos > startPos)
            {
               propStartPos += diff;

               if (propStartPos < 0)
                  propStartPos = 0;

               prop.startPosition = valuePos;
            }
            if (lineNumber == prop.endLine && endPos > startPos)
            {
               endPos += diff;

               if (endPos < 0)
                  endPos = 0;

               prop.endPosition = endPos;
            }
            if (lineNumber == prop.valueLine && valuePos > startPos)
            {
               valuePos += diff;

               if (valuePos < 0)
                  valuePos = 0;

               prop.valuePosition = valuePos;
            }
         }
      }
   }
}

void PersistenceManager::updateLineOffsets(U32 startLine, S32 diff, ParsedObject* skipObject)
{
   if (diff == 0)
      return;

   if (startLine >= mLineBuffer.size())
      return;

   if (startLine + diff >= mLineBuffer.size())
      return;

   // Make sure we don't double offset a SimObject's
   // declaration line
   SimObjectList updated;

   if (skipObject && !skipObject->simObject.isNull())
      updated.push_back_unique(skipObject->simObject);

   for (U32 i = 0; i < mObjectBuffer.size(); i++)
   {
      ParsedObject* object = mObjectBuffer[i];

      // See if this is the skipObject
      if (skipObject && skipObject == object)
         continue;

      // We can safely ignore objects that
      // came earlier in the file
      if (object->endLine < startLine)
         continue;

      if (object->startLine >= startLine)
         object->startLine += diff;

      if (object->nameLine >= startLine)
         object->nameLine += diff;

      for (U32 j = 0; j < object->properties.size(); j++)
      {
         if (object->properties[j].startLine >= startLine)
            object->properties[j].startLine += diff;
         if (object->properties[j].endLine >= startLine)
            object->properties[j].endLine += diff;
         if (object->properties[j].valueLine >= startLine)
            object->properties[j].valueLine += diff;
      }

      if (object->endLine >= startLine)
         object->endLine += diff;

      if (!object->simObject.isNull() &&
          object->simObject->getDeclarationLine() > startLine)
      {
         // Check for already updated SimObject's
         U32 currSize = updated.size();
         updated.push_back_unique(object->simObject);

         if (updated.size() == currSize)
            continue;

         S32 newDeclLine = object->simObject->getDeclarationLine() + diff;

         if (newDeclLine < 0)
            newDeclLine = 0;

         object->simObject->setDeclarationLine(newDeclLine);
      }
   }
}

PersistenceManager::ParsedObject* PersistenceManager::findParentObject(SimObject* object, ParsedObject* parentObject)
{
   ParsedObject* ret = NULL;

   if (!object)
      return ret;

   // First test for the SimGroup it belongs to
   ret = findParsedObject(object->getGroup(), parentObject);

   if (ret)
      return ret;

   // TODO: Test all of the SimSet's that this object belongs to

   return ret;
}

PersistenceManager::ParsedObject* PersistenceManager::findParsedObject(SimObject* object, ParsedObject* parentObject)
{
   if (!object)
      return NULL;

   // See if our object belongs to a parent
   if (!parentObject)
      parentObject = findParentObject(object, parentObject);

   // First let's compare the object to the SimObject's that
   // we matched to our ParsedObject's when we loaded them
   for (U32 i = 0; i < mObjectBuffer.size(); i++)
   {
      ParsedObject* testObj = mObjectBuffer[i];

      if (testObj->simObject == object)
      {
         // Deal with children objects
         if (testObj->parentObject != parentObject)
            continue;

         return testObj;
      }
   }

   // Didn't find it in our ParsedObject's SimObject's
   // so see if we can find one that corresponds to the
   // same name and className
   const char *originalName = object->getOriginalName();

   // Find the corresponding ParsedObject
   if (originalName && originalName[0])
   {
      for (U32 i = 0; i < mObjectBuffer.size(); i++)
      {
         ParsedObject* testObj = mObjectBuffer[i];

         if (testObj->name == originalName)
         {
            // Deal with children objects
            if (testObj->parentObject != parentObject)
               continue;

            return testObj;
         }
      }
   }

   return NULL;
}

void PersistenceManager::updateToken(const U32 lineNumber, const U32 linePosition, const char* oldValue, const char* newValue)
{
   // Make sure we have a valid lineNumber
   if (lineNumber < 0 || linePosition < 0 ||
       lineNumber >= mLineBuffer.size())
      return;

   // Grab the line that the value is on
   const char* line = mLineBuffer[lineNumber];

   U32 oldValueLen = ( oldValue ) ? dStrlen(oldValue) : 0;
   U32 newValueLen = ( newValue ) ? dStrlen(newValue) : 0;

   // Make sure we have a valid linePosition
   if (linePosition >= dStrlen(line) ||
       linePosition + oldValueLen > dStrlen(line))
      return;

   // Get all of the characters up to the value position
   char* preString = dStrdup(line);
   preString[linePosition] = 0;

   // Get all of the characters that occur after the value
   const char* postString = dStrdup(line + linePosition + oldValueLen);

   // Calculate the length of our new line
   U32 newLineLen = 0;

   newLineLen += dStrlen(preString);
   newLineLen += newValueLen;
   newLineLen += dStrlen(postString);

   // Create a buffer for our new line and
   // null terminate it
   char* newLine = new char[newLineLen + 1];
   newLine[0] = 0;

   // Build the new line with the
   // preString + newValue + postString
   dStrcat(newLine, preString);
   if ( newValue )
      dStrcat(newLine, newValue);
   dStrcat(newLine, postString);

   // Clear our existing line
   if (mLineBuffer[lineNumber])
      SAFE_DELETE_ARRAY(mLineBuffer[lineNumber]);

   // Set the new line
   mLineBuffer[lineNumber] = newLine;

   // Figure out the size difference of the old value
   // and new value in case we need to update any else
   // on the line after it
   S32 diff = newValueLen - oldValueLen;

   // Update anything that is on the line after this that needs
   // to change its offsets to reflect the new line
   updatePositions(lineNumber, linePosition, diff);

   // Clean up our buffers
   delete preString;
   delete postString;
}

const char* PersistenceManager::getFieldValue(SimObject* object, const char* fieldName, U32 arrayPos)
{
   // Our return string
   char* ret = NULL;

   // Buffer to hold the string equivalent of the arrayPos
   char arrayPosStr[8];
   dSprintf(arrayPosStr, 8, "%d", arrayPos);

   // Get the object's value
   const char *value = object->getDataField(fieldName, arrayPosStr );
   if (value)
      ret = dStrdup(value);

   return ret;
}

const char* PersistenceManager::createNewProperty(const char* name, const char* value, bool isArray, U32 arrayPos)
{
   if (!name || !value)
      return NULL;

   char* newProp = new char[2048];
   dMemset(newProp, 0, 2048);

   if (value)
   {
      if (isArray)
         dSprintf(newProp, 2048, "%s[%d] = \"%s\";", name, arrayPos, value);
      else
         dSprintf(newProp, 2048, "%s = \"%s\";", name, value);
   }
   else
   {
      if (isArray)
         dSprintf(newProp, 2048, "%s[%d] = \"\";", name, arrayPos);
      else
         dSprintf(newProp, 2048, "%s = \"\";", name);
   }

   return newProp;
}

bool PersistenceManager::isEmptyLine(const char* line)
{
   // Simple test first
   if (!line || dStrlen(line) == 0)
      return true;

   U32 len = dStrlen(line);

   for (U32 i = 0; i < len; i++)
   {
      const char& c = line[i];

      // Skip "empty" characters
      if (c == ' '  ||
          c == '\t' ||
          c == '\r' ||
          c == '\n')
      {
         continue;
      }

      // If we have made it to the an end of the line
      // comment then consider this an empty line
      if (c == '/')
      {
         if (i < len - 1)
         {
            if (line[i + 1] == '/')
               return true;
         }
      }

      // If it isn't an "empty" character or a comment then
      // we have a valid character on the line and it isn't empty
      return false;
   }

   return true;
}

void PersistenceManager::removeLine(U32 lineNumber)
{
   if (lineNumber >= mLineBuffer.size())
      return;

   if (mLineBuffer[lineNumber])
      SAFE_DELETE_ARRAY(mLineBuffer[lineNumber]);

   mLineBuffer.erase(lineNumber);

   updateLineOffsets(lineNumber, -1);
}

void PersistenceManager::removeTextBlock(U32 startLine, U32 endLine, U32 startPos, U32 endPos, bool removeEmptyLines)
{
   // Make sure we have valid lines
   if (startLine >= mLineBuffer.size() || endLine >= mLineBuffer.size())
      return;

   // We assume that the startLine is before the endLine
   if (startLine > endLine)
      return;

   // Grab the lines (they may be the same)
   const char* startLineText = mLineBuffer[startLine];
   const char* endLineText   = mLineBuffer[endLine];

   // Make sure we have a valid startPos
   if (startPos >= dStrlen(startLineText))
      return;

   // Make sure we have a valid startPos
   if (endPos >= dStrlen(endLineText))
      return;

   if (startLine == endLine)
   {
      // Get the full property declaration
      U32 len = endPos - startPos + 1;

      char* prop = new char[len + 1];
      dMemset(prop, 0, len + 1);

      dStrncpy(prop, startLineText + startPos, len);

      // Now let updateToken to the heavy lifting on removing it
      updateToken(startLine, startPos, prop, "");

      // Clean up our temp buffer
      delete [] prop;

      // Handle removing an empty lines if desired
      if (removeEmptyLines)
      {
         const char* line = mLineBuffer[startLine];

         if (isEmptyLine(line))
            removeLine(startLine);
      }
   }
   else
   {
      // Start with clearing the startLine from startPos to the end
      char* prop = dStrdup(startLineText + startPos);

      // Now let updateToken to the heavy lifting on removing it
      updateToken(startLine, startPos, prop, "");

      // Clean up our temp buffer
      delete [] prop;

      // Next remove everything from the beginning of the endLine to our endPos
      prop = dStrdup(endLineText);
      prop[endPos + 1] = 0;

      // Now let updateToken to the heavy lifting on removing it
      updateToken(endLine, 0, prop, "");

      // Clean up our temp buffer
      delete [] prop;

      // Handle removing an empty endLine if desired
      if (removeEmptyLines)
      {
         const char* line = mLineBuffer[endLine];

         if (isEmptyLine(line))
            removeLine(endLine);
      }

      // Handle removing any lines between the startLine and endLine
      for (U32 i = startLine + 1; i < endLine; i++)
         removeLine(startLine + 1);

      // Handle removing an empty startLine if desired
      if (removeEmptyLines)
      {
         const char* line = mLineBuffer[startLine];

         if (isEmptyLine(line))
            removeLine(startLine);
      }
   }
}

void PersistenceManager::removeParsedObject(ParsedObject* parsedObject)
{
   if (!parsedObject)
      return;

   if (parsedObject->startLine < 0 || parsedObject->startLine >= mLineBuffer.size())
      return;

   if (parsedObject->endLine < 0 || parsedObject->startLine >= mLineBuffer.size())
      return;

   removeTextBlock(parsedObject->startLine,     parsedObject->endLine,
                   parsedObject->startPosition, parsedObject->endPosition, true);

   parsedObject->parentObject = NULL;
   parsedObject->simObject    = NULL;
}

void PersistenceManager::removeField(const ParsedProperty& prop)
{
   if (prop.startLine < 0 || prop.startLine >= mLineBuffer.size())
      return;

   if (prop.endLine < 0 || prop.endLine >= mLineBuffer.size())
      return;

  removeTextBlock(prop.startLine, prop.endLine, prop.startPosition, prop.endPosition, true);
}

U32 PersistenceManager::writeProperties(const Vector<const char*>& properties, const U32 insertLine, const char* objectIndent)
{
   U32 currInsertLine = insertLine;

   for (U32 i = 0; i < properties.size(); i++)
   {
      const char* prop = properties[i];

      if (!prop || dStrlen(prop) == 0)
         continue;

      U32 len = dStrlen(objectIndent) + dStrlen(prop) + 4;

      char* newLine = new char[len];

      dSprintf(newLine, len, "%s   %s", objectIndent, prop);

      mLineBuffer.insert(currInsertLine, newLine);
      currInsertLine++;
   }

   return currInsertLine - insertLine;
}

PersistenceManager::ParsedObject* PersistenceManager::writeNewObject(SimObject* object, const Vector<const char*>& properties, const U32 insertLine, ParsedObject* parentObject)
{
   ParsedObject* parsedObject = new ParsedObject;

   parsedObject->name      = object->getName();
   parsedObject->className = object->getClassName();
   parsedObject->simObject = object;

   U32 currInsertLine = insertLine;

   // If the parentObject isn't set see if
   // we can find it in the file
   if (!parentObject)
      parentObject = findParentObject(object);

   parsedObject->parentObject = parentObject;

   char* indent = getObjectIndent(parentObject);

   if (parentObject)
      dStrcat(indent, "   \0");

   // Write out the beginning of the object declaration
   char dclToken[128];
   dMemset(dclToken, 0, 128);

   if (dynamic_cast<Material*>(object) ||
       dynamic_cast<CustomMaterial*>(object) ||
       dynamic_cast<GuiControlProfile*>(object))
      dSprintf(dclToken, 128, "singleton");
   else
   {
      SimDataBlock* db = dynamic_cast<SimDataBlock*>(object);

      if (db && !db->isClientOnly())
         dSprintf(dclToken, 128, "datablock");
      else
         dSprintf(dclToken, 128, "new");
   }

   char newLine[2048];
   dMemset(newLine, 0, 2048);

   // New line before an object declaration
   dSprintf(newLine, 2048, "");

   mLineBuffer.insert(currInsertLine, dStrdup(newLine));
   currInsertLine++;
   dMemset(newLine, 0, 2048);

   parsedObject->startLine    = currInsertLine;
   parsedObject->nameLine     = currInsertLine;
   parsedObject->namePosition = dStrlen(indent) + dStrlen(dclToken) + dStrlen(object->getClassName()) + 2;

   // Objects that had no name were getting saved out as: Object((null))
   if ( object->getName() != NULL )
      dSprintf(newLine, 2048, "%s%s %s(%s)", indent, dclToken, object->getClassName(), object->getName());
   else
      dSprintf(newLine, 2048, "%s%s %s()", indent, dclToken, object->getClassName() );

   mLineBuffer.insert(currInsertLine, dStrdup(newLine));
   currInsertLine++;
   dMemset(newLine, 0, 2048);

   dSprintf(newLine, 2048, "%s{", indent);

   mLineBuffer.insert(currInsertLine, dStrdup(newLine));
   currInsertLine++;
   dMemset(newLine, 0, 2048);

   currInsertLine += writeProperties(properties, currInsertLine, indent);

   parsedObject->endLine = currInsertLine;
   parsedObject->updated = true;

   dSprintf(newLine, 2048, "%s};", indent);

   mLineBuffer.insert(currInsertLine, dStrdup(newLine));
   currInsertLine++;

   updateLineOffsets(insertLine, currInsertLine - insertLine, parsedObject);

   mObjectBuffer.push_back(parsedObject);

   // Update the SimObject to reflect its saved name and declaration line.   
   // These values should always reflect what is in the file, even if the object
   // has not actually been re-created from an execution of that file yet.
   object->setOriginalName( object->getName() );
   object->setDeclarationLine( currInsertLine );
   
   if (mCurrentFile)
      object->setFilename(mCurrentFile);

   return parsedObject;
}

void PersistenceManager::updateObject(SimObject* object, ParsedObject* parentObject)
{
   // Create a default object of the same type
   ConsoleObject *defaultConObject = ConsoleObject::create(object->getClassName());
   SimObject* defaultObject = dynamic_cast<SimObject*>(defaultConObject);

   // ***Really*** shouldn't happen
   if (!defaultObject)
      return;

   Vector<const char*> newLines;

   ParsedObject* parsedObject = findParsedObject(object, parentObject);

   // If we don't already have an association between the ParsedObject
   // and the SimObject then go ahead and create it
   if (parsedObject && parsedObject->simObject.isNull())
      parsedObject->simObject = object;

   // Get our field list
   const AbstractClassRep::FieldList &list = object->getFieldList();

   for(U32 i = 0; i < list.size(); i++)
   {
      const AbstractClassRep::Field* f = &list[i];

      // Skip the special field types.
      if ( f->type >= AbstractClassRep::ARCFirstCustomField )
         continue;

      for(U32 j = 0; S32(j) < f->elementCount; j++)
      {
         const char* value = getFieldValue(object, f->pFieldname, j);

         // Make sure we got a value
         if (!value)
            continue;

         // If this is a field we don't write out then we don't
         // care either for our purposes
         if (dStrlen(value) > 0 && !object->writeField(f->pFieldname, value))
            continue;

         // Let's see if this field is already in the file
         S32 propertyIndex = getPropertyIndex(parsedObject, f->pFieldname, j);

         if (propertyIndex > -1)
         {
            ParsedProperty& prop = parsedObject->properties[propertyIndex];

            // If this field is on the remove list then remove it and continue
            if (findRemoveField(object, f->pFieldname, j))
            {
               removeField(prop);
               continue;
            }

            // Run the parsed value through the console system conditioners so
            // that it will better match the data we got back from the object.
            const char* evalue = Con::getFormattedData(f->type, prop.value, f->table, f->flag);

            // If our data doesn't match then we get to update it
            if (dStricmp(value, evalue) != 0)
            {
               // TODO: This should be wrapped in a helper method... probably.
               // Detect and collapse relative path information
               if (f->type == TypeFilename ||
                   f->type == TypeStringFilename ||
                   f->type == TypeImageFilename)
               {
                  char fnBuf[1024];
                  Con::collapseScriptFilename(fnBuf, 1024, value);

                  updateToken(prop.valueLine, prop.valuePosition, prop.value, fnBuf);
               }
               else
                  updateToken(prop.valueLine, prop.valuePosition, prop.value, value);
            }
         }
         else
         {
            // No need to process a removed field that doesn't exist in the file
            if (findRemoveField(object, f->pFieldname, j))
               continue;

            // If we didn't find the property in the ParsedObject
            // then we need to compare against the default value
            // for this property and save it out if it is different
            const char* defaultValue = getFieldValue(defaultObject, f->pFieldname, j);

            // The default value for most string type fields is
            // NULL so we can't just continue here or we'd never ever
            // write them out...
            //
            //if (!defaultValue)
            //   continue;

            // If the object's value is different from the default
            // value then add it to the ParsedObject's newLines                        
            if ( !defaultValue || dStricmp(value, defaultValue) != 0 )
            {
               // TODO: This should be wrapped in a helper method... probably.
               // Detect and collapse relative path information
               if (f->type == TypeFilename ||
                   f->type == TypeStringFilename ||
                   f->type == TypeImageFilename)
               {
                  char fnBuf[1024];
                  Con::collapseScriptFilename(fnBuf, 1024, value);

                  newLines.push_back(createNewProperty(f->pFieldname, fnBuf, f->elementCount > 1, j));
               }
               else
                  newLines.push_back(createNewProperty(f->pFieldname, value, f->elementCount > 1, j));              
            }

            if (defaultValue)
               delete defaultValue;
         }

         delete value;
      }
   }

   // Handle dynamic fields
   SimFieldDictionary* fieldDict = object->getFieldDictionary();

   for(SimFieldDictionaryIterator itr(fieldDict); *itr; ++itr)
   {
      SimFieldDictionary::Entry * entry = (*itr);

      // Let's see if this field is already in the file
      S32 propertyIndex = getPropertyIndex(parsedObject, entry->slotName);

      if (propertyIndex > -1)
      {
         ParsedProperty& prop = parsedObject->properties[propertyIndex];

         // If this field is on the remove list then remove it and continue
         if (findRemoveField(object, entry->slotName))
         {
            removeField(prop);
            continue;
         }

         // Run the parsed value through the console system conditioners so
         // that it will better match the data we got back from the object.
         const char* evalue = Con::getFormattedData(TypeString, prop.value);

         // If our data doesn't match then we get to update it
         if (dStricmp(entry->value, evalue) != 0)
            updateToken(prop.valueLine, prop.valuePosition, prop.value, entry->value);
      }
      else
      {
         // No need to process a removed field that doesn't exist in the file
         if (findRemoveField(object, entry->slotName))
            continue;

         newLines.push_back(createNewProperty(entry->slotName, entry->value));
      }
   }

   // If we have a parsedObject and the name changed
   // then update the parsedObject to the new name.   
   // NOTE: an object 'can' have a NULL name which crashes in dStricmp.
   if (parsedObject && parsedObject->name != StringTable->insert(object->getName(), true) )
   {
      StringTableEntry objectName = StringTable->insert(object->getName(), true);

      if (parsedObject->name != objectName)
      {
         // Update the name in the file
         updateToken(parsedObject->nameLine, parsedObject->namePosition, parsedObject->name, object->getName());

         // Updated the parsedObject's name
         parsedObject->name = objectName;

         // Updated the object's "original" name to the one that is now in the file
         object->setOriginalName(objectName);
      }
   }

   if (parsedObject && newLines.size() > 0)
   {
      U32 lastPropLine = parsedObject->endLine;

      if (parsedObject->properties.size() > 0)
         lastPropLine = parsedObject->properties.last().valueLine + 1;

      U32 currInsertLine = lastPropLine;

      const char* indent = getObjectIndent(parsedObject);

      // This should handle adding the opening { to an object
      // that formerly did not have {};
      if (!parsedObject->hasBraces)
      {
         updateToken(parsedObject->endLine, parsedObject->endPosition, ";", "\r\n{");

         currInsertLine++;
      }

      currInsertLine += writeProperties(newLines, currInsertLine, indent);

      // This should handle adding the opening } to an object
      // that formerly did not have {};
      if (!parsedObject->hasBraces)
      {
         U32 len = dStrlen(indent) + 3;
         char* newLine = new char[len];

         dSprintf(newLine, len, "%s};", indent);

         mLineBuffer.insert(currInsertLine, newLine);
         currInsertLine++;
      }

      // Update the line offsets to account for the new lines
      updateLineOffsets(lastPropLine, currInsertLine - lastPropLine);
   }
   else if (!parsedObject)
   {
      U32 insertLine = mLineBuffer.size();

      if (!parentObject)
         parentObject = findParentObject(object, parentObject);

      if (parentObject && parentObject->endLine > -1)
         insertLine = parentObject->endLine;

      parsedObject = writeNewObject(object, newLines, insertLine, parentObject);
   }

   // Clean up the newLines memory
   for (U32 i = 0; i < newLines.size(); i++)
   {
      if (newLines[i])
         SAFE_DELETE_ARRAY(newLines[i]);
   }

   newLines.clear();

   SimSet* set = dynamic_cast<SimSet*>(object);

   if (set)
   {
      for(SimSet::iterator i = set->begin(); i != set->end(); i++)
      {
         SimObject* subObject = (SimObject*)(*i);
         updateObject(subObject, parsedObject);
      }
   }

   // Loop through the children of this parsedObject
   // If they haven't been updated then assume that they
   // don't exist in the file anymore
   if (parsedObject)
   {
      for (S32 i = 0; i < mObjectBuffer.size(); i++)
      {
         ParsedObject* removeObj = mObjectBuffer[i];

         if (removeObj->parentObject == parsedObject && !removeObj->updated)
         {
            removeParsedObject(removeObj);

            mObjectBuffer.erase(i);
            i--;

            deleteObject(removeObj);
         }
      }
   }

   // Flag this as an updated object
   if (parsedObject)
      parsedObject->updated = true;
   
   // Cleanup our created default object
   delete defaultConObject;
}

bool PersistenceManager::saveDirtyFile()
{
   FileStream  stream;
   stream.open( mCurrentFile, Torque::FS::File::Write );

   if ( stream.getStatus() != Stream::Ok )
   {
      clearFileData();

      return false;
   }

   for (U32 i = 0; i < mLineBuffer.size(); i++)
      stream.writeLine((const U8*)mLineBuffer[i]);

   stream.close();

   //Con::printf("Successfully opened and wrote %s", mCurrentFile);

   //Con::errorf("Updated Results:");

   //for (U32 i = 0; i < mObjectBuffer.size(); i++)
   //{
   //   ParsedObject* parsedObject = mObjectBuffer[i];

   //   Con::warnf("   mObjectBuffer[%d]:", i);
   //   Con::warnf("      name = %s",      parsedObject->name);
   //   Con::warnf("      className = %s", parsedObject->className);
   //   Con::warnf("      startLine = %d", parsedObject->startLine + 1);
   //   Con::warnf("      endLine   = %d", parsedObject->endLine + 1);

   //   //if (mObjectBuffer[i]->properties.size() > 0)
   //   //{
   //   //   Con::warnf("   properties:");
   //   //   for (U32 j = 0; j < mObjectBuffer[i]->properties.size(); j++)
   //   //      Con::warnf("      %s = %s;", mObjectBuffer[i]->properties[j].name,
   //   //                                   mObjectBuffer[i]->properties[j].value);
   //   //}

   //   if (!parsedObject->simObject.isNull())
   //   {
   //      SimObject* simObject = parsedObject->simObject;

   //      Con::warnf("      SimObject(%s) %d:", simObject->getName(), simObject->getId());
   //      Con::warnf("         declaration line = %d", simObject->getDeclarationLine());
   //   }
   //}

   // Clear our file data
   clearFileData();

   return true;
}

S32 QSORT_CALLBACK PersistenceManager::compareFiles(const void* a,const void* b)
{
   DirtyObject* objectA = (DirtyObject*)(a);
   DirtyObject* objectB = (DirtyObject*)(b);

   if (objectA->object.isNull())
      return -1;
   else if (objectB->object.isNull())
      return 1;

   if (objectA->fileName == objectB->fileName)
      return objectA->object->getDeclarationLine() - objectB->object->getDeclarationLine();

   return dStricmp(objectA->fileName, objectB->fileName);
}

bool PersistenceManager::setDirty(SimObject* inObject, const char* inFileName)
{
   // Check if the object is already in the dirty list...
   DirtyObject *pDirty = findDirtyObject( inObject );   

   // The filename we will save this object to (later)..
   String saveFile;

   // Expand the script filename if we were passed one.
   if ( inFileName )
   {
      char buffer[4096];
      Con::expandScriptFilename( buffer, 4096, inFileName );
      saveFile = buffer;
   }

   // If no filename was passed in, and the object was already dirty,
   // we have nothing to do.   
   if ( saveFile.isEmpty() && pDirty )
      return true;

   // Otherwise default to the simObject's filename.
   if ( saveFile.isEmpty() )
      saveFile = inObject->getFilename();   

   // Error if still no filename.
   if ( saveFile.isEmpty() )
   {
      if (inObject->getName())
         Con::errorf("PersistenceManager::setDirty() - SimObject %s has no file name associated with it - can not save", inObject->getName());
      else
         Con::errorf("PersistenceManager::setDirty() - SimObject %d has no file name associated with it - can not save", inObject->getId());

      return false;
   }

   // Update the DirtyObject's fileName if we have it
   // else create a new one.

   if ( pDirty )
      pDirty->fileName = StringTable->insert( saveFile );
   else
   {    
      // Add the newly dirty object.
      mDirtyObjects.increment();
      mDirtyObjects.last().object = inObject;
      mDirtyObjects.last().fileName = StringTable->insert( saveFile );
   }

   return true;
}

void PersistenceManager::removeDirty(SimObject* object)
{
   for (U32 i = 0; i < mDirtyObjects.size(); i++)
   {
      const DirtyObject& dirtyObject = mDirtyObjects[i];

      if (dirtyObject.object.isNull())
         continue;

      if (dirtyObject.object == object)
      {
         mDirtyObjects.erase(i);
         break;
      }
   }

   for (U32 i = 0; i < mRemoveFields.size(); i++)
   {
      const RemoveField& field = mRemoveFields[i];

      if (field.object != object)
         continue;

      mRemoveFields.erase(i);

      if (i > 0)
         i--;
   }
}

void PersistenceManager::addRemoveField(SimObject* object, const char* fieldName)
{
   // Check to see if this is an array variable
   U32 arrayPos = 0;
   const char* name = fieldName;

   if (dStrlen(fieldName) > 3 && fieldName[dStrlen(fieldName) - 1] == ']')
   {
      // The last character is a ']' which *should* mean
      // there is also a corresponding '['
      const char* arrayPosStart = dStrrchr(fieldName, '[');

      if (!arrayPosStart)
      {
         Con::errorf("PersistenceManager::addRemoveField() - error parsing array position - \
                      was expecting a '[' character");
      }
      else
      {
         // Parse the array position for the variable name
         dSscanf(arrayPosStart, "[%d]", &arrayPos);

         // Trim off the [<pos>] from the variable name
         char* variableName = dStrdup(fieldName);
         variableName[arrayPosStart - fieldName] = 0;

         // Set the variable name to our new shortened name
         name = StringTable->insert(variableName, true);

         // Cleanup our variableName buffer
         delete variableName;  
      }
   }

   // Make sure this field isn't already on the list
   if (!findRemoveField(object, name, arrayPos))
   {
      mRemoveFields.increment();

      RemoveField& field = mRemoveFields.last();

      field.object = object;
      field.fieldName = StringTable->insert(name);
      field.arrayPos = arrayPos;
   }
}

bool PersistenceManager::isDirty(SimObject* object)
{
   return ( findDirtyObject( object ) != NULL );
}

PersistenceManager::DirtyObject* PersistenceManager::findDirtyObject(SimObject* object)
{
   for (U32 i = 0; i < mDirtyObjects.size(); i++)
   {
      const DirtyObject& dirtyObject = mDirtyObjects[i];

      if (dirtyObject.object.isNull())
         continue;

      if (dirtyObject.object == object)
         return &mDirtyObjects[i];
   }

   return NULL;
}

bool PersistenceManager::findRemoveField(SimObject* object, const char* fieldName, U32 arrayPos)
{
   for (U32 i = 0; i < mRemoveFields.size(); i++)
   {
      if (mRemoveFields[i].object == object &&
          mRemoveFields[i].arrayPos == arrayPos &&
          dStricmp(mRemoveFields[i].fieldName, fieldName) == 0)
      {
         return true;
      }
   }

   return false;
}

bool PersistenceManager::saveDirty()
{
   // Remove any null SimObject's first
   for (S32 i = 0; i < mDirtyObjects.size(); i++)
   {
      const DirtyObject& dirtyObject = mDirtyObjects[i];

      if (dirtyObject.object.isNull())
      {
         mDirtyObjects.erase(i);
         i--;
      }
   }

   // Sort by filename and declaration lines
   dQsort(mDirtyObjects.address(), mDirtyObjects.size(), sizeof(DirtyList::value_type), compareFiles);

   for (U32 i = 0; i < mDirtyObjects.size(); i++)
   {
      const DirtyObject& dirtyObject = mDirtyObjects[i];

      if (dirtyObject.object.isNull())
         continue;

      SimObject* object = dirtyObject.object;

      if (!mCurrentFile || dStricmp(mCurrentFile, dirtyObject.fileName) != 0)
      {
         // If mCurrentFile is set then that means we
         // changed file names so save our previous one
         if (mCurrentFile)
            saveDirtyFile();

         // Open our new file and parse it
         bool success = parseFile(dirtyObject.fileName);

         if (!success)
         {
            const char *name = object->getName();
            if (name)
            {
               Con::errorf("PersistenceManager::saveDirty(): Unable to open %s to save %s %s (%d)",
                  dirtyObject.fileName, object->getClassName(), name, object->getId());
            }
            else
            {
               Con::errorf("PersistenceManager::saveDirty(): Unable to open %s to save %s (%d)",
                  dirtyObject.fileName, object->getClassName(), object->getId());
            }

            continue;
         }
      }

      // Update this object's properties
      //
      // An empty script file (with 1 line) gets here with zero
      // elements in the linebuffer, so this would prevent us from
      // ever writing to it... Or is this test preventing bad things from
      // happening if the file didn't exist at all?
      //
      if (mCurrentFile /*&& mLineBuffer.size() > 0*/)
         updateObject(object);
   }

   // Save out our last file
   if (mCurrentFile)
      saveDirtyFile();

   // Done writing out our dirty objects so reset everything
   clearAll();

   return true;
}

bool PersistenceManager::saveDirtyObject(SimObject* object)
{
   // find our object passed in
   for (U32 i = 0; i < mDirtyObjects.size(); i++)
   {
      const DirtyObject& dirtyObject = mDirtyObjects[i];

      if (dirtyObject.object.isNull())
         continue;

      if (dirtyObject.object == object)
      {
         // Open our new file and parse it
         bool success = parseFile(dirtyObject.fileName);

         if (!success)
         {
            const char *name = object->getName();
            if (name)
            {
			      Con::errorf("PersistenceManager::saveDirtyObject(): Unable to open %s to save %s %s (%d)",
				      dirtyObject.fileName, object->getClassName(), name, object->getId());
		      }
		      else
		      {
			      Con::errorf("PersistenceManager::saveDirtyObject(): Unable to open %s to save %s (%d)",
				      dirtyObject.fileName, object->getClassName(), object->getId());
		      }

		      return false;
		   }

         // if the file exists then lets update and save
		   if(mCurrentFile)
		   {
		      updateObject(object);
            saveDirtyFile();
		   }
         
         break;
      }
   }

   // remove this object from the dirty list
   removeDirty(object);

   return true;
}

void PersistenceManager::removeObjectFromFile(SimObject* object, const char* fileName)
{
   if (mCurrentFile)
   {
      Con::errorf("PersistenceManager::removeObjectFromFile(): Can't remove an object from a \
                  file while another is currently opened");

      return;
   }

   const char* file = object->getFilename();
   if (fileName)
   {
      char buffer[1024];
      Con::expandScriptFilename( buffer, 1024, fileName );

      file = StringTable->insert(buffer);
   }

   bool success = false;
   
   if ( file )
      success = parseFile(file);

   if (!success)
   {
      const char *name = object->getName();

      String errorNameStr;
      if ( name )
         errorNameStr = String::ToString( "%s %s (%d)", object->getClassName(), name, object->getId() );
      else
         errorNameStr = String::ToString( "%s (%d)", object->getClassName(), object->getId() );

      if ( !file )
         Con::errorf("PersistenceManager::removeObjectFromFile(): File was null trying to save %s", errorNameStr.c_str() );
      else            
         Con::errorf("PersistenceManager::removeObjectFromFile(): Unable to open %s to save %s", file, errorNameStr.c_str() );            

      // Reset everything
      clearAll();

      return;
   }

   ParsedObject* parsedObject = findParsedObject(object);

   if (!parsedObject)
   {
      const char *name = object->getName();
      if (name)
      {
         Con::errorf("PersistenceManager::removeObjectFromFile(): Unable to find %s %s (%d) in %s",
            object->getClassName(), name, object->getId(), file);
      }
      else
      {
         Con::errorf("PersistenceManager::removeObjectFromFile(): Unable to find %s (%d) in %s",
            object->getClassName(), object->getId(), file);
      }

      // Reset everything
      clearAll();

      return;
   }

   removeParsedObject(parsedObject);

   for (U32 i = 0; i < mObjectBuffer.size(); i++)
   {
      ParsedObject* removeObj = mObjectBuffer[i];

      if (removeObj == parsedObject)
      {
         mObjectBuffer.erase(i);
         break;
      }
   }

   deleteObject(parsedObject);

   // Save out the file
   if (mCurrentFile)
      saveDirtyFile();

   // Reset everything
   clearAll();
}

void PersistenceManager::deleteObjectsFromFile(const char* fileName)
{
   if ( mCurrentFile )
   {
      Con::errorf( "PersistenceManager::deleteObjectsFromFile(): Cannot process while file while another is currently open." );
      return;
   }

   // Expand Script File.
   char buffer[1024];
   Con::expandScriptFilename( buffer, 1024, fileName );

   // Parse File.
   if ( !parseFile( StringTable->insert(buffer) ) )
   {
      // Invalid.
      return;
   }

   // Iterate over the objects.
   for ( Vector<ParsedObject*>::iterator itr = mObjectBuffer.begin(); itr != mObjectBuffer.end(); itr++ )
   {
      SimObject *object;
      if ( Sim::findObject( ( *itr )->name, object ) )
      {
         // Delete the Object.
         object->deleteObject();
      }
   }

   // Clear.
   clearAll();
}

ConsoleMethod( PersistenceManager, deleteObjectsFromFile, void, 3, 3, "( fileName )"
              "Delete all of the objects that are created from the given file." )
{
   // Delete Objects.
   object->deleteObjectsFromFile( argv[2] );
}

ConsoleMethod( PersistenceManager, setDirty, void, 3, 4, "(SimObject object, [filename])"
              "Mark an existing SimObject as dirty (will be written out when saveDirty() is called).")
{
   SimObject *dirtyObject = NULL;
   if (argv[2][0])
   {
      if (!Sim::findObject(argv[2], dirtyObject))
      {
         Con::printf("%s(): Invalid SimObject: %s", argv[0], argv[2]);
         return;
      }
   }

   if (dirtyObject)
   {
      if (argc == 4 && argv[3][0])
         object->setDirty(dirtyObject, argv[3]);
      else
         object->setDirty(dirtyObject);
   }
}

ConsoleMethod( PersistenceManager, removeDirty, void, 3, 3, "(SimObject object)"
              "Remove a SimObject from the dirty list.")
{
   SimObject *dirtyObject = NULL;
   if (argv[2][0])
   {
      if (!Sim::findObject(argv[2], dirtyObject))
      {
         Con::printf("%s(): Invalid SimObject: %s", argv[0], argv[2]);
         return;
      }
   }

   if (dirtyObject)
      object->removeDirty(dirtyObject);
}

ConsoleMethod( PersistenceManager, isDirty, bool, 3, 3, "(SimObject object)"
              "Returns true if the SimObject is on the dirty list.")
{
   SimObject *dirtyObject = NULL;
   if (argv[2][0])
   {
      if (!Sim::findObject(argv[2], dirtyObject))
      {
         Con::printf("%s(): Invalid SimObject: %s", argv[0], argv[2]);
         return false;
      }
   }

   if (dirtyObject)
      return object->isDirty(dirtyObject);

   return false;
}

ConsoleMethod( PersistenceManager, hasDirty, bool, 2, 2, "()"
              "Returns true if the manager has dirty objects to save." )
{
   return object->hasDirty();
}

ConsoleMethod( PersistenceManager, getDirtyObjectCount, S32, 2, 2, "()"
              "Returns the number of dirty objects." )
{
   return object->getDirtyList().size();
}

ConsoleMethod( PersistenceManager, getDirtyObject, S32, 3, 3, "( index )"
              "Returns the ith dirty object." )
{
   const S32 index = dAtoi( argv[2] );
   if ( index < 0 || index >= object->getDirtyList().size() )
   {
      Con::warnf( "PersistenceManager::getDirtyObject() - Index (%s) out of range.", argv[2] );
      return 0;
   }

   // Fetch Object.
   const PersistenceManager::DirtyObject& dirtyObject = object->getDirtyList()[index];

   // Return Id.
   return ( dirtyObject.object ) ? dirtyObject.object->getId() : 0;
}

ConsoleMethod( PersistenceManager, listDirty, void, 2, 2, "()"
              "Prints the dirty list to the console.")
{
   const PersistenceManager::DirtyList dirtyList = object->getDirtyList();

   for(U32 i = 0; i < dirtyList.size(); i++)
   {
      const PersistenceManager::DirtyObject& dirtyObject = dirtyList[i];

      if (dirtyObject.object.isNull())
         continue;

      SimObject *obj = dirtyObject.object;
      bool isSet = dynamic_cast<SimSet *>(obj) != 0;
      const char *name = obj->getName();
      if (name)
      {
         Con::printf("   %d,\"%s\": %s %s %s", obj->getId(), name,
         obj->getClassName(), dirtyObject.fileName, isSet ? "(g)":"");
      }
      else
      {
         Con::printf("   %d: %s %s, %s", obj->getId(), obj->getClassName(),
         dirtyObject.fileName, isSet ? "(g)" : "");
      }
   }
}

ConsoleMethod( PersistenceManager, saveDirty, void, 2, 2, "()"
              "Saves all of the SimObject's on the dirty list to their respective files.")
{
   object->saveDirty();
}

ConsoleMethod( PersistenceManager, saveDirtyObject, void, 3, 3, "(SimObject object)"
              "Save a dirty SimObject to it's file.")
{
   SimObject *dirtyObject = NULL;
   if (argv[2][0])
   {
      if (!Sim::findObject(argv[2], dirtyObject))
      {
         Con::printf("%s(): Invalid SimObject: %s", argv[0], argv[2]);
         return;
      }
   }

   if (dirtyObject)
      object->saveDirtyObject(dirtyObject);
}

ConsoleMethod( PersistenceManager, clearAll, void, 2, 2, "()"
              "Clears all the tracked objects without saving them." )
{
   object->clearAll();
}

ConsoleMethod( PersistenceManager, removeObjectFromFile, void, 3, 4, "(SimObject object, [filename])"
              "Remove an existing SimObject from a file (can optionally specify a different file than \
               the one it was created in.")
{
   SimObject *dirtyObject = NULL;
   if (argv[2][0])
   {
      if (!Sim::findObject(argv[2], dirtyObject))
      {
         Con::printf("%s(): Invalid SimObject: %s", argv[0], argv[2]);
         return;
      }
   }

   if (dirtyObject)
   {
      if (argc == 4 && argv[3][0])
         object->removeObjectFromFile(dirtyObject, argv[3]);
      else
         object->removeObjectFromFile(dirtyObject);
   }
}

ConsoleMethod( PersistenceManager, removeField, void, 4, 4, "(SimObject object, string fieldName)"
              "Remove a specific field from an object declaration.")
{
   SimObject *dirtyObject = NULL;
   if (argv[2][0])
   {
      if (!Sim::findObject(argv[2], dirtyObject))
      {
         Con::printf("%s(): Invalid SimObject: %s", argv[0], argv[2]);
         return;
      }
   }

   if (dirtyObject)
   {
      if (argv[3][0])
         object->addRemoveField(dirtyObject, argv[3]);
   }
}