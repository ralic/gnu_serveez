# Microsoft Developer Studio Project File - Name="libsizzle" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libsizzle - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit\
 NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den\
 Befehl
!MESSAGE 
!MESSAGE NMAKE /f "libsizzle.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "libsizzle.mak" CFG="libsizzle - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "libsizzle - Win32 Release" (basierend auf\
  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libsizzle - Win32 Debug" (basierend auf\
  "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libsizzle - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Opt"
# PROP BASE Intermediate_Dir "Opt"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Opt"
# PROP Intermediate_Dir "Opt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /Ob2 /I "." /I ".." /D "_WINDOWS" /D "__SIZZLE_EXPORT__" /D "WIN32" /D "NDEBUG" /D "_WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386

!ELSEIF  "$(CFG)" == "libsizzle - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Dbg"
# PROP BASE Intermediate_Dir "Dbg"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Dbg"
# PROP Intermediate_Dir "Dbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /Zi /Od /I "." /I ".." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_WIN32" /D "__SIZZLE_EXPORT__" /D "HAVE_CONFIG_H" /D "__MINGW32__" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386
# SUBTRACT LINK32 /map /nodefaultlib

!ENDIF 

# Begin Target

# Name "libsizzle - Win32 Release"
# Name "libsizzle - Win32 Debug"
# Begin Source File

SOURCE=.\alloc.c
# End Source File
# Begin Source File

SOURCE=.\boolean.c
# End Source File
# Begin Source File

SOURCE=.\capi.c
# End Source File
# Begin Source File

SOURCE=.\char.c
# End Source File
# Begin Source File

SOURCE=.\control.c
# End Source File
# Begin Source File

SOURCE=.\cvar.c
# End Source File
# Begin Source File

SOURCE=.\doc.c
# End Source File
# Begin Source File

SOURCE=.\dynlib.c
# End Source File
# Begin Source File

SOURCE=.\eval.c
# End Source File
# Begin Source File

SOURCE=.\fdport.c
# End Source File
# Begin Source File

SOURCE=.\fport.c
# End Source File
# Begin Source File

SOURCE=.\gc.c
# End Source File
# Begin Source File

SOURCE=.\hashtab.c
# End Source File
# Begin Source File

SOURCE=.\iofun.c
# End Source File
# Begin Source File

SOURCE=.\list.c
# End Source File
# Begin Source File

SOURCE=.\macros.c
# End Source File
# Begin Source File

SOURCE=.\misc.c
# End Source File
# Begin Source File

SOURCE=.\modules.c
# End Source File
# Begin Source File

SOURCE=.\net.c
# End Source File
# Begin Source File

SOURCE=.\nls.c
# End Source File
# Begin Source File

SOURCE=.\node.c
# End Source File
# Begin Source File

SOURCE=.\number.c
# End Source File
# Begin Source File

SOURCE=.\pointer.c
# End Source File
# Begin Source File

SOURCE=.\port.c
# End Source File
# Begin Source File

SOURCE=.\posix.c
# End Source File
# Begin Source File

SOURCE=.\profile.c
# End Source File
# Begin Source File

SOURCE=.\promise.c
# End Source File
# Begin Source File

SOURCE=.\read.c
# End Source File
# Begin Source File

SOURCE=.\regexp.c
# End Source File
# Begin Source File

SOURCE=.\scmport.c
# End Source File
# Begin Source File

SOURCE=.\sig.c
# End Source File
# Begin Source File

SOURCE=.\sport.c
# End Source File
# Begin Source File

SOURCE=".\srfi-1.c"
# End Source File
# Begin Source File

SOURCE=".\srfi-13.c"
# End Source File
# Begin Source File

SOURCE=.\stringfun.c
# End Source File
# Begin Source File

SOURCE=.\symbol.c
# End Source File
# Begin Source File

SOURCE=.\syntax.c
# End Source File
# Begin Source File

SOURCE=.\uvector.c
# End Source File
# Begin Source File

SOURCE=.\uvector64.c
# End Source File
# Begin Source File

SOURCE=.\values.c
# End Source File
# Begin Source File

SOURCE=.\vector.c
# End Source File
# End Target
# End Project
