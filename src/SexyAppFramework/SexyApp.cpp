/*
 * Portions of this file are based on the PopCap Games Framework
 * Copyright (C) 2005-2009 PopCap Games, Inc.
 * 
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later AND LicenseRef-PopCap
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#include "SexyApp.h"

#include <time.h>
#include <sys/stat.h>
#include <fstream>

using namespace Sexy;

SexyApp* Sexy::gSexyApp = nullptr;

SexyApp::SexyApp()
{
	gSexyApp = this;

	mDemoPrefix = "pvzp";
	mDemoFileName = mDemoPrefix + ".dmo";	
	mCompanyName = "Community";

	mBuildNum = 0;
}

SexyApp::~SexyApp()
{
}

void SexyApp::ReadFromRegistry()
{
	SexyAppBase::ReadFromRegistry();
}

void SexyApp::WriteToRegistry()
{
	SexyAppBase::WriteToRegistry();
}

void SexyApp::HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue)
{
	if (theParamName == "-version")
	{
		// Just print version info and then quit
		
		std::string aVersionString = 
			"Product: " + mProdName + "\r\n" +
			"Version: " + mProductVersion + "\r\n" +
			"Build Num: " + StrFormat("%d", mBuildNum) + "\r\n" +
			"Build Date: " + mBuildDate;

		printf("%s\n", aVersionString.c_str());
		DoExit(0);
	}
	else
		SexyAppBase::HandleCmdLineParam(theParamName, theParamValue);
}

std::string SexyApp::GetGameSEHInfo()
{
	std::string anInfoString = SexyAppBase::GetGameSEHInfo() + 
		"Build Num: " + StrFormat("%d", mBuildNum) + "\r\n" +
		"Build Date: " + mBuildDate + "\r\n";

	return anInfoString;
}

void SexyApp::PreDisplayHook()
{
}

void SexyApp::InitPropertiesHook()
{
	// Load properties if we need to
	bool checkSig = !IsScreenSaver();
	LoadProperties("properties/partner.xml", false, checkSig);

	mProdName = GetString("ProdName", mProdName);
#if !defined(__IPHONEOS__) && (!defined(__ANDROID__) || defined(__TERMUX__)) && !defined(__SWITCH__) && !defined(__3DS__) && !defined(__EMSCRIPTEN__)
	mIsWindowed = GetBoolean("DefaultWindowed", mIsWindowed);	
#endif

	std::string aNewTitle = GetString("Title", "");
	if (aNewTitle.length() > 0)
		mTitle = aNewTitle + " " + mProductVersion;
}

void SexyApp::Init()
{
	printf("Product: %s\n", mProdName.c_str());
	printf("BuildNum: %d\n", mBuildNum);
	printf("BuildDate: %s\n", mBuildDate.c_str());

	SexyAppBase::Init();
}

void SexyApp::PreTerminate()
{
}

void SexyApp::UpdateFrames()
{
	SexyAppBase::UpdateFrames();
}
