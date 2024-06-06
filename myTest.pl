#!/bin/bash
#set -xv

dbdate=$1
base_dirname=$2

perl -ne 'print "$1\n" if m:<BizMsgIdr>(.*)</BizMsgIdr>:' $base_dirname/*/$dbdate/* > $SUMMITSPOOLDIR/aller_MsgRptIdr.txt
perl -ne 'print "$1\n" if m:<MsgRptIdr>(.*)</MsgRptIdr>:' $base_dirname/$dbdate/SFTR_retour/* > $SUMMITSPOOLDIR/retour_BizMsgIdr.txt
aller_retour_differences=`sort $SUMMITSPOOLDIR/aller_MsgRptIdr.txt $SUMMITSPOOLDIR/retour_BizMsgIdr.txt $SUMMITSPOOLDIR/retour_BizMsgIdr.txt | uniq -u`

aller_dirname="$base_dirname/*/$dbdate"
differences=()
IFS=" "
for BizMsgIdr in $echo $aller_retour_differences
do
	missigFilename=`find $aller_dirname -type f -exec grep -l $BizMsgIdr {} \; | xargs -L1 basename`
	differences+=($missigFilename)
done

echo $differences

exit 0

