/**
 * @file    ProcessOptions.h
 * @brief   Defines a class that handles process argument given in the form of options.
 * @author  MyungHyun Lee(Kent)
 * @date    2008-12-02
 *
 * @details
 * Currently the options are only distinguished by the output message. However the arguments are parsed for all the cases.
 * I assume that a process will not use other process' options.
 * TODO: describe the available options of each process
 */

#ifndef PROCESS_OPTIONS_H
#define PROCESS_OPTIONS_H

#include <boost/program_options.hpp>

#include <string>
#include <vector>

/**
 * @brief   Parses process' options and provides interfaces for accessing the values.
 */
class ProcessOptions
{
public:

    class Port
    {
    public:
        unsigned int port;
        Port(unsigned int p)
            : port(p)
        {
        }
    };

    class String
    {
    public:
        std::string str;
        String(const std::string& s)
            : str(s)
        {
        }
    };

    /**
     * @brief   sets up option descriptions for all processes
     */
    ProcessOptions();

    bool setProcessArgs(const std::vector<std::string>& args);

    inline unsigned int getNumberOfOptions() const
    {
        return variableMap_.size();
    }

    /**
     * @brief   Gets the location of the configuration file
     * @return  The configuration file path
     */
    inline const std::string& getConfigFileDirectory() const
    {
        return configFileDir_;
    }

    inline const std::string& getConfigFile() const
    {
        return configFile_;
    }

    inline bool isVerboseOn() const
    {
        return isVerboseOn_;
    }

    inline const std::string& getLogPrefix() const
    {
        return logPrefix_;
    }

    inline const std::string& getPidFile() const
    {
        return pidFile_;
    }

    inline const std::string& getReportAddr() const
    {
        return reportAddr_;
    }

    inline const std::string& getRegistryAddr() const
    {
        return registryAddr_;
    }

private:

    //Process all the options possible for the processes in 
    void setProcessOptions();

private:
    /// @brief  Stores the option values
    boost::program_options::variables_map variableMap_;

    /**
     * @brief   Description of MIAProcess options
     */
    boost::program_options::options_description processDescription_;

    /// @brief  The file name (path) of the configuration file
    std::string configFileDir_;

    std::string configFile_;

    ///@brief used to recognize the additional unused parameters/words
    boost::program_options::positional_options_description additional_;

    bool isVerboseOn_;

    std::string logPrefix_;

    std::string pidFile_;
    std::string reportAddr_;
    std::string registryAddr_;
};

#endif  //PROCESS_OPTIONS_H
