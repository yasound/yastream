#!/bin/bash
# wrapper d'execution du streamer Yasound.
# definition du librayry path pour inclure la lib necessaire au fonctionnement.
# /data/glusterfs-mnt/replica2all/streamer/current pointe vers le dossier de prod actuel.
# current est un symlink vers le dossier contenant l'application en prod.
# pour changer de version il faut changer le symlink puis killer toutes les instances et les relancer.

MAILDEST="seb@yasound.com"
#MAILDEST="sebastien@iguanesolutions.com, dev@yasound.com"
SUBJECT="[YASOUND][yastream-wrapper][FAILED] `hostname -f`"
LOGS="/home/customer/streamer/work/yastream/yastream.log"
USER="customer"

CMD="./yastream \
-syslog \
-port 8000 \
-host `hostname -a`.ig-1.net \
-datapath /data/glusterfs-mnt/replica2all/song/ \
-redishost yas-sql-01.sadm.ig-1.net \
-flushdb \
-redisport 6379 >>$LOGS"

export LD_LIBRARY_PATH=/home/customer/streamer/work/nui3/bin/debug/:$LD_LIBRARY_PATH
cd /home/customer/streamer/work/yastream/bin/debug/

whoami=$(whoami)

if ! [ "x${whoami}" = "xroot" -o "x${whoami}" = "xmeeloo" ];then
  echo "error id"
  exit 2
fi

start_service ()
{
  CMDLOG="${CMD}"
  if [ "x${whoami}" == "xroot" ];then
    echo "launching service with su"
    su ${USER} -s /bin/bash -c "${CMDLOG}"
  else
    echo "launching service without su"
    /bin/bash -c "${CMDLOG}"
  fi
}

stop_service ()
{
  /usr/bin/pgrep yastream|while read p2k;do kill -9 $p2k;done
  echo "Done"
}

case $1 in
  test)
      echo "Lancement du service en debug. Ne rends pas la main"
      start_service
  ;;
  loop)
      while ( true )
      do
        start_service
        /usr/bin/logger -t yastream-wrapper "Arret non prévu du service yastream."
      echo "`date` : Arret non prévu du service yastream. Restart dans 60 secondes."|mail -s "$SUBJECT" $MAILDEST
        sleep 60
      done
  ;;
  start)
      echo "Lancement du service en demon. "
      $0 loop &
  ;;
  stop)
      echo "Arret du service."
      /usr/bin/pgrep yastream|while read p2k;do kill -9 $p2k;done
  ;;
  *) echo "Argument inconnu. Usage : $0 (test|start|stop)"
  ;;
esac
