if [ "$1" = "" ]
then
	exit 1
fi

#Get Commandline
BuildType=$1

#Get Compiler
if [ "$BuildType" = "prephone" ]
then
	BuildCompiler="cs08q1armel"
else
	BuildCompiler="i686g25"
fi

# Package Sys Updates
echo ""
echo "Packing System Updates..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp SysUpdate/control.armv6 SysUpdate/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build SysUpdate/$BuildType
	
	#Compile Pre
	cp SysUpdate/control.armv7 SysUpdate/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build SysUpdate/$BuildType
	
	#Cleanup
	rm -f SysUpdate/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build SysUpdate/$BuildType
}
fi

# Package Patch
echo ""
echo "Packing Patch..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp Patch/control.armv6 Patch/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Patch/$BuildType
	
	#Compile Pre
	cp Patch/control.armv7 Patch/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Patch/$BuildType
	
	#Cleanup
	rm -f Patch/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build Patch/$BuildType
}
fi

#Package all plugins
echo ""
echo "Packing All Plugins..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp AllPlugins/control.armv6 AllPlugins/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build AllPlugins/$BuildType
	
	#Compile Pre
	cp AllPlugins/control.armv7 AllPlugins/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build AllPlugins/$BuildType
	
	#Cleanup
	rm -f AllPlugins/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build AllPlugins/$BuildType
}
fi

#Package Groupwise plugin
echo ""
echo "Packing Groupwise Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp Groupwise/control.armv6 Groupwise/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Groupwise/$BuildType
	
	#Compile Pre
	cp Groupwise/control.armv7 Groupwise/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Groupwise/$BuildType
	
	#Cleanup
	rm -f Groupwise/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build Groupwise/$BuildType
}
fi

#Package ICQ plugin
echo ""
echo "Packing ICQ Plugin..."
sh /root/optware/i686g25/staging/bin/ipkg-build ICQ

#Package IRC plugin
echo ""
echo "Packing IRC Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp IRC/control.armv6 IRC/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build IRC/$BuildType
	
	#Compile Pre
	cp IRC/control.armv7 IRC/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build IRC/$BuildType
	
	#Cleanup
	rm -f IRC/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build IRC/$BuildType
}
fi

#Package Jabber plugin
echo ""
echo "Packing Jabber Plugin..."
sh /root/optware/i686g25/staging/bin/ipkg-build Jabber

#Package Live plugin
echo ""
echo "Packing Live Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp Live/control.armv6 Live/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Live/$BuildType
	
	#Compile Pre
	cp Live/control.armv7 Live/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Live/$BuildType
	
	#Cleanup
	rm -f Live/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build Live/$BuildType
}
fi

#Package Office Communicator plugin
echo ""
echo "Packing Office Communicator Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp OfficeCommunicator/control.armv6 OfficeCommunicator/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build OfficeCommunicator/$BuildType
	
	#Compile Pre
	cp OfficeCommunicator/control.armv7 OfficeCommunicator/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build OfficeCommunicator/$BuildType
	
	#Cleanup
	rm -f OfficeCommunicator/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build OfficeCommunicator/$BuildType
}
fi

#Package QQ plugin
echo ""
echo "Packing QQ Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp QQ/control.armv6 QQ/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build QQ/$BuildType
	
	#Compile Pre
	cp QQ/control.armv7 QQ/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build QQ/$BuildType
	
	#Cleanup
	rm -f QQ/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build QQ/$BuildType
}
fi

#Package Sametime plugin
echo ""
echo "Packing Sametime Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp Sametime/control.armv6 Sametime/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Sametime/$BuildType
	
	#Compile Pre
	cp Sametime/control.armv7 Sametime/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Sametime/$BuildType
	
	#Cleanup
	rm -f Sametime/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build Sametime/$BuildType
}
fi

#Package XFire plugin
echo ""
echo "Packing XFire Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp XFire/control.armv6 XFire/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build XFire/$BuildType
	
	#Compile Pre
	cp XFire/control.armv7 XFire/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build XFire/$BuildType
	
	#Cleanup
	rm -f XFire/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build XFire/$BuildType
}
fi

#Package Facebook plugin
echo ""
echo "Packing Facebook Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp Facebook/control.armv6 Facebook/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Facebook/$BuildType
	
	#Compile Pre
	cp Facebook/control.armv7 Facebook/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Facebook/$BuildType
	
	#Cleanup
	rm -f Facebook/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build Facebook/$BuildType
}
fi

#Package Gadu Gadu plugin
echo ""
echo "Packing Gadu Gadu Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp Gadu/control.armv6 Gadu/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Gadu/$BuildType
	
	#Compile Pre
	cp Gadu/control.armv7 Gadu/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build Gadu/$BuildType
	
	#Cleanup
	rm -f Gadu/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build Gadu/$BuildType
}
fi

#Package MySpace plugin
echo ""
echo "Packing MySpace Plugin..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp MySpace/control.armv6 MySpace/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build MySpace/$BuildType
	
	#Compile Pre
	cp MySpace/control.armv7 MySpace/prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build MySpace/$BuildType
	
	#Cleanup
	rm -f MySpace/prephone/CONTROL/control
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build MySpace/$BuildType
}
fi