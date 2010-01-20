#Kill LibpurpleAdapterExt if it's running
kill `ps -ef | grep LibpurpleAdapterExt | grep -v grep | awk '{print $2}'`
kill `ps -ef | grep LibpurpleAdapterExt | grep -v grep | awk '{print $2}'`
kill `ps -ef | grep LibpurpleAdapterExt | grep -v grep | awk '{print $2}'`

#Run
/usr/bin/LibpurpleAdapterExt

