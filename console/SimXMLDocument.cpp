//-----------------------------------------------------------------------------
// Torque 3D 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "tinyxml/tinyxml.h"

//-----------------------------------------------------------------------------
// Console implementation of STL map.
//-----------------------------------------------------------------------------

#include "core/frameAllocator.h"
#include "console/simBase.h"
#include "console/consoleInternal.h"
#include "console/SimXMLDocument.h"

IMPLEMENT_CONOBJECT(SimXMLDocument);

// -----------------------------------------------------------------------------
// Constructor.
// -----------------------------------------------------------------------------
SimXMLDocument::SimXMLDocument():
m_qDocument(0),
m_CurrentAttribute(0)
{
}

// -----------------------------------------------------------------------------
// Destructor.
// -----------------------------------------------------------------------------
SimXMLDocument::~SimXMLDocument()
{
}

// -----------------------------------------------------------------------------
// Included for completeness.
// -----------------------------------------------------------------------------
bool SimXMLDocument::processArguments(S32 argc, const char** argv)
{
   return argc == 0;
}

// -----------------------------------------------------------------------------
// Script constructor.
// -----------------------------------------------------------------------------
bool SimXMLDocument::onAdd()
{
   if(!Parent::onAdd())
   {
      return false;
   }

   if(!m_qDocument)
   {
      m_qDocument = new TiXmlDocument();
   }
   return true;
}

// -----------------------------------------------------------------------------
// Script destructor.
// -----------------------------------------------------------------------------
void SimXMLDocument::onRemove()
{
   Parent::onRemove();
   if(m_qDocument)
   {
      m_qDocument->Clear();
      delete(m_qDocument);
   }
}

// -----------------------------------------------------------------------------
// Initialize peristent fields (datablocks).
// -----------------------------------------------------------------------------
void SimXMLDocument::initPersistFields()
{
   Parent::initPersistFields();
}

// -----------------------------------------------------------------------------
// Set this to default state at construction.
// -----------------------------------------------------------------------------
void SimXMLDocument::reset(void)
{
   m_qDocument->Clear();
   m_paNode.clear();
   m_CurrentAttribute = 0;
}
ConsoleMethod(SimXMLDocument, reset, void, 2, 2,
              "Set this to default state at construction.")
{
   object->reset();
}

// -----------------------------------------------------------------------------
// Get true if file loads successfully.
// -----------------------------------------------------------------------------
S32 SimXMLDocument::loadFile(const char* rFileName)
{
   reset();
   return m_qDocument->LoadFile(rFileName);
}
ConsoleMethod(SimXMLDocument, loadFile, S32, 3, 3, "Load file from given filename.")
{
   return object->loadFile( argv[2] );
}

// -----------------------------------------------------------------------------
// Get true if file saves successfully.
// -----------------------------------------------------------------------------
S32 SimXMLDocument::saveFile(const char* rFileName)
{
   return m_qDocument->SaveFile( rFileName );
}
ConsoleMethod(SimXMLDocument, saveFile, S32, 3, 3, "Save file to given filename.")
{
   return object->saveFile( argv[2] );
}

// -----------------------------------------------------------------------------
// Get true if XML text parses correctly.
// -----------------------------------------------------------------------------
S32 SimXMLDocument::parse(const char* rText)
{
   reset();
   m_qDocument->Parse( rText );
   return 1;
}
ConsoleMethod(SimXMLDocument, parse, S32, 3, 3, "Create document from XML string.")
{
   return object->parse( argv[2] );
}

// -----------------------------------------------------------------------------
// Clear contents of XML document.
// -----------------------------------------------------------------------------
void SimXMLDocument::clear(void)
{
   reset();
}
ConsoleMethod(SimXMLDocument, clear, void, 2, 2, "Clear contents of XML document.")
{
   object->clear();
}

// -----------------------------------------------------------------------------
// Get error description of XML document.
// -----------------------------------------------------------------------------
const char* SimXMLDocument::getErrorDesc(void) const
{
   if(!m_qDocument)
   {
      return StringTable->insert("No document");
   }
   return m_qDocument->ErrorDesc();
}
ConsoleMethod(SimXMLDocument, getErrorDesc, const char*, 2, 2,
              "Get current error description.")
{
   // Create Returnable Buffer (duration of error description is unknown).
   char* pBuffer = Con::getReturnBuffer(256);
   // Format Buffer.
   dSprintf(pBuffer, 256, "%s", object->getErrorDesc());
   // Return Velocity.
   return pBuffer;
}

// -----------------------------------------------------------------------------
// Clear error description of this.
// -----------------------------------------------------------------------------
void SimXMLDocument::clearError(void)
{
   m_qDocument->ClearError();
}
ConsoleMethod(SimXMLDocument, clearError, void, 2, 2,
              "Clear error description.")
{
   object->clearError();
}

// -----------------------------------------------------------------------------
// Get true if first child element was successfully pushed onto stack.
// -----------------------------------------------------------------------------
bool SimXMLDocument::pushFirstChildElement(const char* rName)
{
   // Clear the current attribute pointer
   m_CurrentAttribute = 0;

   // Push the first element found under the current element of the given name
   TiXmlElement* pElement;
   if(!m_paNode.empty())
   {
      const int iLastElement = m_paNode.size() - 1;
      TiXmlElement* pNode = m_paNode[iLastElement];
      if(!pNode)
      {
         return false;
      }
      pElement = pNode->FirstChildElement(rName);
   }
   else
   {
      if(!m_qDocument)
      {
         return false;
      }
      pElement = m_qDocument->FirstChildElement(rName);
   }

   if(!pElement)
   {
      return false;
   }
   m_paNode.push_back(pElement);
   return true;
}
ConsoleMethod(SimXMLDocument, pushFirstChildElement, bool, 3, 3,
              "Push first child element with given name onto stack.")
{
   return object->pushFirstChildElement( argv[2] );
}

// -----------------------------------------------------------------------------
// Get true if first child element was successfully pushed onto stack.
// -----------------------------------------------------------------------------
bool SimXMLDocument::pushChildElement(S32 index)
{
   // Clear the current attribute pointer
   m_CurrentAttribute = 0;

   // Push the first element found under the current element of the given name
   TiXmlElement* pElement;
   if(!m_paNode.empty())
   {
      const int iLastElement = m_paNode.size() - 1;
      TiXmlElement* pNode = m_paNode[iLastElement];
      if(!pNode)
      {
         return false;
      }
      pElement = pNode->FirstChildElement();
      for( S32 i = 0; i < index; i++ )
      {
         if( !pElement )
            return false;

         pElement = pElement->NextSiblingElement();
      }
   }
   else
   {
      if(!m_qDocument)
      {
         return false;
      }
      pElement = m_qDocument->FirstChildElement();
      for( S32 i = 0; i < index; i++ )
      {
         if( !pElement )
            return false;

         pElement = pElement->NextSiblingElement();
      }
   }

   if(!pElement)
   {
      return false;
   }
   m_paNode.push_back(pElement);
   return true;
}
ConsoleMethod(SimXMLDocument, pushChildElement, bool, 3, 3,
              "Push the child element at the given index onto stack.")
{
   return object->pushChildElement( dAtoi( argv[2] ) );
}

// -----------------------------------------------------------------------------
// Convert top stack element into its next sibling element.
// -----------------------------------------------------------------------------
bool SimXMLDocument::nextSiblingElement(const char* rName)
{
   // Clear the current attribute pointer
   m_CurrentAttribute = 0;

   // Attempt to find the next sibling element
   if(m_paNode.empty())
   {
      return false;
   }
   const int iLastElement = m_paNode.size() - 1;
   TiXmlElement*& pElement = m_paNode[iLastElement];
   if(!pElement)
   {
      return false;
   }

   pElement = pElement->NextSiblingElement(rName);
   if(!pElement)
   {
      return false;
   }

   return true;
}
ConsoleMethod(SimXMLDocument, nextSiblingElement, bool, 3, 3,
              "Set top element on stack to next element with given name.")
{
   return object->nextSiblingElement( argv[2] );
}

// -----------------------------------------------------------------------------
// Get element value if it exists.  Used to extract a text node from the element.
// for example.
// -----------------------------------------------------------------------------
const char* SimXMLDocument::elementValue()
{
   if(m_paNode.empty())
   {
      return StringTable->insert("");
   }
   const int iLastElement = m_paNode.size() - 1;
   TiXmlElement* pNode = m_paNode[iLastElement];
   if(!pNode)
   {
      return StringTable->insert("");
   }

   return pNode->Value();
}
ConsoleMethod(SimXMLDocument, elementValue, const char*, 2, 2,
              "Get element value if it exists (string).")
{
   // Create Returnable Buffer (because duration of value is unknown).
   char* pBuffer = Con::getReturnBuffer(256);
   dSprintf(pBuffer, 256, "%s", object->elementValue());
   return pBuffer;
}

// -----------------------------------------------------------------------------
// Pop last element off of stack.
// -----------------------------------------------------------------------------
void SimXMLDocument::popElement(void)
{
   m_paNode.pop_back();
}
ConsoleMethod(SimXMLDocument, popElement, void, 2, 2,
              "Pop last element off of stack.")
{
   object->popElement();
}

// -----------------------------------------------------------------------------
// Get attribute value if it exists.
// -----------------------------------------------------------------------------
const char* SimXMLDocument::attribute(const char* rAttribute)
{
   if(m_paNode.empty())
   {
      return StringTable->insert("");
   }
   const int iLastElement = m_paNode.size() - 1;
   TiXmlElement* pNode = m_paNode[iLastElement];
   if(!pNode)
   {
      return StringTable->insert("");
   }

   if(!pNode->Attribute(rAttribute))
   {
      return StringTable->insert("");
   }

   return pNode->Attribute(rAttribute);
}
ConsoleMethod(SimXMLDocument, attribute, const char*, 3, 3,
              "Get attribute value if it exists (string).")
{
   // Create Returnable Buffer (because duration of attribute is unknown).
   char* pBuffer = Con::getReturnBuffer(256);
   // Format Buffer.
   dSprintf(pBuffer, 256, "%s", object->attribute( argv[2] ));
   // Return Velocity.
   return pBuffer;
}
ConsoleMethod(SimXMLDocument, attributeF32, F32, 3, 3,
              "Get attribute value if it exists (float).")
{
   return dAtof( object->attribute( argv[2] ) );
}
ConsoleMethod(SimXMLDocument, attributeS32, S32, 3, 3,
              "Get attribute value if it exists (int).")
{
   return dAtoi( object->attribute( argv[2] ) );
}

// -----------------------------------------------------------------------------
// Get true if given attribute exists.
// -----------------------------------------------------------------------------
bool SimXMLDocument::attributeExists(const char* rAttribute)
{
   if(m_paNode.empty())
   {
      return false;
   }
   const int iLastElement = m_paNode.size() - 1;
   TiXmlElement* pNode = m_paNode[iLastElement];
   if(!pNode)
   {
      return false;
   }

   if(!pNode->Attribute(rAttribute))
   {
      return false;
   }

   return true;
}
ConsoleMethod(SimXMLDocument, attributeExists, bool, 3, 3,
              "Get true if named attribute exists.")
{
   return object->attributeExists( argv[2] );
}

// -----------------------------------------------------------------------------
// Obtain the name of the current element's first attribute
// -----------------------------------------------------------------------------
const char* SimXMLDocument::firstAttribute()
{
   // Get the current element
   if(m_paNode.empty())
   {
      return StringTable->insert("");
   }
   const int iLastElement = m_paNode.size() - 1;
   TiXmlElement* pNode = m_paNode[iLastElement];
   if(!pNode)
   {
      return StringTable->insert("");
   }

   // Gets its first attribute, if any
   m_CurrentAttribute = pNode->FirstAttribute();
   if(!m_CurrentAttribute)
   {
      return StringTable->insert("");
   }

   return m_CurrentAttribute->Name();
}
ConsoleMethod(SimXMLDocument, firstAttribute, const char*, 2, 2,
              "Obtain the name of the current element's first attribute.")
{
   const char* name = object->firstAttribute();

   // Create Returnable Buffer (because duration of attribute is unknown).
   char* pBuffer = Con::getReturnBuffer(dStrlen(name)+1);
   dStrcpy(pBuffer, name);
   return pBuffer;
}

// -----------------------------------------------------------------------------
// Obtain the name of the current element's last attribute
// -----------------------------------------------------------------------------
const char* SimXMLDocument::lastAttribute()
{
   // Get the current element
   if(m_paNode.empty())
   {
      return StringTable->insert("");
   }
   const int iLastElement = m_paNode.size() - 1;
   TiXmlElement* pNode = m_paNode[iLastElement];
   if(!pNode)
   {
      return StringTable->insert("");
   }

   // Gets its last attribute, if any
   m_CurrentAttribute = pNode->LastAttribute();
   if(!m_CurrentAttribute)
   {
      return StringTable->insert("");
   }

   return m_CurrentAttribute->Name();
}
ConsoleMethod(SimXMLDocument, lastAttribute, const char*, 2, 2,
              "Obtain the name of the current element's last attribute.")
{
   const char* name = object->lastAttribute();

   // Create Returnable Buffer (because duration of attribute is unknown).
   char* pBuffer = Con::getReturnBuffer(dStrlen(name)+1);
   dStrcpy(pBuffer, name);
   return pBuffer;
}

// -----------------------------------------------------------------------------
// Get the name of the next attribute for the current element after a call
// to firstAttribute().
// -----------------------------------------------------------------------------
const char* SimXMLDocument::nextAttribute()
{
   if(!m_CurrentAttribute)
   {
      return StringTable->insert("");
   }

   // Gets its next attribute, if any
   m_CurrentAttribute = m_CurrentAttribute->Next();
   if(!m_CurrentAttribute)
   {
      return StringTable->insert("");
   }

   return m_CurrentAttribute->Name();
}
ConsoleMethod(SimXMLDocument, nextAttribute, const char*, 2, 2,
              "Get the name of the next attribute for the current element after a call to firstAttribute().")
{
   const char* name = object->nextAttribute();

   // Create Returnable Buffer (because duration of attribute is unknown).
   char* pBuffer = Con::getReturnBuffer(dStrlen(name)+1);
   dStrcpy(pBuffer, name);
   return pBuffer;
}

// -----------------------------------------------------------------------------
// Get the name of the previous attribute for the current element after a call
// to lastAttribute().
// -----------------------------------------------------------------------------
const char* SimXMLDocument::prevAttribute()
{
   if(!m_CurrentAttribute)
   {
      return StringTable->insert("");
   }

   // Gets its next attribute, if any
   m_CurrentAttribute = m_CurrentAttribute->Previous();
   if(!m_CurrentAttribute)
   {
      return StringTable->insert("");
   }

   return m_CurrentAttribute->Name();
}
ConsoleMethod(SimXMLDocument, prevAttribute, const char*, 2, 2,
              "Get the name of the previous attribute for the current element after a call to lastAttribute().")
{
   const char* name = object->prevAttribute();

   // Create Returnable Buffer (because duration of attribute is unknown).
   char* pBuffer = Con::getReturnBuffer(dStrlen(name)+1);
   dStrcpy(pBuffer, name);
   return pBuffer;
}

// -----------------------------------------------------------------------------
// Set attribute of top stack element to given value.
// -----------------------------------------------------------------------------
void SimXMLDocument::setAttribute(const char* rAttribute, const char* rVal)
{
   if(m_paNode.empty())
   {
      return;
   }

   const int iLastElement = m_paNode.size() - 1;
   TiXmlElement* pElement = m_paNode[iLastElement];
   if(!pElement)
   {
      return;
   }
   pElement->SetAttribute(rAttribute, rVal);
}
ConsoleMethod(SimXMLDocument, setAttribute, void, 4, 4,
              "Set attribute of top stack element to given value.")
{
   object->setAttribute(argv[2], argv[3]);
}

// -----------------------------------------------------------------------------
// Set attribute of top stack element to given value.
// -----------------------------------------------------------------------------
void SimXMLDocument::setObjectAttributes(const char* objectID)
{
   if( !objectID || !objectID[0] )
      return;

   if(m_paNode.empty())
      return;

   SimObject *pObject = Sim::findObject( objectID );

   if( pObject == NULL )
      return;

   const int iLastElement = m_paNode.size() - 1;
   TiXmlElement* pElement = m_paNode[iLastElement];
   if(!pElement)
      return;

   char textbuf[1024];
   TiXmlElement field( "Field" );
   TiXmlElement group( "FieldGroup" );
   pElement->SetAttribute( "Name", pObject->getName() );


      // Iterate over our filed list and add them to the XML document...
      AbstractClassRep::FieldList fieldList = pObject->getFieldList();
      AbstractClassRep::FieldList::iterator itr;
      for(itr = fieldList.begin(); itr != fieldList.end(); itr++)
      {

         if( itr->type == AbstractClassRep::DeprecatedFieldType ||
            itr->type == AbstractClassRep::StartGroupFieldType ||
            itr->type == AbstractClassRep::EndGroupFieldType) continue;

         // Not an Array
         if(itr->elementCount == 1)
         {
            // get the value of the field as a string.
            ConsoleBaseType *cbt = ConsoleBaseType::getType(itr->type);

            const char *val = Con::getData(itr->type, (void *) (((const char *)pObject) + itr->offset), 0, itr->table, itr->flag);

            // Make a copy for the field check.
            if (!val)
               continue;

            FrameTemp<char> valCopy( dStrlen( val ) + 1 );
            dStrcpy( (char *)valCopy, val );

            if (!pObject->writeField(itr->pFieldname, valCopy))
               continue;

            val = valCopy;


            expandEscape(textbuf, val);

            if( !pObject->writeField( itr->pFieldname, textbuf ) )
               continue;

            field.SetValue( "Property" );
            field.SetAttribute( "name",  itr->pFieldname );
            if( cbt != NULL )
               field.SetAttribute( "type", cbt->getTypeName() );
            else
               field.SetAttribute( "type", "TypeString" );
            field.SetAttribute( "data", textbuf );

            pElement->InsertEndChild( field );

            continue;
         }
      }

      //// IS An Array
      //for(U32 j = 0; S32(j) < f->elementCount; j++)
      //{

      //   // If the start of a group create an element for the group and
      //   // the our chache to it
      //   const char *val = Con::getData(itr->type, (void *) (((const char *)pObject) + itr->offset), j, itr->table, itr->flag);

      //   // Make a copy for the field check.
      //   if (!val)
      //      continue;

      //   FrameTemp<char> valCopy( dStrlen( val ) + 1 );
      //   dStrcpy( (char *)valCopy, val );

      //   if (!pObject->writeField(itr->pFieldname, valCopy))
      //      continue;

      //   val = valCopy;

      //      // get the value of the field as a string.
      //      ConsoleBaseType *cbt = ConsoleBaseType::getType(itr->type);
      //      const char * dstr = Con::getData(itr->type, (void *)(((const char *)pObject) + itr->offset), 0, itr->table, itr->flag);
      //      if(!dstr)
      //         dstr = "";
      //      expandEscape(textbuf, dstr);


      //      if( !pObject->writeField( itr->pFieldname, dstr ) )
      //         continue;

      //      field.SetValue( "Property" );
      //      field.SetAttribute( "name",  itr->pFieldname );
      //      if( cbt != NULL )
      //         field.SetAttribute( "type", cbt->getTypeName() );
      //      else
      //         field.SetAttribute( "type", "TypeString" );
      //      field.SetAttribute( "data", textbuf );

      //      pElement->InsertEndChild( field );
      //}

}
ConsoleMethod(SimXMLDocument, setObjectAttributes, void, 3, 3,
              "Set attribute of top stack element to given value.")
{
   object->setObjectAttributes(argv[2]);
}

// -----------------------------------------------------------------------------
// Create a new element and set to child of current stack element.
// New element is placed on top of element stack.
// -----------------------------------------------------------------------------
void SimXMLDocument::pushNewElement(const char* rName)
{    
   TiXmlElement cElement( rName );
   TiXmlElement* pStackTop = 0;
   if(m_paNode.empty())
   {
      pStackTop = dynamic_cast<TiXmlElement*>
         (m_qDocument->InsertEndChild( cElement ) );
   }
   else
   {
      const int iFinalElement = m_paNode.size() - 1;
      TiXmlElement* pNode = m_paNode[iFinalElement];
      if(!pNode)
      {
         return;
      }
      pStackTop = dynamic_cast<TiXmlElement*>
         (pNode->InsertEndChild( cElement ));
   }
   if(!pStackTop)
   {
      return;
   }
   m_paNode.push_back(pStackTop);
}
ConsoleMethod(SimXMLDocument, pushNewElement, void, 3, 3,
              "Create new element as child of current stack element "
              "and push new element on to stack.")
{
   object->pushNewElement( argv[2] );
}

// -----------------------------------------------------------------------------
// Create a new element and set to child of current stack element.
// New element is placed on top of element stack.
// -----------------------------------------------------------------------------
void SimXMLDocument::addNewElement(const char* rName)
{    
   TiXmlElement cElement( rName );
   TiXmlElement* pStackTop = 0;
   if(m_paNode.empty())
   {
      pStackTop = dynamic_cast<TiXmlElement*>
         (m_qDocument->InsertEndChild( cElement ));
      if(!pStackTop)
      {
         return;
      }
      m_paNode.push_back(pStackTop);
      return;
   }

   const int iParentElement = m_paNode.size() - 2;
   if(iParentElement < 0)
   {
      pStackTop = dynamic_cast<TiXmlElement*>
         (m_qDocument->InsertEndChild( cElement ));
      if(!pStackTop)
      {
         return;
      }
      m_paNode.push_back(pStackTop);
      return;
   }
   else
   {
      TiXmlElement* pNode = m_paNode[iParentElement];
      if(!pNode)
      {
         return;
      }   
      pStackTop = dynamic_cast<TiXmlElement*>
         (pNode->InsertEndChild( cElement ));
      if(!pStackTop)
      {
         return;
      }

      // Overwrite top stack position.
      const int iFinalElement = m_paNode.size() - 1;
      m_paNode[iFinalElement] = pStackTop;
      //pNode = pStackTop;
   }
}
ConsoleMethod(SimXMLDocument, addNewElement, void, 3, 3,
              "Create new element as child of current stack element "
              "and push new element on to stack.")
{
   object->addNewElement( argv[2] );
}

// -----------------------------------------------------------------------------
// Write XML document declaration.
// -----------------------------------------------------------------------------
void SimXMLDocument::addHeader(void)
{
   TiXmlDeclaration cDeclaration("1.0", "utf-8", "yes");
   m_qDocument->InsertEndChild(cDeclaration);
}
ConsoleMethod(SimXMLDocument, addHeader, void, 2, 2, "Add XML header to document.")
{
   object->addHeader();
}

void SimXMLDocument::addComment(const char* comment)
{
   TiXmlComment cComment;
   cComment.SetValue(comment);
   m_qDocument->InsertEndChild(cComment);
}
ConsoleMethod(SimXMLDocument, addComment, void, 3, 3, "Add the given comment as a child of current stack element.")
{
   object->addComment(argv[2]);
}

const char* SimXMLDocument::readComment( S32 index )
{
   // Clear the current attribute pointer
   m_CurrentAttribute = 0;

   // Push the first element found under the current element of the given name
   if(!m_paNode.empty())
   {
      const int iLastElement = m_paNode.size() - 1;
      TiXmlElement* pNode = m_paNode[iLastElement];
      if(!pNode)
      {
         return "";
      }
      TiXmlNode* node = pNode->FirstChild();
      for( S32 i = 0; i < index; i++ )
      {
         if( !node )
            return "";

         node = node->NextSiblingElement();
      }

      if( node )
      {
         TiXmlComment* comment = node->ToComment();
         if( comment )
            return comment->Value();
      }
   }
   else
   {
      if(!m_qDocument)
      {
         return "";
      }
      TiXmlNode* node = m_qDocument->FirstChild();
      for( S32 i = 0; i < index; i++ )
      {
         if( !node )
            return "";

         node = node->NextSibling();
      }

      if( node )
      {
         TiXmlComment* comment = node->ToComment();
         if( comment )
            return comment->Value();
      }
   }
   return "";
}
ConsoleMethod(SimXMLDocument, readComment, const char*, 3, 3, "Returns the comment at the specified index.")
{
   return object->readComment( dAtoi( argv[2] ) );
}

void SimXMLDocument::addText(const char* text)
{
   if(m_paNode.empty())
      return;

   const int iFinalElement = m_paNode.size() - 1;
   TiXmlElement* pNode = m_paNode[iFinalElement];
   if(!pNode)
      return;

   TiXmlText cText(text);
   pNode->InsertEndChild( cText );
}

ConsoleMethod(SimXMLDocument, addText, void, 3, 3, "Add the given text as a child of current stack element.")
{
   object->addText(argv[2]);
}

const char* SimXMLDocument::getText()
{
   if(m_paNode.empty())
      return "";

   const int iFinalElement = m_paNode.size() - 1;
   TiXmlNode* pNode = m_paNode[iFinalElement];
   if(!pNode)
      return "";

   if(!pNode->FirstChild())
      return "";

   TiXmlText* text = pNode->FirstChild()->ToText();
   if( !text )
      return "";

   return text->Value();
}

ConsoleMethod(SimXMLDocument, getText, const char*, 2, 2, "Gets the text from the current stack element.")
{
   const char* text = object->getText();
   if( !text )
      return "";

   char* pBuffer = Con::getReturnBuffer(dStrlen( text ) + 1);
   dStrcpy( pBuffer, text );
   return pBuffer;
}

void SimXMLDocument::removeText()
{
   if(m_paNode.empty())
      return;

   const int iFinalElement = m_paNode.size() - 1;
   TiXmlElement* pNode = m_paNode[iFinalElement];
   if(!pNode)
      return;

   TiXmlText* text = pNode->FirstChild()->ToText();
   if( !text )
      return;

   pNode->RemoveChild(text);
}

ConsoleMethod(SimXMLDocument, removeText, void, 2, 2, "Remove any text on the current stack element.")
{
   object->removeText();
}

void SimXMLDocument::addData(const char* text)
{
   if(m_paNode.empty())
      return;

   const int iFinalElement = m_paNode.size() - 1;
   TiXmlElement* pNode = m_paNode[iFinalElement];
   if(!pNode)
      return;

   TiXmlText cText(text);
   pNode->InsertEndChild( cText );
}

ConsoleMethod(SimXMLDocument, addData, void, 3, 3, "Add the given text as a child of current stack element.")
{
   object->addData(argv[2]);
}

const char* SimXMLDocument::getData()
{
   if(m_paNode.empty())
      return "";

   const int iFinalElement = m_paNode.size() - 1;
   TiXmlNode* pNode = m_paNode[iFinalElement];
   if(!pNode)
      return "";

   TiXmlText* text = pNode->FirstChild()->ToText();
   if( !text )
      return "";

   return text->Value();
}

ConsoleMethod(SimXMLDocument, getData, const char*, 2, 2, "Gets the text from the current stack element.")
{
   const char* text = object->getData();
   if( !text )
      return "";

   char* pBuffer = Con::getReturnBuffer(dStrlen( text ) + 1);
   dStrcpy( pBuffer, text );
   return pBuffer;
}

////EOF/////////////////////////////////////////////////////////////////////////
