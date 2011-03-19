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
exec 1>/media/internal/DebugMP/DebugMP_Account_Creation_$(date +%m-%d-%Y-%H-%M-%S).log

#Kill org.webosinternals.imaccountvalidator if it's running
killall -HUP org.webosinternals.imaccountvalidator || true
killall -HUP org.webosinternals.imaccountvalidator || true
killall -HUP org.webosinternals.imaccountvalidator || true

#Run
/var/usr/sbin/org.webosinternals.imaccountvalidator

