// Config using file system
// Rob Dobson 2016-2018

#include "ConfigFile.h"

static const char* MODULE_PREFIX = "ConfigFile: ";

// Get max length
int ConfigFile::getMaxLen()
{
    return _configMaxDataLen;
}

// Clear
void ConfigFile::clear()
{
    // Delete the backing SPIFFS file
    _fileManager.deleteFile(_fileSystem, _filename);
    
    // Set the config str to empty
    ConfigBase::clear();

    // Clear the config string
    setConfigData("");
}

// Initialise
bool ConfigFile::setup()
{
    // Setup base class
    ConfigBase::setup();

    // Get config string
    String configData = _fileManager.getFileContents(_fileSystem.c_str(), _filename, _configMaxDataLen);
    setConfigData(configData.c_str());
    Log.trace("%sConfig %s read len: %d\n", MODULE_PREFIX, _filename.c_str(), configData.length());
    Log.trace("%sConfig %s read: %s\n", MODULE_PREFIX, _filename.c_str(), configData.c_str());

    // Ok
    return true;
}

// Write configuration string
bool ConfigFile::writeConfig()
{
    // Check the limits on config size
    if (_dataStrJSON.length() >= _configMaxDataLen)
    {
        String truncatedStr = _dataStrJSON.substring(0, _configMaxDataLen-1);
        // Write config file
        _fileManager.setFileContents(_fileSystem.c_str(), _filename, truncatedStr);
    }
    else
    {
        // Write config file
        _fileManager.setFileContents(_fileSystem.c_str(), _filename, _dataStrJSON);
    }

    Log.trace("%sWritten %s config truncated: %d\n", MODULE_PREFIX,
                    _filename.c_str(), _dataStrJSON.length() >= _configMaxDataLen);

    // Ok
    return true;
}
