#!/bin/bash
#set -xv

if [ $# -ne 3 ]; then
    echo "Error: Three parameters are required: DATE(YYYYMMDD) reportType and assetClass."
    exit 1
fi

dbdate="$1"
rprtType="$2"
assetClass="$3"

# Ensure SUMMITSPOOLDIR is defined
if [ -z "$SUMMITSPOOLDIR" ]; then
    echo "SUMMITSPOOLDIR is not set."
    exit 1
fi

base_dirname="$SUMMITSPOOLDIR/DTCC_REFIT/$rprtType/$assetClass"
aller_dirname="$base_dirname/$dbdate/Aller"
retour_dirname="$base_dirname/$dbdate/Retour"

echo "$aller_dirname"
echo "$retour_dirname"

# Ensure aller_dirname is well defined
if [ -z "$aller_dirname" ]; then
    echo "$aller_dirname is not set."
    exit 1
fi

differences=()
IFS=" "

aller_file_name="emir_aller_${assetClass}${rprtType}_${dbdate}.txt"
retour_file_name="emir_retour_${assetClass}${rprtType}_${dbdate}.txt"
diff_file_name="emir_diff_${assetClass}${rprtType}_${dbdate}.txt"

# Create the diff output file
touch "$SUMMITSPOOLDIR/$diff_file_name"

echo "$aller_file_name"
echo "$retour_file_name"
echo "$diff_file_name"


# Ensure retour_dirname exit
# If not retour ALL sent files
if [ -z "$retour_dirname" ]; then
    echo "$retour_dirname does not exist."

    for file in "$aller_dirname"/*; do
        differences+=("$(basename "$file")")
    done

    if [[ -n "${differences[*]}" ]]; then
         echo "${differences[@]}" > "$SUMMITSPOOLDIR/$diff_file_name"
    fi
     exit 0
fi

find "$aller_dirname" -type f -exec perl -ne 'print "$1\n" if m:<BizMsgIdr>(.*)</BizMsgIdr>:' {} + > "$SUMMITSPOOLDIR/$aller_file_name"
find "$retour_dirname" -type f -exec perl -ne 'print "$1\n" if m:<BizMsgIdr>(.*)</BizMsgIdr>:' {} + > "$SUMMITSPOOLDIR/$retour_file_name"

# Find differences
aller_retour_differences=$(sort "$SUMMITSPOOLDIR/$aller_file_name" "$SUMMITSPOOLDIR/$retour_file_name" "$SUMMITSPOOLDIR/$retour_file_name" | uniq -u)

for BizMsgIdr in $aller_retour_differences
do
    missingFilename=$(find "$aller_dirname" -type f -exec grep -l "$BizMsgIdr" {} \; | xargs -L1 basename)
    differences+=("$missingFilename")
done

if [[ -n "${differences[*]}" ]]; then
    echo "${differences[@]}" > "$SUMMITSPOOLDIR/$diff_file_name"
fi

exit 0
