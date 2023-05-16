//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Foundation/NSFileManager.hpp
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "NSDefines.hpp"
#include "NSObject.hpp"
#include "NSTypes.hpp"

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace NS
{

_NS_ENUM(uint32_t, SearchPathDirectory) {
    ApplicationDirectory           = 1,
    DemoApplicationDirectory       = 2,
    DeveloperApplicationDirectory  = 3,
    AdminApplicationDirectory      = 4,
    LibraryDirectory               = 5,
    UserDirectory                  = 7,
    DocumentationDirectory         = 8,
    DocumentDirectory              = 9,
    DesktopDirectory               = 12,
    CachesDirectory                = 13,
    ApplicationSupportDirectory    = 14,
    DownloadsDirectory             = 15,
    InputMethodsDirectory          = 16,
    MoviesDirectory                = 17,
    MusicDirectory                 = 18,
    PicturesDirectory              = 19,
};

_NS_OPTIONS(uint32_t, SearchPathDomainMask) {
    UserDomainMask    = 1,
    LocalDomainMask   = 2,
    NetworkDomainMask = 4,
    SystemDomainMask  = 8,
    AllDomainsMask    = 0x0ffff
};

class FileManager : public Referencing<FileManager>
{
public:
    static FileManager *defaultManager();

    class NS::Array* URLsForDirectory(SearchPathDirectory directory, SearchPathDomainMask domainMask);
};
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_NS_INLINE NS::FileManager* NS::FileManager::defaultManager()
{
    return Object::sendMessage<FileManager*>(_NS_PRIVATE_CLS(NSFileManager), _NS_PRIVATE_SEL(defaultManager));
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_NS_INLINE NS::Array* NS::FileManager::URLsForDirectory(SearchPathDirectory directory, SearchPathDomainMask domainMask)
{
    return Object::sendMessage<Array*>(this, _NS_PRIVATE_SEL(URLsForDirectory_inDomains_), directory, domainMask);
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
