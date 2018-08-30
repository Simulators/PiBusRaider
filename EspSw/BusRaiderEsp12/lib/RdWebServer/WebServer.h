// WebServer
// Rob Dobson 2012-2018

#pragma once

#include <Arduino.h>
#include <FS.h>
#if defined (ESP8266)
#include "ESPAsyncTCP.h"
#else
#include "AsyncTCP.h"
#endif
#include <ESPAsyncWebServer.h>
#include "RestAPIEndpoints.h"
#include "WebAutogenResources.h"

typedef void WebServerFileUploadHandlerT(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);


class WebServer
{
  public:
    AsyncWebServer *_pServer;
    bool _begun;
    bool _webServerEnabled;
    WebServerFileUploadHandlerT* _pFileUploadHandler;
    String _fileUploadResponseJson;

    WebServer()
    {
        _pServer = NULL;
        _begun = false;
        _webServerEnabled = false;
        _pFileUploadHandler = NULL;
    }

    ~WebServer()
    {
        if (_pServer)
            delete _pServer;
    }

    void setup(ConfigBase& hwConfig)
    {
        // Enable
        _webServerEnabled = hwConfig.getLong("webServerEnabled", 0) != 0;

        // Create server
        if (_webServerEnabled)
        {
            int port = hwConfig.getLong("webServerPort", 80);
            _pServer = new AsyncWebServer(port);
        
            // Add the autogenResources (static web pages, favicon, etc)
            addStaticResources(__webAutogenResources, __webAutogenResourcesCount);
        }
    }

    void addEndpoints(RestAPIEndpoints &endpoints)
    {
        // Check enabled
        if (!_webServerEnabled)
            return;

        // Number of endpoints to add
        int numEndPoints = endpoints.getNumEndpoints();

        // Add each endpoint
        for (int i = 0; i < numEndPoints; i++)
        {
            // Get endpoint info
            RestAPIEndpointDef::EndpointType endpointType = RestAPIEndpointDef::ENDPOINT_CALLBACK;
            RestAPIEndpointDef::EndpointMethod endpointMethod = RestAPIEndpointDef::ENDPOINT_GET;
            RestAPIFunction endpointCallback;
            const char *pEndpointStr = endpoints.getNthEndpointStr(i, endpointType, endpointMethod, endpointCallback);
            RestAPIEndpointDef *pEndpoint = endpoints.getEndpoint(pEndpointStr);
            if (!pEndpoint)
                continue;
            // const char *pEndpointMethodStr = endpoints.getEndpointMethodStr(endpointMethod);

            // Mapping of GET/POST/ETC
            int webMethod = HTTP_GET;
            switch (endpointMethod)
            {
            case RestAPIEndpointDef::ENDPOINT_PUT:
                webMethod = HTTP_PUT;
                break;
            case RestAPIEndpointDef::ENDPOINT_POST:
                webMethod = HTTP_POST;
                break;
            case RestAPIEndpointDef::ENDPOINT_DELETE:
                webMethod = HTTP_DELETE;
                break;
            default:
                webMethod = HTTP_GET;
                break;
            }

            // Debug
            Log.trace("WebServer: Adding API %s type %s\n", pEndpoint->_endpointStr.c_str(), endpoints.getEndpointTypeStr(endpointType));

            // Handle the endpoint
            _pServer->on(("/" + pEndpoint->_endpointStr).c_str(), webMethod, [pEndpoint, endpointCallback](AsyncWebServerRequest *request) {
                // Default response
                String respStr("{ \"rslt\": \"unknown\" }");

                // Make the required action
                if (pEndpoint->_endpointType == RestAPIEndpointDef::ENDPOINT_CALLBACK)
                {
                    // endpointCallback(request->url, respStr);
                    String reqUrl = request->url();
                    if (pEndpoint->_endpointStr.equals("q"))
                        Log.verbose("WebServer: Calling %s url %s\n", pEndpoint->_endpointStr.c_str(), request->url().c_str());
                    else
                        Log.trace("WebServer: Calling %s url %s\n", pEndpoint->_endpointStr.c_str(), request->url().c_str());
                    pEndpoint->callFn(reqUrl, respStr);
                }
                else
                {
                    Log.trace("WebServer: Unknown type %s url %s\n", pEndpoint->_endpointStr.c_str(), request->url().c_str());
                }
                request->send(200, "application/json", respStr.c_str());
            });
        }
    }

    void begin(bool accessControlAllowOriginAll)
    {
        if (_pServer && !_begun)
        {
            if (accessControlAllowOriginAll)
                DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
            _pServer->begin();
            _begun = true;
            Log.notice("WebServer: begun\n");
        }
    }

    // Add resources to the web server
    void addStaticResources(const WebServerResource *pResources, int numResources)
    {
        // Add each resource
        for (int i = 0; i < numResources; i++)
        {
            String alias = pResources[i]._pResId;
            alias.trim();
            if (!alias.startsWith("/"))
                alias = "/" + alias;
            addStaticResource(&(pResources[i]), alias.c_str());
            // Check for index.html and alias to "/"
            if (strcasecmp(pResources[i]._pResId, "index.html") == 0)
            {
                addStaticResource(&(pResources[i]), "/");
            }
        }
    }

    static void parseAndAddHeaders(AsyncWebServerResponse *response, const char *pHeaders)
    {
        String headerStr(pHeaders);
        unsigned int strPos = 0;
        while (strPos < headerStr.length())
        {
            int sepPos = headerStr.indexOf(":", strPos);
            if (sepPos == -1)
                break;
            String headerName = headerStr.substring(strPos, sepPos);
            headerName.trim();
            int lineEndPos = std::max(headerStr.indexOf("\r"), headerStr.indexOf("\n"));
            if (lineEndPos == -1)
                lineEndPos = headerStr.length() - 1;
            String headerVal = headerStr.substring(sepPos + 1, lineEndPos);
            headerVal.trim();
            if ((headerName.length() > 0) && (headerVal.length() > 0))
            {
                Log.notice("Adding header %s: %s", headerName.c_str(), headerVal.c_str());
            }
            // Move to next header
            strPos = lineEndPos + 1;
        }
    }

    void addFileUploadHandler(WebServerFileUploadHandlerT* pFileUploadHandler, 
                const char * responseJSON)
    {
        _pFileUploadHandler = pFileUploadHandler;
        if (_pFileUploadHandler == NULL)
            return;

        // upload a file to /upload
        _fileUploadResponseJson = responseJSON;
        _pServer->on("/upload", HTTP_POST, [this](AsyncWebServerRequest *request){
            request->send(200, "application/json", _fileUploadResponseJson.c_str());
        }, (*_pFileUploadHandler));        
    }

  private:
    void addStaticResource(const WebServerResource *pResource, const char *pAliasPath = NULL)
    {
        // Check for alias
        const char *pPath = pResource->_pResId;
        if (pAliasPath != NULL)
            pPath = pAliasPath;

        // Debug
        Log.trace("WebServer: Adding static resource %s mime %s encoding %s len %d\n", pPath,
                  pResource->_pMimeType, pResource->_pContentEncoding, pResource->_dataLen);

        // Static pages
        _pServer->on(pPath, HTTP_GET, [pResource](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", pResource->_pData, pResource->_dataLen);
            if ((pResource->_pContentEncoding != NULL) && (strlen(pResource->_pContentEncoding) != 0))
                response->addHeader("Content-Encoding", pResource->_pContentEncoding);
            if ((pResource->_pAccessControlAllowOrigin != NULL) && (strlen(pResource->_pAccessControlAllowOrigin) != 0))
                response->addHeader("Access-Control-Allow-Origin", pResource->_pAccessControlAllowOrigin);
            if (pResource->_noCache)
                response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            if ((pResource->_pExtraHeaders != NULL) && (strlen(pResource->_pExtraHeaders) != 0))
                parseAndAddHeaders(response, pResource->_pExtraHeaders);
            request->send(response);
        });
    }
};