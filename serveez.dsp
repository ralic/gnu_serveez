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
# PROP BASE Output_Dir "Opt"
# PROP BASE Intermediate_Dir "Opt"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Opt"
# PROP Intermediate_Dir "Opt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /Ob2 /I "." /I ".." /I "../sizzle" /I "./src" /I "../.." /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "_WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /D __STDC__=0 /D "__SIZZLE_IMPORT__" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib wsock32.lib advapi32.lib shell32.lib user32.lib sizzle.lib /nologo /subsystem:console /machine:I386 /libpath:"../sizzle/libsizzle/Opt"

!ELSEIF  "$(CFG)" == "serveez - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "." /I ".." /I "../sizzle" /I "./src" /I "../.." /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_WIN32" /D "__MINGW32__" /D "HAVE_CONFIG_H" /D __STDC__=0 /D "__SIZZLE_IMPORT__" /D ENABLE_DEBUG=1 /FD /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib wsock32.lib advapi32.lib shell32.lib user32.lib sizzle.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept /libpath:"../sizzle/libsizzle/Dbg"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "serveez - Win32 Release"
# Name "serveez - Win32 Debug"
# Begin Source File

SOURCE=.\src\alloc.c
# End Source File
# Begin Source File

SOURCE=".\src\awcs-server\awcs-proto.c"
# End Source File
# Begin Source File

SOURCE=.\src\cfgfile.c
# End Source File
# Begin Source File

SOURCE=.\src\connect.c
# End Source File
# Begin Source File

SOURCE=".\src\ctrl-server\control-proto.c"
# End Source File
# Begin Source File

SOURCE=.\src\coserver\coserver.c
# End Source File
# Begin Source File

SOURCE=.\src\coserver\dns.c
# End Source File
# Begin Source File

SOURCE=".\src\foo-server\foo-proto.c"
# End Source File
# Begin Source File

SOURCE=".\src\nut-server\gnutella.c"
# End Source File
# Begin Source File

SOURCE=.\src\hash.c
# End Source File
# Begin Source File

SOURCE=".\src\http-server\http-cache.c"
# End Source File
# Begin Source File

SOURCE=".\src\http-server\http-cgi.c"
# End Source File
# Begin Source File

SOURCE=".\src\http-server\http-core.c"
# End Source File
# Begin Source File

SOURCE=".\src\http-server\http-dirlist.c"
# End Source File
# Begin Source File

SOURCE=".\src\http-server\http-proto.c"
# End Source File
# Begin Source File

SOURCE=.\src\icmp-socket.c
# End Source File
# Begin Source File

SOURCE=.\src\coserver\ident.c
# End Source File
# Begin Source File

SOURCE=.\src\interface.c
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-config.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-core\irc-core.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-crypt.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-event-1.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-event-2.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-event-3.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-event-4.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-event-5.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-event-6.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-event-7.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-proto.c"
# End Source File
# Begin Source File

SOURCE=".\src\irc-server\irc-server.c"
# End Source File
# Begin Source File

SOURCE=".\src\nut-server\nut-core.c"
# End Source File
# Begin Source File

SOURCE=".\src\nut-server\nut-hostlist.c"
# End Source File
# Begin Source File

SOURCE=".\src\nut-server\nut-request.c"
# End Source File
# Begin Source File

SOURCE=".\src\nut-server\nut-route.c"
# End Source File
# Begin Source File

SOURCE=".\src\nut-server\nut-transfer.c"
# End Source File
# Begin Source File

SOURCE=.\src\option.c
# End Source File
# Begin Source File

SOURCE=".\src\pipe-socket.c"
# End Source File
# Begin Source File

SOURCE=".\src\q3key-server\q3key-proto.c"
# End Source File
# Begin Source File

SOURCE=".\src\coserver\reverse-dns.c"
# End Source File
# Begin Source File

SOURCE=.\src\serveez.c
# End Source File
# Begin Source File

SOURCE=".\src\server-core.c"
# End Source File
# Begin Source File

SOURCE=".\src\server-loop.c"
# End Source File
# Begin Source File

SOURCE=".\src\server-socket.c"
# End Source File
# Begin Source File

SOURCE=.\src\server.c
# End Source File
# Begin Source File

SOURCE=.\src\snprintf.c
# End Source File
# Begin Source File

SOURCE=.\src\socket.c
# End Source File
# Begin Source File

SOURCE=".\src\udp-socket.c"
# End Source File
# Begin Source File

SOURCE=.\src\util.c
# End Source File
# Begin Source File

SOURCE=.\src\windoze.c
# End Source File
# End Target
# End Project
