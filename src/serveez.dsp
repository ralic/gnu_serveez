# Microsoft Developer Studio Project File - Name="serveez" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=serveez - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit\
 NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den\
 Befehl
!MESSAGE 
!MESSAGE NMAKE /f "serveez.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "serveez.mak" CFG="serveez - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "serveez - Win32 Release" (basierend auf\
  "Win32 (x86) Console Application")
!MESSAGE "serveez - Win32 Debug" (basierend auf\
  "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "serveez - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I ".." /I "." /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "__SERVEEZ_IMPORT__" /D "__SIZZLE_IMPORT__" /D "WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /D __STDC__=0 /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib libserveez.lib ws2_32.lib libsizzle.lib shell32.lib /nologo /subsystem:console /pdb:none /machine:I386 /libpath:"libserveez/Opt" /libpath:"../../sizzle/libsizzle/Opt"

!ELSEIF  "$(CFG)" == "serveez - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I ".." /I "." /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "__SERVEEZ_IMPORT__" /D "__SIZZLE_IMPORT__" /D "ENABLE_DEBUG" /D "WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /D __STDC__=0 /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib libserveez.lib ws2_32.lib libsizzle.lib shell32.lib /nologo /subsystem:console /pdb:none /debug /machine:I386 /libpath:"libserveez/Dbg" /libpath:"../../sizzle/libsizzle/Dbg"

!ENDIF 

# Begin Target

# Name "serveez - Win32 Release"
# Name "serveez - Win32 Debug"
# Begin Source File

SOURCE=".\awcs-server\awcs-proto.c"
# End Source File
# Begin Source File

SOURCE=.\cfgfile.c
# End Source File
# Begin Source File

SOURCE=".\ctrl-server\control-proto.c"
# End Source File
# Begin Source File

SOURCE=".\foo-server\foo-proto.c"
# End Source File
# Begin Source File

SOURCE=".\nut-server\gnutella.c"
# End Source File
# Begin Source File

SOURCE=".\http-server\http-cache.c"
# End Source File
# Begin Source File

SOURCE=".\http-server\http-cgi.c"
# End Source File
# Begin Source File

SOURCE=".\http-server\http-core.c"
# End Source File
# Begin Source File

SOURCE=".\http-server\http-dirlist.c"
# End Source File
# Begin Source File

SOURCE=".\http-server\http-proto.c"
# End Source File
# Begin Source File

SOURCE=".\fakeident-server\ident-proto.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-config.c"
# End Source File
# Begin Source File

SOURCE=".\irc-core\irc-core.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-crypt.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-event-1.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-event-2.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-event-3.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-event-4.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-event-5.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-event-6.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-event-7.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-proto.c"
# End Source File
# Begin Source File

SOURCE=".\irc-server\irc-server.c"
# End Source File
# Begin Source File

SOURCE=".\nut-server\nut-core.c"
# End Source File
# Begin Source File

SOURCE=".\nut-server\nut-hostlist.c"
# End Source File
# Begin Source File

SOURCE=".\nut-server\nut-request.c"
# End Source File
# Begin Source File

SOURCE=".\nut-server\nut-route.c"
# End Source File
# Begin Source File

SOURCE=".\nut-server\nut-transfer.c"
# End Source File
# Begin Source File

SOURCE=.\option.c
# End Source File
# Begin Source File

SOURCE=.\serveez.c
# End Source File
# Begin Source File

SOURCE=.\serveez.rc

!IF  "$(CFG)" == "serveez - Win32 Release"

!ELSEIF  "$(CFG)" == "serveez - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sizzle.c
# End Source File
# Begin Source File

SOURCE=".\sntp-server\sntp-proto.c"
# End Source File
# Begin Source File

SOURCE=".\tunnel-server\tunnel.c"
# End Source File
# End Target
# End Project
