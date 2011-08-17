#include "omnetconfiguration.h"
#include <assert.h>
#include <algorithm>
#include <sstream>
#include <stdio.h>

#include "common/opp_ctype.h"
#include "cconfigreader.h"
#include "common/patternmatcher.h"
#include "envir/valueiterator.h"
#include "cexception.h"
#include "envir/scenario.h"
#include "cconfigoption.h"
#include "common/stringtokenizer.h"
#include "timeutil.h"
#include "platmisc.h"
//#include "envir/indexedfileoutvector


using namespace std;

#define VARPOS_PREFIX std::string("&")

//----------------------------------------------------------

Register_GlobalConfigOptionU(CFGID_TOTAL_STACK, "total-stack", "B", NULL, "Specifies the maximum memory for activity() simple module stacks. You need to increase this value if you get a ``Cannot allocate coroutine stack'' error.");
Register_GlobalConfigOption(CFGID_PARALLEL_SIMULATION, "parallel-simulation", CFG_BOOL, "false", "Enables parallel distributed simulation.");
Register_GlobalConfigOption(CFGID_SCHEDULER_CLASS, "scheduler-class", CFG_STRING, "cSequentialScheduler", "Part of the Envir plugin mechanism: selects the scheduler class. This plugin interface allows for implementing real-time, hardware-in-the-loop, distributed and distributed parallel simulation. The class has to implement the cScheduler interface.");
Register_GlobalConfigOption(CFGID_PARSIM_COMMUNICATIONS_CLASS, "parsim-communications-class", CFG_STRING, "cFileCommunications", "If parallel-simulation=true, it selects the class that implements communication between partitions. The class must implement the cParsimCommunications interface.");
Register_GlobalConfigOption(CFGID_PARSIM_SYNCHRONIZATION_CLASS, "parsim-synchronization-class", CFG_STRING, "cNullMessageProtocol", "If parallel-simulation=true, it selects the parallel simulation algorithm. The class must implement the cParsimSynchronizer interface.");
Register_GlobalConfigOption(CFGID_OUTPUTVECTORMANAGER_CLASS, "outputvectormanager-class", CFG_STRING, "cIndexedFileOutputVectorManager", "Part of the Envir plugin mechanism: selects the output vector manager class to be used to record data from output vectors. The class has to implement the cOutputVectorManager interface.");
Register_GlobalConfigOption(CFGID_OUTPUTSCALARMANAGER_CLASS, "outputscalarmanager-class", CFG_STRING, "cFileOutputScalarManager", "Part of the Envir plugin mechanism: selects the output scalar manager class to be used to record data passed to recordScalar(). The class has to implement the cOutputScalarManager interface.");
Register_GlobalConfigOption(CFGID_SNAPSHOTMANAGER_CLASS, "snapshotmanager-class", CFG_STRING, "cFileSnapshotManager", "Part of the Envir plugin mechanism: selects the class to handle streams to which snapshot() writes its output. The class has to implement the cSnapshotManager interface.");
Register_GlobalConfigOption(CFGID_FNAME_APPEND_HOST, "fname-append-host", CFG_BOOL, NULL, "Turning it on will cause the host name and process Id to be appended to the names of output files (e.g. omnetpp.vec, omnetpp.sca). This is especially useful with distributed simulation. The default value is true if parallel simulation is enabled, false otherwise.");
Register_GlobalConfigOption(CFGID_DEBUG_ON_ERRORS, "debug-on-errors", CFG_BOOL, "false", "When set to true, runtime errors will cause the simulation program to break into the C++ debugger (if the simulation is running under one, or just-in-time debugging is activated). Once in the debugger, you can view the stack trace or examine variables.");
Register_GlobalConfigOption(CFGID_PRINT_UNDISPOSED, "print-undisposed", CFG_BOOL, "true", "Whether to report objects left (that is, not deallocated by simple module destructors) after network cleanup.");
Register_GlobalConfigOption(CFGID_SIMTIME_SCALE, "simtime-scale", CFG_INT, "-12", "Sets the scale exponent, and thus the resolution of time for the 64-bit fixed-point simulation time representation. Accepted values are -18..0; for example, -6 selects microsecond resolution. -12 means picosecond resolution, with a maximum simtime of ~110 days.");
Register_GlobalConfigOption(CFGID_NED_PATH, "ned-path", CFG_PATH, "", "A semicolon-separated list of directories. The directories will be regarded as roots of the NED package hierarchy, and all NED files will be loaded from their subdirectory trees. This option is normally left empty, as the OMNeT++ IDE sets the NED path automatically, and for simulations started outside the IDE it is more convenient to specify it via a command-line option or the NEDPATH environment variable.");

Register_PerRunConfigOption(CFGID_NETWORK, "network", CFG_STRING, NULL, "The name of the network to be simulated.  The package name can be omitted if the ini file is in the same directory as the NED file that contains the network.");
Register_PerRunConfigOption(CFGID_WARNINGS, "warnings", CFG_BOOL, "true", "Enables warnings.");
Register_PerRunConfigOptionU(CFGID_SIM_TIME_LIMIT, "sim-time-limit", "s", NULL, "Stops the simulation when simulation time reaches the given limit. The default is no limit.");
Register_PerRunConfigOptionU(CFGID_CPU_TIME_LIMIT, "cpu-time-limit", "s", NULL, "Stops the simulation when CPU usage has reached the given limit. The default is no limit.");
Register_PerRunConfigOptionU(CFGID_WARMUP_PERIOD, "warmup-period", "s", NULL, "Length of the initial warm-up period. When set, results belonging to the first x seconds of the simulation will not be recorded into output vectors, and will not be counted into output scalars (see option **.result-recording-modes). This option is useful for steady-state simulations. The default is 0s (no warmup period). Note that models that compute and record scalar results manually (via recordScalar()) will not automatically obey this setting.");
Register_PerRunConfigOption(CFGID_FINGERPRINT, "fingerprint", CFG_STRING, NULL, "The expected fingerprint of the simulation. When provided, a fingerprint will be calculated from the simulation event times and other quantities during simulation, and checked against the given one. Fingerprints are suitable for crude regression tests. As fingerprints occasionally differ across platforms, more than one fingerprint values can be specified here, separated by spaces, and a match with any of them will be accepted. To calculate the initial fingerprint, enter any dummy string (such as \"none\"), and run the simulation.");
Register_PerRunConfigOption(CFGID_NUM_RNGS, "num-rngs", CFG_INT, "1", "The number of random number generators.");
Register_PerRunConfigOption(CFGID_RNG_CLASS, "rng-class", CFG_STRING, "cMersenneTwister", "The random number generator class to be used. It can be `cMersenneTwister', `cLCG32', `cAkaroaRNG', or you can use your own RNG class (it must be subclassed from cRNG).");
Register_PerRunConfigOption(CFGID_SEED_SET, "seed-set", CFG_INT, "${runnumber}", "Selects the kth set of automatic random number seeds for the simulation. Meaningful values include ${repetition} which is the repeat loop counter (see repeat= key), and ${runnumber}.");
Register_PerRunConfigOption(CFGID_RESULT_DIR, "result-dir", CFG_STRING, "results", "Value for the ${resultdir} variable, which is used as the default directory for result files (output vector file, output scalar file, eventlog file, etc.)");
Register_PerRunConfigOption(CFGID_RECORD_EVENTLOG, "record-eventlog", CFG_BOOL, "false", "Enables recording an eventlog file, which can be later visualized on a sequence chart. See eventlog-file= option too.");
Register_PerRunConfigOption(CFGID_DEBUG_STATISTICS_RECORDING, "debug-statistics-recording", CFG_BOOL, "false", "Turns on the printing of debugging information related to statistics recording (@statistic properties)");
Register_PerObjectConfigOption(CFGID_PARTITION_ID, "partition-id", CFG_STRING, NULL, "With parallel simulation: in which partition the module should be instantiated. Specify numeric partition ID, or a comma-separated list of partition IDs for compound modules that span across multiple partitions. Ranges (\"5..9\") and \"*\" (=all) are accepted too.");
Register_PerObjectConfigOption(CFGID_RNG_K, "rng-%", CFG_INT, "", "Maps a module-local RNG to one of the global RNGs. Example: **.gen.rng-1=3 maps the local RNG 1 of modules matching `**.gen' to the global RNG 3. The default is one-to-one mapping.");
Register_PerObjectConfigOption(CFGID_RESULT_RECORDING_MODES, "result-recording-modes", CFG_STRING, "default", "Defines how to calculate results from the @statistic property matched by the wildcard. Special values: default, all: they select the modes listed in the record= key of @statistic; all selects all of them, default selects the non-optional ones (i.e. excludes the ones that end in a question mark). Example values: vector, count, last, sum, mean, min, max, timeavg, stats, histogram. More than one values are accepted, separated by commas. Expressions are allowed. Items prefixed with '-' get removed from the list. Example: **.queueLength.result-recording-modes=default,-vector,+timeavg");

//------------------------------------------------------------------

Register_GlobalConfigOption(CFGID_SECTIONBASEDCONFIG_CONFIGREADER_CLASS, "sectionbasedconfig-configreader-class", CFG_STRING, "", "When configuration-class=SectionBasedConfiguration: selects the configuration reader C++ class, which must subclass from cConfigurationReader.");
Register_PerRunConfigOption(CFGID_DESCRIPTION, "description", CFG_STRING, NULL, "Descriptive name for the given simulation configuration. Descriptions get displayed in the run selection dialog.");
Register_PerRunConfigOption(CFGID_EXTENDS, "extends", CFG_STRING, NULL, "Name of the configuration this section is based on. Entries from that section will be inherited and can be overridden. In other words, configuration lookups will fall back to the base section.");
Register_PerRunConfigOption(CFGID_CONSTRAINT, "constraint", CFG_STRING, NULL, "For scenarios. Contains an expression that iteration variables (${} syntax) must satisfy for that simulation to run. Example: $i < $j+1.");
Register_PerRunConfigOption(CFGID_REPEAT, "repeat", CFG_INT, "1", "For scenarios. Specifies how many replications should be done with the same parameters (iteration variables). This is typically used to perform multiple runs with different random number seeds. The loop variable is available as ${repetition}. See also: seed-set= key.");
Register_PerRunConfigOption(CFGID_EXPERIMENT_LABEL, "experiment-label", CFG_STRING, "${configname}", "Identifies the simulation experiment (which consists of several, potentially repeated measurements). This string gets recorded into result files, and may be referred to during result analysis.");
Register_PerRunConfigOption(CFGID_MEASUREMENT_LABEL, "measurement-label", CFG_STRING, "${iterationvars}", "Identifies the measurement within the experiment. This string gets recorded into result files, and may be referred to during result analysis.");
Register_PerRunConfigOption(CFGID_REPLICATION_LABEL, "replication-label", CFG_STRING, "#${repetition}", "Identifies one replication of a measurement (see repeat= and measurement-label= as well). This string gets recorded into result files, and may be referred to during result analysis.");
Register_PerRunConfigOption(CFGID_RUNNUMBER_WIDTH, "runnumber-width", CFG_INT, "0", "Setting a nonzero value will cause the $runnumber variable to get padded with leading zeroes to the given length.");

//extern cConfigOption *CFGID_NETWORK;
//extern cConfigOption *CFGID_RESULT_DIR;
//extern cConfigOption *CFGID_SEED_SET;


// table to be kept consistent with scave/fields.cc
static struct ConfigVarDescription { const char *name, *description; } configVarDescriptions[] = {
    { CFGVAR_RUNID,            "A reasonably globally unique identifier for the run, produced by concatenating the configuration name, run number, date/time, etc." },
    { CFGVAR_INIFILE,          "Name of the (primary) inifile" },
    { CFGVAR_CONFIGNAME,       "Name of the active configuration" },
    { CFGVAR_RUNNUMBER,        "Sequence number of the current run within all runs in the active configuration" },
    { CFGVAR_NETWORK,          "Value of the \"network\" configuration option" },
    { CFGVAR_EXPERIMENT,       "Value of the \"experiment-label\" configuration option" },
    { CFGVAR_MEASUREMENT,      "Value of the \"measurement-label\" configuration option" },
    { CFGVAR_REPLICATION,      "Value of the \"replication-label\" configuration option" },
    { CFGVAR_PROCESSID,        "PID of the simulation process" },
    { CFGVAR_DATETIME,         "Date and time the simulation run was started" },
    { CFGVAR_RESULTDIR,        "Value of the \"result-dir\" configuration option" },
    { CFGVAR_REPETITION,       "The iteration number in 0..N-1, where N is the value of the \"repeat\" configuration option" },
    { CFGVAR_SEEDSET,          "Value of the \"seed-set\" configuration option" },
    { CFGVAR_ITERATIONVARS,    "Concatenation of all user-defined iteration variables in name=value form" },
    { CFGVAR_ITERATIONVARS2,   "Concatenation of all user-defined iteration variables in name=value form, plus ${repetition}" },
    { NULL,                    NULL }
};

std::string OmnetConfiguration::KeyValue1::nullbasedir;

OmnetConfiguration::OmnetConfiguration() {
  printf("configuring...\n");

  ini = NULL;
  activeRunNumber = 0;
}


static std::string opp_makedatetimestring()
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timestr[32];
    sprintf(timestr, "%04d%02d%02d-%02d:%02d:%02d",
            1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    return timestr;
}
static int findInArray(const char *s, const char **array)
{
    for (int i=0; array[i]!=NULL; i++)
        if (!strcmp(s, array[i]))
            return i;
    return -1;
}
static const char *partAfterLastDot(const char *s)
{
    const char *lastDotPos = strrchr(s, '.');
    return lastDotPos==NULL ? NULL : lastDotPos+1;
}

void OmnetConfiguration::clear()
{
    // note: this gets called between activateConfig() calls, so "ini" must NOT be NULL'ed out here
    activeConfig = "";
    activeRunNumber = 0;
    config.clear();
    suffixGroups.clear();
    wildcardSuffixGroup.entries.clear();
    variables.clear();
}

int OmnetConfiguration::internalFindSection(const char *section) const
{
    // not very efficient (linear search), but we only invoke it a few times during activateConfig()
    for (int i=0; i<ini->getNumSections(); i++)
        if (strcmp(section, ini->getSectionName(i))==0)
            return i;
    return -1;
}

int OmnetConfiguration::internalGetSectionId(const char *section) const
{
    int sectionId = internalFindSection(section);
    if (sectionId == -1)
        throw cRuntimeError("no such section: %s", section);
    return sectionId;
}

int OmnetConfiguration::internalFindEntry(int sectionId, const char *key) const
{
    // not very efficient (linear search), but we only invoke from activateConfig(),
    // and only once per section
    for (int i=0; i<ini->getNumEntries(sectionId); i++)
        if (strcmp(key, ini->getEntry(sectionId, i).getKey())==0)
            return i;
    return -1;
}

const char *OmnetConfiguration::internalGetValue(const std::vector<int>& sectionChain, const char *key, const char *fallbackValue) const
{
    for (int i=0; i<(int)commandLineOptions.size(); i++)
        if (strcmp(key, commandLineOptions[i].getKey())==0)
            return commandLineOptions[i].getValue();

    for (int i=0; i<(int)sectionChain.size(); i++)
    {
        int sectionId = sectionChain[i];
        int entryId = internalFindEntry(sectionId, key);
        if (entryId != -1)
            return ini->getEntry(sectionId, entryId).getValue();
    }
    return fallbackValue;
}

int OmnetConfiguration::resolveConfigName(const char *configName) const
{
    if (!configName || !configName[0])
        throw cRuntimeError("Empty config name specified");
    int id = -1;
    if (!strcmp(configName, "General"))
        id = internalFindSection("General");
    if (id == -1)
        id = internalFindSection((std::string("Config ")+configName).c_str());
    return id;
}

std::vector<int> OmnetConfiguration::resolveSectionChain(int sectionId) const
{
    return resolveSectionChain(ini->getSectionName(sectionId));
}

std::vector<int> OmnetConfiguration::resolveSectionChain(const char *sectionName) const
{
    // determine the list of sections, from this one following the "extends" chain up to [General]
    std::vector<int> sectionChain;
    int generalSectionId = internalFindSection("General");
    int sectionId = internalGetSectionId(sectionName);\
    while (true)
    {
      if (std::find(sectionChain.begin(), sectionChain.end(), sectionId) != sectionChain.end())
            throw cRuntimeError("Cycle detected in section fallback chain at: [%s]", ini->getSectionName(sectionId));
        sectionChain.push_back(sectionId);
        int entryId = internalFindEntry(sectionId, CFGID_EXTENDS->getName());
        std::string extends = entryId==-1 ? "" : ini->getEntry(sectionId, entryId).getValue();
        if (extends.empty() && generalSectionId!=-1 && sectionId!=generalSectionId)
            extends = "General";
        if (extends.empty())
            break;
        sectionId = resolveConfigName(extends.c_str());
        if (sectionId == -1)
            break; // wrong config name
    }

    return sectionChain;
}


void OmnetConfiguration::addEntry(const KeyValue1& entry)
{
    const std::string& key = entry.key;
    const char *lastDot = strrchr(key.c_str(), '.');
    if (!lastDot && !PatternMatcher::containsWildcards(key.c_str()))
    {
        // config: add if not already in there
        if (config.find(key)==config.end())
            config[key] = entry;
    }
    else
    {
        // key contains wildcard or dot: parameter or per-object configuration
        // (example: "**", "**.param", "**.partition-id")
        // Note: since the last part of they key might contain wildcards, it is not really possible
        // to distinguish the two. Cf "vector-recording", "vector-*" and "vector*"

        // analyze key and create appropriate entry
        std::string ownerName;
        std::string suffix;
        splitKey(key.c_str(), ownerName, suffix);
        bool suffixContainsWildcards = PatternMatcher::containsWildcards(suffix.c_str());

        KeyValue2 entry2(entry);
        if (!ownerName.empty())
            entry2.ownerPattern = new PatternMatcher(ownerName.c_str(), true, true, true);
         else
            entry2.fullPathPattern = new PatternMatcher(key.c_str(), true, true, true);
        entry2.suffixPattern = suffixContainsWildcards ? new PatternMatcher(suffix.c_str(), true, true, true) : NULL;

        // find which group it should go into
        if (!suffixContainsWildcards)
        {
            // no wildcard in suffix (=group name)
            if (suffixGroups.find(suffix)==suffixGroups.end()) {
                // suffix group not yet exists, create it
                SuffixGroup& group = suffixGroups[suffix];

                // initialize group with matching wildcard keys seen so far
                for (int k=0; k<(int)wildcardSuffixGroup.entries.size(); k++)
                    if (wildcardSuffixGroup.entries[k].suffixPattern->matches(suffix.c_str()))
                        group.entries.push_back(wildcardSuffixGroup.entries[k]);
            }
            suffixGroups[suffix].entries.push_back(entry2);
        }
        else
        {
            // suffix contains wildcards: we need to add it to all existing suffix groups it matches
            // Note: if suffix also contains a hyphen, that's actually illegal (per-object
            // config entry names cannot be wildcarded, ie. "foo.bar.cmdenv-*" is illegal),
            // but causes no harm, because getPerObjectConfigEntry() won't look into the
            // wildcard group
            wildcardSuffixGroup.entries.push_back(entry2);
            for (std::map<std::string,SuffixGroup>::iterator it = suffixGroups.begin(); it!=suffixGroups.end(); it++)
                if (entry2.suffixPattern->matches(it->first.c_str()))
                    (it->second).entries.push_back(entry2);
        }
    }
}


void OmnetConfiguration::splitKey(const char *key, std::string& outOwnerName, std::string& outGroupName)
{
    std::string tmp = key;

    const char *lastDotPos = strrchr(key, '.');
    const char *doubleAsterisk = !lastDotPos ? NULL : strstr(lastDotPos, "**");

    if (!lastDotPos || doubleAsterisk)
    {
        // complicated special case: there's a "**" after the last dot
        // (or there's no dot at all). Examples: "**baz", "net.**.foo**",
        // "net.**.foo**bar**baz"
        // Problem with this: are "foo" and "bar" part of the paramname (=group)
        // or the module name (=owner)? Can be either way. Only feasible solution
        // is to force matching of the full path (modulepath.paramname) against
        // the full pattern. Group name can be "*" plus segment of the pattern
        // after the last "**". (For example, for "net.**foo**bar", the group name
        // will be "*bar".)

        // find last "**"
        while (doubleAsterisk && strstr(doubleAsterisk+1, "**"))
            doubleAsterisk = strstr(doubleAsterisk+1, "**");
        outOwnerName = ""; // empty owner means "do fullPath match"
        outGroupName = !doubleAsterisk ? "*" : doubleAsterisk+1;
    }
    else
    {
        // normal case: group is the part after the last dot
        outOwnerName.assign(key, lastDotPos - key);
        outGroupName.assign(lastDotPos+1);
    }
}


bool OmnetConfiguration::entryMatches(const KeyValue2& entry, const char *moduleFullPath, const char *paramName)
{
    if (!entry.fullPathPattern)
    {
        // typical
        return entry.ownerPattern->matches(moduleFullPath) && (entry.suffixPattern==NULL || entry.suffixPattern->matches(paramName));
    }
    else
    {
        // less efficient, but very rare
        std::string paramFullPath = std::string(moduleFullPath) + "." + paramName;
        return entry.fullPathPattern->matches(paramFullPath.c_str());
    }
}


std::vector<OmnetConfiguration::IterationVariable> OmnetConfiguration::collectIterationVariables(const std::vector<int>& sectionChain) const
{ 
    std::vector<IterationVariable> v;
    int unnamedCount = 0;
    for (int i=0; i<(int)sectionChain.size(); i++)
    {
        int sectionId = sectionChain[i];
        for (int entryId=0; entryId<ini->getNumEntries(sectionId); entryId++)
        {
            const cConfigurationReader::KeyValue& entry = ini->getEntry(sectionId, entryId);
            const char *pos = entry.getValue();
            int k = 0;
            while ((pos = strstr(pos, "${")) != NULL)
            {
                IterationVariable loc;
                try {
                    parseVariable(pos, loc.varname, loc.value, loc.parvar, pos);
                } catch (std::exception& e) {
                    throw cRuntimeError("Scenario generator: %s at %s=%s", e.what(), entry.getKey(), entry.getValue());
                }

                if (!loc.value.empty() && loc.parvar.empty())
                {
                    // store variable
                    if (!loc.varname.empty())
                    {
                        // check it does not conflict with other iteration variables or predefined variables
                        for (int j=0; j<(int)v.size(); j++)
                            if (v[j].varname==loc.varname)
                                throw cRuntimeError("Scenario generator: redefinition of iteration variable ${%s} in the configuration", loc.varname.c_str());
                        if (isPredefinedVariable(loc.varname.c_str()))
                            throw cRuntimeError("Scenario generator: ${%s} is a predefined variable and cannot be changed", loc.varname.c_str());
                        // use name for id
                        loc.varid = loc.varname;
                    }
                    else
                    {
                        // unnamed variable: generate id (identifies location) and name ($0,$1,$2,etc)
                        loc.varid = opp_stringf("%d-%d-%d", sectionId, entryId, k);
                        loc.varname = opp_stringf("%d", unnamedCount++);
                    }
                    v.push_back(loc);
                }
                k++;
            }
        }
    }

    // register ${repetition}, based on the repeat= config entry
    const char *repeat = internalGetValue(sectionChain, CFGID_REPEAT->getName());
    int repeatCount = (int) parseLong(repeat, NULL, 1);
    IterationVariable repetition;
    repetition.varid = repetition.varname = CFGVAR_REPETITION;
    repetition.value = opp_stringf("0..%d", repeatCount-1);
    v.push_back(repetition);

    return v;

}


void OmnetConfiguration::parseVariable(const char *pos, std::string& outVarname, std::string& outValue, std::string& outParvar, const char *&outEndPos)
{ 
    Assert(pos[0]=='$' && pos[1]=='{'); // this is the way we've got to be invoked
    outEndPos = strchr(pos, '}');
    if (!outEndPos)
        throw cRuntimeError("missing '}' for '${'");

    // parse what's inside the ${...}
    const char *varbegin = NULL;
    const char *varend = NULL;
    const char *valuebegin = NULL;
    const char *valueend = NULL;
    const char *parvarbegin = NULL;
    const char *parvarend = NULL;

    const char *s = pos+2;
    while (opp_isspace(*s)) s++;
    if (opp_isalpha(*s))
    {
        // must be a variable or a variable reference
        varbegin = varend = s;
        while (opp_isalnum(*varend)) varend++;
        s = varend;
        while (opp_isspace(*s)) s++;
        if (*s=='}') {
            // ${x} syntax -- OK
        }
        else if (*s=='=' && *(s+1)!='=') {
            // ${x=...} syntax -- OK
            valuebegin = s+1;
        }
        else if (strchr(s,',')) {
            // part of a valuelist -- OK
            valuebegin = varbegin;
            varbegin = varend = NULL;
        }
        else {
            throw cRuntimeError("missing '=' after '${varname'");
        }
    } else {
        valuebegin = s;
    }
    valueend = outEndPos;

    if (valuebegin)
    {
        // try to parse parvar, present when value ends in "! variable"
        const char *exclamationMark = strrchr(valuebegin, '!');
        if (exclamationMark)
        {
            const char *s = exclamationMark+1;
            while (opp_isspace(*s)) s++;
            if (opp_isalpha(*s))
            {
                parvarbegin = s;
                while (opp_isalnum(*s)) s++;
                parvarend = s;
                while (opp_isspace(*s)) s++;
                if (s!=valueend)
                {
                    parvarbegin = parvarend = NULL; // no parvar after all
                }
            }
            if (parvarbegin)  {
                valueend = exclamationMark;  // chop off "!parvarname"
            }
        }
    }

    if (varbegin && parvarbegin)
        throw cRuntimeError("the ${var=...} and ${...!var} syntaxes cannot be used together");

    outVarname = outValue = outParvar = "";
    if (varbegin)
        outVarname.assign(varbegin, varend-varbegin);
    if (valuebegin)
        outValue.assign(valuebegin, valueend-valuebegin);
    if (parvarbegin)
        outParvar.assign(parvarbegin, parvarend-parvarbegin);
    //printf("DBG: var=`%s', value=`%s', parvar=`%s'\n", outVarname.c_str(), outValue.c_str(), outParvar.c_str());
}

std::string OmnetConfiguration::substituteVariables(const char *text, int sectionId, int entryId) const
{
    std::string result = opp_nulltoempty(text);
    int k = 0;  // counts "${" occurrences
    const char *pos, *endPos;
    while ((pos = strstr(result.c_str(), "${")) != NULL)
    {
        std::string varname, iterationstring, parvar;
        parseVariable(pos, varname, iterationstring, parvar, endPos);
        std::string value;
        if (parvar.empty())
        {
            // handle named and unnamed iteration variable references
            std::string varid = !varname.empty() ? varname : opp_stringf("%d-%d-%d", sectionId, entryId, k);
            StringMap::const_iterator it = variables.find(varid.c_str());
            if (it==variables.end())
                throw cRuntimeError("no such variable: ${%s}", varid.c_str());
            value = it->second;
        }
        else
        {
            // handle parallel iterations: if parvar is at its kth value,
            // we should take the kth value from iterationstring as well
            StringMap::const_iterator it = variables.find(VARPOS_PREFIX+parvar);
            if (it==variables.end())
                throw cRuntimeError("no such variable: ${%s}", parvar.c_str());
            int parvarPos = atoi(it->second.c_str());
            ValueIterator v(iterationstring.c_str());
            if (parvarPos >= v.length())
                throw cRuntimeError("parallel iterator ${...!%s} does not have enough values", parvar.c_str());
            value = v.get(parvarPos);
        }
        result.replace(pos-result.c_str(), endPos-pos+1, value);
        k++;
    }
    return result;
}


bool OmnetConfiguration::isPredefinedVariable(const char *varname) const
{
    for (int i=0; configVarDescriptions[i].name; i++)
        if (strcmp(varname, configVarDescriptions[i].name)==0)
            return true;
    return false;
}

void OmnetConfiguration::setupVariables(const char *configName, int runNumber, Scenario *scenario, const std::vector<int>& sectionChain)
{ 
    // create variables
    int runnumberWidth = std::max(0,atoi(opp_nulltoempty(internalGetValue(sectionChain, CFGID_RUNNUMBER_WIDTH->getName()))));
    variables[CFGVAR_INIFILE] = opp_nulltoempty(getFileName());
    variables[CFGVAR_CONFIGNAME] = configName;
    
    variables[CFGVAR_RUNNUMBER] = opp_stringf("%0*d", runnumberWidth, runNumber);
    
    variables[CFGVAR_NETWORK] = opp_nulltoempty(internalGetValue(sectionChain, CFGID_NETWORK->getName()));
    variables[CFGVAR_PROCESSID] = opp_stringf("%d", (int) getpid());
    
    variables[CFGVAR_DATETIME] = opp_makedatetimestring();
    variables[CFGVAR_RESULTDIR] = opp_nulltoempty(internalGetValue(sectionChain, CFGID_RESULT_DIR->getName(), CFGID_RESULT_DIR->getDefaultValue()));
    variables[CFGVAR_RUNID] = runId = variables[CFGVAR_CONFIGNAME]+"-"+variables[CFGVAR_RUNNUMBER]+"-"+variables[CFGVAR_DATETIME]+"-"+variables[CFGVAR_PROCESSID];
    
    // store iteration variables, and also their "positions" (iteration count) as "&varid"
    const std::vector<IterationVariable>& itervars = scenario->getIterationVariables();
    for (int i=0; i<(int)itervars.size(); i++)
    {
        const char *varid = itervars[i].varid.c_str();
        variables[varid] = scenario->getVariable(varid);
        variables[VARPOS_PREFIX+varid] = opp_stringf("%d", scenario->getIteratorPosition(varid));
    }

    // assemble ${iterationvars}
    std::string iterationvars, iterationvars2;
    for (int i=0; i<(int)itervars.size(); i++)
    {
        std::string txt = "$" + itervars[i].varname + "=" + scenario->getVariable(itervars[i].varid.c_str());
        if (itervars[i].varname != CFGVAR_REPETITION)
            iterationvars += std::string(i>0?", ":"") + txt;
        iterationvars2 += std::string(i>0?", ":"") + txt;
    }
    variables[CFGVAR_ITERATIONVARS] = iterationvars;
    variables[CFGVAR_ITERATIONVARS2] = iterationvars2;

    // experiment/measurement/replication must be done as last, because they may depend on the above vars
    variables[CFGVAR_SEEDSET] = substituteVariables(internalGetValue(sectionChain, CFGID_SEED_SET->getName(), CFGID_SEED_SET->getDefaultValue()), -1, -1);
    variables[CFGVAR_EXPERIMENT] = substituteVariables(internalGetValue(sectionChain, CFGID_EXPERIMENT_LABEL->getName(), CFGID_EXPERIMENT_LABEL->getDefaultValue()), -1, -1);
    variables[CFGVAR_MEASUREMENT] = substituteVariables(internalGetValue(sectionChain, CFGID_MEASUREMENT_LABEL->getName(), CFGID_MEASUREMENT_LABEL->getDefaultValue()), -1, -1);
    variables[CFGVAR_REPLICATION] = substituteVariables(internalGetValue(sectionChain, CFGID_REPLICATION_LABEL->getName(), CFGID_REPLICATION_LABEL->getDefaultValue()), -1, -1);
}


OmnetConfiguration::KeyValue1 OmnetConfiguration::convert(int sectionId, int entryId)
{
    const cConfigurationReader::KeyValue& e = ini->getEntry(sectionId, entryId);
    std::string value = substituteVariables(e.getValue(), sectionId, entryId);

    StringSet::iterator it = basedirs.find(e.getBaseDirectory());
    if (it == basedirs.end()) {
        basedirs.insert(e.getBaseDirectory());
        it = basedirs.find(e.getBaseDirectory());
    }
    const std::string *basedirRef = &(*it);
    return KeyValue1(basedirRef, e.getKey(), value.c_str());
}


bool OmnetConfiguration::isIgnorableConfigKey(const char *ignoredKeyPatterns, const char *key)
{
    // see if any element in ignoredKeyPatterns matches it
    StringTokenizer tokenizer(ignoredKeyPatterns ? ignoredKeyPatterns : "");
    while (tokenizer.hasMoreTokens())
        if (PatternMatcher(tokenizer.nextToken(), true, true, true).matches(key))
            return true;
    return false;
}


cConfigOption *OmnetConfiguration::lookupConfigOption(const char *key)
{
    cConfigOption *e = (cConfigOption *) configOptions.getInstance()->lookup(key);
    if (e)
        return e;  // found it, great

    // Maybe it matches on a cConfigOption which has '*' or '%' in its name,
    // such as "seed-1-mt" matches on the "seed-%-mt" cConfigOption.
    // We have to iterate over all cConfigOptions to verify this.
    // "%" means "any number" in config keys.
    int n = configOptions.getInstance()->size();
    for (int i=0; i<n; i++)
    {
        cConfigOption *e = (cConfigOption *) configOptions.getInstance()->get(i);
        if (PatternMatcher::containsWildcards(e->getName()) || strchr(e->getName(),'%')!=NULL)
        {
            std::string pattern = opp_replacesubstring(e->getName(), "%", "{..}", true);
            if (PatternMatcher(pattern.c_str(), false, true, true).matches(key))
                return e;
        }
    }
    return NULL;
}

void OmnetConfiguration::activateConfig(const char *configName, int runNumber)
{ 
    clear();

    activeConfig = configName==NULL ? "" : configName;
    activeRunNumber = runNumber;

    int sectionId = resolveConfigName(configName);
    if (sectionId == -1 && !strcmp(configName, "General"))
        return;  // allow activating "General" even if it's empty
    if (sectionId == -1)
        throw cRuntimeError("No such config: %s", configName);

    // determine the list of sections, from this one up to [General]
    std::vector<int> sectionChain = resolveSectionChain(sectionId);

    // extract all iteration vars from values within this section
    std::vector<IterationVariable> itervars = collectIterationVariables(sectionChain);

    // see if there's a constraint given
    const char *constraint = internalGetValue(sectionChain, CFGID_CONSTRAINT->getName(), NULL);

    // determine the values to substitute into the iteration vars (${...})
    try
    {
        Scenario scenario(itervars, constraint);
        int numRuns = scenario.getNumRuns();
        if (runNumber<0 || runNumber>=numRuns)
            throw cRuntimeError("Run number %d is out of range for configuration `%s': it contains %d run(s)", runNumber, configName, numRuns);

        scenario.gotoRun(runNumber);
        setupVariables(getActiveConfigName(), getActiveRunNumber(), &scenario, sectionChain);
    }
    catch (std::exception& e)
    {
        throw cRuntimeError("Scenario generator: %s", e.what());
    }

    // walk the list of fallback sections, and add entries to our tables
    // (config[] and params[]). Meanwhile, substitute the iteration values.
    // Note: entries added first will have precedence over those added later.
    for (int i=0; i < (int)commandLineOptions.size(); i++)
    {
        addEntry(commandLineOptions[i]);
    }
    for (int i=0; i < (int)sectionChain.size(); i++)
    {
        int sectionId = sectionChain[i];
        for (int entryId=0; entryId<ini->getNumEntries(sectionId); entryId++)
        {
            // add entry to our tables
            addEntry(convert(sectionId, entryId));
        }
    }
}

void OmnetConfiguration::setConfigurationReader(cConfigurationReader *ini)
{
    clear();
    this->ini = ini;
    nullEntry.setBaseDirectory(ini->getDefaultBaseDirectory());
}


void OmnetConfiguration::setCommandLineConfigOptions(const std::map<std::string,std::string>& options)
{
    commandLineOptions.clear();
    for (StringMap::const_iterator it=options.begin(); it!=options.end(); it++)
    {
        // validate the key, then store the option
        //XXX we should better use the code in the validate() method...
        const char *key = it->first.c_str();
        const char *value = it->second.c_str();
        const char *option = strchr(key,'.') ? strrchr(key,'.')+1 : key; // check only the part after the last dot, i.e. recognize per-object keys as well
        cConfigOption *e = lookupConfigOption(option);
        if (!e)
            throw cRuntimeError("Unknown command-line configuration option --%s", key);
        if (!e->isPerObject() && key!=option)
            throw cRuntimeError("Wrong command-line configuration option --%s: %s is not a per-object option", key, e->getName());
        std::string tmp;
        if (e->isPerObject() && key==option)
            key = (tmp=std::string("**.")+key).c_str(); // prepend with "**." (XXX this should be done in inifile contents too)
        if (!value[0])
            throw cRuntimeError("Missing value for command-line configuration option --%s", key);
        commandLineOptions.push_back(KeyValue1(NULL, key, value));
    }
}


void OmnetConfiguration::dump() const
{
    printf("Config:\n");
    for (std::map<std::string,KeyValue1>::const_iterator it = config.begin(); it!=config.end(); it++)
        printf("  %s = %s\n", it->first.c_str(), it->second.value.c_str());

    for (std::map<std::string,SuffixGroup>::const_iterator it = suffixGroups.begin(); it!=suffixGroups.end(); it++)
    {
        const std::string& suffix = it->first;
        const SuffixGroup& group = it->second;
        printf("Suffix Group %s:\n", suffix.c_str());
        for (int i=0; i<(int)group.entries.size(); i++)
            printf("  %s = %s\n", group.entries[i].key.c_str(), group.entries[i].value.c_str());
    }
    printf("Wildcard Suffix Group:\n");
    for (int i=0; i<(int)wildcardSuffixGroup.entries.size(); i++)
        printf("  %s = %s\n", wildcardSuffixGroup.entries[i].key.c_str(), wildcardSuffixGroup.entries[i].value.c_str());
}


void OmnetConfiguration::initializeFrom(cConfiguration *bootConfig)
{
    std::string classname = bootConfig->getAsString(CFGID_SECTIONBASEDCONFIG_CONFIGREADER_CLASS);
    if (classname.empty())
        throw cRuntimeError("OmnetConfiguration: no configuration reader class specified (missing %s option)",
                             CFGID_SECTIONBASEDCONFIG_CONFIGREADER_CLASS->getName());
    cConfigurationReader *reader = dynamic_cast<cConfigurationReader *>(createOne(classname.c_str()));
    if (!reader)
        throw cRuntimeError("Class \"%s\" is not subclassed from cConfigurationReader", classname.c_str());
    reader->initializeFrom(bootConfig);
    setConfigurationReader(reader);
}

const char *OmnetConfiguration::getFileName() const
{
    return ini==NULL ? NULL : ini->getFileName();
}


void OmnetConfiguration::validate(const char *ignorableConfigKeys) const
{
    const char *obsoleteSections[] = {
        "Parameters", "Cmdenv", "Tkenv", "OutVectors", "Partitioning", "DisplayStrings", NULL
    };
    const char *cmdenvNames[] = {
        "autoflush", "event-banner-details", "event-banners", "express-mode",
        "message-trace", "module-messages", "output-file", "performance-display",
        "runs-to-execute", "status-frequency", NULL
    };
    const char *tkenvNames[] = {
        "anim-methodcalls", "animation-enabled", "animation-msgclassnames",
        "animation-msgcolors", "animation-msgnames", "animation-speed",
        "default-run", "expressmode-autoupdate", "image-path", "methodcalls-delay",
        "next-event-markers", "penguin-mode", "plugin-path", "print-banners",
        "senddirect-arrows", "show-bubbles", "show-layouting", "slowexec-delay",
        "update-freq-express", "update-freq-fast", "use-mainwindow",
        "use-new-layouter", NULL
    };

    // warn for obsolete section names and config keys
    for (int i=0; i<ini->getNumSections(); i++)
    {
        const char *section = ini->getSectionName(i);
        if (findInArray(section, obsoleteSections) != -1)
            throw cRuntimeError("Obsolete section name [%s] found, please convert the ini file to 4.x format", section);

        int numEntries = ini->getNumEntries(i);
        for (int j=0; j<numEntries; j++)
        {
            const char *key = ini->getEntry(i, j).getKey();
            if (findInArray(key, cmdenvNames) != -1 || findInArray(key, tkenvNames) != -1)
                throw cRuntimeError("Obsolete configuration key %s= found, please convert the ini file to 4.x format", key);
        }
    }

    // check section names
    std::set<std::string> configNames;
    for (int i=0; i<ini->getNumSections(); i++)
    {
        const char *section = ini->getSectionName(i);
        const char *configName = NULL;
        if (strcmp(section, "General")==0)
            ; // OK
        else if (strncmp(section, "Config ", 7)==0)
            configName  = section+7;
        else
            throw cRuntimeError("Invalid section name [%s], should be [General] or [Config <name>]", section);
        if (configName)
        {
            if (*configName == ' ')
                throw cRuntimeError("Invalid section name [%s]: too many spaces", section);
            if (!opp_isalpha(*configName) && *configName!='_')
                throw cRuntimeError("Invalid section name [%s]: config name must begin with a letter or underscore", section);
            for (const char *s=configName; *s; s++)
                if (!opp_isalnum(*s) && strchr("-_@", *s)==NULL)
                    throw cRuntimeError("Invalid section name [%s], contains illegal character '%c'", section, *s);
            if (configNames.find(configName)!=configNames.end())
                throw cRuntimeError("Configuration name '%s' not unique", configName, section);
            configNames.insert(configName);
        }

    }

    // check keys
    for (int i=0; i<ini->getNumSections(); i++)
    {
        const char *section = ini->getSectionName(i);
        int numEntries = ini->getNumEntries(i);
        for (int j=0; j<numEntries; j++)
        {
            const char *key = ini->getEntry(i, j).getKey();
            bool containsDot = strchr(key, '.')!=NULL;

            if (!containsDot && !PatternMatcher::containsWildcards(key))
            {
                // warn for unrecognized (or misplaced) config keys
                // NOTE: values don't need to be validated here, that will be
                // done when the config gets actually used
                cConfigOption *e = lookupConfigOption(key);
                if (!e && isIgnorableConfigKey(ignorableConfigKeys, key))
                    continue;
                if (!e)
                    throw cRuntimeError("Unknown configuration key: %s", key);
                if (e->isPerObject())
                    throw cRuntimeError("Configuration key %s should be specified per object, try **.%s=", key, key);
                if (e->isGlobal() && strcmp(section, "General")!=0)
                    throw cRuntimeError("Configuration key %s may only occur in the [General] section", key);

                // check section hierarchy
                if (strcmp(key, CFGID_EXTENDS->getName())==0)
                {
                    if (strcmp(section, "General")==0)
                        throw cRuntimeError("The [General] section cannot extend other sections");

                    // warn for invalid "extends" names
                    const char *value = ini->getEntry(i, j).getValue();
                    if (configNames.find(value)==configNames.end())
                        throw cRuntimeError("No such config: %s", value);

                    // check for section cycles
                    resolveSectionChain(section);  //XXX move that check here?
                }
            }
            else
            {
                // check for per-object configuration subkeys (".ev-enabled", ".record-interval")
                std::string ownerName;
                std::string suffix;
                splitKey(key, ownerName, suffix);
                bool containsHyphen = strchr(suffix.c_str(), '-')!=NULL;
                if (containsHyphen)
                {
                    // this is a per-object config
                    //XXX suffix (probably) should not contain wildcard; but surely not "**" !!!!
                    cConfigOption *e = lookupConfigOption(suffix.c_str());
                    if (!e && isIgnorableConfigKey(ignorableConfigKeys, suffix.c_str()))
                        continue;
                    if (!e || !e->isPerObject())
                        throw cRuntimeError("Unknown per-object configuration key `%s' in %s", suffix.c_str(), key);
                }
            }
        }
    }
}

std::vector<std::string> OmnetConfiguration::getConfigNames()
{
    std::vector<std::string> result;
    for (int i=0; i<ini->getNumSections(); i++)
    {
        const char *section = ini->getSectionName(i);
        if (strcmp(section, "General")==0)
            result.push_back(section);
        else if (strncmp(section, "Config ", 7)==0)
            result.push_back(section+7);
        else
            ; // nothing - leave out bogus section names
    }
    return result;
}

std::string OmnetConfiguration::getConfigDescription(const char *configName) const
{ 
    int sectionId = resolveConfigName(configName);
    if (sectionId == -1)
        throw cRuntimeError("No such config: %s", configName);

    // determine the list of sections, from this one up to [General]
    std::vector<int> sectionChain = resolveSectionChain(sectionId);
    return opp_nulltoempty(internalGetValue(sectionChain, CFGID_DESCRIPTION->getName()));
}


std::string OmnetConfiguration::getBaseConfig(const char *configName) const
{
    int sectionId = resolveConfigName(configName);
    if (sectionId == -1)
        throw cRuntimeError("No such config: %s", configName);
    int entryId = internalFindEntry(sectionId, CFGID_EXTENDS->getName());
    std::string extends = entryId==-1 ? "" : ini->getEntry(sectionId, entryId).getValue();
    if (extends.empty())
        extends = "General";
    int baseSectionId = resolveConfigName(extends.c_str());
    return baseSectionId==-1 ? "" : extends;
}


int OmnetConfiguration::getNumRunsInConfig(const char *configName) const
{
    int sectionId = resolveConfigName(configName);
    if (sectionId == -1)
        return 0;  // no such config

    // extract all iteration vars from values within this section
    std::vector<int> sectionChain = resolveSectionChain(sectionId);
    std::vector<IterationVariable> v = collectIterationVariables(sectionChain);

    // see if there's a constraint given
    const char *constraint = internalGetValue(sectionChain, CFGID_CONSTRAINT->getName(), NULL);

    // count the runs and return the result
    try {
        return Scenario(v, constraint).getNumRuns();
    }
    catch (std::exception& e) {
        throw cRuntimeError("Scenario generator: %s", e.what());
	}
}

std::vector<std::string> OmnetConfiguration::unrollConfig(const char *configName, bool detailed) const
{ 
    int sectionId = resolveConfigName(configName);
    if (sectionId == -1)
        throw cRuntimeError("No such config: %s", configName);

    // extract all iteration vars from values within this section
    std::vector<int> sectionChain = resolveSectionChain(sectionId);
    std::vector<IterationVariable> itervars = collectIterationVariables(sectionChain);

    // see if there's a constraint given
    const char *constraint = internalGetValue(sectionChain, CFGID_CONSTRAINT->getName(), NULL);

    // setupVariables() overwrites variables[], so we need to save/restore it
    StringMap savedVariables = variables;

    // iterate over all runs in the scenario
    try {
        Scenario scenario(itervars, constraint);
        std::vector<std::string> result;
        if (scenario.restart())
        {
            for (;;)
            {
                // generate a string for the current run
                std::string runstring;
                if (!detailed)
                {
                    runstring = scenario.str();
                }
                else
                {
                    // itervars, plus all entries that contain ${..}
                    runstring += std::string("\t# ") + scenario.str() + "\n";
                    (const_cast<OmnetConfiguration *>(this))->setupVariables(configName, result.size(), &scenario, sectionChain);
                    for (int i=0; i<(int)sectionChain.size(); i++)
                    {
                        int sectionId = sectionChain[i];
                        for (int entryId=0; entryId<ini->getNumEntries(sectionId); entryId++)
                        {
                            // add entry to our tables
                            const cConfigurationReader::KeyValue& entry = ini->getEntry(sectionId, entryId);
                            if (strstr(entry.getValue(), "${")!=NULL)
                            {
                                std::string actualValue = substituteVariables(entry.getValue(), sectionId, entryId);
                                runstring += std::string("\t") + entry.getKey() + " = " + actualValue + "\n";
                            }
                        }
                    }
                }
                result.push_back(runstring);

                // move to the next run
                if (!scenario.next())
                    break;
            }
        }
        (const_cast<OmnetConfiguration *>(this))->variables = savedVariables;
        return result;
    }
    catch (std::exception& e)
    {
        (const_cast<OmnetConfiguration *>(this))->variables = savedVariables;
        throw cRuntimeError("Scenario generator: %s", e.what());
    }
}


const char *OmnetConfiguration::getActiveConfigName() const
{
    return activeConfig.c_str();
}


int OmnetConfiguration::getActiveRunNumber() const
{
    return activeRunNumber;
}


const char *OmnetConfiguration::getConfigValue(const char *key) const
{
    std::map<std::string,KeyValue1>::const_iterator it = config.find(key);
    return it==config.end() ? NULL : it->second.value.c_str();
}


const cConfiguration::KeyValue& OmnetConfiguration::getConfigEntry(const char *key) const
{
    std::map<std::string,KeyValue1>::const_iterator it = config.find(key);
    return it==config.end() ? (KeyValue&)nullEntry : (KeyValue&)it->second;
}

std::vector<const char *> OmnetConfiguration::getMatchingConfigKeys(const char *pattern) const
{
    std::vector<const char *> result;
    PatternMatcher matcher(pattern, true, true, true);

    // iterate over the map -- this is going to be sloooow...
    for (std::map<std::string,KeyValue1>::const_iterator it = config.begin(); it != config.end(); ++it)
        if (matcher.matches(it->first.c_str()))
            result.push_back(it->first.c_str());
    return result;
}


const char *OmnetConfiguration::getParameterValue(const char *moduleFullPath, const char *paramName, bool hasDefaultValue) const
{
    const OmnetConfiguration::KeyValue2& entry = (KeyValue2&) getParameterEntry(moduleFullPath, paramName, hasDefaultValue);
    return entry.getKey()==NULL ? NULL : entry.value.c_str();
}

const cConfiguration::KeyValue& OmnetConfiguration::getParameterEntry(const char *moduleFullPath, const char *paramName, bool hasDefaultValue) const
{
    // look up which group; paramName serves as suffix (ie. group name)
    std::map<std::string,SuffixGroup>::const_iterator it = suffixGroups.find(paramName);
    const SuffixGroup *group = it==suffixGroups.end() ? &wildcardSuffixGroup : &it->second;

    // find first match in the group
    for (int i=0; i<(int)group->entries.size(); i++)
    {
        const KeyValue2& entry = group->entries[i];
        if (entryMatches(entry, moduleFullPath, paramName))
            if (hasDefaultValue || entry.value != "default")
                return entry;
    }
    return nullEntry; // not found
}


std::vector<const char *> OmnetConfiguration::getParameterKeyValuePairs() const
{
    std::vector<const char *> result;
    //FIXME TBD
    return result;
}

const char *OmnetConfiguration::getPerObjectConfigValue(const char *objectFullPath, const char *keySuffix) const
{
    const OmnetConfiguration::KeyValue2& entry = (KeyValue2&) getPerObjectConfigEntry(objectFullPath, keySuffix);
    return entry.getKey()==NULL ? NULL : entry.value.c_str();
}

const cConfiguration::KeyValue& OmnetConfiguration::getPerObjectConfigEntry(const char *objectFullPath, const char *keySuffix) const
{
    // look up which group; keySuffix serves as group name
    // Note: we do not accept wildcards in the config key's name (ie. "**.record-*" is invalid),
    // so we ignore the wildcard group.
    std::map<std::string,SuffixGroup>::const_iterator it = suffixGroups.find(keySuffix);
    if (it==suffixGroups.end())
        return nullEntry; // no such group

    const SuffixGroup *suffixGroup = &it->second;

    // find first match in the group
    for (int i=0; i<(int)suffixGroup->entries.size(); i++)
    {
        const KeyValue2& entry = suffixGroup->entries[i];
        if (entryMatches(entry, objectFullPath, keySuffix))
            return entry;  // found value
    }
    return nullEntry; // not found
}

std::vector<const char *> OmnetConfiguration::getMatchingPerObjectConfigKeys(const char *objectFullPathPattern, const char *keySuffixPattern) const
{
    std::vector<const char *> result;

    // only concrete objects or "**" is accepted, because we are not prepared
    // to handle the "pattern matches pattern" case (see below as well).
    bool anyObject = strcmp(objectFullPathPattern, "**")==0;
    if (!anyObject && PatternMatcher::containsWildcards(objectFullPathPattern))
        throw cRuntimeError("getMatchingPerObjectConfigKeys: invalid objectFullPath parameter: the only wildcard pattern accepted is '**'");

    // check all suffix groups whose name matches the pattern
    PatternMatcher suffixMatcher(keySuffixPattern, true, true, true);
    for (std::map<std::string,SuffixGroup>::const_iterator it = suffixGroups.begin(); it != suffixGroups.end(); ++it)
    {
        const char *suffix = it->first.c_str();
        if (suffixMatcher.matches(suffix))
        {
            // find all matching entries from this suffix group.
            // We'll have a little problem where key ends in wildcard (i.e. entry.suffixPattern!=NULL);
            // there we'd have to determine whether two *patterns* match. We resolve this
            // by checking whether one pattern matches the other one as string, and vica versa.
            const SuffixGroup& group = it->second;
            for (int i=0; i<(int)group.entries.size(); i++)
            {
                const KeyValue2& entry = group.entries[i];
                if ((anyObject || entry.ownerPattern->matches(objectFullPathPattern))
                    &&
                    (entry.suffixPattern==NULL ||
                     suffixMatcher.matches(partAfterLastDot(entry.key.c_str())) ||
                     entry.suffixPattern->matches(keySuffixPattern)))
                    result.push_back(entry.key.c_str());
            }
        }
    }
    return result;
}

std::vector<const char *> OmnetConfiguration::getMatchingPerObjectConfigKeySuffixes(const char *objectFullPath, const char *keySuffixPattern) const
{
    std::vector<const char *> result = getMatchingPerObjectConfigKeys(objectFullPath, keySuffixPattern);
    for (int i=0; i<(int)result.size(); i++)
        result[i] = partAfterLastDot(result[i]);
    return result;
}


const char *OmnetConfiguration::getVariable(const char *varname) const
{
    StringMap::const_iterator it = variables.find(varname);
    return it==variables.end() ? NULL : it->second.c_str();
}

std::vector<const char *> OmnetConfiguration::getIterationVariableNames() const
{
    std::vector<const char *> result;
    for (StringMap::const_iterator it = variables.begin(); it!=variables.end(); ++it)
        if (opp_isalpha(it->first[0]) && !isPredefinedVariable(it->first.c_str()))  // skip unnamed and predefined ones
            result.push_back(it->first.c_str());
    return result;
}

std::vector<const char *> OmnetConfiguration::getPredefinedVariableNames() const
{
    std::vector<const char *> result;
    for (int i=0; configVarDescriptions[i].name; i++)
        result.push_back(configVarDescriptions[i].name);
    return result;
}

const char *OmnetConfiguration::getVariableDescription(const char *varname) const
{ 
    for (int i=0; configVarDescriptions[i].name; i++)
        if (strcmp(varname, configVarDescriptions[i].name)==0)
            return configVarDescriptions[i].description;
    if (!opp_isempty(getVariable(varname)))
        return "User-defined iteration variable";
	return NULL;
}


const char *OmnetConfiguration::substituteVariables(const char *value)
{
    if (value==NULL || strstr(value, "${")==NULL)
        return value;

    // returned string needs to be stringpooled
    std::string result = substituteVariables(value, -1, -1);
    return stringPool.get(result.c_str());
}
