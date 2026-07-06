/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"

#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "objectstream.h"
#include "mcio.h"
#include "license.h"

#include "exec.h"

Boolean MCenvironmentactive = False;

void MCLicenseSetRevLicenseLimits(MCExecContext& ctxt, MCArrayRef p_settings)
{
    if(!MCenvironmentactive)
        return;
    
    bool t_case_sensitive = ctxt . GetCaseSensitive();
    MCValueRef t_value;
    MCStringRef t_string;
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("token"), t_value)
            && ctxt . ConvertToString(t_value, t_string))
    {
        MCValueRelease(MClicenseparameters . license_token);
        MClicenseparameters . license_token = t_string;
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("name"), t_value)
            && ctxt . ConvertToString(t_value, t_string))
    {
        MCValueRelease(MClicenseparameters . license_name);
        MClicenseparameters . license_name = t_string;
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("organization"), t_value)
            && ctxt . ConvertToString(t_value, t_string))
    {
        MCValueRelease( MClicenseparameters . license_organization);
         MClicenseparameters . license_organization = t_string;
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("class"), t_value))
    {
        MCAutoStringRef t_class;
        MCLicenseClass t_license_class;
        if (ctxt . ConvertToString(t_value, &t_class) && MCStringToLicenseClass(*t_class, t_license_class))
        {
            MClicenseparameters . license_class = t_license_class;
        }
        else
            MClicenseparameters . license_class = kMCLicenseClassNone;
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("multiplicity"), t_value))
    {
	    MCAutoNumberRef t_number;
	    if (ctxt.ConvertToNumber(t_value, &t_number))
	    {
		    MClicenseparameters . license_multiplicity = MCNumberFetchAsUnsignedInteger(*t_number);
	    }
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("scriptlimit"), t_value))
    {
	    MCAutoNumberRef t_number;
	    if (ctxt.ConvertToNumber(t_value, &t_number))
	    {
		    integer_t t_limit;
		    t_limit = MCNumberFetchAsInteger(*t_number);
		    MClicenseparameters . script_limit = t_limit <= 0 ? 0 : t_limit;
	    }
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("dolimit"), t_value))
    {
	    MCAutoNumberRef t_number;
	    if (ctxt.ConvertToNumber(t_value, &t_number))
	    {
		    integer_t t_limit;
		    t_limit = MCNumberFetchAsInteger(*t_number);
		    MClicenseparameters . do_limit = t_limit <= 0 ? 0 : t_limit;
	    }
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("usinglimit"), t_value))
    {
	    MCAutoNumberRef t_number;
	    if (ctxt.ConvertToNumber(t_value, &t_number))
	    {
		    integer_t t_limit;
		    t_limit = MCNumberFetchAsInteger(*t_number);
		    MClicenseparameters . using_limit = t_limit <= 0 ? 0 : t_limit;
	    }
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("insertlimit"), t_value))
    {
	    MCAutoNumberRef t_number;
	    if (ctxt.ConvertToNumber(t_value, &t_number))
	    {
		    integer_t t_limit;
		    t_limit = MCNumberFetchAsInteger(*t_number);
		    MClicenseparameters . insert_limit = t_limit <= 0 ? 0 : t_limit;
	    }
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("deploy"), t_value))
    {
        static struct { const char *tag; uint32_t value; } s_deploy_map[] =
        {
            { "windows", kMCLicenseDeployToWindows },
            { "macosx", kMCLicenseDeployToMacOSX },
            { "linux", kMCLicenseDeployToLinux },
            { "ios", kMCLicenseDeployToIOS },
            { "android", kMCLicenseDeployToAndroid },
            { "winmobile", kMCLicenseDeployToWinMobile },
            { "meego", kMCLicenseDeployToLinuxMobile },
            { "server", kMCLicenseDeployToServer },
            { "ios-embedded", kMCLicenseDeployToIOSEmbedded },
            { "android-embedded", kMCLicenseDeployToIOSEmbedded },
            { "html5", kMCLicenseDeployToHTML5 },
            { "filemaker", kMCLicenseDeployToFileMaker },
        };
        
        MClicenseparameters . deploy_targets = 0;
        
        MCAutoStringRef t_params;
        if (ctxt . ConvertToString(t_value, &t_params))
        {
            MCAutoArrayRef t_split_strings;
            MCValueRef t_fetched_string;
            if (MCStringSplit(*t_params, MCSTR(","), nil, kMCCompareExact, &t_split_strings))
            {
                for(uint32_t i = 0; i < MCArrayGetCount(*t_split_strings); i++)
                {
                    // Fetch the string value created with MCStringSplit
                    MCArrayFetchValueAtIndex(*t_split_strings, i+1, t_fetched_string);
                    
                    for(uint32_t j = 0; j < sizeof(s_deploy_map) / sizeof(s_deploy_map[0]); j++)
                        if (MCStringIsEqualToCString((MCStringRef)t_fetched_string, s_deploy_map[j] . tag, kMCStringOptionCompareCaseless))
                        {
                            MClicenseparameters . deploy_targets |= s_deploy_map[j] . value;
                            break;
                        }
                }
            }
        }
    }
    
    if (MCArrayFetchValue(p_settings, t_case_sensitive, MCNAME("addons"), t_value) && MCValueIsArray(t_value))
    {
        MCValueRelease(MClicenseparameters . addons);
        MCArrayCopy((MCArrayRef)t_value, MClicenseparameters . addons);
    }
}

void MCLicenseGetRevLicenseLimits(MCExecContext& ctxt, MCArrayRef& r_limits)
{
    r_limits = MCValueRetain(kMCEmptyArray);
}


