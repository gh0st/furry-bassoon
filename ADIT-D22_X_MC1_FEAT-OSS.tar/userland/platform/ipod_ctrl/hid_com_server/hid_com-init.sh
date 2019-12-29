#!/bin/sh
#
#   /etc/init.d/hid_com-init: start or stop hid_com server for ipod_ctrl
#

#check_config() {
#}

start()
{	
	echo -n "starting IPOD_CTRL hid_com daemon"
	start-stop-daemon -S -b -x /bin/hid_com
	echo "."

	if [ $? != 0 ]; then
		echo
		echo "hid_com start failed!!"
		exit 1
	fi


	echo "."
}

stop()
{
	echo -n "Shutting down IPOD_CTRL hid_com daemon"
	start-stop-daemon -K -x /bin/hid_com
	echo "."
}

status()
{
	ps x | grep hid_com | grep -v grep
	echo "hid_com ^ ^ ^ ^ in ps ?"
}


case "$1" in
  start)
	start      
    ;;
    
  stop)
    stop    
    ;;
    
  status)
    status    
    ;;

  restart|force-reload)
    echo "Restarting IPOD_CTRL hid_com daemon"
    
	stop
	sleep 1
	start
    ;;
    
  *)
      echo "Usage: /etc/init.d/hid_com {start|stop|restart|force-reload|status}"
      exit 1
    ;;
esac

exit 0
    
