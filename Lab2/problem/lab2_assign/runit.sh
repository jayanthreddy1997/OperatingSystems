#!/bin/bash

# Author: Hubertus Franke  (frankeh@cims.nyu.edu)

#     if you want -v output  run with 
#                  ./runit.sh youroutputdir   sched -v 

#------- some simple error checking on the parameters
usage() {
    [[ "${1}" == "" ]] || echo "${1}"
    echo "usage: $0 <outputdir> <scheduler+optionalargs>"
    exit
}

[[ ${#} -lt 2 ]]   && usage ""                             # check right arguments
[[ ! -d ${1} ]]    && usage "<${1}> not a valid directory" # check output dir
[[ ! -f ${2} ]]    && usage "<${2}> is not a file"         # check scheduler exists
[[ ! -x ${2} ]]    && usage "<${2}> is not an executable"  # check scheduler is executable

#------- passed basic test now lets run

OUTDIR=${1}
shift
#SCHED=${*:-"please-provide-executable-with-additional-flags"
SCHED=${*}
[[ ${} -lt 2 ]] && usage "shit"


if [[ "${SCHED}" == "" ]]; then
    echo "no scheduler specified"
    exit
fi
    
echo "sched=<$SCHED> outdir=<$OUTDIR>"

# if you want -v output  run with ...  ./runit.sh youroutputdir   sched -v 

RFILE=./rfile
INS="0 1 2 3 4 5 6"

SCHEDS="  F    L    S   R2    R5    P2   P5:3  E2:5 E4"  

#SCHEDS="   F"     # example if you only want to run the F scheduler during development        


for f in ${INS}; do
	for s in ${SCHEDS}; do 
		echo "${SCHED} ${SCHEDARGS} -s${s} input${f} ${RFILE}"
		${SCHED} -s${s} input${f} ${RFILE} > ${OUTDIR}/out_${f}_${s//\:/_}
	done
done

