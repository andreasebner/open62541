#!/bin/bash

#program path or install dir
LOC=/home/developer/projects/open62541-fork/cmake-build-debug/bin/examples/
IF=wlp3s0
CPU_NR=2
PRIO=90

reset() {
  echo ondemand >/sys/devices/system/cpu/cpu$CPU_NR/cpufreq/scaling_governor
  #standard on most systems. ondemand -> Dynamic CPU-Freq.
  cd /sys/devices/system/cpu/cpu$CPU_NR/cpuidle
  for i in *
  do
    echo 0 >$i/disable
    #~disable sleep state
  done

  phy=$IF #get interface name
  for i in `ps ax | grep -v grep | grep $phy | sed "s/^ //" | cut -d" " -f1`
  #get pid's from interface irq
  do
    taskset -pc 0-$CPU_NR $i >/dev/null
    #retrive or set a process's CPU affinity
    chrt -pf 50 $i
    #manipulate the real-time attributes of a process -p priority -f scheduling policy to SCHED_FIFO
  done
  systemctl start irqbalance
  #distribute hardware interrupts across processsors on a muliprocessor system
}

trap reset ERR
systemctl stop irqbalance

phy=$IF
for i in `ps ax | grep -v grep | grep $phy | sed "s/^ //" | cut -d" " -f1`
do
  taskset -pc $CPU_NR $i >/dev/null
  chrt -pf $PRIO $i
done

cd /sys/devices/system/cpu/cpu$CPU_NR/cpuidle
for i in `ls -1r`
do
  echo 1 >$i/disable
done

echo performance >/sys/devices/system/cpu/cpu$CPU_NR/cpufreq/scaling_governor

cd $LOC
taskset -c $CPU_NR chrt -f $PRIO $LOC/pubsub_interrupt_publish
