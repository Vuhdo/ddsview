#pragma once

#define _CRT_SECURE_NO_WARNINGS			// warning C4996: 'sprintf': This function or variable may be unsafe. Consider using sprintf_s instead.

#include <windows.h>
#include <shlobj.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <algorithm>
#include <cassert>

#include "il/il.h"
#include "il/ilu.h"
#include "il/ilut.h"

#pragma warning( push )
#pragma warning( disable : 4005 )		// warning C4005: 'assert' : macro redefinition
//*sigh* DevIL's exported API is not expressive enough...
#include "IL/devil_internal_exports.h"
#pragma warning( pop )