// Copyright (c) 2008 - Blue Onion Software, All rights reserved
//

#include "stdafx.h"
#include "windows.h"
#include "shellapi.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    ShellExecute(NULL, L"open", L"http://blueonionsoftware.com/donate.aspx?p=freesnap", NULL, NULL, SW_SHOW);
	return 0;
}

