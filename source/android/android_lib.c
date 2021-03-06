/*
Copyright (C) 2014 SiPlus, Chasseur de bots

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <dlfcn.h>
#include "android_sys.h"
#include "../qcommon/sys_library.h"

/*
* Sys_Library_Close
*/
bool Sys_Library_Close( void *lib )
{
	return dlclose( lib ) ? false : true;
}

/*
* Sys_Library_GetFullName
*/
const char *Sys_Library_GetFullName( const char *name )
{
	static char tempname[PATH_MAX];
	Q_snprintfz( tempname, sizeof( tempname ), "/data/data/%s/lib/%s", sys_android_packageName, COM_FileBase( name ) );
	return tempname;
}

/*
* Sys_Library_GetGameLibPath
*/
const char *Sys_Library_GetGameLibPath( const char *name, int64_t time, int randomizer )
{
	// no randomizer because only one instance can run at once
	static char tempname[PATH_MAX];
	Q_snprintfz( tempname, sizeof( tempname ), "/data/data/%s/cache/%d.%d/%s/tempmodules"
#ifdef DEDICATED_ONLY
		"_server"
#endif
		"/%s", sys_android_packageName, APP_VERSION_MAJOR, APP_VERSION_MINOR, FS_GameDirectory(), name );
	return tempname;
}

/*
* Sys_Library_Open
*/
void *Sys_Library_Open( const char *name )
{
	return dlopen( name, RTLD_NOW );
}

/*
* Sys_Library_ProcAddress
*/
void *Sys_Library_ProcAddress( void *lib, const char *apifuncname )
{
	return dlsym( lib, apifuncname );
}

/*
* Sys_Library_ErrorString
*/
const char *Sys_Library_ErrorString( void )
{
	return dlerror();
}

#ifdef SYS_SYMBOL
/*
* Sys_GetSymbol
*/
void *Sys_GetSymbol( const char *moduleName, const char *symbolName )
{
	void *module = dlopen( moduleName, RTLD_NOW );
	if( module )
	{
		void *symbol = dlsym( module, symbolName );
		dlclose( module );
		return symbol;
	}
	return NULL;
}
#endif // SYS_SYMBOL
