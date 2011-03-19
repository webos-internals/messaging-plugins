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

# Package Plugins
echo ""
echo "Packing Messaging Plugins..."
if [ "$BuildType" = "prephone" ]
then
{
	#Compile Pixi
	cp control.armv6 prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build $BuildType
	
	#Compile Pre
	cp control.armv7 prephone/CONTROL/control
	sh /root/optware/i686g25/staging/bin/ipkg-build $BuildType
	
	#Cleanup
	rm -f prephone/CONTROL/control
	
	mv com.palm.webosinternals.messaging_3.0.0_armv6.ipk org.webosinternals.messaging_3.0.0_armv6.ipk
	mv com.palm.webosinternals.messaging_3.0.0_armv7.ipk org.webosinternals.messaging_3.0.0_armv7.ipk
}
else
{
	sh /root/optware/i686g25/staging/bin/ipkg-build $BuildType
	mv com.palm.webosinternals.messaging_3.0.0_i686.ipk org.webosinternals.messaging_3.0.0_i686.ipk
}
fi