# Microsoft Developer Studio Project File - Name="libserveez" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libserveez - Win32 Debug Memory Leaks
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit\
 NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den\
 Befehl
!MESSAGE 
!MESSAGE NMAKE /f "libserveez.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "libserveez.mak" CFG="libserveez - Win32 Debug Memory Leaks"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "libserveez - Win32 Release" (basierend auf\
  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libserveez - Win32 Debug" (basierend auf\
  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libserveez - Win32 Debug Memory Leaks" (basierend auf\
  "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libserveez - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Opt"
# PROP Intermediate_Dir "Opt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "../.." /I ".." /D "NDEBUG" /D "_WINDOWS" /D "__SERVEEZ_EXPORT__" /D "WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /D __STDC__=0 /D "__BUILD_SVZ_LIBRARY__" /FD /c
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
# ADD LINK32 kernel32.lib ws2_32.lib mswsock.lib advapi32.lib user32.lib shell32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386

!ELSEIF  "$(CFG)" == "libserveez - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Dbg"
# PROP Intermediate_Dir "Dbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "../.." /I ".." /D "_DEBUG" /D "ENABLE_DEBUG" /D "_WINDOWS" /D "__SERVEEZ_EXPORT__" /D "WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /D __STDC__=0 /D "__BUILD_SVZ_LIBRARY__" /FD /c
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
# ADD LINK32 kernel32.lib ws2_32.lib mswsock.lib advapi32.lib user32.lib shell32.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386

!ELSEIF  "$(CFG)" == "libserveez - Win32 Debug Memory Leaks"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libserve"
# PROP BASE Intermediate_Dir "libserve"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Dbg"
# PROP Intermediate_Dir "Dbg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "../.." /I ".." /D "_DEBUG" /D "ENABLE_DEBUG" /D "_WINDOWS" /D "__SERVEEZ_EXPORT__" /D "WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /D __STDC__=0 /D "__BUILD_SVZ_LIBRARY__" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "../.." /I ".." /D "_WINDOWS" /D "__SERVEEZ_EXPORT__" /D "__BUILD_SVZ_LIBRARY__" /D "_DEBUG" /D "ENABLE_DEBUG" /D "WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /D __STDC__=0 /D "DEBUG_MEMORY_LEAKS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib ws2_32.lib mswsock.lib advapi32.lib user32.lib shell32.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386
# ADD LINK32 kernel32.lib ws2_32.lib mswsock.lib advapi32.lib user32.lib shell32.lib  imagehlp.lib /nologo /subsystem:windows /dll /pdb:none /debug /machine:I386

!ENDIF 

# Begin Target

# Name "libserveez - Win32 Release"
# Name "libserveez - Win32 Debug"
# Name "libserveez - Win32 Debug Memory Leaks"
# Begin Source File

SOURCE=.\alloc.c
# End Source File
# Begin Source File

SOURCE=.\array.c
# End Source File
# Begin Source File

SOURCE=.\binding.c
# End Source File
# Begin Source File

SOURCE=.\boot.c
# End Source File
# Begin Source File

SOURCE=.\core.c
# End Source File
# Begin Source File

SOURCE=.\coserver\coserver.c
# End Source File
# Begin Source File

SOURCE=.\coserver\dns.c
# End Source File
# Begin Source File

SOURCE=.\dynload.c
# End Source File
# Begin Source File

SOURCE=.\hash.c
# End Source File
# Begin Source File

SOURCE=".\icmp-socket.c"
# End Source File
# Begin Source File

SOURCE=.\coserver\ident.c
# End Source File
# Begin Source File

SOURCE=.\interface.c
# End Source File
# Begin Source File

SOURCE=.\passthrough.c
# End Source File
# Begin Source File

SOURCE=".\pipe-socket.c"
# End Source File
# Begin Source File

SOURCE=.\portcfg.c
# End Source File
# Begin Source File

SOURCE=".\raw-socket.c"
# End Source File
# Begin Source File

SOURCE=".\coserver\reverse-dns.c"
# End Source File
# Begin Source File

SOURCE=".\server-core.c"
# End Source File
# Begin Source File

SOURCE=".\server-loop.c"
# End Source File
# Begin Source File

SOURCE=".\server-socket.c"
# End Source File
# Begin Source File

SOURCE=.\server.c
# End Source File
# Begin Source File

SOURCE=.\snprintf.c
# End Source File
# Begin Source File

SOURCE=.\socket.c
# End Source File
# Begin Source File

SOURCE=.\sparsevec.c
# End Source File
# Begin Source File

SOURCE=".\tcp-socket.c"
# End Source File
# Begin Source File

SOURCE=".\udp-socket.c"
# End Source File
# Begin Source File

SOURCE=.\util.c
# End Source File
# Begin Source File

SOURCE=.\vector.c
# End Source File
# Begin Source File

SOURCE=.\windoze.c
# End Source File
# End Target
# End Project
