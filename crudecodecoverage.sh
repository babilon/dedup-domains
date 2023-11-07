#!/bin/bash

TAGLINES=linetags.fmt
RAWLINES=lineshit.raw
FMTLINES=lineshit.fmt
WINLINES=lineshit.win

/usr/bin/git grep -nE "^\s+(\<ADD_CC\>|\<ADD_TCC\>|\<ADD_CC_SINGLE\>);$" "*.c" | sed 's/:/ /g' | awk '{ printf("%20s %5d\n", $1, $2) }' > ./${TAGLINES}


./bin/codecoverage.real -t -i 5 -r 5
ret=$?
if [[ $ret -ne 0 ]]; then
    echo "Returned error code: $ret"
    exit
fi

cat ./${RAWLINES} | awk '{ printf("%20s %5d\n", $1, $2) }' > ./${FMTLINES}

comm -23 <(sort -u ./${TAGLINES}) <(sort -u ./${FMTLINES}) > ${WINLINES}

echo "Lines tagged (should be > zero):"
wc -l ./${TAGLINES}
echo "Lines hit (raw | formatted):"
wc -l ./${RAWLINES} ./${FMTLINES}
echo "Lines that were NOT hit (should be zero):"
wc -l ./${WINLINES}

RED='\033[0;31m'
GRN='\033[0;34m'
NC='\033[0m'
if [ ! -s ./${WINLINES} ]; then
    echo -e "${GRN}All tagged lines were hit!${NC}"
    rm ./${WINLINES}
else
    echo -e "${RED}NOTICE: At least one tagged line was NOT hit.${NC}"
fi
