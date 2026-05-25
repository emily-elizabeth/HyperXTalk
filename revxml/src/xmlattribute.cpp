#include "cxml.h"

char *CXMLAttribute::GetName()
{
	if (!isinited()) 	return NULL;
	return (char *)attribute->name;

}

Bool CXMLAttribute::GoNext()
{
	if (!isinited()) return False;
	Bool retval = attribute->next != NULL;
	if (attribute->next) attribute = attribute->next;
	return retval;
}

Bool CXMLAttribute::GoPrev()
{
	if (!isinited()) return False;
		// MDW-2013-07-09: [[ RevXmlXPath ]]
		Bool retval = attribute->prev != NULL;
		if (attribute->prev) attribute = attribute->prev;
		return retval;
}

char *CXMLAttribute::GetContent()
{
	if (!isinited()) return NULL;
	if (attribute->children && attribute->children->content)
			return (char *)attribute->children->content;
	return XMLNULLSTRING;
}
