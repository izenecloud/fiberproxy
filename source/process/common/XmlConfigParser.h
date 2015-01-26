/**
 * @file XmlConfigParser.h
 * @brief Defines FibpConfig class, which is a XML configuration file parser for SF-1 v5.0
 * @author MyungHyun (Kent)
 * @date 2008-09-05
 */

#ifndef _XML_CONFIG_PARSER_H_
#define _XML_CONFIG_PARSER_H_

#include <common/ByteSizeParser.h>
#include <configuration-manager/BrokerAgentConfig.h>
#include <configuration-manager/LogServerConnectionConfig.h>
#include <configuration-manager/DistributedUtilConfig.h>
#include <configuration-manager/DistributedTopologyConfig.h>

#include <util/singleton.h>
#include <util/ticpp/ticpp.h>
#include <net/aggregator/AggregatorConfig.h>

#include <boost/unordered_set.hpp>
#include <boost/utility.hpp>
#include <boost/filesystem.hpp>

#include <string>
#include <sstream>
#include <map>

namespace fibp
{
namespace ticpp = izenelib::util::ticpp;


// ------------------------- HELPER FUNCTIONS --------------------------

/// @brief  Converts the given string to lower-case letters (only for ascii)
void downCase(std::string & str);

///
/// @brief The method finds out if the string is true(y, yes) or false(n, no), or neither
/// @return  -1:false, 0:neither,  1:true
///
int parseTruth(const string & str);

/// @brief  Parses a given string based on commas ','
void parseByComma(const std::string & str, std::vector<std::string> & subStrList);

///@ brief  The exception class
class XmlConfigParserException : public std::exception
{
public:
    XmlConfigParserException(const std::string & details)
    : details_(details)
    {}

    ~XmlConfigParserException() throw()
    {}

    /// Override std::exception::what() to return details_
    const char* what() const throw()
    {
        return details_.c_str();
    }

    std::string details_;
};

class XmlConfigParser
{
protected:
    //---------------------------- HELPER FUNCTIONS -------------------------------
    /// @brief  Gets a single child element. There should be no multiple definitions of the element
    /// @param ele The parent element
    /// @param name The name of the Child element
    /// @param throwIfNoElement If set to "true", the method will throw exception if there is
    ///             no Child Element
    inline ticpp::Element * getUniqChildElement(
            const ticpp::Element * ele, const std::string & name, bool throwIfNoElement = true) const;

    /// @brief The internal method for getAttribute_* methods. Checks if a value exists and retrieves in
    ///             std::string form if it exists. User can decide if the attribute is essential with the
    ///             attribute throwIfNoAttribute
    /// @param ele The element that holds the attribute
    /// @param name The name of the attribute
    /// @param val The return container of the attribute
    /// @param torhowIfNoAttribute  Throws exception if attribute does not exist.
    /// @return Returns true  if the attribute is found and has a value.
    //               false if the attribute is not found or has no value.
    bool getAttribute(
            const ticpp::Element * ele,
            const std::string & name,
            std::string & val,
            bool throwIfNoAttribute = true) const;

    /// @brief  Gets a float type attribute. User can decide if the attribute is essential
    /// with the attribute throwIfNoAttribute
    /// @param ele The element that holds the attribute
    /// @param name The name of the attribute
    /// @param val The return container of the attribute
    /// @param torhowIfNoAttribute  Throws exception if attribute does not exist.
    /// @return Returns true  if the attribute is found and has a value.
    //               false if the attribute is not found or has no value.
    inline bool getAttribute_FloatType(
            const ticpp::Element * ele,
            const std::string & name,
            float & val,
            bool throwIfNoAttribute = true) const
    {
        std::string temp;

        if (!getAttribute(ele, name, temp, throwIfNoAttribute))
            return false;

        val = boost::lexical_cast<float>(temp);
        return true;
    }

    /// @brief  Gets a integer type attribute. User can decide if the attribute is essential
    /// with the attribute throwIfNoAttribute
    /// @param ele The element that holds the attribute
    /// @param name The name of the attribute
    /// @param val The return container of the attribute
    /// @param torhowIfNoAttribute  Throws exception if attribute does not exist.
    /// @return Returns true  if the attribute is found and has a value.
    //               false if the attribute is not found or has no value.
    template <class Type>
    inline bool getAttribute_IntType(
            const ticpp::Element * ele,
            const std::string & name,
            Type & val,
            bool throwIfNoAttribute = true) const
    {
        std::string temp;

        if (!getAttribute(ele, name, temp, throwIfNoAttribute))
            return false;

        val = boost::lexical_cast<int64_t>(temp);
        return true;
    }

    /// @brief      Overloaded function for getting "int" attributes
    inline bool getAttribute(
            const ticpp::Element * ele,
            const std::string & name,
            int32_t & val,
            bool throwIfNoAttribute = true) const
    {
        return getAttribute_IntType(ele, name, val, throwIfNoAttribute);
    }

    /// @brief      Overloaded function for getting "int64_t" attributes
    inline bool getAttribute(
            const ticpp::Element * ele,
            const std::string & name,
            int64_t & val,
            bool throwIfNoAttribute = true) const
    {
        return getAttribute_IntType(ele, name, val, throwIfNoAttribute);
    }

    /// @brief      Overloaded function for getting "int" attributes
    inline bool getAttribute(
            const ticpp::Element * ele,
            const std::string & name,
            uint32_t & val,
            bool throwIfNoAttribute = true) const
    {
        return getAttribute_IntType(ele, name, val, throwIfNoAttribute);
    }

    /// @brief      Overloaded function for getting "int64_t" attributes
    inline bool getAttribute(
            const ticpp::Element * ele,
            const std::string & name,
            uint64_t & val,
            bool throwIfNoAttribute = true) const
    {
        return getAttribute_IntType(ele, name, val, throwIfNoAttribute);
    }

    /// @brief  Gets a bool type attribute. User can decide if the attribute is essential
    ///         with the attribute throwIfNoAttribute.
    /// @details This version will always throw an exception is the given value is neither of
    ///          "yes/y/no/n" (case insesitive)
    /// @param ele                  The element that holds the attribute
    /// @param name                 The name of the attribute
    /// @param val                  The return container of the attribute
    /// @param torhowIfNoAttribute  Throws exception if attribute does not exist.
    /// @return     Returns true  if the attribute is found and has a value.
    //                      false if the attribute is not found or has no value.
    inline bool getAttribute(
            const ticpp::Element * ele,
            const std::string & name,
            bool & val,
            bool throwIfNoAttribute = true) const;

    /// @brief  Gets a integer type attribute for byte size. User can decide if
    ///         the attribute is essential with the attribute throwIfNoAttribute.
    /// @param ele The element that holds the attribute
    /// @param name The name of the attribute.
    /// @param val The return container of the attribute, when the string value
    ///            is "1m", the @p val would be 1048576.
    /// @param torhowIfNoAttribute  Throws exception if attribute does not exist.
    /// @return Returns true if the attribute is found and has a value.
    //          false if the attribute is not found or has no value.
    template <class Type>
    inline bool getAttribute_ByteSize(
            const ticpp::Element * ele,
            const std::string & name,
            Type & val,
            bool throwIfNoAttribute = true) const
    {
        std::string temp;

        if (!getAttribute(ele, name, temp, throwIfNoAttribute))
            return false;

        val = ByteSizeParser::get()->parse<Type>(temp);

        return true;
    }

    // ----------------------------- THROW METHODS -----------------------------

    // 1. ELEMENTS ---------------

    /// @brief  Throws an exception when an element does not exist
    /// @param  name  The name of the element
    inline void throw_MultipleElement(const std::string & name) const
    {
        std::stringstream msg;
        msg << "Multiple definitions of <" << name << "> element";
        throw XmlConfigParserException(msg.str());
    }

    /// @brief  Throws an exception when an element does not exist
    /// @param  name  The name of the element
    inline void throw_NoElement(const std::string & name) const
    {
        std::stringstream msg;
        msg << "Definitions of element <" << name << "> is missing";
        throw XmlConfigParserException(msg.str());
    }

    // 2. ATTRIBUTES ---------------

    // TODO: suggest type, e.g. "yes|y|no|n", "integer type"
    /// @brief          Throws an exception when an attribute is given the wrong data type
    /// @param ele      The Element which holds the attribute
    /// @param name The name of the attribute
    /// @param valuStr The value parsed for the attribute, which was incorrect
    inline void throw_TypeMismatch(
            const ticpp::Element * ele,
            const std::string & name,
            const std::string & valueStr = "") const
    {
        stringstream msg;
        msg << "<" << ele->Value() << ">, wrong data type is given for attribute \"" << name << "\"";
        if (!valueStr.empty())
            msg << " value: " << valueStr;
        throw XmlConfigParserException(msg.str());
    }

    /// @brief Throws an exception when an attribute is given the wrong data type
    /// @param ele The Element which holds the attribute
    /// @param name The name of the attribute
    /// @param valuStr The value parsed for the attribute, which was incorrect
    /// @param validValuStr  The value(s) which are valid for the attribute
    inline void throw_TypeMismatch(
            const ticpp::Element * ele,
            const std::string & name,
            const std::string & valueStr,
            const std::string & validValueStr) const
    {
        stringstream msg;
        msg << "<" << ele->Value() << ">, wrong data type is given for attribute \"" << name << "\"";
        if (!valueStr.empty())
            msg << " value: " << valueStr;
        if (!validValueStr.empty())
            msg << " suggestion : " << validValueStr;
        throw XmlConfigParserException(msg.str());
    }

    /// @brief          Throws an exception when an attribute is given the wrong data type
    /// @param ele      The Element which holds the attribute
    /// @param name     The name of the attribute
    /// @param valuLong  The value parsed for the attribute, which was incorrect
    /// @param validValuStr  The value(s) which are valid for the attribute
    inline void throw_TypeMismatch(
            const ticpp::Element * ele,
            const std::string & name,
            const long valueLong,
            const std::string & validValueStr) const
    {
        stringstream msg;
        msg << "<" << ele->Value() << ">, wrong data type is given for attribute \"" << name << "\"";
        msg << " value: " << valueLong;
        if (!validValueStr.empty())
            msg << " suggestion : " << validValueStr;
        throw XmlConfigParserException(msg.str());
    }

    /// @brief Throws an exception when an attribute does not exist
    /// @param ele The Element which holds the attribute
    /// @param name The name of the attribute
    inline void throw_NoAttribute(const ticpp::Element * ele, const std::string & name) const
    {
        stringstream msg;
        msg << "<" << ele->Value() << ">, requires attribute \"" << name << "\"";
        throw XmlConfigParserException(msg.str());
    }

    izenelib::util::UString::EncodingType parseEncodingType(const std::string& encoding_str);

    /// @brief Return true if given id only consists of alphabets, numbers, dash(-) and underscore(_)
    /// @param id The string to be checked
    /// @return true if given id consists of alaphabets, numbers, dash(-) and underscore(_)
    inline bool validateID(const string & id) const
    {
        const char *chars = id.c_str();
        for (unsigned int i = 0; i < id.size(); i++)
        {
            if (!isalnum(chars[i]) && chars[i] != '-' && chars[i] != '_' && chars[i] != '.')
                return false;
        }

        return true;
    }

};

/// @brief   This class parses a SF-1 v5.0 configuration file, in the form of a xml file
///
class FibpConfig : boost::noncopyable, XmlConfigParser
{
public:
    FibpConfig();
    ~FibpConfig();

    static FibpConfig* get()
    {
        return izenelib::util::Singleton<FibpConfig>::get();
    }

    /// @brief Starts parsing the configruation file
    /// @param fileName  The path of the configuration file
    /// @details
    /// The configuration file <System>, <Environment>, and"<Document> are processed
    ///
    bool parseConfigFile(const std::string & fileName) throw(XmlConfigParserException);

    const std::string& getResourceDir() const
    {
        return resource_dir_;
    }

    const std::string& getWorkingDir() const
    {
        return working_dir_;
    }

    /// @brief Gets the configuration related to BrokerAgent
    /// @return The settings for BrokerAgent
    const BrokerAgentConfig & getBrokerAgentConfig()
    {
        return brokerAgentConfig_;
    }

    /// @brief Gets the configuration related to BrokerAgent
    /// @param brokerAgentConfig    The settings for BrokerAgent
    void getBrokerAgentConfig(BrokerAgentConfig& brokerAgentConfig)
    {
        brokerAgentConfig = brokerAgentConfig_;
    }

    const LogServerConnectionConfig& getLogServerConfig()
    {
        return logServerConnectionConfig_;
    }

    void setHomeDirectory(const std::string& homeDir)
    {
        homeDir_ = homeDir;
    }

    const std::string& getHomeDirectory() const
    {
        return homeDir_;
    }

private:
    /// @brief                  Parse <System> settings
    /// @param system           Pointer to the Element
    void parseSystemSettings(const ticpp::Element * system);
    /// @brief                  Parse <Deploy> settings
    /// @param system           Pointer to the Element
    void parseDeploymentSettings(const ticpp::Element * deploy);
    /// @brief                  Parse <BrokerAgnet> settings
    /// @param system           Pointer to the Element
    void parseBrokerAgent(const ticpp::Element * brokerAgent);
    /// @brief                  Parse <Broker> settings
    /// @param system           Pointer to the Element
    void parseDistributedUtil(const ticpp::Element * distributedUtil);
    void parseDistributedCommon(const ticpp::Element * distributedCommon);

public:
    //----------------------------  PRIVATE MEMBER VARIABLES  ----------------------------
    // STATIC VALUES -----------------

    /// @brief  Rank value representing "light" setting
    static constexpr float  RANK_LIGHT  = 0.5f;
    /// @brief  Rank value representing "normal" setting
    static constexpr float  RANK_NORMAL = 1.0f;
    /// @brief  Rank value representing "heavy" setting
    static constexpr float  RANK_HEAVY  = 2.0f;
    /// @brief  Rank value representing "max" setting
    static constexpr float  RANK_MAX    = 4.0f;

    /// @brief  Max length for <Date> field
    static constexpr int DATE_MAXLEN = 1024;

    // CONFIGURATION ITEMS ---------------

    std::string resource_dir_;

    std::string working_dir_;

    /// @brief Log server network address
    LogServerConnectionConfig logServerConnectionConfig_;

    /// @brief  Configurations for BrokerAgent
    BrokerAgentConfig brokerAgentConfig_;

    /// @brief Configurations for distributed topologies
    DistributedCommonConfig distributedCommonConfig_;

    /// @brief Configurations for distributed util
    DistributedUtilConfig distributedUtilConfig_;

    /// @bried home of configuration files
    std::string homeDir_;

};

} 

#endif //_XML_CONFIG_PARSER_H_
