/*
    This file is part of Spike Guard.

    Spike Guard is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Spike Guard is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Spike Guard.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "yara_wrapper.h"
#include "plugin_framework/plugin_interface.h"
#include "plugin_framework/auto_register.h"

namespace plugin
{

class YaraPlugin : public IPlugin
{

public:
	YaraPlugin(const std::string& rule_file) : _rule_file(rule_file) {}

	/**
	 *	@brief	Helper function designed to generically prepare a result based on a Yara scan.
	 *
	 *	@param	const sg::PE& pe The PE to scan.
	 *	const std::string& summary The summary to set if there is a match.
	 *	Result::LEVEL level The threat level to set if there is a match.
	 *	const std::string& meta_field_name The meta field name (of the yara rule) to query to
	 *									   extract results.
	 *	bool show_strings Adds the matched strings/patterns to the result.
	 *
	 *	@return	A pResult detailing the findings of the scan.
	 */
	pResult scan(const sg::PE& pe, const std::string& summary, Result::LEVEL level, const std::string& meta_field_name, bool show_strings = false)
	{
		pResult res = pResult(new Result);
		if (!_load_rules()) {
			return res;
		}

		yara::matches m = _engine.scan_file(*pe.get_path());
		if (m.size() > 0) 
		{
			res->set_level(level);
			res->set_summary(summary);
			for (yara::matches::iterator it = m.begin() ; it != m.end() ; ++it) 
			{
				if (!show_strings) {
					res->add_information((*it)->operator[](meta_field_name));
				}
				else
				{
					res->add_information((*it)->operator[](meta_field_name) + " String(s) found:");
					std::set<std::string> found = (*it)->get_found_strings();
					for (std::set<std::string>::iterator it = found.begin() ; it != found.end() ; ++it) {
						res->add_information("\t" + *it);
					}
				}
			}
		}

		return res;
	}

	int get_api_version() { return 1; }

private:
	std::string _rule_file;
	yara::Yara _engine;
	
	bool _load_rules()
	{
		if (!_engine.load_rules(_rule_file)) 
		{
			PRINT_ERROR << "Could not load " << _rule_file << "!" << std::endl;
			return false;
		}
		return true;
	}

};

class ClamavPlugin : public YaraPlugin
{
public:
	ClamavPlugin() : YaraPlugin("yara_rules/clamav.yara") {}

	pResult analyze(const sg::PE& pe) {
		return scan(pe, "Matching ClamAV signature(s):", Result::MALICIOUS, "signature");
	}

	pString get_id() { 
		return pString (new std::string("clamav"));
	}

	pString  get_description() { 
		return pString (new std::string("Scans the binary with ClamAV virus definitions."));
	}
};

class CompilerDetectionPlugin : public YaraPlugin
{
public:
	CompilerDetectionPlugin() : YaraPlugin("yara_rules/compilers.yara") {}

	pResult analyze(const sg::PE& pe) {
		return scan(pe, "Matching compiler(s):", Result::NO_OPINION, "description");
	}

	boost::shared_ptr<std::string> get_id() { 
		return boost::shared_ptr<std::string>(new std::string("compilers"));
	}

	boost::shared_ptr<std::string> get_description() { 
		return boost::shared_ptr<std::string>(new std::string("Tries to determine which compiler generated the binary."));
	}
};

class PEiDPlugin : public YaraPlugin
{
public:
	PEiDPlugin() : YaraPlugin("yara_rules/peid.yara") {}

	pResult analyze(const sg::PE& pe) {
		return scan(pe, "PEiD Signature:", Result::SUSPICIOUS, "packer_name");
	}

	boost::shared_ptr<std::string> get_id() { 
		return boost::shared_ptr<std::string>(new std::string("peid"));
	}

	boost::shared_ptr<std::string> get_description() { 
		return boost::shared_ptr<std::string>(new std::string("Returns the PEiD signature of the binary."));
	}
};


class SuspiciousStringsPlugin : public YaraPlugin
{
public:
	SuspiciousStringsPlugin() : YaraPlugin("yara_rules/suspicious_strings.yara") {}

	pResult analyze(const sg::PE& pe) {
		return scan(pe, "Strings found in the binary may indicate undesirable behavior:", Result::SUSPICIOUS, "description", true);
	}

	boost::shared_ptr<std::string> get_id() { 
		return boost::shared_ptr<std::string>(new std::string("strings"));
	}

	boost::shared_ptr<std::string> get_description() { 
		return boost::shared_ptr<std::string>(new std::string("Looks for suspicious strings (anti-VM, process names...)."));
	}
};

AutoRegister<ClamavPlugin> auto_register_clamav;
AutoRegister<CompilerDetectionPlugin> auto_register_compiler;
AutoRegister<PEiDPlugin> auto_register_peid;
AutoRegister<SuspiciousStringsPlugin> auto_register_strings;

}