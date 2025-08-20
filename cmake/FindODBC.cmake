
# Find ODBC (or UnixODBC) for Windows 10
 
# Determina piattaforma ODBC (x86/x64)
if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    set(ODBC_PLATFORM_DIR "x64")
else()
    set(ODBC_PLATFORM_DIR "x86")
endif()
 
  
  
  
# Imposta i percorsi base dei Windows Kits
set(_WINDOWS_KITS_INCLUDE "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um")
set(_WINDOWS_KITS_LIB "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/${ODBC_PLATFORM_DIR}")

# Trova la directory degli header ODBC
find_path(ODBC_INCLUDE_DIR NAMES sql.h
    HINTS
		/usr/include
		/usr/include/odbc
		/usr/local/include
		/usr/local/include/odbc
		/usr/local/odbc/include
		${_WINDOWS_KITS_INCLUDE}
    DOC "The ODBC include directory"
)

# Trova la libreria ODBC
find_library(ODBC_LIBRARY NAMES iodbc odbc odbc32
    HINTS
	    /usr/lib
		/usr/lib/odbc
		/usr/local/lib
		/usr/local/lib/odbc
		/usr/local/odbc/lib
        ${_WINDOWS_KITS_LIB}
    DOC "The ODBC library"
)

 
# Gestione standard args
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ODBC DEFAULT_MSG ODBC_LIBRARY ODBC_INCLUDE_DIR)

# Test se la libreria funziona
 
if(ODBC_FOUND)
  include(MacroCheckLibraryWorks)
  CHECK_LIBRARY_WORKS("sql.h" "SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, 0);" "${ODBC_INCLUDE_DIR}" "${ODBC_LIBRARY}" "ODBC_WORKS")
  if (NOT ${ODBC_WORKS})
      set(ODBC_FOUND FALSE)
  endif()

  set( ODBC_LIBRARIES ${ODBC_LIBRARY} )
  set( ODBC_INCLUDE_DIRS ${ODBC_INCLUDE_DIR} )
endif(ODBC_FOUND)




mark_as_advanced(ODBC_INCLUDE_DIR ODBC_LIBRARY)
