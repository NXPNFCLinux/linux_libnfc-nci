/******************************************************************************
 *
 *  Copyright (C) 2011-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2013-2014 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include "OverrideLog.h"
#include "nci_config.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include "phNxpLog.h"
#include <unordered_map>

#define LOG_TAG "NfcAdaptation"

const char alternative_config_path[] = CONFIG_PATH;
const char transport_config_path[] = "/etc/";

#define config_name             "libnfc-nci.conf"
#define extra_config_base       "libnfc-nci-"
#define extra_config_ext        ".conf"
#define     IsStringValue       0x80000000

using namespace::std;

unordered_map<string, void*> confMap;

class CNfcParam : public string
{
public:
    CNfcParam();
    CNfcParam(const char* name, const string& value);
    CNfcParam(const char* name, unsigned long value);
    virtual ~CNfcParam();
    unsigned long numValue() const {return m_numValue;}
    const char*   str_value() const {return m_str_value.c_str();}
    size_t        str_len() const   {return m_str_value.length();}
private:
    string          m_str_value;
    unsigned long   m_numValue;
};

class CNfcConfig : public vector<const CNfcParam*>
{
public:
    virtual ~CNfcConfig();
    static CNfcConfig& GetInstance();
    friend void readOptionalConfig(const char* optional);

    bool    getValue(const char* name, char* pValue, size_t& len) const;
    bool    getValue(const char* name, unsigned long& rValue) const;
    bool    getValue(const char* name, unsigned short & rValue) const;
    const CNfcParam*    find(const char* p_name) const;
    void    clean();
private:
    CNfcConfig();
    bool    readConfig(const char* name, bool bResetContent);
    void    moveFromList();
    void    moveToList();
    void    add(const CNfcParam* pParam);
    list<const CNfcParam*> m_list;
    bool    mValidFile;

    unsigned long   state;

    inline bool Is(unsigned long f) {return (state & f) == f;}
    inline void Set(unsigned long f) {state |= f;}
    inline void Reset(unsigned long f) {state &= ~f;}
};

/*******************************************************************************
**
** Function:    isPrintable()
**
** Description: detremine if a char is printable
**
** Returns:     none
**
*******************************************************************************/
inline bool isPrintable(char c)
{
    return  (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '/' || c == '_' || c == '-' || c == '.';
}

/*******************************************************************************
**
** Function:    isDigit()
**
** Description: detremine if a char is numeral digit
**
** Returns:     none
**
*******************************************************************************/
inline bool isDigit(char c, int base)
{
    if ('0' <= c && c <= '9')
        return true;
    if (base == 16)
    {
        if (('A' <= c && c <= 'F') ||
            ('a' <= c && c <= 'f') )
            return true;
    }
    return false;
}

/*******************************************************************************
**
** Function:    getDigitValue()
**
** Description: return numercal value of a char
**
** Returns:     none
**
*******************************************************************************/
inline int getDigitValue(char c, int base)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    if (base == 16)
    {
        if ('A' <= c && c <= 'F')
            return c - 'A' + 10;
        else if ('a' <= c && c <= 'f')
            return c - 'a' + 10;
    }
    return 0;
}

/*******************************************************************************
**
** Function:    CNfcConfig::readConfig()
**
** Description: read Config settings and parse them into a linked list
**              move the element from linked list to a array at the end
**
** Returns:     none
**
*******************************************************************************/
bool CNfcConfig::readConfig(const char* name, bool bResetContent)
{
    enum {
        BEGIN_LINE = 1,
        TOKEN,
        STR_VALUE,
        NUM_VALUE,
        BEGIN_HEX,
        BEGIN_QUOTE,
        END_LINE
    };

    FILE*   fd = NULL;
    string  token;
    string  strValue;
    unsigned long    numValue = 0;
    CNfcParam* pParam = NULL;
    int     i = 0;
    int     base = 0;
    char    c = 0;

    state = BEGIN_LINE;
    /* open config file, read it into a buffer */
    if ((fd = fopen(name, "rb")) == NULL)
    {
        //ALOGD("%s Cannot open config file %s\n", __func__, name);
        if (bResetContent)
        {
            //ALOGD("%s Using default value for all settings\n", __func__);
        mValidFile = false;
        }
        return false;
    }
    //ALOGD("%s Opened %s config %s\n", __func__, (bResetContent ? "base" : "optional"), name);

    mValidFile = true;
    if (size() > 0)
    {
        if (bResetContent)
        clean();
        else
            moveToList();
    }

    for (;;)
    {
        if (feof(fd) || fread(&c, 1, 1, fd) != 1)
        {
            if (state == BEGIN_LINE)
                break;

            // got to the EOF but not in BEGIN_LINE state so the file
            // probably does not end with a newline, so the parser has
            // not processed current line, simulate a newline in the file
            c = '\n';
        }

        switch (state & 0xff)
        {
        case BEGIN_LINE:
            if (c == '#')
                state = END_LINE;
            else if (isPrintable(c))
            {
                i = 0;
                token.erase();
                strValue.erase();
                state = TOKEN;
                token.push_back(c);
            }
            break;
        case TOKEN:
            if (c == '=')
            {
                token.push_back('\0');
                state = BEGIN_QUOTE;
            }
            else if (isPrintable(c))
                token.push_back(c);
            else
                state = END_LINE;
            break;
        case BEGIN_QUOTE:
            if (c == '"')
            {
                state = STR_VALUE;
                base = 0;
            }
            else if (c == '0')
                state = BEGIN_HEX;
            else if (isDigit(c, 10))
            {
                state = NUM_VALUE;
                base = 10;
                numValue = getDigitValue(c, base);
                i = 0;
            }
            else if (c == '{')
            {
                state = NUM_VALUE;
                base = 16;
                i = 0;
                Set(IsStringValue);
            }
            else
                state = END_LINE;
            break;
        case BEGIN_HEX:
            if (c == 'x' || c == 'X')
            {
                state = NUM_VALUE;
                base = 16;
                numValue = 0;
                i = 0;
                break;
            }
            else if (isDigit(c, 10))
            {
                state = NUM_VALUE;
                base = 10;
                numValue = getDigitValue(c, base);
                break;
            }
            else if (c != '\n' && c != '\r')
            {
                state = END_LINE;
                break;
            }
            // fal through to numValue to handle numValue

        case NUM_VALUE:
            if (isDigit(c, base))
            {
                numValue *= base;
                numValue += getDigitValue(c, base);
                ++i;
            }
            else if (base == 16 && (c == ':' || c == '-' || c == ' ' || c == '}'))
            {
                if (i > 0)
                {
                    int n = (i+1) / 2;
                    while (n-- > 0)
                    {
                        unsigned char c = (numValue >> (n * 8))  & 0xFF;
                        strValue.push_back(c);
                    }
                }
                Set(IsStringValue);
                numValue = 0;
                i = 0;
            }
            else
            {
                if (c == '\n' || c == '\r')
                    state = BEGIN_LINE;
                else
                    state = END_LINE;
                if (Is(IsStringValue) && base == 16 && i > 0)
                {
                    int n = (i+1) / 2;
                    while (n-- > 0)
                        strValue.push_back(((numValue >> (n * 8))  & 0xFF));
                }
                if (strValue.length() > 0)
                    pParam = new CNfcParam(token.c_str(), strValue);
                else
                    pParam = new CNfcParam(token.c_str(), numValue);
                add(pParam);
                strValue.erase();
                numValue = 0;
            }
            break;
        case STR_VALUE:
            if (c == '"')
            {
                strValue.push_back('\0');
                state = END_LINE;
                pParam = new CNfcParam(token.c_str(), strValue);
                add(pParam);
            }
            else if (isPrintable(c))
                strValue.push_back(c);
            break;
        case END_LINE:
            if (c == '\n' || c == '\r')
                state = BEGIN_LINE;
            break;
        default:
            break;
        }

        if (feof(fd))
            break;
    }

    fclose(fd);

    moveFromList();
    return size() > 0;
}

/*******************************************************************************
**
** Function:    CNfcConfig::CNfcConfig()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig::CNfcConfig() :
    mValidFile(true),
    state(0)
{
}

/*******************************************************************************
**
** Function:    CNfcConfig::~CNfcConfig()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig::~CNfcConfig()
{
}

/*******************************************************************************
**
** Function:    CNfcConfig::GetInstance()
**
** Description: get class singleton object
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig& CNfcConfig::GetInstance()
{
    static CNfcConfig theInstance;
    string strPath;
    string cfg_name;

    cfg_name.assign(config_name);

    if (theInstance.size() == 0 && theInstance.mValidFile)
    {
        if (alternative_config_path[0] != '\0')
        {
            strPath.assign(alternative_config_path);
            strPath += cfg_name;
#if (NFC_CFG_DEBUG == 0x01)
            /* Since the config file is loaded before reading the configuration,
             * using the log macro may not print the below debug message, hence
             * fprintf used */
            fprintf(stderr, "%s: Reading Config File: %s...\n", __FUNCTION__, strPath.c_str());
#endif
            theInstance.readConfig(strPath.c_str(), true);
        }
        if (theInstance.empty())
        {
            strPath.assign(transport_config_path);
            strPath += cfg_name;
#if (NFC_CFG_DEBUG == 0x01)
            fprintf(stderr, "%s: Reading Config File: %s...\n", __FUNCTION__, strPath.c_str());
#endif
            theInstance.readConfig(strPath.c_str(), true);
        }
    }

    return theInstance;
}

/*******************************************************************************
**
** Function:    CNfcConfig::getValue()
**
** Description: get a string value of a setting
**
** Returns:     true if setting exists
**              false if setting does not exist
**
*******************************************************************************/
bool CNfcConfig::getValue(const char* name, char* pValue, size_t& len) const
{
    const CNfcParam* pParam = find(name);
    if (pParam == NULL)
        return false;

    if (pParam->str_len() > 0)
    {
        memset(pValue, 0, len);
        if (len > pParam->str_len())
            len  = pParam->str_len();
        memcpy(pValue, pParam->str_value(), len);
        return true;
    }
    return false;
}

/*******************************************************************************
**
** Function:    CNfcConfig::getValue()
**
** Description: get a long numerical value of a setting
**
** Returns:     true if setting exists
**              false if setting does not exist
**
*******************************************************************************/
bool CNfcConfig::getValue(const char* name, unsigned long& rValue) const
{
    const CNfcParam* pParam = find(name);
    if (pParam == NULL)
        return false;

    if (pParam->str_len() == 0)
    {
        rValue = static_cast<unsigned long>(pParam->numValue());
        return true;
    }
    return false;
}

/*******************************************************************************
**
** Function:    CNfcConfig::getValue()
**
** Description: get a short numerical value of a setting
**
** Returns:     true if setting exists
**              false if setting does not exist
**
*******************************************************************************/
bool CNfcConfig::getValue(const char* name, unsigned short& rValue) const
{
    const CNfcParam* pParam = find(name);
    if (pParam == NULL)
        return false;

    if (pParam->str_len() == 0)
    {
        rValue = static_cast<unsigned short>(pParam->numValue());
        return true;
    }
    return false;
}

/*******************************************************************************
**
** Function:    CNfcConfig::find()
**
** Description: search if a setting exist in the setting array
**
** Returns:     pointer to the setting object
**
*******************************************************************************/
const CNfcParam* CNfcConfig::find(const char* p_name) const
{
    if (size() == 0)
        return NULL;

    for (const_iterator it = begin(), itEnd = end(); it != itEnd; ++it)
    {
        if (**it < p_name)
            continue;
        else if (**it == p_name)
        {
            if((*it)->str_len() > 0)
                //ALOGD("%s found %s=%s\n", __func__, p_name, (*it)->str_value());
                ;
            else
                //ALOGD("%s found %s=(0x%lX)\n", __func__, p_name, (*it)->numValue());
                ;
            return *it;
        }
        else
            break;
    }
    return NULL;
}

/*******************************************************************************
**
** Function:    CNfcConfig::clean()
**
** Description: reset the setting array
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::clean()
{
    if (size() == 0)
        return;

    for (iterator it = begin(), itEnd = end(); it != itEnd; ++it)
        delete *it;
    clear();
}

/*******************************************************************************
**
** Function:    CNfcConfig::Add()
**
** Description: add a setting object to the list
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::add(const CNfcParam* pParam)
{
    if (m_list.size() == 0)
    {
        m_list.push_back(pParam);
        return;
    }
    for (list<const CNfcParam*>::iterator it = m_list.begin(), itEnd = m_list.end(); it != itEnd; ++it)
    {
        if (**it < pParam->c_str())
            continue;
        m_list.insert(it, pParam);
        return;
    }
    m_list.push_back(pParam);
}

/*******************************************************************************
**
** Function:    CNfcConfig::moveFromList()
**
** Description: move the setting object from list to array
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::moveFromList()
{
    if (m_list.size() == 0)
        return;

    for (list<const CNfcParam*>::iterator it = m_list.begin(), itEnd = m_list.end(); it != itEnd; ++it)
        push_back(*it);
    m_list.clear();
}

/*******************************************************************************
**
** Function:    CNfcConfig::moveToList()
**
** Description: move the setting object from array to list
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::moveToList()
{
    if (m_list.size() != 0)
        m_list.clear();

    for (iterator it = begin(), itEnd = end(); it != itEnd; ++it)
        m_list.push_back(*it);
    clear();
}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam() :
    m_numValue(0)
{
}

/*******************************************************************************
**
** Function:    CNfcParam::~CNfcParam()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::~CNfcParam()
{
}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class copy constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam(const char* name,  const string& value) :
    string(name),
    m_str_value(value),
    m_numValue(0)
{
}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class copy constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam(const char* name,  unsigned long value) :
    string(name),
    m_numValue(value)
{
}

/*******************************************************************************
**
** Function:    GetStrValue
**
** Description: API function for getting a string value of a setting
**
** Returns:     none
**
*******************************************************************************/
extern "C" int GetStrValue(const char* name, char* pValue, unsigned long l)
{
    std::string key = name;
    std::unordered_map<std::string,void *>::const_iterator result = confMap.find(key);
    if(result != confMap.end()){
        memcpy(pValue, result->second, l);
        return true;
    }

    size_t len = l;
    CNfcConfig& rConfig = CNfcConfig::GetInstance();

    bool b = rConfig.getValue(name, pValue, len);
    //printf("%s: NCI Config Parameter : %s=%s\n", __FUNCTION__, name, pValue);

   return b ? len : 0;
}

/*******************************************************************************
**
** Function:    GetNumValue
**
** Description: API function for getting a numerical value of a setting
**
** Returns:     none
**
*******************************************************************************/
extern "C" int GetNumValue(const char* name, void* pValue, unsigned long len)
{
    if (!pValue)
        return false;

    std::string key = name;
    std::unordered_map<std::string,void *>::const_iterator result = confMap.find(key);
    if(result != confMap.end()){
        memcpy(pValue, result->second, len);
        return true;
    }

    CNfcConfig& rConfig = CNfcConfig::GetInstance();
    const CNfcParam* pParam = rConfig.find(name);

    if (pParam == NULL)
        return false;
    unsigned long v = pParam->numValue();
    if (v == 0 && pParam->str_len() > 0 && pParam->str_len() < 4)
    {
        const unsigned char* p = (const unsigned char*)pParam->str_value();
        for (size_t i = 0 ; i < pParam->str_len(); ++i)
        {
            v *= 256;
            v += *p++;
        }
    }

    if(strcmp(name,"NXP_FWD_FUNCTIONALITY_ENABLE") &&
           strcmp(name,"HOST_LISTEN_ENABLE"))
    {
//      printf("%s: NCI Config Parameter :%s = (0x%x)\n", __FUNCTION__, name, v);
    }

    switch (len)
    {
    case sizeof(unsigned long):
        *(static_cast<unsigned long*>(pValue)) = (unsigned long)v;
        break;
    case sizeof(unsigned short):
        *(static_cast<unsigned short*>(pValue)) = (unsigned short)v;
        break;
    case sizeof(unsigned char):
        *(static_cast<unsigned char*> (pValue)) = (unsigned char)v;
        break;
    default:
        return false;
    }
    return true;
}

/*******************************************************************************
**
** Function:    SetValue
**
** Description: API function for setting the value of a setting
**
** Returns:     none
**
*******************************************************************************/
extern "C" int SetValue(const char* name, void* p_value, unsigned long len){

    void* destValue = malloc(len);
    memcpy(destValue, p_value, len);
    std::string key = name;

    std::unordered_map<std::string,void *>::const_iterator result = confMap.find(key);
    if(result != confMap.end()){
        free(result->second);
    }
    confMap[key] = destValue;
    return 0;
}

/*******************************************************************************
**
** Function:    resetConfig
**
** Description: reset settings array
**
** Returns:     none
**
*******************************************************************************/
extern void resetConfig()
{
    CNfcConfig& rConfig = CNfcConfig::GetInstance();

    rConfig.clean();
}

/*******************************************************************************
**
** Function:    readOptionalConfig()
**
** Description: read Config settings from an optional conf file
**
** Returns:     none
**
*******************************************************************************/
void readOptionalConfig(const char* extra)
{
    string strPath;
    strPath.assign(transport_config_path);
    strPath += extra_config_base;
    strPath += extra;
    strPath += extra_config_ext;
    CNfcConfig::GetInstance().readConfig(strPath.c_str(), false);
}

