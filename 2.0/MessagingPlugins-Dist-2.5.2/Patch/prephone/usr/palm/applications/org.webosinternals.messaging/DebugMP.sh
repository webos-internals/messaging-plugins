#Create Directory if it does not exsist already
directory="/media/internal/DebugMP"

# bash check if directory exists
if [ -d $directory ]; then
	echo "Directory exists"
	else 
		echo "Directory does not exists"
		echo "Creating Directory"
		mkdir /media/internal/DebugMP
	fi 

#Write Output to file on USB Drive
exec 1>/media/internal/DebugMP/DebugMP.$(date +%m-%d-%Y-%H-%M-%S).log

#Kill LibpurpleAdapterExt if it's running
kill `ps -ef | grep LibpurpleAdapterExt | grep -v grep | awk '{print $2}'`
kill `ps -ef | grep LibpurpleAdapterExt | grep -v grep | awk '{print $2}'`
kill `ps -ef | grep LibpurpleAdapterExt | grep -v grep | awk '{print $2}'`

#Run
/usr/bin/LibpurpleAdapterExt

