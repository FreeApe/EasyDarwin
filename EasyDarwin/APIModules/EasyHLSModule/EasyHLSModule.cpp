/*
	Copyright (c) 2013-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
/*
    File:       EasyHLSModule.cpp
    Contains:   EasyHLSModule
*/
#include "EasyHLSModule.h"
#include "QTSSModuleUtils.h"
#include "OSArrayObjectDeleter.h"
#include "QTSSMemoryDeleter.h"

#include "ReflectorSession.h"
#include "MyAssert.h"
#include "EasyHLSSession.h"

#ifdef __Win32__
#define EasyHLS_KEY "333565546A4969576B5A7341476C4A58714B336B6B76464659584E355247467964326C754C6D56345A534E58444661672F704C67523246326157346D516D466962334E68514449774D545A4659584E355247467964326C75564756686257566863336B3D"
#else
#define EasyHLS_KEY "333565546A4A4F576B5971414A553558714C485A4576566C59584E355A47467964326C753456634D5671442B6B75424859585A7062695A4359574A76633246414D6A41784E6B566863336C4559584A33615735555A5746745A57467A65513D3D"
#endif

// STATIC DATA
static QTSS_PrefsObject         sServerPrefs		= NULL;
static OSRefTable*				sHLSSessionMap		= NULL;
static QTSS_ServerObject		sServer				= NULL;
static QTSS_ModulePrefsObject	sModulePrefs		= NULL;

// FUNCTION PROTOTYPES
static QTSS_Error EasyHLSModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);
static QTSS_Error Register(QTSS_Register_Params* inParams);
static QTSS_Error Initialize(QTSS_Initialize_Params* inParams);
static QTSS_Error RereadPrefs();
static QTSS_Error GetDeviceStream(Easy_GetDeviceStream_Params* inParams);

//static QTSS_Error EasyHLSOpen(Easy_HLSOpen_Params* inParams);
//static QTSS_Error EasyHLSClose(Easy_HLSClose_Params* inParams);
//static char* GetHLSUrl(char* inSessionName);

// FUNCTION IMPLEMENTATIONS
QTSS_Error EasyHLSModule_Main(void* inPrivateArgs)
{
    return _stublibrary_main(inPrivateArgs, EasyHLSModuleDispatch);
}

QTSS_Error  EasyHLSModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
{
    switch (inRole)
    {
        case QTSS_Register_Role:
            return Register(&inParams->regParams);
        case QTSS_Initialize_Role:
            return Initialize(&inParams->initParams);
        case QTSS_RereadPrefs_Role:
            return RereadPrefs();
		case Easy_GetDeviceStream_Role:
			return GetDeviceStream(&inParams->easyGetDeviceStreamParams);
		//case Easy_HLSOpen_Role:
		//	return EasyHLSOpen(&inParams->easyHLSOpenParams);
		//case Easy_HLSClose_Role:
		//	return EasyHLSClose(&inParams->easyHLSCloseParams);
    }
    return QTSS_NoErr;
}

QTSS_Error Register(QTSS_Register_Params* inParams)
{

	int isEasyHLSActivated = EasyHLS_Activate(EasyHLS_KEY);
	switch(isEasyHLSActivated)
	{
	case EASY_ACTIVATE_INVALID_KEY:
		printf("EasyHLS_KEY is EASY_ACTIVATE_INVALID_KEY!\n");
		break;
	case EASY_ACTIVATE_TIME_ERR:
		printf("EasyHLS_KEY is EASY_ACTIVATE_TIME_ERR!\n");
		break;
	case EASY_ACTIVATE_PROCESS_NAME_LEN_ERR:
		printf("EasyHLS_KEY is EASY_ACTIVATE_PROCESS_NAME_LEN_ERR!\n");
		break;
	case EASY_ACTIVATE_PROCESS_NAME_ERR:
		printf("EasyHLS_KEY is EASY_ACTIVATE_PROCESS_NAME_ERR!\n");
		break;
	case EASY_ACTIVATE_VALIDITY_PERIOD_ERR:
		printf("EasyHLS_KEY is EASY_ACTIVATE_VALIDITY_PERIOD_ERR!\n");
		break;
	case EASY_ACTIVATE_SUCCESS:
		//printf("EasyHLS_KEY is EASY_ACTIVATE_SUCCESS!\n");
		break;
	}

	if(EASY_ACTIVATE_SUCCESS != isEasyHLSActivated)
		return QTSS_RequestFailed;


    // Do role & attribute setup
    (void)QTSS_AddRole(QTSS_Initialize_Role);
    (void)QTSS_AddRole(QTSS_RereadPrefs_Role);
	(void)QTSS_AddRole(Easy_GetDeviceStream_Role);
	//(void)QTSS_AddRole(Easy_HLSOpen_Role); 
	//(void)QTSS_AddRole(Easy_HLSClose_Role); 
    
    // Tell the server our name!
    static char* sModuleName = "EasyHLSModule";
    ::strcpy(inParams->outModuleName, sModuleName);

    return QTSS_NoErr;
}

QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
    // Setup module utils
    QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);
	sHLSSessionMap =  QTSServerInterface::GetServer()->GetHLSSessionMap();

    // Setup global data structures
    sServerPrefs = inParams->inPrefs;
    sServer = inParams->inServer;
    sModulePrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);

    // Call helper class initializers
    EasyHLSSession::Initialize(sModulePrefs);

	RereadPrefs();

    return QTSS_NoErr;
}

QTSS_Error RereadPrefs()
{
	return QTSS_NoErr;
}


QTSS_Error GetDeviceStream(Easy_GetDeviceStream_Params* inParams)
{
	QTSS_Error theErr = QTSS_Unimplemented;

	while (inParams->inDevice && inParams->inStreamType == easyHLSType)
	{
		char theStreamName[QTSS_MAX_NAME_LENGTH] = { 0 };
		sprintf(theStreamName, "%s%s%d", inParams->inDevice, EASY_KEY_SPLITER, inParams->inChannel);
		StrPtrLen inStreamName(theStreamName);

		EasyHLSSession* hlsSe = NULL;
		OSRef* sessionRef = sHLSSessionMap->Resolve(&inStreamName);
		if (sessionRef != NULL)
		{
			hlsSe = (EasyHLSSession*)sessionRef->GetObject();
		}
		else
		{
			OSRefTable* rtspSessionMap = QTSServerInterface::GetServer()->GetReflectorSessionMap();
			OSMutexLocker locker(rtspSessionMap->GetMutex());
			OSRef* theSessionRef = rtspSessionMap->Resolve(&inStreamName);
			ReflectorSession* theSession = NULL;

			if (theSessionRef == NULL)
			{
				theErr = QTSS_FileNotFound;
				break;
			}

			theSession = (ReflectorSession*)theSessionRef->GetObject();
			QTSS_ClientSessionObject clientSession = theSession->GetBroadcasterSession();
			Assert(theSession != NULL);

			if (clientSession == NULL)
			{
				theErr = QTSS_FileNotFound;
				break;
			}

			char* theFullRequestURL = NULL;
			(void)QTSS_GetValueAsString(clientSession, qtssCliSesFullURL, 0, &theFullRequestURL);
			QTSSCharArrayDeleter theFileNameStrDeleter(theFullRequestURL);

			if (theFullRequestURL == NULL)
			{
				theErr = QTSS_FileNotFound;
				break;
			}

			StrPtrLen inURL(theFullRequestURL);
			StrPtrLen inName(inParams->inDevice);
			hlsSe = new EasyHLSSession(&inName, &inURL, inParams->inChannel);

			QTSS_Error theErr = hlsSe->SessionStart();

			if (theErr == QTSS_NoErr)
			{
				OS_Error theErr = sHLSSessionMap->Register(hlsSe->GetRef());
				Assert(theErr == QTSS_NoErr);
			}
			else
			{
				hlsSe->Signal(Task::kKillEvent);
				theErr = QTSS_Unimplemented;
				break;
			}

			OSRef* debug = sHLSSessionMap->Resolve(&inStreamName);
			Assert(debug == hlsSe->GetRef());


			rtspSessionMap->Release(theSessionRef);
		}

		strcpy(inParams->outUrl, hlsSe->GetHLSURL());
		sHLSSessionMap->Release(hlsSe->GetRef());
		theErr = QTSS_NoErr;
		break;
	}

	return theErr;
}

//QTSS_Error EasyHLSOpen(Easy_HLSOpen_Params* inParams)
//{
//	OSMutexLocker locker (sHLSSessionMap->GetMutex());
//
//	EasyHLSSession* session = NULL;
//	StrPtrLen streamName(inParams->inStreamName);
//	OSRef* clientSesRef = sHLSSessionMap->Resolve(&streamName);
//	if(clientSesRef != NULL)
//	{
//		session = (EasyHLSSession*)clientSesRef->GetObject();
//	}
//	else
//	{
//		session = NEW EasyHLSSession(&streamName);
//
//		OS_Error theErr = sHLSSessionMap->Register(session->GetRef());
//		Assert(theErr == QTSS_NoErr);
//
//		//增加一次对RelaySession的无效引用，后面会统一释放
//		OSRef* debug = sHLSSessionMap->Resolve(&streamName);
//		Assert(debug == session->GetRef());
//	}
//	
//	//到这里，肯定是有一个EasyHLSSession可用的
//	session->SessionStart();
//
//	if(inParams->outHLSUrl)
//		qtss_sprintf(inParams->outHLSUrl,"%s",session->GetHLSURL());
//
//	sHLSSessionMap->Release(session->GetRef());
//
//	return QTSS_NoErr;
//}
//
//QTSS_Error EasyHLSClose(Easy_HLSClose_Params* inParams)
//{
//	OSMutexLocker locker (sHLSSessionMap->GetMutex());
//
//	StrPtrLen streamName(inParams->inStreamName);
//
//	OSRef* clientSesRef = sHLSSessionMap->Resolve(&streamName);
//
//	if(NULL == clientSesRef) return QTSS_RequestFailed;
//
//	EasyHLSSession* session = (EasyHLSSession*)clientSesRef->GetObject();
//
//	session->SessionRelease();
//
//	sHLSSessionMap->Release(session->GetRef());
//
//    if (session->GetRef()->GetRefCount() == 0)
//    {   
//        qtss_printf("EasyHLSModule.cpp:EasyHLSClose UnRegister and delete session =%p refcount=%"   _U32BITARG_   "\n", session->GetRef(), session->GetRef()->GetRefCount() ) ;       
//        sHLSSessionMap->UnRegister(session->GetRef());
//		session->Signal(Task::kKillEvent);
//    }
//	return QTSS_NoErr;
//}
//
//char* GetHLSUrl(char* inSessionName)
//{
//	OSRefTable* sHLSSessionMap =  QTSServerInterface::GetServer()->GetHLSSessionMap();
//
//	OSMutexLocker locker (sHLSSessionMap->GetMutex());
//
//	char* hlsURL = NULL;
//	StrPtrLen streamName(inSessionName);
//
//	OSRef* clientSesRef = sHLSSessionMap->Resolve(&streamName);
//
//	if(NULL == clientSesRef) return NULL;
//
//	EasyHLSSession* session = (EasyHLSSession*)clientSesRef->GetObject();
//
//	hlsURL = session->GetHLSURL();
//
//	sHLSSessionMap->Release(session->GetRef());
//
//	return hlsURL;
//}