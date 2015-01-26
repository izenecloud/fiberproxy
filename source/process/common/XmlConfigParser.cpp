/**
 * @file XmlConfigParser.cpp
 * @brief Implements FibpConfig class, which is a XML configuration file parser for SF-1 v5.0
 * @author MyungHyun (Kent)
 * @date 2008-09-05
 */

#include "XmlConfigParser.h"
#include "XmlSchema.h"
#include <util/ustring/UString.h>
#include <net/distribute/Util.h>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/shared_ptr.hpp>

#include <glog/logging.h>

#include <iostream>
#include <sstream>

#include <boost/asio.hpp>

using namespace std;
using namespace izenelib::util::ticpp;

namespace fibp
{

//------------------------- HELPER FUNCTIONS -------------------------
void downCase(std::string & str)
{
    for (string::iterator it = str.begin(); it != str.end(); it++)
    {
        *it = tolower(*it);
    }
}

int parseTruth(const string & str)
{
    std::string temp = str;
    downCase(temp);

    if (temp == "y" || temp == "yes")
        return 1;
    else if (temp == "n" || temp == "no")
        return 0;
    else
        return -1;
}

void parseByComma(const string & str, vector<string> & subStrList)
{
    subStrList.clear();

    std::size_t startIndex=0, endIndex=0;

    while ((startIndex = str.find_first_not_of(" ,\t", startIndex)) != std::string::npos)
    {
        endIndex = str.find_first_of(" ,\t", startIndex + 1);

        if (endIndex == string::npos)
        {
            endIndex = str.length();
        }

        std::string substr = str.substr(startIndex, endIndex - startIndex);
        startIndex = endIndex + 1;

        subStrList.push_back(substr);
    }
}

// ------------------------- HELPER MEMBER FUNCTIONS of XmlConfigParser -------------------------

ticpp::Element * XmlConfigParser::getUniqChildElement(
        const ticpp::Element * ele, const std::string & name, bool throwIfNoElement) const
{
    ticpp::Element * temp = NULL;
    temp = ele->FirstChildElement(name, false);

    if (!temp)
    {
        if (throwIfNoElement)
            throw_NoElement(name);
        else
            return NULL;
    }

    if (temp->NextSibling(name, false))
    {
        throw_MultipleElement(name);
    }

    return temp;
}

inline bool XmlConfigParser::getAttribute(
        const ticpp::Element * ele,
        const std::string & name,
        std::string & val,
        bool throwIfNoAttribute) const
{
    val = ele->GetAttribute(name);

    if (val.empty())
    {
        if (throwIfNoAttribute)
            throw_NoAttribute(ele, name);
        else
            return false;
    }

    return true;
}

bool XmlConfigParser::getAttribute(
        const ticpp::Element * ele,
        const std::string & name,
        bool & val,
        bool throwIfNoAttribute) const
{
    std::string temp;

    if (!getAttribute(ele, name, temp, throwIfNoAttribute))
        return false;

    switch(parseTruth(temp))
    {
        case 1:
            val = true;
            break;
        case 0:
            val = false;
            break;
        case -1:
            throw_TypeMismatch(ele, name, temp);
            break;
    }

    return true;
}

izenelib::util::UString::EncodingType XmlConfigParser::parseEncodingType(const std::string& encoding)
{
    izenelib::util::UString::EncodingType eType = izenelib::util::UString::UTF_8;
    if (encoding == "utf-8" || encoding == "utf8")
        eType = izenelib::util::UString::UTF_8;
    else if (encoding == "euc-kr" || encoding == "euckr")
        eType = izenelib::util::UString::EUC_KR;
    else if (encoding == "cp949")
        eType = izenelib::util::UString::CP949;
    else if (encoding == "euc-jp" || encoding == "eucjp")
        eType = izenelib::util::UString::EUC_JP;
    else if (encoding == "sjis")
        eType = izenelib::util::UString::SJIS;
    else if (encoding == "gb2312")
        eType = izenelib::util::UString::GB2312;
    else if (encoding == "big5")
        eType = izenelib::util::UString::BIG5;
    else if (encoding == "iso8859-15")
        eType = izenelib::util::UString::ISO8859_15;
    return eType;
}

// ------------------------- FibpConfig-------------------------

FibpConfig::FibpConfig()
{
}

FibpConfig::~FibpConfig()
{
}

bool FibpConfig::parseConfigFile(const string & fileName) throw(XmlConfigParserException)
{
    namespace bf=boost::filesystem;

    try
    {
        if (!boost::filesystem::exists(fileName))
        {
            std::cerr << "[FibpConfig] Config File doesn't exist." << std::endl;
            return false;
        }

        /** schema validate begin */
        bf::path config_file(fileName);
        bf::path config_dir = config_file.parent_path();
        bf::path schema_file = config_dir/"schema"/"config.xsd";
        std::string schema_file_string = schema_file.string();
        std::cout<<"XML Schema File: "<<schema_file_string<<std::endl;
        if (!boost::filesystem::exists(schema_file_string))
        {
            std::cerr << "[FibpConfig] Schema File doesn't exist." << std::endl;
            return false;
        }

        XmlSchema schema(schema_file_string);
        bool schema_valid = schema.validate(fileName);
        std::list<std::string> schema_warning = schema.getSchemaValidityWarnings();
        if (schema_warning.size()>0)
        {
            std::list<std::string>::iterator it = schema_warning.begin();
            while (it!= schema_warning.end())
            {
                std::cout<<"[Schema-Waring] "<<*it<<std::endl;
                it++;
            }
        }
        if (!schema_valid)
        {
            //output schema errors
            std::list<std::string> schema_error = schema.getSchemaValidityErrors();
            if (schema_error.size()>0)
            {
                std::list<std::string>::iterator it = schema_error.begin();
                while (it!= schema_error.end())
                {
                    std::cerr<<"[Schema-Error] "<<*it<<std::endl;
                    it++;
                }
            }
            return false;
        }
        /** schema validate end */

        ticpp::Document configDocument(fileName.c_str());
        configDocument.LoadFile();

        // make sure the top level element is "FibpConfig"; if it isn't, an exception is thrown
        Element * config = NULL;
        if ((config = configDocument.FirstChildElement("FibpConfig", false)) == NULL)
        {
            throw_NoElement("FibpConfig");
        }

        parseSystemSettings(getUniqChildElement(config, "System"));
        parseDeploymentSettings(getUniqChildElement(config, "Deployment"));
    }
    catch (ticpp::Exception err)
    {
        size_t substart = err.m_details.find("\nDescription: ");
        substart = substart + strlen("\nDescription: ");

        string msg = err.m_details.substr(substart);
        size_t pos = 0;

        while ((pos = msg.find("\n", pos)) != string::npos)
        {
            msg.replace(pos, 1, " ");
            pos++;
        }

        msg.insert(0, "Exception occured while parsing file: ");

        throw XmlConfigParserException(msg);
    }

    return true;
} // END - FibpConfig::parseConfigFile()

// 1. SYSTEM SETTINGS  -------------------------------------

void FibpConfig::parseSystemSettings(const ticpp::Element * system)
{
    //get resource dir
    getAttribute(getUniqChildElement(system, "Resource"), "path", resource_dir_);

    getAttribute(getUniqChildElement(system, "WorkingDir"), "path", working_dir_);

    getAttribute(getUniqChildElement(system, "LogServerConnection"), "host", logServerConnectionConfig_.host);
    getAttribute(getUniqChildElement(system, "LogServerConnection"), "port", logServerConnectionConfig_.port);
    getAttribute(getUniqChildElement(system, "LogServerConnection"), "log_service", logServerConnectionConfig_.log_service);
    getAttribute(getUniqChildElement(system, "LogServerConnection"), "log_tag", logServerConnectionConfig_.log_tag);
}

void FibpConfig::parseDeploymentSettings(const ticpp::Element * deploy)
{
    parseBrokerAgent(getUniqChildElement(deploy, "BrokerAgent"));

    parseDistributedCommon(getUniqChildElement(deploy, "DistributedCommon"));
    parseDistributedUtil(getUniqChildElement(deploy, "DistributedUtil"));
}

void FibpConfig::parseBrokerAgent(const ticpp::Element * brokerAgent)
{
    getAttribute(brokerAgent, "enabletest", brokerAgentConfig_.enableTest_,false);
    getAttribute(brokerAgent, "threadnum", brokerAgentConfig_.threadNum_,false);
    getAttribute(brokerAgent, "port", brokerAgentConfig_.port_,false);
}

void FibpConfig::parseDistributedCommon(const ticpp::Element * distributedCommon)
{
    getAttribute(distributedCommon, "username", distributedCommonConfig_.userName_);
    getAttribute(distributedCommon, "workerport", distributedCommonConfig_.workerPort_);
    getAttribute(distributedCommon, "masterport", distributedCommonConfig_.masterPort_);
    std::string interface;
    getAttribute(distributedCommon, "localinterface", interface);

    distributedCommonConfig_.baPort_ = brokerAgentConfig_.port_;

    if (!net::distribute::Util::getLocalHostIp(distributedCommonConfig_.localHost_, interface))
    {
        getAttribute(distributedCommon, "localhost", distributedCommonConfig_.localHost_);
        std::cout << "failed to detect local host ip, set by config: " << distributedCommonConfig_.localHost_ << std::endl;
    }
    else
        std::cout << "local host ip : " << distributedCommonConfig_.localHost_ << std::endl;

}

void FibpConfig::parseDistributedUtil(const ticpp::Element * distributedUtil)
{
    // ZooKeeper configuration
    ticpp::Element* zk = getUniqChildElement(distributedUtil, "ZooKeeper");
    getAttribute(zk, "disable", distributedUtilConfig_.zkConfig_.disabled_);
    getAttribute(zk, "servers", distributedUtilConfig_.zkConfig_.zkHosts_);
    getAttribute(zk, "sessiontimeout", distributedUtilConfig_.zkConfig_.zkRecvTimeout_);

    ticpp::Element* dns = getUniqChildElement(distributedUtil, "ServiceDiscovery");
    getAttribute(dns, "servers", distributedUtilConfig_.dns_config_.servers_);
    
    // Distributed File System configuration
    ticpp::Element* dfs = getUniqChildElement(distributedUtil, "DFS");
    getAttribute(dfs, "type", distributedUtilConfig_.dfsConfig_.type_, false);
    getAttribute(dfs, "supportfuse", distributedUtilConfig_.dfsConfig_.isSupportFuse_, false);
    getAttribute(dfs, "mountdir", distributedUtilConfig_.dfsConfig_.mountDir_, false);
    getAttribute(dfs, "server", distributedUtilConfig_.dfsConfig_.server_, false);
    getAttribute(dfs, "port", distributedUtilConfig_.dfsConfig_.port_, false);
}

} // END - namespace 
