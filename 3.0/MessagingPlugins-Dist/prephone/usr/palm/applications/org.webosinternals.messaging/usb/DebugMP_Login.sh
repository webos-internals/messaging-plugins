#Create Directory if it does not exsist already
directory="/media/internal/DebugMP"

# bash check if directory exists
if [ -d $directory ]; then
	echo "Directory exists"
	else 
		echo "Directory does not exist"
		echo "Creating Directory"
		mkdir /media/internal/DebugMP
	fi 

#Write Output to file on USB Drive
exec 1>/media/internal/DebugMP/DebugMP_Login_$(date +%m-%d-%Y-%H-%M-%S).log

#Kill org.webosinternals.imlibpurpletransport if it's running
killall -HUP org.webosinternals.imlibpurpletransport || true
killall -HUP org.webosinternals.imlibpurpletransport || true
killall -HUP org.webosinternals.imlibpurpletransport || true

#Run
/var/usr/sbin/org.webosinternals.imlibpurpletransport

