/** 
 * XMLSec library
 *
 * Simple Keys Manager
 * 
 * See Copyright for the status of this software.
 * 
 * Author: Aleksey Sanin <aleksey@aleksey.com>
 */
#include "globals.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#include <xmlsec/xmlsec.h>
#include <xmlsec/xmltree.h>
#include <xmlsec/list.h>
#include <xmlsec/keys.h>
#include <xmlsec/transforms.h>
#include <xmlsec/keysmngr.h>
#include <xmlsec/errors.h>

/****************************************************************************
 *
 * Keys Manager
 *
 ***************************************************************************/
xmlSecKeysMngrPtr 
xmlSecKeysMngrCreate(void) {
    xmlSecKeysMngrPtr mngr;

    /* Allocate a new xmlSecKeysMngr and fill the fields. */
    mngr = (xmlSecKeysMngrPtr)xmlMalloc(sizeof(xmlSecKeysMngr));
    if(mngr == NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    "xmlSecKeysMngr",
		    "xmlMalloc",
		    XMLSEC_ERRORS_R_MALLOC_FAILED,
		    "sizeof(xmlSecKeysMngr)=%d", 
		    sizeof(xmlSecKeysMngr));
	return(NULL);
    }
    memset(mngr, 0, sizeof(xmlSecKeysMngr));    
    return(mngr);    
}

void
xmlSecKeysMngrDestroy(xmlSecKeysMngrPtr mngr) {
    xmlSecAssert(mngr != NULL);

    /* destroy keys store */
    if(mngr->keysStore != NULL) {
	xmlSecKeyDataStoreDestroy(mngr->keysStore);
    }
    
    /* destroy other data stores */
    if(mngr->storesList != NULL) {
	xmlSecPtrListDestroy(mngr->storesList);
    }

    memset(mngr, 0, sizeof(xmlSecKeysMngr));    
    xmlFree(mngr);    
}

int
xmlSecKeysMngrFindKey(xmlSecKeysMngrPtr mngr, xmlSecKeyPtr key, const xmlChar* name, 
		     xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecKeyDataStorePtr store;
    const xmlChar* params[1];
    int ret;
    
    xmlSecAssert2(mngr != NULL, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    
    store = xmlSecKeysMngrGetKeysStore(mngr);
    if(store == NULL) {
	/* no store. is it an error? */
	return(0);
    }
    
    params[0] = name;
    ret = xmlSecKeyDataStoreFind(store, key, params, 1, keyInfoCtx);
    if(ret < 0) {	
	xmlSecError(XMLSEC_ERRORS_HERE,
		    "xmlSecKeysMngr",
		    "xmlSecKeyDataStoreFind",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    return(0);    
}

int	
xmlSecKeysMngrFindKeyData(xmlSecKeysMngrPtr mngr, xmlSecKeyDataStoreId storeId,
			xmlSecKeyPtr key, const xmlChar** params, size_t paramsSize,
			xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecKeyDataStorePtr store;
    int ret;
    
    xmlSecAssert2(mngr != NULL, -1);
    xmlSecAssert2(storeId != xmlSecKeyDataStoreIdUnknown, -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    
    store = xmlSecKeysMngrGetDataStore(mngr, storeId);
    if(store == NULL) {
	/* no store. is it an error? */
	return(0);
    }
    
    ret = xmlSecKeyDataStoreFind(store, key, params, paramsSize, keyInfoCtx);
    if(ret < 0) {	
	xmlSecError(XMLSEC_ERRORS_HERE,
		    "xmlSecKeysMngr",
		    "xmlSecKeyDataStoreFind",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    return(0);    
}

int
xmlSecKeysMngrAdoptKeysStore(xmlSecKeysMngrPtr mngr, xmlSecKeyDataStorePtr store) {
    xmlSecAssert2(mngr != NULL, -1);
    xmlSecAssert2(xmlSecKeyDataStoreIsValid(store), -1);
    
    if(mngr->keysStore != NULL) {
	xmlSecKeyDataStoreDestroy(mngr->keysStore);
    }
    mngr->keysStore = store;
    
    return(0);
}

xmlSecKeyDataStorePtr
xmlSecKeysMngrGetKeysStore(xmlSecKeysMngrPtr mngr) {
    xmlSecAssert2(mngr != NULL, NULL);
    
    return(mngr->keysStore);
}

int
xmlSecKeysMngrAdoptDataStore(xmlSecKeysMngrPtr mngr, xmlSecKeyDataStorePtr store) {
    xmlSecKeyDataStorePtr keysStore;
    xmlSecKeyDataStorePtr tmp;
    size_t pos, size;
    
    xmlSecAssert2(mngr != NULL, -1);
    xmlSecAssert2(xmlSecKeyDataStoreIsValid(store), -1);

    /* special case */
    keysStore = xmlSecKeysMngrGetKeysStore(mngr);
    if((keysStore != NULL) && (keysStore->id == store->id)) {
	return(xmlSecKeysMngrAdoptKeysStore(mngr, store));
    }

    if(mngr->storesList == NULL) {
	mngr->storesList = xmlSecPtrListCreate(xmlSecKeyDataStorePtrListId);
	if(mngr->storesList == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			"xmlSecKeysMngr",
			"xmlSecPtrListCreate",
		        XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"xmlSecKeyDataStorePtrListId");
	    return(-1);
	}
    }

    size = xmlSecPtrListGetSize(mngr->storesList);
    for(pos = 0; pos < size; ++pos) {
	tmp = (xmlSecKeyDataStorePtr)xmlSecPtrListGetItem(mngr->storesList, pos);
	if((tmp != NULL) && (tmp->id == store->id)) {	
	    return(xmlSecPtrListSet(mngr->storesList, store, pos));
	}
    }
    
    return(xmlSecPtrListAdd(mngr->storesList, store));
}

xmlSecKeyDataStorePtr 
xmlSecKeysMngrGetDataStore(xmlSecKeysMngrPtr mngr, xmlSecKeyDataStoreId id) {
    xmlSecKeyDataStorePtr keysStore;
    
    xmlSecAssert2(mngr != NULL, NULL);
    xmlSecAssert2(id != xmlSecKeyDataStoreIdUnknown, NULL);

    /* special case */
    keysStore = xmlSecKeysMngrGetKeysStore(mngr);
    if((keysStore != NULL) && (keysStore->id == id)) {
	return(keysStore);
    } else if(mngr->storesList != NULL) {
	xmlSecKeyDataStorePtr tmp;
	size_t pos, size;
	
	size = xmlSecPtrListGetSize(mngr->storesList);
	for(pos = 0; pos < size; ++pos) {
	    tmp = (xmlSecKeyDataStorePtr)xmlSecPtrListGetItem(mngr->storesList, pos);
	    if((tmp != NULL) && (tmp->id == id)) {	
		return(tmp);
	    }
	}
    }
    
    return(NULL);
}

/****************************************************************************
 *
 * Simple Keys Store
 * 
 * keys list (xmlSecPtrList) is located after xmlSecKeyDataStore
 *
 ***************************************************************************/
#define xmlSecSimpleKeysStoreSize \
	(sizeof(xmlSecKeyDataStore) + sizeof(xmlSecPtrList))
#define xmlSecSimpleKeysStoreGetList(store) \
    ((xmlSecKeyDataStoreCheckSize((store), xmlSecSimpleKeysStoreSize)) ? \
	(xmlSecPtrListPtr)(((unsigned char*)(store)) + sizeof(xmlSecKeyDataStore)) : \
	(xmlSecPtrListPtr)NULL)

static int			xmlSecSimpleKeysStoreInitialize	(xmlSecKeyDataStorePtr store);
static void			xmlSecSimpleKeysStoreFinalize	(xmlSecKeyDataStorePtr store);
static int			xmlSecSimpleKeysStoreFind	(xmlSecKeyDataStorePtr store,
								 xmlSecKeyPtr key,
								 const xmlChar** params,
								 size_t paramsSize,
								 xmlSecKeyInfoCtxPtr keyInfoCtx);

static xmlSecKeyDataStoreKlass xmlSecSimpleKeysStoreKlass = {
    sizeof(xmlSecKeyDataKlass),
    xmlSecSimpleKeysStoreSize,

    /* data */
    BAD_CAST "simple-keys-store",	/* const xmlChar* name; */ 
        
    /* constructors/destructor */
    xmlSecSimpleKeysStoreInitialize,	/* xmlSecKeyDataStoreInitializeMethod initialize; */
    xmlSecSimpleKeysStoreFinalize,	/* xmlSecKeyDataStoreFinalizeMethod finalize; */
    xmlSecSimpleKeysStoreFind,		/* xmlSecKeyDataStoreFindMethod find; */

    /* reserved for the future */
    NULL,					/* void* reserved0; */
    NULL,					/* void* reserved1; */
};

xmlSecKeyDataStoreId 
xmlSecSimpleKeysStoreGetKlass(void) {
    return(&xmlSecSimpleKeysStoreKlass);
}

int 
xmlSecSimpleKeysStoreAdoptKey(xmlSecKeyDataStorePtr store, xmlSecKeyPtr key) {
    xmlSecPtrListPtr list;
    int ret;
    
    xmlSecAssert2(xmlSecKeyDataStoreCheckId(store, xmlSecSimpleKeysStoreId), -1);
    xmlSecAssert2(key != NULL, -1);

    list = xmlSecSimpleKeysStoreGetList(store);
    xmlSecAssert2(xmlSecPtrListCheckId(list, xmlSecKeyPtrListId), -1);

    ret = xmlSecPtrListAdd(list, key);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    "xmlSecPtrListAdd",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }

    return(0);
}

xmlSecKeyPtr 
xmlSecSimpleKeysStoreFindKey(xmlSecKeyDataStorePtr store, const xmlChar* name, 
			    xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecPtrListPtr list;
    xmlSecKeyPtr key;
    size_t pos, size;

    xmlSecAssert2(xmlSecKeyDataStoreCheckId(store, xmlSecSimpleKeysStoreId), NULL);
    xmlSecAssert2(keyInfoCtx != NULL, NULL);

    list = xmlSecSimpleKeysStoreGetList(store);
    xmlSecAssert2(xmlSecPtrListCheckId(list, xmlSecKeyPtrListId), NULL);

    size = xmlSecPtrListGetSize(list);
    for(pos = 0; pos < size; ++pos) {
	key = (xmlSecKeyPtr)xmlSecPtrListGetItem(list, pos);
	if((key != NULL) && (xmlSecKeyMatch(key, name, &(keyInfoCtx->keyReq)) == 1)) {
	    return(key);
	}
    }
    return(NULL);
}

int
xmlSecSimpleKeysStoreLoad(xmlSecKeyDataStorePtr store, const char *uri, 
			    xmlSecKeysMngrPtr keysMngr) {
    xmlDocPtr doc;
    xmlNodePtr root;
    xmlNodePtr cur;
    xmlSecKeyPtr key;
    xmlSecKeyInfoCtx keyInfoCtx;
    int ret;

    xmlSecAssert2(xmlSecKeyDataStoreCheckId(store, xmlSecSimpleKeysStoreId), -1);
    xmlSecAssert2(uri != NULL, -1);    

    doc = xmlParseFile(uri);
    if(doc == NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    "xmlParseFile",
		    XMLSEC_ERRORS_R_XML_FAILED,
		    "uri=\"%s\"", uri);
	return(-1);
    }
    
    root = xmlDocGetRootElement(doc);
    if(!xmlSecCheckNodeName(root, BAD_CAST "Keys", xmlSecNs)) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    xmlSecNodeGetName(root),
		    XMLSEC_ERRORS_R_INVALID_NODE,
		    "<xmlsec:Keys>");
	xmlFreeDoc(doc);
	return(-1);
    }
        
    cur = xmlSecGetNextElementNode(root->children);
    while((cur != NULL) && xmlSecCheckNodeName(cur, BAD_CAST "KeyInfo", xmlSecDSigNs)) {  
	key = xmlSecKeyCreate();
	if(key == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			xmlSecNodeGetName(cur),
			XMLSEC_ERRORS_R_INVALID_NODE,
			"<dsig:KeyInfo>");
	    xmlFreeDoc(doc);
	    return(-1);
	}

	ret = xmlSecKeyInfoCtxInitialize(&keyInfoCtx, NULL);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
		        xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			"xmlSecKeyInfoCtxInitialize",
			XMLSEC_ERRORS_R_INVALID_NODE,
			XMLSEC_ERRORS_NO_MESSAGE);
	    xmlSecKeyDestroy(key);
	    xmlFreeDoc(doc);
	    return(-1);
	}
	
	keyInfoCtx.mode 		= xmlSecKeyInfoModeRead;
	keyInfoCtx.keysMngr		= keysMngr;
	keyInfoCtx.stopWhenKeyFound	= 0;
        keyInfoCtx.keyReq.keyId		= xmlSecKeyDataIdUnknown;
	keyInfoCtx.keyReq.keyType	= xmlSecKeyDataTypeAny;
	keyInfoCtx.keyReq.keyUsage 	= xmlSecKeyDataUsageAny;

	ret = xmlSecKeyInfoNodeRead(cur, key, &keyInfoCtx);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			"xmlSecKeyInfoNodeRead",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    xmlSecKeyInfoCtxFinalize(&keyInfoCtx);
	    xmlSecKeyDestroy(key);
	    xmlFreeDoc(doc);
	    return(-1);
	}
	xmlSecKeyInfoCtxFinalize(&keyInfoCtx);
	
	if(xmlSecKeyIsValid(key)) {
    	    ret = xmlSecSimpleKeysStoreAdoptKey(store, key);
	    if(ret < 0) {
		xmlSecError(XMLSEC_ERRORS_HERE,
			    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			    "xmlSecSimpleKeysStoreAdoptKey",
			    XMLSEC_ERRORS_R_XMLSEC_FAILED,
			    XMLSEC_ERRORS_NO_MESSAGE);
		xmlSecKeyDestroy(key);
		xmlFreeDoc(doc);
		return(-1);
	    }
	} else {
	    /* we have an unknown key in our file, just ignore it */
	    xmlSecKeyDestroy(key);
	}
        cur = xmlSecGetNextElementNode(cur->next);
    }
    
    if(cur != NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    xmlSecNodeGetName(cur),
		    XMLSEC_ERRORS_R_INVALID_NODE,
		    "no more nodes expected");
	xmlFreeDoc(doc);
	return(-1);	    
    }
    
    xmlFreeDoc(doc);
    return(0);

}

int
xmlSecSimpleKeysStoreSave(xmlSecKeyDataStorePtr store, const char *filename, xmlSecKeyDataType type) {
    xmlSecKeyInfoCtx keyInfoCtx;
    xmlSecPtrListPtr list;
    xmlSecKeyPtr key;
    size_t i, keysSize;    
    xmlDocPtr doc;
    xmlNodePtr root;
    xmlNodePtr cur;
    xmlSecKeyDataPtr data;
    xmlSecPtrListPtr idsList;
    xmlSecKeyDataId dataId;
    size_t idsSize, j;
    int ret;

    xmlSecAssert2(xmlSecKeyDataStoreCheckId(store, xmlSecSimpleKeysStoreId), -1);
    xmlSecAssert2(filename != NULL, -1);    

    list = xmlSecSimpleKeysStoreGetList(store);
    xmlSecAssert2(xmlSecPtrListCheckId(list, xmlSecKeyPtrListId), -1);

    /* create doc */
    doc = xmlNewDoc(BAD_CAST "1.0");
    if(doc == NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    "xmlNewDoc",
		    XMLSEC_ERRORS_R_XML_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    /* create root node "Keys" */
    root = xmlNewDocNode(doc, NULL, BAD_CAST "Keys", NULL); 
    if(root == NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    "xmlNewDocNode",
		    XMLSEC_ERRORS_R_XML_FAILED,
		    "Keys");
	xmlFreeDoc(doc);
	return(-1);
    }
    xmlDocSetRootElement(doc, root);
    if(xmlNewNs(root, xmlSecNs, NULL) == NULL) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    "xmlNewNs",
		    XMLSEC_ERRORS_R_XML_FAILED,
		    "xmlSecNs");
	xmlFreeDoc(doc); 
	return(-1);
    }
    
    
    idsList = xmlSecKeyDataIdsGet();	
    xmlSecAssert2(idsList != NULL, -1);
	
    keysSize = xmlSecPtrListGetSize(list);
    idsSize = xmlSecPtrListGetSize(idsList);
    for(i = 0; i < keysSize; ++i) {
	key = (xmlSecKeyPtr)xmlSecPtrListGetItem(list, i);
	xmlSecAssert2(key != NULL, -1);
	    
    	cur = xmlSecAddChild(root, BAD_CAST "KeyInfo", xmlSecDSigNs);
	if(cur == NULL) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			"xmlSecAddChild",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			"<dsig:KeyInfo>");
	    xmlFreeDoc(doc); 
	    return(-1);
	}

	/* special data key name */
	if(xmlSecKeyGetName(key) != NULL) {
    	    if(xmlSecAddChild(cur, BAD_CAST "KeyName", xmlSecDSigNs) == NULL) {
		xmlSecError(XMLSEC_ERRORS_HERE,
			    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			    "xmlSecAddChild",
			    XMLSEC_ERRORS_R_XMLSEC_FAILED,
			    "<dsig:KeyName>");
		xmlFreeDoc(doc); 
		return(-1);
	    }
	}
    
	/* create nodes for other keys data */
	for(j = 0; j < idsSize; ++j) {
	    dataId = (xmlSecKeyDataId)xmlSecPtrListGetItem(idsList, j);
	    xmlSecAssert2(dataId != xmlSecKeyDataIdUnknown, -1);

	    if(dataId->dataNodeName == NULL) {
		continue;
	    }
	    
	    data = xmlSecKeyGetData(key, dataId);
	    if(data == NULL) {
		continue;
	    }

	    if(xmlSecAddChild(cur, dataId->dataNodeName, dataId->dataNodeNs) == NULL) {
		xmlSecError(XMLSEC_ERRORS_HERE,
			    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			    "xmlSecAddChild",
			    XMLSEC_ERRORS_R_XMLSEC_FAILED,
			    "node=\"%s\"", dataId->dataNodeName);
		xmlFreeDoc(doc); 
	        return(-1);
	    }
	}

    	ret = xmlSecKeyInfoCtxInitialize(&keyInfoCtx, NULL);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
		    	xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			"xmlSecKeyInfoCtxInitialize",
			XMLSEC_ERRORS_R_INVALID_NODE,
			XMLSEC_ERRORS_NO_MESSAGE);
	    xmlFreeDoc(doc);
	    return(-1);
	}

	keyInfoCtx.mode 		= xmlSecKeyInfoModeWrite;
	keyInfoCtx.stopWhenKeyFound	= 0;
    	keyInfoCtx.keyReq.keyId	= xmlSecKeyDataIdUnknown;
	keyInfoCtx.keyReq.keyType	= type;
	keyInfoCtx.keyReq.keyUsage 	= xmlSecKeyDataUsageAny;

	/* finally write key in the node */
	ret = xmlSecKeyInfoNodeWrite(cur, key, &keyInfoCtx);
	if(ret < 0) {
	    xmlSecError(XMLSEC_ERRORS_HERE,
			xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
			"xmlSecKeyInfoNodeWrite",
			XMLSEC_ERRORS_R_XMLSEC_FAILED,
			XMLSEC_ERRORS_NO_MESSAGE);
	    xmlSecKeyInfoCtxFinalize(&keyInfoCtx);
	    xmlFreeDoc(doc); 
	    return(-1);
	}		
	xmlSecKeyInfoCtxFinalize(&keyInfoCtx);
    }
    
    /* now write result */
    ret = xmlSaveFormatFile(filename, doc, 1);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    "xmlSaveFormatFile",
		    XMLSEC_ERRORS_R_XML_FAILED,
		    "filename=\"%s\"", filename);
	xmlFreeDoc(doc); 
	return(-1);
    }	   
    
    xmlFreeDoc(doc);
    return(0);
}

static int
xmlSecSimpleKeysStoreInitialize(xmlSecKeyDataStorePtr store) {
    xmlSecPtrListPtr list;
    int ret;

    xmlSecAssert2(xmlSecKeyDataStoreCheckId(store, xmlSecSimpleKeysStoreId), -1);

    list = xmlSecSimpleKeysStoreGetList(store);
    xmlSecAssert2(list != NULL, -1);
    
    ret = xmlSecPtrListInitialize(list, xmlSecKeyPtrListId);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    "xmlSecPtrListInitialize",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    "xmlSecKeyPtrListId");
	return(-1);
    }

    return(0);    
}

static void
xmlSecSimpleKeysStoreFinalize(xmlSecKeyDataStorePtr store) {
    xmlSecPtrListPtr list;
    
    xmlSecAssert(xmlSecKeyDataStoreCheckId(store, xmlSecSimpleKeysStoreId));
    
    list = xmlSecSimpleKeysStoreGetList(store);
    xmlSecAssert(list != NULL);
    
    xmlSecPtrListFinalize(list);
}

static int
xmlSecSimpleKeysStoreFind(xmlSecKeyDataStorePtr store,  xmlSecKeyPtr key, 
			const xmlChar** params, size_t paramsSize,
			xmlSecKeyInfoCtxPtr keyInfoCtx) {
    xmlSecKeyPtr storedKey;
    const xmlChar* name = NULL;
    int ret;
    
    xmlSecAssert2(xmlSecKeyDataStoreCheckId(store, xmlSecSimpleKeysStoreId), -1);
    xmlSecAssert2(key != NULL, -1);
    xmlSecAssert2(keyInfoCtx != NULL, -1);
    xmlSecAssert2((paramsSize == 0) || (paramsSize == 1), -1);

    /* get name (it might be NULL) */
    if(paramsSize > 0) {	
	name = params[0];
    }
    
    storedKey = xmlSecSimpleKeysStoreFindKey(store, name, keyInfoCtx);
    if(storedKey == NULL) {
	/* found nothing */
	/* todo: add key info ctx parameter to report error/bail out in this case */
	return(0);
    }
    
    /* erase any current information in the key */
    xmlSecKeyEmpty(key);
    
    /* and copy the key from keys storage */
    ret = xmlSecKeyCopy(key, storedKey);
    if(ret < 0) {
	xmlSecError(XMLSEC_ERRORS_HERE,
		    xmlSecErrorsSafeString(xmlSecKeyDataStoreGetName(store)),
		    "xmlSecKeyCopy",
		    XMLSEC_ERRORS_R_XMLSEC_FAILED,
		    XMLSEC_ERRORS_NO_MESSAGE);
	return(-1);
    }
    
    return(0);
}

