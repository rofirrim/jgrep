#!/bin/bash

export EXTRAE_HOME=$HOME/soft/extrae/install

source ${EXTRAE_HOME}/etc/extrae.sh

# cp -v ${EXTRAE_HOME}/share/example/PTHREAD/extrae.xml .

export EXTRAE_CONFIG_FILE=extrae.xml
LD_PRELOAD=${EXTRAE_HOME}/lib/libpttrace.so $*
