////////////////////////////////////////////////////////////////////////////////
//
//  Private Header File:
//    license.h
//
//  Description:
//    This file contains the definitions for things relating to licensing mode.
//
//  Changes:
//    2009-07-01 Updated to use new edition/class enum values.
//               Removed obsolete licensing parameters.
//               Added externals/enable_externals licensing parameters.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __LICENSE_H
#define __LICENSE_H

enum
{
	kMCLicenseDeployToWindows = 1 << 0,
	kMCLicenseDeployToMacOSX = 1 << 1,
	kMCLicenseDeployToLinux = 1 << 2,
	kMCLicenseDeployToIOS = 1 << 3,
	kMCLicenseDeployToAndroid = 1 << 4,
	kMCLicenseDeployToWinMobile = 1 << 5,
	kMCLicenseDeployToLinuxMobile = 1 << 6,
	kMCLicenseDeployToServer = 1 << 7,
	kMCLicenseDeployToIOSEmbedded = 1 << 8,
	kMCLicenseDeployToAndroidEmbedded = 1 << 9,
    kMCLicenseDeployToHTML5 = 1 << 10,
    kMCLicenseDeployToFileMaker = 1 << 11,
};

enum MCLicenseClass
{
	kMCLicenseClassNone,
	kMCLicenseClassCommunity,
//    kMCLicenseClassCommunityPlus,
//    kMCLicenseClassEvaluation,
//	kMCLicenseClassCommercial,
//	kMCLicenseClassProfessionalEvaluation,
//	kMCLicenseClassProfessional,
};

struct MCLicenseParameters
{
    MCStringRef license_token;
    MCStringRef license_name;
    MCStringRef license_organization;
	MCLicenseClass license_class;
	uint4 license_multiplicity;

	uint4 script_limit;
	uint4 do_limit;
	uint4 using_limit;
	uint4 insert_limit;
	
	uint32_t deploy_targets;

	MCArrayRef addons;
};

extern MCLicenseParameters MClicenseparameters;
extern Boolean MCenvironmentactive;

void MCLicenseSetRevLicenseLimits(MCExecContext& ctxt, MCArrayRef p_settings);
void MCLicenseGetRevLicenseLimits(MCExecContext& ctxt, MCArrayRef& r_limits);
static const struct { MCLicenseClass license_class; const char *class_string; const char *edition_string; } s_class_map[] =
{
    { kMCLicenseClassCommunity, "community", "community" },
//    { kMCLicenseClassCommunityPlus, "communityplus", "communityplus" },
//    { kMCLicenseClassEvaluation, "evaluation", "indy evaluation" },
//    { kMCLicenseClassCommercial, "commercial", "indy" },
//    { kMCLicenseClassProfessionalEvaluation, "professional evaluation", "business evaluation" },
//    { kMCLicenseClassProfessional, "professional", "business" },
    { kMCLicenseClassNone, "", "" }
};

inline bool MCStringToLicenseClass(MCStringRef p_class, MCLicenseClass &r_class)
{
	r_class = kMCLicenseClassCommunity;
	return true;
//    for(uindex_t t_index = 0; t_index < sizeof(s_class_map) / sizeof(s_class_map[0]); ++t_index)
//    {
//        if (MCStringIsEqualToCString(p_class, s_class_map[t_index].class_string, kMCCompareCaseless))
//        {
//            r_class = s_class_map[t_index].license_class;
//            return true;
//        }
//    }
    
//    return false;
}

inline bool MCEditionStringFromLicenseClass(MCLicenseClass p_class, MCStringRef &r_edition)
{
//    if (p_class == kMCLicenseClassEvaluation)
//    {
//        p_class = kMCLicenseClassCommercial;
//    }
//    else if (p_class == kMCLicenseClassProfessionalEvaluation)
//    {
//        p_class = kMCLicenseClassProfessional;
//    }
//    else if (p_class == kMCLicenseClassNone)
//    {
		p_class = kMCLicenseClassCommunity;
//	}

	return (MCStringCreateWithCString("community", r_edition));

//    for(uindex_t t_index = 0; t_index < sizeof(s_class_map) / sizeof(s_class_map[0]); ++t_index)
//    {
//        if (s_class_map[t_index].license_class == p_class)
//        {
//            return MCStringCreateWithCString(s_class_map[t_index].edition_string, r_edition);
//        }
//    }
    
//    return false;
}

//inline bool MCEditionStringToLicenseClass(MCStringRef p_edition, MCLicenseClass &r_class)
//{
//    for(uindex_t t_index = 0; t_index < sizeof(s_class_map) / sizeof(s_class_map[0]); ++t_index)
//    {
//        if (MCStringIsEqualToCString(p_edition, s_class_map[t_index].edition_string, kMCCompareCaseless))
//        {
//            r_class = s_class_map[t_index].license_class;
//            return true;
//        }
//    }
//    
//    return false;
//}

#endif
